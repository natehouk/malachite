// Malachite preset check: every sound preset plays the factory pattern I-1 A for four bars at 125 BPM.
// g++ -O2 -std=c++11 presets.cpp -o presets && ./presets
#include "../plugins/Malachite/MalachiteEngine.hpp"
#include "../plugins/Malachite/Presets.hpp"
#include <cstdio>
#include <vector>
using namespace malachite;
int main() {
    const int fs = 48000, len = (int)(fs * 4 * 4 * 60 / 125.0);
    int bad = 0;
    for (uint32_t p = 0; p < kNumPresets; ++p) {
        Engine e; e.setSampleRate((float)fs); std::copy(presets()[p].v, presets()[p].v + kParamCount, e.p); e.p[kSeqMode] = kSeqFree; e.p[kTempo] = 125; e.reset();
        std::vector<float> l(len), r(len); e.process(l.data(), r.data(), len);
        double s = 0; float pk = 0; bool fin = true;
        for (float v : l) { s += v * v; pk = std::max(pk, std::fabs(v)); if (!std::isfinite(v)) fin = false; }
        const double rms = 10 * std::log10(s / len + 1e-30);
        const bool ok = fin && pk < 1.5f && rms > -40 && rms < -6;
        if (!ok) ++bad;
        printf("%-16s %-14s rms %6.1f dBFS  peak %.2f %s\n", presets()[p].name, presets()[p].category, rms, pk, ok ? "" : "<-- CHECK");
    }
    printf("%u presets, %d flagged\n", kNumPresets, bad); return bad;
}
