// Malachite fuzz test: random parameters, MIDI, host transport and patterns at 44.1-192 kHz; output must stay finite and bounded.
// g++ -O2 -std=c++11 fuzz.cpp -o fuzz && ./fuzz
#include "../plugins/Malachite/MalachiteEngine.hpp"
#include "../plugins/Malachite/ParamDefs.hpp"
#include <cstdio>
#include <random>
#include <vector>
using namespace malachite;
static Pattern pats[kNumPatterns];
int main() {
    std::mt19937 rng(99); std::uniform_real_distribution<float> u(0.f, 1.f);
    const float rates[] = { 44100, 48000, 88200, 96000, 192000 };
    int bad = 0; float worst = 0;
    for (int trial = 0; trial < 60; ++trial) {
        Engine e; const float fs = rates[trial % 5]; e.setSampleRate(fs);
        auto randomize = [&]() { for (uint32_t i = 0; i < kParamCount; ++i) if (!(paramDef(i).hints & kHintOutput)) e.p[i] = fromNorm(i, u(rng)); };
        randomize(); e.reset();
        double ppq = u(rng) * 64; const double bpm = 60 + 200 * u(rng);
        std::vector<float> l(512), r(512);
        for (int b = 0; b < (int)(fs * 3 / 512); ++b) {
            if (u(rng) < 0.05f) {                       // a random pattern bank
                for (Pattern& pt : pats) { pt.len = 1 + (int)(u(rng) * 16) % 16; for (Step& s : pt.s) { s.note = (int8_t)(u(rng) * 13) % 13; s.oct = (int8_t)((int)(u(rng) * 3) - 1); s.accent = u(rng) < 0.5f; s.slide = u(rng) < 0.5f; s.gate = (int8_t)((int)(u(rng) * 3) % 3); } }
                e.queuePatterns(serializePatterns(pats).c_str());
            }
            if (u(rng) < 0.2f) { const uint32_t p = (uint32_t)(u(rng) * kParamCount); if (!(paramDef(p).hints & kHintOutput)) e.p[p] = fromNorm(p, u(rng)); }
            if (u(rng) < 0.02f) randomize();
            if (u(rng) < 0.3f) e.midiNoteOn((int)(u(rng) * 128), 1 + (int)(u(rng) * 126));
            if (u(rng) < 0.3f) e.midiNoteOff((int)(u(rng) * 128));
            if (u(rng) < 0.01f) ppq = u(rng) * 64;      // the host jumps
            e.setTransport(u(rng) < 0.98f, u(rng) < 0.9f, ppq, bpm);
            e.process(l.data(), r.data(), 512);
            ppq += 512 * bpm / 60.0 / fs;
            for (int i = 0; i < 512; ++i) { if (!std::isfinite(l[i])) ++bad; worst = std::max(worst, std::fabs(l[i])); }
        }
    }
    printf("60 trials at 44.1-192 kHz with random settings, patterns, MIDI and transport jumps: %d non-finite samples, peak %.2f\n", bad, worst);
    const bool ok = bad == 0 && worst <= 8.f;
    printf("%s\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}
