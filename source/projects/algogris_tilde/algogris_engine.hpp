/// @file
/// A plain C++ front end to AlgoGRIS. It does what SpatGRIS does around the
/// spatialization algorithms (source bookkeeping, OSC message semantics, speaker
/// gains) without exposing any JUCE or AlgoGRIS types, so the Max side never
/// includes them.
/// @license GPL-3.0-or-later, like AlgoGRIS.

#pragma once

#include <memory>
#include <string>
#include <vector>

namespace algogris_max
{
enum class Algorithm { fromSetup, vbap, mbap, hybrid };
enum class Render { speakers, binaural, stereo };

struct Settings {
    std::string setupPath;   ///< Absolute path to a SpatGRIS speaker setup (.xml).
    std::string dataDir;     ///< Folder holding hrtf_compact/ and tests/util/BINAURAL_SPEAKER_SETUP.xml.
    Algorithm algorithm{ Algorithm::fromSetup };
    Render render{ Render::speakers };
    int numSources{ 16 };
    double sampleRate{ 48000.0 };
    int blockSize{ 64 };
    float interpolation{ 0.0f }; ///< SpatGRIS "gain interpolation", 0..1.
    float masterGainDb{ 0.0f };
    bool multicore{ false };
    bool monitor{ false };   ///< Also render a binaural monitor of the speaker feeds (ignored for binaural/stereo).
    bool distanceAttenuation{ false }; ///< MBAP: attenuate and lowpass sources beyond radius 1.
    float attenuationDb{ 0.0f };       ///< Level reached at the extended radius (1.667).
    float attenuationHz{ 16000.0f };   ///< Lowpass cutoff reached at the extended radius.
};

/** One speaker of the current setup. Angles are in degrees, azimuth 0 = front, positive clockwise. */
struct SpeakerInfo {
    int patch{};
    float x{}, y{}, z{};
    float azimuth{}, elevation{}, distance{};
    bool directOut{};
    float gainDb{};
    float highpassHz{}; ///< 0 when the speaker has no high-pass.
};

struct Status {
    bool ok{};
    std::string message;
    int numOutputs{};     ///< Output channels: the highest output patch, or 2 for binaural/stereo.
    int numSpeakers{};
    std::string algorithm; ///< "vbap", "mbap" or "hybrid", after resolving Algorithm::fromSetup.
    bool monitor{};        ///< Whether the binaural monitor is running.
};

class Engine
{
public:
    Engine();
    ~Engine();
    Engine(Engine const &) = delete;
    Engine & operator=(Engine const &) = delete;

    /// Builds or rebuilds the renderer. Call from the main thread. Source positions are kept.
    Status configure(Settings const & settings);

    /// Output channel count of the current renderer, 0 if none.
    [[nodiscard]] int numOutputs() const noexcept;

    /// The speakers of the current setup, in output patch order.
    [[nodiscard]] std::vector<SpeakerInfo> layout() const;

    /// Reads a speaker setup without building a renderer. For tools that only need the layout.
    [[nodiscard]] static std::vector<SpeakerInfo> readLayout(std::string const & setupPath, std::string & error);

    // Source control, with the semantics of SpatGRIS's /spat/serv OSC messages.
    // Safe to call from any non-audio thread. Return false if the source index is out of range.
    // pol, deg, car, clear and alg use 1-based source indices; legacy uses a 0-based id.
    bool pol(int index, float azimuth, float elevation, float radius, float hSpan, float vSpan);
    bool deg(int index, float azimuth, float elevation, float radius, float hSpan, float vSpan);
    bool car(int index, float x, float y, float z, float hSpan, float vSpan);
    bool legacy(int id, float azimuth, float elevation, float azimuthSpan, float elevationSpan, float distance);
    bool clear(int index);
    bool alg(int index, bool cube);

    /// Audio thread. in: one channel per source; out: one channel per output patch (or stereo);
    /// monitor: 2 channels of binaural monitoring of the speaker feeds, or none.
    void process(double const * const * in,
                 int numIn,
                 double * const * out,
                 int numOut,
                 double * const * monitor,
                 int numMonitor,
                 int numFrames) noexcept;

    /// Folder containing this module (the .mxe64 / .mxo), or empty if unknown.
    static std::string moduleDirectory();

private:
    struct Impl;
    std::unique_ptr<Impl> mImpl;
};
} // namespace algogris_max
