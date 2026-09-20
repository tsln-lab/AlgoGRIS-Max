/// @file
/// @license GPL-3.0-or-later, like AlgoGRIS.
///
/// The host code here follows SpatGRIS 4.1.5: sg_AudioProcessor.cpp (processAudio,
/// processInputPeaks, processOutputModifiersAndPeaks), sg_OscInput.cpp (message
/// parsing) and sg_MainComponent.cpp (setSourcePosition, setLegacySourcePosition).

#include "algogris_engine.hpp"

#include <sg_AbstractSpatAlgorithm.hpp>
#include <juce_audio_formats/juce_audio_formats.h>
#include <Data/sg_LegacyLbapPosition.hpp>
#include <Data/sg_LogicStrucs.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <mutex>
#include <cmath>
#include <optional>

#ifdef ALGOGRIS_MONITOR_DEBUG
    #include <cstdio>
#endif

#if defined(_WIN32)
    #define WIN32_LEAN_AND_MEAN
    #define NOMINMAX
    #include <windows.h>
#else
    #include <dlfcn.h>
#endif

namespace algogris_max
{
using namespace gris;

namespace
{
//==============================================================================
/** One speaker binauralised: its head-related impulse response and the tail of its feed. */
struct MonitorVoice {
    output_patch_t patch{};
    std::vector<float> left{};    ///< HRIR for the listener's left ear.
    std::vector<float> right{};
    std::vector<float> history{}; ///< Ring buffer of past samples, as long as the HRIR.
    size_t writeIndex{};
};

/** Everything the audio thread touches. Built on the main thread, then swapped in whole. */
struct Renderer {
    SpatGrisData data{};
    std::unique_ptr<AudioConfig> config{};
    std::unique_ptr<AbstractSpatAlgorithm> algorithm{};
    SourceAudioBuffer sourceBuffer{};
    SpeakerAudioBuffer speakerBuffer{};
    juce::AudioBuffer<float> stereoBuffer{};
    SourcePeaks sourcePeaks{};
    std::array<ColdSpeakerHighpass, MAX_NUM_SPEAKERS + 1> highpassStates{};
    juce::Random random{};
    int numSources{};
    int numOutputs{};
    int blockSize{};
    bool stereoOutput{};
    std::vector<MonitorVoice> monitorVoices{};
};

//==============================================================================
/** Temporarily changes the working directory. AlgoGRIS's binaural mode finds its files relative to it. */
class ScopedWorkingDirectory
{
    juce::File mPrevious{ juce::File::getCurrentWorkingDirectory() };

public:
    explicit ScopedWorkingDirectory(juce::File const & dir) { dir.setAsCurrentWorkingDirectory(); }
    ~ScopedWorkingDirectory() { mPrevious.setAsCurrentWorkingDirectory(); }
    ScopedWorkingDirectory(ScopedWorkingDirectory const &) = delete;
    ScopedWorkingDirectory & operator=(ScopedWorkingDirectory const &) = delete;
};

//==============================================================================
std::string errorToString(AbstractSpatAlgorithm::Error const error)
{
    switch (error) {
    case AbstractSpatAlgorithm::Error::notEnoughDomeSpeakers:
        return "not enough speakers for a dome (VBAP) layout";
    case AbstractSpatAlgorithm::Error::notEnoughCubeSpeakers:
        return "not enough speakers for a cube (MBAP) layout";
    case AbstractSpatAlgorithm::Error::flatDomeSpeakersTooFarApart:
        return "speakers of this flat dome are too far apart";
    case AbstractSpatAlgorithm::Error::failedToSpawnThreadpool:
        return "could not start the multicore thread pool";
    }
    return "unknown error";
}

//==============================================================================
std::string spatModeName(SpatMode const mode)
{
    switch (mode) {
    case SpatMode::vbap:
        return "vbap";
    case SpatMode::mbap:
        return "mbap";
    case SpatMode::hybrid:
        return "hybrid";
    case SpatMode::invalid:
        break;
    }
    return "invalid";
}
//==============================================================================
/** Speaker positions and settings, in output patch order (SpeakerInfo's angles are degrees). */
std::vector<SpeakerInfo> layoutOf(SpeakerSetup const & setup)
{
    std::vector<SpeakerInfo> result{};
    for (auto const & speaker : setup.speakers) {
        auto const & c{ speaker.value->position.getCartesian() };
        SpeakerInfo info{};
        info.patch = speaker.key.get();
        info.x = c.x;
        info.y = c.y;
        info.z = c.z;
        // Azimuth 0 is the front (+y) and grows clockwise, as in SpatGRIS's deg messages.
        info.azimuth = juce::radiansToDegrees(std::atan2(c.x, c.y));
        info.elevation = juce::radiansToDegrees(std::atan2(c.z, std::hypot(c.x, c.y)));
        info.distance = std::sqrt(c.x * c.x + c.y * c.y + c.z * c.z);
        info.directOut = speaker.value->isDirectOutOnly;
        info.gainDb = speaker.value->gain.get();
        info.highpassHz = speaker.value->highpassData ? speaker.value->highpassData->freq.get() : 0.0f;
        result.push_back(info);
    }
    std::sort(result.begin(), result.end(), [](auto const & a, auto const & b) { return a.patch < b.patch; });
    return result;
}

//==============================================================================
/** The MIT KEMAR file closest to a direction. Files hold azimuths 0..180, the listener's right side. */
juce::File nearestHrir(juce::File const & hrtfDirectory, float const azimuth, float const elevation)
{
    static constexpr std::array<int, 14> ELEVATIONS{ -40, -30, -20, -10, 0, 10, 20, 30, 40, 50, 60, 70, 80, 90 };
    auto const elevationStep{ *std::min_element(ELEVATIONS.begin(), ELEVATIONS.end(), [&](int a, int b) {
        return std::abs(a - elevation) < std::abs(b - elevation);
    }) };

    auto const directory{ hrtfDirectory.getChildFile("elev" + juce::String{ elevationStep }) };
    auto const wanted{ std::abs(azimuth) };
    juce::File best{};
    auto bestDistance{ std::numeric_limits<float>::max() };
    for (auto const & file : directory.findChildFiles(juce::File::findFiles, false, "*.wav")) {
        auto const name{ file.getFileNameWithoutExtension() }; // H<elev>e<azimuth>a
        auto const azimuthText{ name.fromFirstOccurrenceOf("e", false, false).dropLastCharacters(1) };
        auto const distance{ std::abs(azimuthText.getFloatValue() - wanted) };
        if (distance < bestDistance) {
            bestDistance = distance;
            best = file;
        }
    }
    return best;
}

//==============================================================================
/** Loads a KEMAR response for one speaker, resampled to the session rate and swapped for the left side. */
bool loadMonitorVoice(juce::File const & hrtfDirectory, SpeakerInfo const & speaker, double const sampleRate,
                      MonitorVoice & voice)
{
    auto const file{ nearestHrir(hrtfDirectory, speaker.azimuth, speaker.elevation) };
    if (!file.existsAsFile()) {
        return false;
    }

    juce::AudioFormatManager formats{};
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> const reader{ formats.createReaderFor(file) };
    if (!reader || reader->numChannels < 2) {
        return false;
    }

    juce::AudioBuffer<float> source{ 2, static_cast<int>(reader->lengthInSamples) };
    reader->read(&source, 0, source.getNumSamples(), 0, true, true);

    // The files are 44.1 kHz; resample if the session runs at another rate. Linear interpolation:
    // JUCE's interpolators have kernels longer than these 128-sample responses and would smear them away.
    auto const ratio{ reader->sampleRate / sampleRate };
    auto const inputLength{ source.getNumSamples() };
    auto const numSamples{ static_cast<int>(std::ceil(inputLength / ratio)) };
    juce::AudioBuffer<float> resampled{ 2, numSamples };
    for (int channel{}; channel < 2; ++channel) {
        auto const * input{ source.getReadPointer(channel) };
        auto * output{ resampled.getWritePointer(channel) };
        for (int i{}; i < numSamples; ++i) {
            auto const position{ i * ratio };
            auto const index{ static_cast<int>(position) };
            auto const fraction{ static_cast<float>(position - index) };
            auto const a{ index < inputLength ? input[index] : 0.0f };
            auto const b{ index + 1 < inputLength ? input[index + 1] : 0.0f };
            output[i] = a + fraction * (b - a);
        }
    }

    // Channel 1 is the left ear, channel 2 the right; a speaker on the left is the mirror image.
    auto const leftChannel{ speaker.azimuth < 0.0f ? 1 : 0 };
    voice.left.assign(resampled.getReadPointer(leftChannel), resampled.getReadPointer(leftChannel) + numSamples);
    voice.right.assign(resampled.getReadPointer(1 - leftChannel), resampled.getReadPointer(1 - leftChannel) + numSamples);
    voice.history.assign(static_cast<size_t>(numSamples), 0.0f);
#ifdef ALGOGRIS_MONITOR_DEBUG
    auto peak = [](std::vector<float> const & v) {
        float m{};
        for (auto s : v) m = std::max(m, std::abs(s));
        return m;
    };
    std::printf("    voice patch %d az %.1f el %.1f -> %s, %d taps, peak L %.4f R %.4f (source peaks %.4f %.4f)\n",
                speaker.patch, speaker.azimuth, speaker.elevation, file.getFileName().toRawUTF8(), numSamples,
                peak(voice.left), peak(voice.right), source.getMagnitude(0, 0, source.getNumSamples()),
                source.getMagnitude(1, 0, source.getNumSamples()));
#endif
    return true;
}
} // namespace

//==============================================================================
struct Engine::Impl {
    std::mutex controlMutex; // serializes source messages and rebuilds
    std::mutex audioMutex;   // held by the audio thread while it renders
    std::unique_ptr<Renderer> renderer{};
    SourcesData sources{}; // positions as received, independent of the current renderer
    SpatMode projectSpatMode{ SpatMode::vbap };
    std::atomic<int> numOutputs{};
    std::vector<SpeakerInfo> speakerLayout{};

    //==============================================================================
    SourceData * findSource(int const index)
    {
        source_index_t const key{ index };
        return sources.contains(key) ? &sources[key] : nullptr;
    }

    //==============================================================================
    SpatMode effectiveSpatMode(SourceData const & source) const
    {
        return projectSpatMode == SpatMode::hybrid ? source.hybridSpatMode : projectSpatMode;
    }

    //==============================================================================
    /** The data sent to the algorithm: dome sources on the unit sphere, cube sources clamped (setSourcePosition). */
    SourceData toSpatData(SourceData const & source) const
    {
        auto result{ source };
        if (result.position) {
            switch (effectiveSpatMode(source)) {
            case SpatMode::vbap:
                result.position = Position{ result.position->getPolar().normalized() };
                break;
            case SpatMode::mbap:
                result.position = Position{ result.position->getCartesian().clampedToFarField() };
                break;
            case SpatMode::hybrid:
            case SpatMode::invalid:
                break;
            }
        }
        return result;
    }

    //==============================================================================
    /** Call with controlMutex held. */
    void send(int const index, SourceData const & source)
    {
        if (renderer && renderer->algorithm) {
            renderer->algorithm->updateSpatData(source_index_t{ index }, toSpatData(source));
        }
    }

    //==============================================================================
    bool setPosition(int const index, Position const & position, float const hSpan, float const vSpan)
    {
        std::lock_guard const lock{ controlMutex };
        auto * source{ findSource(index) };
        if (!source) {
            return false;
        }
        source->position = position;
        source->azimuthSpan = std::clamp(hSpan, 0.0f, 1.0f);
        source->zenithSpan = std::clamp(vSpan, 0.0f, 1.0f);
        send(index, *source);
        return true;
    }
};

//==============================================================================
Engine::Engine() : mImpl(std::make_unique<Impl>())
{
}

Engine::~Engine() = default;

//==============================================================================
Status Engine::configure(Settings const & settings)
{
    Status status{};
    auto & impl{ *mImpl };
    std::lock_guard const controlLock{ impl.controlMutex };

    // Speaker setup
    juce::File const setupFile{ juce::String::fromUTF8(settings.setupPath.c_str()) };
    if (!setupFile.existsAsFile()) {
        status.message = "speaker setup not found: " + settings.setupPath;
        return status;
    }
    auto const xml{ juce::XmlDocument::parse(setupFile) };
    auto speakerSetup{ xml ? SpeakerSetup::fromXml(*xml) : tl::nullopt };
    if (!speakerSetup) {
        status.message = "not a SpatGRIS speaker setup: " + settings.setupPath;
        return status;
    }

    SpatMode spatMode{ speakerSetup->spatMode };
    switch (settings.algorithm) {
    case Algorithm::fromSetup:
        break;
    case Algorithm::vbap:
        spatMode = SpatMode::vbap;
        break;
    case Algorithm::mbap:
        spatMode = SpatMode::mbap;
        break;
    case Algorithm::hybrid:
        spatMode = SpatMode::hybrid;
        break;
    }
    if (spatMode == SpatMode::invalid) {
        spatMode = SpatMode::vbap;
    }

    // Sources: keep existing ones, add or drop to match the requested count.
    auto const numSources{ std::clamp(settings.numSources, 1, MAX_NUM_SOURCES) };
    for (int i{ 1 }; i <= MAX_NUM_SOURCES; ++i) {
        source_index_t const key{ i };
        if (i <= numSources && !impl.sources.contains(key)) {
            impl.sources.add(key, std::make_unique<SourceData>());
        } else if (i > numSources && impl.sources.contains(key)) {
            impl.sources.remove(key);
        }
    }
    impl.projectSpatMode = spatMode;

    // Project data, as SpatGRIS would hold it
    auto renderer{ std::make_unique<Renderer>() };
    auto & data{ renderer->data };
    data.speakerSetup = std::move(*speakerSetup);
    data.project.spatMode = spatMode;
    data.project.spatGainsInterpolation = std::clamp(settings.interpolation, 0.0f, 1.0f);
    data.project.masterGain = dbfs_t{ settings.masterGainDb };
    data.project.useMulticoreDSP = settings.multicore;
    data.project.mbapDistanceAttenuationData.freq = hz_t{ settings.attenuationHz };
    data.project.mbapDistanceAttenuationData.attenuation = dbfs_t{ settings.attenuationDb };
    data.project.mbapDistanceAttenuationData.attenuationBypassState
        = settings.distanceAttenuation ? AttenuationBypassSate::off : AttenuationBypassSate::on;
    for (auto const & source : impl.sources) {
        data.project.sources.add(source.key, std::make_unique<SourceData>(*source.value));
        data.project.ordering.add(source.key);
    }
    switch (settings.render) {
    case Render::speakers:
        break;
    case Render::binaural:
        data.appData.stereoMode = StereoMode::hrtf;
        break;
    case Render::stereo:
        data.appData.stereoMode = StereoMode::stereo;
        break;
    }
    data.appData.audioSettings.sampleRate = settings.sampleRate;
    data.appData.audioSettings.bufferSize = settings.blockSize;
    renderer->config = data.toAudioConfig();

    // Algorithm
    {
        std::optional<ScopedWorkingDirectory> workingDirectory{};
        if (settings.render == Render::binaural) {
            juce::File const dataDir{ juce::String::fromUTF8(settings.dataDir.c_str()) };
            if (!dataDir.getChildFile("hrtf_compact").isDirectory()
                || !dataDir.getChildFile("tests/util/BINAURAL_SPEAKER_SETUP.xml").existsAsFile()) {
                status.message = "binaural data not found in " + settings.dataDir;
                return status;
            }
            workingDirectory.emplace(dataDir);
        }
        renderer->algorithm = AbstractSpatAlgorithm::make(data.speakerSetup,
                                                          spatMode,
                                                          data.appData.stereoMode,
                                                          data.project.sources,
                                                          settings.sampleRate,
                                                          settings.blockSize,
                                                          settings.multicore);
    }
    if (!renderer->algorithm) {
        status.message = "could not create the spatialization algorithm";
        return status;
    }
    if (auto const error{ renderer->algorithm->getError() }) {
        status.message = errorToString(*error);
        return status;
    }

    // Buffers
    juce::Array<source_index_t> sourceKeys{};
    for (auto const & source : data.project.sources) {
        sourceKeys.add(source.key);
    }
    renderer->sourceBuffer.init(sourceKeys);
    renderer->sourceBuffer.setNumSamples(settings.blockSize);

    juce::Array<output_patch_t> speakerKeys{};
    int highestPatch{};
    for (auto const & speaker : data.speakerSetup.speakers) {
        speakerKeys.add(speaker.key);
        highestPatch = std::max(highestPatch, speaker.key.get());
    }
    renderer->speakerBuffer.init(speakerKeys);
    renderer->speakerBuffer.setNumSamples(settings.blockSize);
    renderer->stereoBuffer.setSize(2, settings.blockSize);
    renderer->stereoBuffer.clear();

    // Binaural monitor: one KEMAR response per panned speaker, convolved with that speaker's feed.
    impl.speakerLayout = layoutOf(data.speakerSetup);
    if (settings.monitor && !renderer->stereoOutput) {
        juce::File const hrtfDirectory{ juce::String::fromUTF8(settings.dataDir.c_str()) };
        for (auto const & speaker : impl.speakerLayout) {
            if (speaker.directOut) {
                continue;
            }
            MonitorVoice voice{};
            voice.patch = output_patch_t{ speaker.patch };
            if (!loadMonitorVoice(hrtfDirectory.getChildFile("hrtf_compact"), speaker, settings.sampleRate, voice)) {
                status.message = "could not read the binaural data in " + settings.dataDir;
                return status;
            }
            renderer->monitorVoices.push_back(std::move(voice));
        }
    }

    renderer->numSources = numSources;
    renderer->blockSize = settings.blockSize;
    renderer->stereoOutput = data.appData.stereoMode.has_value();
    renderer->numOutputs = renderer->stereoOutput ? 2 : highestPatch;

    // Give the new algorithm the current source positions.
    for (auto const & source : impl.sources) {
        if (source.value->position) {
            renderer->algorithm->updateSpatData(source.key, impl.toSpatData(*source.value));
        }
    }

    status.ok = true;
    status.numOutputs = renderer->numOutputs;
    status.numSpeakers = data.speakerSetup.numOfSpatializedSpeakers();
    status.algorithm = spatModeName(spatMode);
    status.monitor = !renderer->monitorVoices.empty();

    // Swap it in. The old renderer is destroyed after the audio lock is released.
    {
        std::lock_guard const audioLock{ impl.audioMutex };
        std::swap(impl.renderer, renderer);
        impl.numOutputs = impl.renderer->numOutputs;
    }
    return status;
}

//==============================================================================
int Engine::numOutputs() const noexcept
{
    return mImpl->numOutputs;
}

//==============================================================================
std::vector<SpeakerInfo> Engine::layout() const
{
    std::lock_guard const lock{ mImpl->controlMutex };
    return mImpl->speakerLayout;
}

//==============================================================================
std::vector<SpeakerInfo> Engine::readLayout(std::string const & setupPath, std::string & error)
{
    juce::File const file{ juce::String::fromUTF8(setupPath.c_str()) };
    if (!file.existsAsFile()) {
        error = "speaker setup not found: " + setupPath;
        return {};
    }
    auto const xml{ juce::XmlDocument::parse(file) };
    auto const setup{ xml ? SpeakerSetup::fromXml(*xml) : tl::nullopt };
    if (!setup) {
        error = "not a SpatGRIS speaker setup: " + setupPath;
        return {};
    }
    error.clear();
    return layoutOf(*setup);
}

//==============================================================================
bool Engine::pol(int const index,
                 float const azimuth,
                 float const elevation,
                 float const radius,
                 float const hSpan,
                 float const vSpan)
{
    auto const azimuthRadians{ HALF_PI - radians_t{ azimuth } };
    radians_t const elevationRadians{ elevation };
    Position const position{ PolarVector{ azimuthRadians.balanced(), elevationRadians.balanced(), radius } };
    return mImpl->setPosition(index, position, hSpan, vSpan);
}

//==============================================================================
bool Engine::deg(int const index,
                 float const azimuth,
                 float const elevation,
                 float const radius,
                 float const hSpan,
                 float const vSpan)
{
    auto const azimuthRadians{ HALF_PI - radians_t{ degrees_t{ azimuth } } };
    radians_t const elevationRadians{ degrees_t{ elevation } };
    Position const position{ PolarVector{ azimuthRadians.balanced(), elevationRadians.balanced(), radius } };
    return mImpl->setPosition(index, position, hSpan, vSpan);
}

//==============================================================================
bool Engine::car(int const index, float const x, float const y, float const z, float const hSpan, float const vSpan)
{
    return mImpl->setPosition(index, Position{ CartesianVector{ x, y, z } }, hSpan, vSpan);
}

//==============================================================================
bool Engine::legacy(int const id,
                    float const azimuth,
                    float const elevation,
                    float const azimuthSpan,
                    float const elevationSpan,
                    float const distance)
{
    auto & impl{ *mImpl };
    std::lock_guard const lock{ impl.controlMutex };
    auto const index{ id + 1 };
    auto * source{ impl.findSource(index) };
    if (!source) {
        return false;
    }

    auto const azimuthRadians{ HALF_PI - radians_t{ azimuth }.balanced() };
    auto const zenithRadians{ HALF_PI - radians_t{ elevation } };
    switch (impl.effectiveSpatMode(*source)) {
    case SpatMode::mbap:
        source->position = LegacyLbapPosition{ azimuthRadians, zenithRadians, distance }.toPosition();
        break;
    case SpatMode::vbap:
    case SpatMode::hybrid:
    case SpatMode::invalid:
        source->position = Position{ PolarVector{ azimuthRadians, zenithRadians, 1.0f } };
        break;
    }
    source->azimuthSpan = std::clamp(azimuthSpan / 2.0f, 0.0f, 1.0f);
    source->zenithSpan = std::clamp(elevationSpan * 2.0f, 0.0f, 1.0f);
    impl.send(index, *source);
    return true;
}

//==============================================================================
bool Engine::clear(int const index)
{
    auto & impl{ *mImpl };
    std::lock_guard const lock{ impl.controlMutex };
    auto * source{ impl.findSource(index) };
    if (!source) {
        return false;
    }
    source->position = tl::nullopt;
    impl.send(index, *source);
    return true;
}

//==============================================================================
bool Engine::alg(int const index, bool const cube)
{
    auto & impl{ *mImpl };
    std::lock_guard const lock{ impl.controlMutex };
    auto * source{ impl.findSource(index) };
    if (!source) {
        return false;
    }
    source->hybridSpatMode = cube ? SpatMode::mbap : SpatMode::vbap;
    if (impl.projectSpatMode == SpatMode::hybrid) {
        // Clear the source in both algorithms, then send it to the right one.
        auto cleared{ *source };
        cleared.position = tl::nullopt;
        impl.send(index, cleared);
        impl.send(index, *source);
    }
    return true;
}

//==============================================================================
void Engine::process(double const * const * in,
                     int const numIn,
                     double * const * out,
                     int const numOut,
                     double * const * monitor,
                     int const numMonitor,
                     int const numFrames) noexcept
{
    for (int channel{}; channel < numOut; ++channel) {
        std::fill_n(out[channel], numFrames, 0.0);
    }
    for (int channel{}; channel < numMonitor; ++channel) {
        std::fill_n(monitor[channel], numFrames, 0.0);
    }

    auto & impl{ *mImpl };
    std::unique_lock const lock{ impl.audioMutex, std::try_to_lock };
    if (!lock.owns_lock() || !impl.renderer || numFrames > impl.renderer->blockSize) {
        return; // rebuilding, or not configured for this block size yet
    }
    auto & r{ *impl.renderer };
    auto const & config{ *r.config };

    // Sources, with peaks (processInputPeaks)
    r.sourceBuffer.setNumSamples(numFrames);
    r.speakerBuffer.setNumSamples(numFrames);
    for (int i{ 1 }; i <= r.numSources; ++i) {
        source_index_t const key{ i };
        auto & buffer{ r.sourceBuffer[key] };
        auto * samples{ buffer.getWritePointer(0) };
        if (i <= numIn) {
            std::transform(in[i - 1], in[i - 1] + numFrames, samples, [](double s) { return static_cast<float>(s); });
        } else {
            std::fill_n(samples, numFrames, 0.0f);
        }
        r.sourcePeaks[key] = config.sourcesAudioConfig[key].isMuted ? 0.0f : buffer.getMagnitude(0, 0, numFrames);
    }

    r.speakerBuffer.silence();
    r.stereoBuffer.clear();
    r.algorithm->process(config, r.sourceBuffer, r.speakerBuffer, r.stereoBuffer, r.sourcePeaks, nullptr);

    if (r.stereoOutput) {
        for (int channel{}; channel < std::min(numOut, 2); ++channel) {
            auto const * samples{ r.stereoBuffer.getReadPointer(channel) };
            for (int i{}; i < numFrames; ++i) {
                out[channel][i] = samples[i] * config.masterGain;
            }
        }
        return;
    }

    // Speaker gains and high-pass (processOutputModifiersAndPeaks)
    for (auto const & speaker : config.speakersAudioConfig) {
        auto const channel{ speaker.key.get() - 1 };
        if (channel < 0 || channel >= numOut) {
            continue;
        }
        auto const gain{ config.masterGain * speaker.value.gain };
        if (speaker.value.isMuted || gain < SMALL_GAIN) {
            continue;
        }
        auto & buffer{ r.speakerBuffer[speaker.key] };
        buffer.applyGain(0, 0, numFrames, gain);
        auto * samples{ buffer.getWritePointer(0) };
        if (speaker.value.highpassConfig) {
            auto const & highpass{ *speaker.value.highpassConfig };
            auto & state{ r.highpassStates[static_cast<size_t>(speaker.key.get())] };
            if (highpass.isNewConfig) {
                state.resetValues();
                highpass.isNewConfig = false;
            }
            highpass.process(samples, numFrames, state, r.random);
        }
        std::copy_n(samples, numFrames, out[channel]);
    }

    if (numMonitor < 2 || r.monitorVoices.empty()) {
        return;
    }

    // Binaural monitor: convolve each speaker's feed with its own head-related response.
    // Direct convolution; the responses are short (128 samples at 44.1 kHz).
    for (auto & voice : r.monitorVoices) {
        auto const * feed{ r.speakerBuffer[voice.patch].getReadPointer(0) };
        auto const taps{ voice.history.size() };
        auto * history{ voice.history.data() };
        auto const * leftTaps{ voice.left.data() };
        auto const * rightTaps{ voice.right.data() };
        auto index{ voice.writeIndex };
        for (int i{}; i < numFrames; ++i) {
            history[index] = feed[i];
            float left{};
            float right{};
            auto tail{ index };
            for (size_t tap{}; tap < taps; ++tap) {
                auto const sample{ history[tail] };
                left += sample * leftTaps[tap];
                right += sample * rightTaps[tap];
                tail = tail == 0 ? taps - 1 : tail - 1;
            }
            monitor[0][i] += left;
            monitor[1][i] += right;
            index = index + 1 == taps ? 0 : index + 1;
        }
        voice.writeIndex = index;
    }
}

//==============================================================================
std::string Engine::moduleDirectory()
{
#if defined(_WIN32)
    HMODULE module{};
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCWSTR>(&Engine::moduleDirectory),
                            &module)) {
        return {};
    }
    wchar_t path[MAX_PATH * 4]{};
    GetModuleFileNameW(module, path, static_cast<DWORD>(std::size(path)));
    return juce::File{ juce::String{ path } }.getParentDirectory().getFullPathName().toStdString();
#else
    Dl_info info{};
    if (!dladdr(reinterpret_cast<void const *>(&Engine::moduleDirectory), &info) || !info.dli_fname) {
        return {};
    }
    // Contents/MacOS/<binary> inside the .mxo bundle: go up to the folder holding the bundle.
    return juce::File{ info.dli_fname }.getParentDirectory().getParentDirectory().getParentDirectory()
        .getParentDirectory().getFullPathName().toStdString();
#endif
}
} // namespace algogris_max
