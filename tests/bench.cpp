// Malachite CPU benchmark: g++ -O3 -std=c++11 bench.cpp -o bench && ./bench
#include "../plugins/Malachite/MalachiteEngine.hpp"
#include <chrono>
#include <cstdio>
#include <vector>
using namespace malachite;
int main() {
    Engine e; e.setSampleRate(48000); e.p[kSeqMode] = kSeqFree; e.p[kTempo] = 130; e.reset();
    const int n = 256, secs = 30; std::vector<float> l(n), r(n);
    const auto t0 = std::chrono::steady_clock::now();
    for (int b = 0; b < 48000 * secs / n; ++b) { e.p[kCutoff] = 30 + 20 * std::sin(b * 0.01f); e.process(l.data(), r.data(), n); }
    const double el = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    printf("sequencer running, cutoff moving: %.2f%% of one core at 48 kHz\n", 100 * el / secs);
}
