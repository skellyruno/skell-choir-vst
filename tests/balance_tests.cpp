// Goal-based level and balance tests. Unlike staged_tests ("same as v0.5"), these fail when the plugin does not do its job:
// at the plugin's own defaults the choir must be a choir (heard against the lead), the output must not collapse in level,
// every section must be audible, and CHOIR LEVEL must do what it says. They exist because "the choir matches v0.5" once
// protected a choir that sat 11 dB under the dry voice.
#include "test_support.h"

// usage: balance_tests [project root]   (default: the current directory)

using namespace ts;

// --- parse the default of one addFloat(layout,"id",...) line out of PluginProcessor.cpp
static std::vector<std::string> splitArgs(const std::string& s)
{
    std::vector<std::string> out; std::string cur;
    for (char c : s) { if (c == ',') { out.push_back(cur); cur.clear(); } else cur += c; }
    out.push_back(cur); return out;
}
static bool layoutArgs(const std::string& src, const std::string& idToken, std::vector<std::string>& args)
{
    const std::string key = "addFloat(layout," + idToken + ",";
    const auto at = src.find(key); if (at == std::string::npos) return false;
    const auto end = src.find(");", at); if (end == std::string::npos) return false;
    args = splitArgs(src.substr(at + 9, end - at - 9));   // [0]=layout [1]=id [2]=name [3]=min [4]=max [5]=step [6]=default
    return args.size() >= 7;
}
static double num(const std::string& t) { return std::atof(t.c_str()); }

int main(int argc, char** argv)
{
    const std::string root = argc > 1 ? argv[1] : ".";
    const int sr = 44100;

    // 1. the test defaults are the plugin's defaults ---------------------------------------------------------------
    {
        std::ifstream f(root + "/Source/PluginProcessor.cpp");
        CHECK(f.good(), "cannot open Source/PluginProcessor.cpp under %s (pass the project root as the first argument)", root.c_str());
        std::stringstream ss; ss << f.rdbuf(); const std::string src = ss.str();
        const auto d = pluginDefaults();
        struct Row { const char* id; double expect; };
        const Row rows[] = { {"ensemble", d.ensemble}, {"humanize", d.humanize}, {"formant", d.formant}, {"sensitivity", d.sensitivity},
                             {"roomSize", d.roomSize}, {"roomSeconds", d.roomSeconds}, {"absorption", d.absorption}, {"diffusion", d.diffusion},
                             {"wet", d.wet}, {"width", d.width}, {"leadY", d.leadY}, {"leadGain", d.leadGain}, {"mix", d.mix},
                             {"master", d.master}, {"choirLevel", d.choirLevelDb} };
        for (const auto& r : rows)
        {
            std::vector<std::string> a; CHECK(layoutArgs(src, std::string("\"") + r.id + "\"", a), "parameter '%s' not found in PluginProcessor.cpp", r.id);
            CHECK(std::fabs(num(a[6]) - r.expect) < 1e-4, "default of '%s': plugin %s, tests/test_support.h %.4f (update test_support.h)", r.id, a[6].c_str(), r.expect);
        }
        std::vector<std::string> g; CHECK(layoutArgs(src, "\"gain\"+s", g), "gain parameter not found");
        CHECK(std::fabs(num(g[6]) - d.gain[0]) < 1e-4, "section gain default drifted");
        std::vector<std::string> c; layoutArgs(src, "\"choirLevel\"", c);
        CHECK(std::fabs(num(c[3]) + choir::StagedChoir::kChoirLevelMaxDb) < 1e-4 && std::fabs(num(c[4]) - choir::StagedChoir::kChoirLevelMaxDb) < 1e-4,
              "CHOIR LEVEL range in PluginProcessor.cpp (%s..%s) must equal +-kChoirLevelMaxDb", c[3].c_str(), c[4].c_str());
        std::printf("[1] tests/test_support.h defaults match PluginProcessor.cpp (%zu parameters checked)\n", sizeof(rows) / sizeof(rows[0]) + 1);
    }

    const auto p0 = pluginDefaults();

    // 2. at the defaults the choir is a choir: level with or above the lead it sits behind -------------------------
    for (double f0 : { 110.0, 196.0, 330.0 })
    {
        const auto x = sungPhrase(sr, 10, std::pow(10.0, -18.0 / 20), f0);
        const auto b = balance(x, sr, p0);
        const double vsLead = dB(b.choir) - dB(b.lead), vsInput = dB(b.total) - dB(b.input);
        std::printf("[2] f0 %3.0f Hz: choir %+.1f dB against the lead; whole output %+.1f dB against the input\n", f0, vsLead, vsInput);
        CHECK(vsLead > 0.0 && vsLead < 9.0, "the choir should sit 0..+9 dB over the lead at the defaults, got %+.1f dB", vsLead);
        CHECK(vsInput > -9.0, "the plugin at its defaults is %.1f dB below its input (limit -9 dB)", vsInput);
    }

    // 3. that balance does not depend on how loudly the singer sings (gate and tracker must not fade the choir out) --
    {
        double lo = 1e9, hi = -1e9;
        for (double lvl : { -33.0, -27.0, -21.0, -15.0, -9.0 })
        {
            const auto x = sungPhrase(sr, 10, std::pow(10.0, lvl / 20), 196);
            const auto b = balance(x, sr, p0); const double v = dB(b.choir) - dB(b.lead);
            lo = std::min(lo, v); hi = std::max(hi, v);
        }
        std::printf("[3] choir against lead from -33 to -9 dBFS input: %+.1f .. %+.1f dB\n", lo, hi);
        CHECK(hi - lo < 1.5, "the choir/lead balance moves by %.1f dB with the singing level (limit 1.5 dB)", hi - lo);
        CHECK(lo > 0.0, "the choir drops below the lead at some singing level (%+.1f dB)", lo);
    }

    // 4. every section is audible, none dominates ------------------------------------------------------------------
    for (double f0 : { 110.0, 220.0 })
    {
        const auto x = sungPhrase(sr, 10, std::pow(10.0, -18.0 / 20), f0); const auto m = voicedMask(x, sr);
        double lv[4], mean = 0;
        for (int s = 0; s < 4; ++s)
        {
            auto p = p0; p.mix = 1.f; p.leadGain = 0.f; for (int o = 0; o < 4; ++o) if (o != s) p.gain[(size_t) o] = 0.f;
            lv[s] = dB(rms(render(x, sr, p), m)); mean += lv[s] / 4;
        }
        std::printf("[4] f0 %3.0f Hz: sections soprano/alto/tenor/bass %+.1f / %+.1f / %+.1f / %+.1f dB around their mean\n", f0, lv[0] - mean, lv[1] - mean, lv[2] - mean, lv[3] - mean);
        for (int s = 0; s < 4; ++s) CHECK(std::fabs(lv[s] - mean) < 3.0, "section %d is %+.1f dB from the others (limit 3 dB)", s, lv[s] - mean);
    }

    // 5. CHOIR LEVEL does what it says -----------------------------------------------------------------------------
    {
        const auto x = sungPhrase(sr, 10, std::pow(10.0, -18.0 / 20), 196); const auto m = voicedMask(x, sr);
        auto at = [&](float db) { auto p = p0; p.mix = 1.f; p.leadGain = 0.f; p.choirLevelDb = db; return dB(rms(render(x, sr, p), m)); };
        const double base = at(0.f);
        std::printf("[5] CHOIR LEVEL -12 / -6 / +6 / +12 dB moves the choir by %+.2f / %+.2f / %+.2f / %+.2f dB\n", at(-12) - base, at(-6) - base, at(6) - base, at(12) - base);
        CHECK(std::fabs(at(6) - base - 6) < .7 && std::fabs(at(-6) - base + 6) < .7, "+-6 dB is not +-6 dB");
        CHECK(std::fabs(at(12) - base - 12) < 1.0 && std::fabs(at(-12) - base + 12) < 1.0, "+-12 dB is not +-12 dB");
        CHECK(std::fabs(at(40) - at(12)) < .05 && std::fabs(at(-40) - at(-12)) < .05, "values beyond +-12 dB must be clamped");
        // the A/B classic stage is bit-identical to v0.5 and must ignore the dial
        auto pc = p0; pc.classicStage = true; auto pd = pc; pd.choirLevelDb = 12.f;
        const auto a = render(x, sr, pc), b = render(x, sr, pd); double worst = 0;
        for (size_t i = 0; i < a.l.size(); ++i) worst = std::max(worst, (double) std::fabs(a.l[i] - b.l[i]));
        CHECK(worst == 0.0, "the classic stage changed with CHOIR LEVEL by %g", worst);
        // a sudden change of the dial must not click: the output step it causes stays small
        auto ps = p0; ps.mix = 1.f; ps.leadGain = 0.f;
        choir::StagedChoir sc; sc.prepare(sr); sc.setParameters(ps);
        std::vector<float> l(x.size()), r(x.size()); const size_t half = x.size() / 2; double maxStep = 0, maxStepRef = 0, prev = 0;
        sc.process(x.data(), x.data(), l.data(), r.data(), (int) half);
        auto pj = ps; pj.choirLevelDb = 12.f; sc.setParameters(pj);
        sc.process(x.data() + half, x.data() + half, l.data() + half, r.data() + half, (int) (x.size() - half));
        for (size_t i = 1; i < x.size(); ++i) { const double d = std::fabs((double) l[i] - l[i - 1]); (i < half ? maxStepRef : maxStep) = std::max(i < half ? maxStepRef : maxStep, d); }
        (void) prev;
        std::printf("[5] largest sample-to-sample step: before the +12 dB jump %.4f, after %.4f\n", maxStepRef, maxStep);
        CHECK(maxStep < 6.0 * maxStepRef + 1e-3, "CHOIR LEVEL jump clicks (step %.4f vs %.4f before)", maxStep, maxStepRef);
    }

    // 6. a hot vocal with everything turned up never passes full scale ---------------------------------------------
    {
        const auto x = sungPhrase(sr, 10, std::pow(10.0, -8.0 / 20), 196);
        auto p = p0; p.master = 1.5f; p.choirLevelDb = 12.f; p.mix = .9f;
        const auto o = render(x, sr, p);
        std::printf("[6] -8 dBFS vocal, OUTPUT 1.5, CHOIR LEVEL +12: peak %.3f\n", peak(o));
        CHECK(peak(o) <= 1.0, "output peak %.3f is over full scale", peak(o));
    }
    std::printf("ALL BALANCE TESTS PASSED\n");
}
