/// @file
///	@ingroup 	algogris
/// @license	GPL-3.0-or-later, like AlgoGRIS.

#include "c74_min.h"
#include "algogris_engine.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <filesystem>

using namespace c74::min;

class algogris : public object<algogris>, public mc_operator<> {
public:
    MIN_DESCRIPTION { "Spatialize sources with the SpatGRIS algorithms (VBAP, MBAP, hybrid, binaural) from AlgoGRIS. "
                      "Takes one multichannel signal with a channel per source and outputs one channel per speaker "
                      "output patch, or stereo in binaural and stereo modes. "
                      "Understands SpatGRIS's /spat/serv OSC messages, so [udpreceive 18032] can drive it directly." };
    MIN_TAGS        { "audio, spatialization" };
    MIN_AUTHOR      { "AlgoGRIS: GRIS (Université de Montréal) and SAT" };
    MIN_RELATED     { "mc.pack~, mc.unpack~, udpreceive" };

    inlet<>  m_inlet  { this, "(multichannelsignal) one channel per source; source messages (car, pol, deg, clr, alg, list)" };
    outlet<> m_output { this, "(multichannelsignal) one channel per speaker output patch, or stereo", "multichannelsignal" };
    outlet<> m_status { this, "(list) status: outputs, speakers, algorithm; error messages" };

    // Attributes. Every change rebuilds the renderer; source positions are kept.

    attribute<symbol> setup { this, "setup", "",
        description { "SpatGRIS speaker setup (.xml): an absolute path, a file in Max's search path, "
                      "or a file in the package's setups folder. Empty uses Dome_default_speaker_setup.xml." },
        setter { MIN_FUNCTION { request_rebuild(); return args; } }
    };

    attribute<symbol> mode { this, "mode", "setup",
        description { "Spatialization algorithm. setup: whatever the speaker setup declares (Dome = vbap, Cube = mbap)." },
        range { "setup", "vbap", "mbap", "hybrid" },
        setter { MIN_FUNCTION { request_rebuild(); return args; } }
    };

    attribute<symbol> render { this, "render", "speakers",
        description { "speakers: one output per output patch. binaural: headphones (KEMAR HRTF, 16 virtual speakers). "
                      "stereo: SpatGRIS stereo reduction." },
        range { "speakers", "binaural", "stereo" },
        setter { MIN_FUNCTION { request_rebuild(); return args; } }
    };

    attribute<int> sources { this, "sources", 16,
        description { "Number of sources. Input channel n feeds source n." },
        range { 1, 256 },
        setter { MIN_FUNCTION { request_rebuild(); return args; } }
    };

    attribute<number> interpolation { this, "interpolation", 0.0,
        description { "Gain interpolation, 0 to 1. 0 ramps gains linearly over one signal vector; higher values glide more slowly." },
        range { 0.0, 1.0 },
        setter { MIN_FUNCTION { request_rebuild(); return args; } }
    };

    attribute<number> gain { this, "gain", 0.0,
        description { "Master gain in dB." },
        setter { MIN_FUNCTION { request_rebuild(); return args; } }
    };

    attribute<bool> multicore { this, "multicore", false,
        description { "Use AlgoGRIS's parallel VBAP / MBAP. Helps with many sources and speakers." },
        setter { MIN_FUNCTION { request_rebuild(); return args; } }
    };

    attribute<bool> attenuation { this, "attenuation", false,
        description { "MBAP distance attenuation: sources beyond radius 1 get quieter and darker, "
                      "reaching attenuation_db and attenuation_freq at radius 1.667." },
        setter { MIN_FUNCTION { request_rebuild(); return args; } }
    };

    attribute<number> attenuation_db { this, "attenuation_db", 0.0,
        description { "MBAP distance attenuation: level at radius 1.667, in dB." },
        setter { MIN_FUNCTION { request_rebuild(); return args; } }
    };

    attribute<number> attenuation_freq { this, "attenuation_freq", 16000.0,
        description { "MBAP distance attenuation: lowpass cutoff at radius 1.667, in Hz." },
        range { 20.0, 20000.0 },
        setter { MIN_FUNCTION { request_rebuild(); return args; } }
    };

    // Source messages, as SpatGRIS's /spat/serv OSC messages (without the address).

    message<> car { this, "car", "Move a source (cartesian): index x y z hspan vspan. Index starts at 1.",
        MIN_FUNCTION { return position_message("car", args); }
    };

    message<> pol { this, "pol", "Move a source (polar, radians): index azimuth elevation radius hspan vspan. Index starts at 1.",
        MIN_FUNCTION { return position_message("pol", args); }
    };

    message<> deg { this, "deg", "Move a source (polar, degrees): index azimuth elevation radius hspan vspan. Index starts at 1.",
        MIN_FUNCTION { return position_message("deg", args); }
    };

    message<> clr { this, "clr", "Clear a source's position, silencing it: index. Index starts at 1.",
        MIN_FUNCTION {
            if (args.size() != 1) {
                cerr << "clr: expected a source index" << endl;
            } else if (!m_engine.clear(round_index(args[0]))) {
                cerr << "clr: source index out of range" << endl;
            }
            return {};
        }
    };

    message<> reset { this, "reset", "Clear a source's position (legacy): id. Id starts at 0.",
        MIN_FUNCTION {
            if (args.size() != 1) {
                cerr << "reset: expected a source id" << endl;
            } else if (!m_engine.clear(round_index(args[0]) + 1)) {
                cerr << "reset: source id out of range" << endl;
            }
            return {};
        }
    };

    message<> alg { this, "alg", "Choose a source's algorithm in hybrid mode: index dome|cube. Index starts at 1.",
        MIN_FUNCTION {
            symbol const which { args.size() == 2 ? symbol(args[1]) : symbol("") };
            std::string const name { which.c_str() };
            if (name != "dome" && name != "cube" && name != "Dome" && name != "Cube") {
                cerr << "alg: expected an index and dome or cube" << endl;
            } else if (!m_engine.alg(round_index(args[0]), name == "cube" || name == "Cube")) {
                cerr << "alg: source index out of range" << endl;
            }
            return {};
        }
    };

    message<> list { this, "list", "Move a source (legacy ControlGRIS format): id azimuth elevation azimuthspan elevationspan distance [gain]. Id starts at 0.",
        MIN_FUNCTION {
            if (args.size() < 6) {
                cerr << "list: expected id azimuth elevation azimuthspan elevationspan distance [gain]" << endl;
            } else if (!m_engine.legacy(round_index(args[0]), args[1], args[2], args[3], args[4], args[5])) {
                cerr << "list: source id out of range" << endl;
            }
            return {};
        }
    };

    message<> spat_serv { this, "/spat/serv", "A SpatGRIS OSC message, as output by [udpreceive].",
        MIN_FUNCTION {
            if (args.empty()) {
                return {};
            }
            if (args[0].a_type == c74::max::A_SYM) {
                symbol const command { args[0] };
                atoms const rest(args.begin() + 1, args.end()); // parentheses: braces would build one atom from the iterators
                std::string const name { command.c_str() };
                if (name == "car" || name == "pol" || name == "deg") {
                    return position_message(name, rest);
                }
                if (name == "clr")    { clr(rest);   return {}; }
                if (name == "reset")  { reset(rest); return {}; }
                if (name == "alg")    { alg(rest);   return {}; }
                if (name == "colour") { return {}; } // cosmetic in SpatGRIS; ignored
                cerr << "/spat/serv: unknown command " << name << endl;
                return {};
            }
            list(args);
            return {};
        }
    };

    message<> rebuild { this, "rebuild", "Reload the speaker setup and rebuild the renderer.",
        MIN_FUNCTION { request_rebuild(); return {}; }
    };

    // DSP

    message<> dspsetup { this, "dspsetup",
        MIN_FUNCTION {
            double const sample_rate { args[0] };
            int const vector_size { args[1] };
            if (sample_rate != m_sample_rate || vector_size != m_vector_size) {
                m_sample_rate = sample_rate;
                m_vector_size = vector_size;
                configure(true);
            }
            return {};
        }
    };

    void operator()(audio_bundle input, audio_bundle output) {
        m_engine.process(input.samples(), static_cast<int>(input.channel_count()),
                         output.samples(), static_cast<int>(output.channel_count()),
                         static_cast<int>(input.frame_count()));
    }

    // MC: Min has no hook for the output channel count, so register Max's methods directly.

    message<> maxclass_setup { this, "maxclass_setup",
        MIN_FUNCTION {
            c74::max::t_class* c = args[0];
            c74::max::class_addmethod(c, reinterpret_cast<c74::max::method>(multichanneloutputs),
                                      "multichanneloutputs", c74::max::A_CANT, 0);
            return {};
        }
    };

    static long multichanneloutputs(c74::max::t_object* x, long const outlet_index) {
        auto* self = reinterpret_cast<minwrap<algogris>*>(x);
        return outlet_index == 0 ? std::max(1, self->m_min_object.m_num_outputs.load()) : 0;
    }

    algogris(atoms const& args = {}) {
        m_rebuild_queue.set();
    }

private:
    algogris_max::Engine m_engine;
    std::atomic<int> m_num_outputs { 0 };
    double m_sample_rate { c74::max::sys_getsr() };
    int m_vector_size { c74::max::sys_getblksize() };

    queue<> m_rebuild_queue { this,
        MIN_FUNCTION { configure(false); return {}; }
    };

    // Asks Max to recompile the signal chain, outside of any compile in progress.
    queue<> m_chain_queue { this,
        MIN_FUNCTION {
            if (auto* chain = c74::max::dspchain_fromobject(maxobj())) {
                c74::max::dspchain_setbroken(chain);
            }
            return {};
        }
    };

    void request_rebuild() {
        if (initialized()) {
            m_rebuild_queue.set();
        }
    }

    static int round_index(atom const& a) {
        return static_cast<int>(std::lround(static_cast<double>(a)));
    }

    atoms position_message(std::string const& name, atoms const& args) {
        if (args.size() != 6) {
            cerr << name << ": expected index and 5 numbers" << endl;
            return {};
        }
        int const index { round_index(args[0]) };
        float const a { args[1] }, b { args[2] }, c { args[3] }, h { args[4] }, v { args[5] };
        bool const ok { name == "car" ? m_engine.car(index, a, b, c, h, v)
                      : name == "pol" ? m_engine.pol(index, a, b, c, h, v)
                                      : m_engine.deg(index, a, b, c, h, v) };
        if (!ok) {
            cerr << name << ": source index " << index << " out of range" << endl;
        }
        return {};
    }

    /// Absolute path of the speaker setup, or empty if it can't be found.
    std::string resolve_setup() {
        namespace fs = std::filesystem;
        std::string name { setup.get().c_str() };
        if (name.empty()) {
            name = "Dome_default_speaker_setup.xml";
        }

        std::error_code error;
        if (fs::path const as_is { fs::u8path(name) }; as_is.is_absolute() && fs::is_regular_file(as_is, error)) {
            return name;
        }
        if (std::string const package { package_directory() }; !package.empty()) {
            std::string const in_package { package + "/setups/" + name };
            if (fs::is_regular_file(fs::u8path(in_package), error)) {
                return in_package;
            }
        }

        // Max's search path
        char filename[c74::max::MAX_PATH_CHARS] {};
        c74::max::strncpy_zero(filename, name.c_str(), c74::max::MAX_PATH_CHARS);
        short path_id {};
        c74::max::t_fourcc type {};
        char full_path[c74::max::MAX_PATH_CHARS] {};
        if (c74::max::locatefile_extended(filename, &path_id, &type, nullptr, 0) == 0
            && c74::max::path_toabsolutesystempath(path_id, filename, full_path) == 0) {
            return full_path;
        }
        return {};
    }

    /// The package folder: the parent of the folder holding this external.
    static std::string package_directory() {
        std::string dir { algogris_max::Engine::moduleDirectory() };
        auto const slash { dir.find_last_of("/\\") };
        return slash == std::string::npos ? std::string{} : dir.substr(0, slash);
    }

    void configure(bool const from_dsp) {
        std::string const setup_path { resolve_setup() };
        if (setup_path.empty()) {
            cerr << "speaker setup not found: " << setup.get() << endl;
            m_status.send("error", "setup_not_found");
            return;
        }

        algogris_max::Settings settings {};
        settings.setupPath = setup_path;
        settings.dataDir = package_directory() + "/support/algogris-data";

        std::string const mode_name { mode.get().c_str() };
        settings.algorithm = mode_name == "vbap"   ? algogris_max::Algorithm::vbap
                           : mode_name == "mbap"   ? algogris_max::Algorithm::mbap
                           : mode_name == "hybrid" ? algogris_max::Algorithm::hybrid
                                                   : algogris_max::Algorithm::fromSetup;
        std::string const render_name { render.get().c_str() };
        settings.render = render_name == "binaural" ? algogris_max::Render::binaural
                        : render_name == "stereo"   ? algogris_max::Render::stereo
                                                    : algogris_max::Render::speakers;
        settings.numSources = sources;
        settings.sampleRate = m_sample_rate;
        settings.blockSize = m_vector_size;
        settings.interpolation = static_cast<float>(static_cast<double>(interpolation));
        settings.masterGainDb = static_cast<float>(static_cast<double>(gain));
        settings.multicore = multicore;
        settings.distanceAttenuation = attenuation;
        settings.attenuationDb = static_cast<float>(static_cast<double>(attenuation_db));
        settings.attenuationHz = static_cast<float>(static_cast<double>(attenuation_freq));

        auto const status { m_engine.configure(settings) };
        if (!status.ok) {
            cerr << status.message << endl;
            m_status.send("error", status.message);
            return;
        }

        int const previous { m_num_outputs.exchange(status.numOutputs) };
        m_status.send("outputs", status.numOutputs, "speakers", status.numSpeakers, "algorithm", status.algorithm);
        if (previous != status.numOutputs) {
            // The output channel count changed, so the signal chain must be rebuilt. During a
            // compile (dspsetup) that has to wait until the compile is over.
            if (from_dsp) {
                m_chain_queue.set();
            } else {
                m_chain_queue();
            }
        }
    }
};

MIN_EXTERNAL(algogris);
