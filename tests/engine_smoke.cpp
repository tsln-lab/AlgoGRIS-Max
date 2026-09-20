// Runs the engine outside Max: builds each render mode, moves a source, checks the output.
// Build and run with tests/run_engine_smoke.sh (Linux or macOS, against an AlgoGRIS build).
#include "../source/projects/algogris_tilde/algogris_engine.hpp"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

using namespace algogris_max;

namespace
{
int gFailures{};

void check(bool const condition, std::string const & what)
{
    std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what.c_str());
    gFailures += condition ? 0 : 1;
}

/// Renders `blocks` blocks of a 1 kHz sine on source 1 and returns each output channel's RMS.
std::vector<double> render(Engine & engine, int const numIn, int const numOut, int const blockSize, int const blocks)
{
    std::vector<std::vector<double>> in(numIn, std::vector<double>(blockSize));
    std::vector<std::vector<double>> out(numOut, std::vector<double>(blockSize));
    std::vector<double const *> inPtrs{};
    std::vector<double *> outPtrs{};
    for (auto & c : in) inPtrs.push_back(c.data());
    for (auto & c : out) outPtrs.push_back(c.data());
    std::vector<double> sumSquares(numOut);
    long phase{};
    for (int b{}; b < blocks; ++b) {
        for (int i{}; i < blockSize; ++i, ++phase) {
            in[0][i] = 0.5 * std::sin(2.0 * M_PI * 1000.0 * phase / 48000.0);
        }
        engine.process(inPtrs.data(), numIn, outPtrs.data(), numOut, nullptr, 0, blockSize);
        if (b < blocks / 2) continue; // let gain ramps settle
        for (int c{}; c < numOut; ++c)
            for (double s : out[c]) sumSquares[c] += s * s;
    }
    for (auto & s : sumSquares) s = std::sqrt(s / (blockSize * (blocks - blocks / 2)));
    return sumSquares;
}

int loudest(std::vector<double> const & rms)
{
    int best{};
    for (int i{}; i < static_cast<int>(rms.size()); ++i)
        if (rms[i] > rms[best]) best = i;
    return best;
}
} // namespace

int main(int argc, char ** argv)
{
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <package dir>\n", argv[0]);
        return 2;
    }
    std::string const package{ argv[1] };
    Settings settings{};
    settings.dataDir = package + "/support/algogris-data";
    settings.numSources = 4;
    settings.blockSize = 256;

    std::puts("dome, vbap");
    {
        Engine engine{};
        settings.setupPath = package + "/setups/Dome_default_speaker_setup.xml";
        auto const status{ engine.configure(settings) };
        check(status.ok, "configure: " + (status.ok ? status.algorithm + ", " + std::to_string(status.numSpeakers)
                                                       + " speakers, " + std::to_string(status.numOutputs) + " outputs"
                                                 : status.message));
        auto silent{ render(engine, 4, status.numOutputs, 256, 8) };
        check(loudest(silent) == 0 && silent[0] == 0.0, "silent before any position");
        engine.deg(1, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f); // front
        auto front{ render(engine, 4, status.numOutputs, 256, 8) };
        engine.deg(1, 180.0f, 0.0f, 1.0f, 0.0f, 0.0f); // back
        auto back{ render(engine, 4, status.numOutputs, 256, 8) };
        check(front[loudest(front)] > 0.1, "front source reaches a speaker (patch " + std::to_string(loudest(front) + 1) + ")");
        check(loudest(front) != loudest(back), "moving to the back changes the loudest speaker (patch "
                                                   + std::to_string(loudest(back) + 1) + ")");
        engine.clear(1);
        auto cleared{ render(engine, 4, status.numOutputs, 256, 16) };
        check(cleared[loudest(cleared)] < 1e-6, "clr silences the source");
    }

    std::puts("cube, mbap");
    {
        Engine engine{};
        settings.setupPath = package + "/setups/Cube_default_speaker_setup.xml";
        auto const status{ engine.configure(settings) };
        check(status.ok, "configure: " + (status.ok ? status.algorithm + ", " + std::to_string(status.numSpeakers)
                                                       + " speakers, " + std::to_string(status.numOutputs) + " outputs"
                                                 : status.message));
        engine.car(1, -1.0f, 1.0f, 0.0f, 0.0f, 0.0f); // front left
        auto left{ render(engine, 4, status.numOutputs, 256, 8) };
        engine.car(1, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f); // front right
        auto right{ render(engine, 4, status.numOutputs, 256, 8) };
        check(left[loudest(left)] > 0.05, "source reaches a speaker (patch " + std::to_string(loudest(left) + 1) + ")");
        check(loudest(left) != loudest(right), "left and right land on different speakers (patches "
                                                   + std::to_string(loudest(left) + 1) + ", "
                                                   + std::to_string(loudest(right) + 1) + ")");
        engine.legacy(0, 0.0f, 1.5707963f, 0.0f, 0.0f, 1.0f); // legacy id 0 = source 1, front, on the floor
        auto legacyFront{ render(engine, 4, status.numOutputs, 256, 8) };
        check(legacyFront[loudest(legacyFront)] > 0.05, "legacy message moves source 1");
        check(!engine.car(5, 0, 0, 0, 0, 0), "out-of-range source index rejected");
    }

    std::puts("7.1.4 (cube)");
    {
        Engine engine{};
        settings.setupPath = package + "/setups/Cube_7.1.4_speaker_setup.xml";
        auto const status{ engine.configure(settings) };
        check(status.ok && status.numOutputs == 12 && status.numSpeakers == 11,
              "configure: " + (status.ok ? status.algorithm + ", " + std::to_string(status.numSpeakers)
                                               + " speakers (LFE excluded), " + std::to_string(status.numOutputs)
                                               + " outputs"
                                         : status.message));
        // A source at a speaker's own position should come out loudest on that speaker's patch.
        struct Placement { float x, y, z; int patch; char const* name; };
        constexpr float E { 0.76f }, CEIL { 0.85f };
        Placement const placements[] {
            { -E,  E, 0.0f,   1, "L" },   {  E,  E, 0.0f,   2, "R" },   { 0.0f, E, 0.0f, 3, "C" },
            { -E, 0.0f, 0.0f, 5, "Lss" }, {  E, 0.0f, 0.0f, 6, "Rss" },
            { -E, -E, 0.0f,   7, "Lrs" }, {  E, -E, 0.0f,   8, "Rrs" },
            { -E,  E, CEIL,   9, "Ltf" }, {  E,  E, CEIL,  10, "Rtf" },
            { -E, -E, CEIL,  11, "Ltr" }, {  E, -E, CEIL,  12, "Rtr" },
        };
        bool allCorrect { true };
        double lfeWorst {};
        for (auto const& p : placements) {
            engine.car(1, p.x, p.y, p.z, 0.0f, 0.0f);
            auto const rms { render(engine, 4, status.numOutputs, 256, 8) };
            int const got { loudest(rms) + 1 };
            lfeWorst = std::max(lfeWorst, rms[3]);
            if (got != p.patch) {
                std::printf("       %s: expected patch %d, loudest was %d\n", p.name, p.patch, got);
                allCorrect = false;
            }
        }
        check(allCorrect, "each of the 11 speakers is loudest for a source at its own position");
        check(lfeWorst == 0.0, "LFE (patch 4, direct out) stays silent");
    }

    std::puts("7.1.4 (dome, vbap)");
    {
        Engine engine{};
        settings.setupPath = package + "/setups/Dome_7.1.4_speaker_setup.xml";
        auto const status{ engine.configure(settings) };
        check(status.ok && status.algorithm == "vbap" && status.numOutputs == 12 && status.numSpeakers == 11,
              "configure: " + (status.ok ? status.algorithm + ", " + std::to_string(status.numSpeakers)
                                               + " speakers, " + std::to_string(status.numOutputs) + " outputs"
                                         : status.message));
        // A source pointed at a speaker should come out of that speaker alone.
        struct Direction { float azimuth, elevation; int patch; char const* name; };
        Direction const directions[] {
            { -30, 0, 1, "L" },   { 30, 0, 2, "R" },    { 0, 0, 3, "C" },
            { -90, 0, 5, "Lss" }, { 90, 0, 6, "Rss" },
            { -135, 0, 7, "Lrs" },{ 135, 0, 8, "Rrs" },
            { -45, 45, 9, "Ltf" },{ 45, 45, 10, "Rtf" },
            { -135, 45, 11, "Ltr" }, { 135, 45, 12, "Rtr" },
        };
        bool allCorrect { true };
        for (auto const& d : directions) {
            engine.deg(1, d.azimuth, d.elevation, 1.0f, 0.0f, 0.0f);
            auto const rms { render(engine, 4, status.numOutputs, 256, 8) };
            int const got { loudest(rms) + 1 };
            if (got != d.patch) {
                std::printf("       %s: expected patch %d, loudest was %d\n", d.name, d.patch, got);
                allCorrect = false;
            }
        }
        check(allCorrect, "each of the 11 speakers is loudest for a source in its direction");
        // Straight up: nothing is directly overhead, so the four top speakers share it.
        engine.deg(1, 0.0f, 90.0f, 1.0f, 0.0f, 0.0f);
        auto const zenith { render(engine, 4, status.numOutputs, 256, 8) };
        double topEnergy {}, otherEnergy {};
        for (int c = 0; c < status.numOutputs; ++c) {
            (c >= 8 ? topEnergy : otherEnergy) += zenith[c] * zenith[c];
        }
        check(topEnergy > 0.001 && topEnergy > 10 * otherEnergy, "a source overhead uses the top speakers");
    }

    std::puts("binaural monitor of the speaker feeds");
    {
        Engine engine{};
        settings.setupPath = package + "/setups/Cube_7.1.4_speaker_setup.xml";
        settings.monitor = true;
        auto const status{ engine.configure(settings) };
        check(status.ok && status.monitor, "configure: " + (status.ok ? "monitor running" : status.message));

        // Render both the speaker feeds and the monitor, and compare the ears.
        auto measure = [&](float x, float y, float z) {
            std::vector<std::vector<double>> in(4, std::vector<double>(256));
            std::vector<std::vector<double>> out(status.numOutputs, std::vector<double>(256));
            std::vector<std::vector<double>> mon(2, std::vector<double>(256));
            std::vector<double const*> inPtrs{};
            std::vector<double*> outPtrs{}, monPtrs{};
            for (auto& c : in) inPtrs.push_back(c.data());
            for (auto& c : out) outPtrs.push_back(c.data());
            for (auto& c : mon) monPtrs.push_back(c.data());
            engine.car(1, x, y, z, 0.0f, 0.0f);
            double sum[2] { 0, 0 };
            long phase = 0;
            for (int b = 0; b < 16; ++b) {
                for (int i = 0; i < 256; ++i, ++phase)
                    in[0][i] = 0.5 * std::sin(2.0 * M_PI * 1000.0 * phase / 48000.0);
                engine.process(inPtrs.data(), 4, outPtrs.data(), status.numOutputs, monPtrs.data(), 2, 256);
                if (b < 8) continue;
                for (int c = 0; c < 2; ++c)
                    for (double v : mon[c]) sum[c] += v * v;
            }
            return std::pair<double, double>{ std::sqrt(sum[0] / (256 * 8)), std::sqrt(sum[1] / (256 * 8)) };
        };

        auto const [leftL, leftR] { measure(-0.76f, 0.0f, 0.0f) };  // source at the left speaker
        auto const [rightL, rightR] { measure(0.76f, 0.0f, 0.0f) }; // at the right speaker
        char buffer[200];
        std::snprintf(buffer, sizeof buffer,
                      "left speaker louder in L (%.3f vs %.3f), right speaker louder in R (%.3f vs %.3f)",
                      leftL, leftR, rightR, rightL);
        check(leftL > 1.5 * leftR && rightR > 1.5 * rightL, buffer);
        check(leftL > 0.01 && rightR > 0.01, "monitor produces signal");
        settings.monitor = false;
    }

    std::puts("binaural");
    {
        Engine engine{};
        settings.setupPath = package + "/setups/Dome_default_speaker_setup.xml";
        settings.render = Render::binaural;
        auto const status{ engine.configure(settings) };
        check(status.ok && status.numOutputs == 2, "configure: " + (status.ok ? std::to_string(status.numOutputs) + " outputs" : status.message));
        engine.deg(1, -90.0f, 0.0f, 1.0f, 0.0f, 0.0f); // hard left
        auto left{ render(engine, 4, 2, 256, 16) };
        engine.deg(1, 90.0f, 0.0f, 1.0f, 0.0f, 0.0f); // hard right
        auto right{ render(engine, 4, 2, 256, 16) };
        char buffer[160];
        std::snprintf(buffer, sizeof buffer, "left source louder in L (%.3f vs %.3f), right in R (%.3f vs %.3f)",
                      left[0], left[1], right[1], right[0]);
        check(left[0] > 1.5 * left[1] && right[1] > 1.5 * right[0], buffer);
        settings.render = Render::speakers;
    }

    std::puts("reconfigure keeps positions");
    {
        Engine engine{};
        settings.setupPath = package + "/setups/Dome_default_speaker_setup.xml";
        auto const first{ engine.configure(settings) };
        engine.deg(1, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f);
        auto before{ render(engine, 4, first.numOutputs, 256, 8) };
        settings.interpolation = 0.5f;
        auto const second{ engine.configure(settings) };
        auto after{ render(engine, 4, second.numOutputs, 256, 16) };
        check(second.ok && loudest(before) == loudest(after) && after[loudest(after)] > 0.1,
              "same speaker after rebuilding");
        settings.interpolation = 0.0f;
    }

    std::printf("%s (%d failure%s)\n", gFailures ? "FAILED" : "PASSED", gFailures, gFailures == 1 ? "" : "s");
    return gFailures ? 1 : 0;
}
