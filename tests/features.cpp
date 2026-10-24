// Malachite feature tests: g++ -O2 -std=c++11 features.cpp -o features && ./features
#include "../plugins/Malachite/MalachiteEngine.hpp"
#include "../plugins/Malachite/Presets.hpp"
#include <cstdio>
#include <vector>
using namespace malachite;

static int fails = 0;
#define check(cond, ...) do { const bool ok_ = (cond); printf("%s  ", ok_ ? "PASS" : "FAIL"); printf(__VA_ARGS__); printf("\n"); if (!ok_) ++fails; } while (0)
static const float FS = 48000.f;

struct Ev { int at; int note; int vel; };      // vel 0 = note off
static std::vector<float> render(Engine& e, int len, std::vector<Ev> evs = {}) {
    std::vector<float> l(len), r(len); size_t k = 0; int pos = 0;
    while (pos < len) {
        int until = len; if (k < evs.size()) until = std::min(until, evs[k].at);
        if (until > pos) { e.process(&l[pos], &r[pos], until - pos); pos = until; }
        while (k < evs.size() && evs[k].at <= pos) { if (evs[k].vel > 0) e.midiNoteOn(evs[k].note, evs[k].vel); else e.midiNoteOff(evs[k].note); ++k; }
    }
    return l;
}
static Engine* make(std::initializer_list<std::pair<uint32_t, float>> ps) {
    Engine* e = new Engine(); e->setSampleRate(FS); e->p[kSeqMode] = kSeqOff;
    for (auto& kv : ps) e->p[kv.first] = kv.second;
    e->reset(); return e;
}
static double rmsDb(const std::vector<float>& a, int from, int to) { double s = 0; for (int i = from; i < to; ++i) s += (double)a[i] * a[i]; return 10 * std::log10(s / std::max(1, to - from) + 1e-30); }
static float peakAbs(const std::vector<float>& a, int from, int to) { float p = 0; for (int i = from; i < to; ++i) p = std::max(p, std::fabs(a[i])); return p; }
// frequency from upward zero crossings
static float freqAt(const std::vector<float>& a, int from, int to) {
    int first = -1, last = -1, n = 0;
    for (int i = from + 1; i < to; ++i) if (a[i - 1] < 0 && a[i] >= 0) { if (first < 0) first = i; last = i; ++n; }
    return n > 1 ? (n - 1) * FS / (last - first) : 0.f;
}
// energy above 1 kHz (two 12 dB/oct high-pass stages) relative to the whole
static double brightness(const std::vector<float>& a, int from, int to) {
    const double w = 2 * 3.14159265358979 * 1000 / FS, al = std::sin(w) / (2 * 0.7071), a0 = 1 + al, cs = std::cos(w);
    const double b0 = (1 + cs) / 2 / a0, b1 = -(1 + cs) / a0, b2 = b0, a1 = -2 * cs / a0, a2 = (1 - al) / a0;
    double x1[2] = {}, x2[2] = {}, y1[2] = {}, y2[2] = {}, e = 0, h = 0;
    for (int i = std::max(0, from - 2400); i < to; ++i) {
        double v = a[i];
        for (int k = 0; k < 2; ++k) { const double y = b0 * v + b1 * x1[k] + b2 * x2[k] - a1 * y1[k] - a2 * y2[k]; x2[k] = x1[k]; x1[k] = v; y2[k] = y1[k]; y1[k] = y; v = y; }
        if (i >= from) { e += (double)a[i] * a[i]; h += v * v; }
    }
    return 10 * std::log10(h / (e + 1e-30) + 1e-30);
}
// harmonic amplitude by correlation
static double harm(const std::vector<float>& a, int from, int to, float f) {
    double c = 0, s = 0; for (int i = from; i < to; ++i) { c += a[i] * std::cos(2 * kPi * f * i / FS); s += a[i] * std::sin(2 * kPi * f * i / FS); }
    return 2 * std::sqrt(c * c + s * s) / (to - from);
}
// hand a pattern to the engine and let it pick it up (one silent sample)
static void setPattern(Engine& e, int idx, const Pattern& p) {
    static Pattern all[kNumPatterns]; std::copy(e.pats, e.pats + kNumPatterns, all); all[idx] = p; e.queuePatterns(serializePatterns(all).c_str());
    const float m = e.p[kSeqMode]; e.p[kSeqMode] = kSeqOff; float l, r; e.process(&l, &r, 1); e.p[kSeqMode] = m;
}

int main() {
    const int N = (int)FS;
    // 1. saw has every harmonic, square mostly odd ones
    { Engine* s = make({ { kCutoff, 100 }, { kResonance, 0 }, { kEnvMod, 0 } }), * q = make({ { kWave, 1 }, { kCutoff, 100 }, { kResonance, 0 }, { kEnvMod, 0 } });
      const std::vector<float> a = render(*s, N, { { 0, 45, 80 } }), b = render(*q, N, { { 0, 45, 80 } });
      const float f = 110.f;
      const double sa = harm(a, 4800, 24000, 2 * f) / harm(a, 4800, 24000, f), sq = harm(b, 4800, 24000, 2 * f) / harm(b, 4800, 24000, f);
      check(sa > 0.3 && sq < 0.15, "waveform: 2nd harmonic relative to the 1st: saw %.2f, square %.2f", sa, sq); delete s; delete q; }

    // 2. tuning: +12 semitones doubles the pitch
    { Engine* a = make({ { kCutoff, 100 }, { kResonance, 0 }, { kEnvMod, 0 } }), * b = make({ { kCutoff, 100 }, { kResonance, 0 }, { kEnvMod, 0 }, { kTune, 12 } });
      const float fa = freqAt(render(*a, N, { { 0, 45, 80 } }), 4800, 30000), fb = freqAt(render(*b, N, { { 0, 45, 80 } }), 4800, 30000);
      check(std::fabs(fa - 110) < 1.5f && std::fabs(fb / fa - 2) < 0.02f, "pitch: A2 plays %.1f Hz; tuning +12 st plays %.1f Hz", fa, fb); delete a; delete b; }

    // 3. cutoff opens the sound up
    { Engine* lo = make({ { kCutoff, 10 }, { kEnvMod, 0 }, { kResonance, 0 } }), * hi = make({ { kCutoff, 90 }, { kEnvMod, 0 }, { kResonance, 0 } });
      const double bl = brightness(render(*lo, N, { { 0, 45, 80 } }), 4800, 24000), bh = brightness(render(*hi, N, { { 0, 45, 80 } }), 4800, 24000);
      check(bh - bl > 15, "cutoff 10%% -> 90%%: high-frequency share %.1f -> %.1f dB", bl, bh); delete lo; delete hi; }

    // 4. resonance emphasises the cutoff, and never rings on its own
    { Engine* r0 = make({ { kCutoff, 45 }, { kEnvMod, 0 }, { kResonance, 0 } }), * r1 = make({ { kCutoff, 45 }, { kEnvMod, 0 }, { kResonance, 100 } });
      const std::vector<float> a = render(*r0, N, { { 0, 33, 80 }, { 24000, 33, 0 } }), b = render(*r1, N, { { 0, 33, 80 }, { 24000, 33, 0 } });
      const double ba = brightness(a, 4800, 20000), bb = brightness(b, 4800, 20000);
      const double tail = rmsDb(b, 24000 + 9600, N), body = rmsDb(b, 4800, 20000);
      check(bb - ba > 4 && body - tail > 80, "resonance 0 -> 100%%: high share %.1f -> %.1f dB; 0.2 s after release %.0f dB down (no self-oscillation)", ba, bb, body - tail);
      delete r0; delete r1; }

    // 5. the filter envelope: decay sets how long the note stays bright
    { auto bright = [&](float dec) { Engine* e = make({ { kCutoff, 15 }, { kEnvMod, 80 }, { kResonance, 40 }, { kDecay, dec } }); const std::vector<float> a = render(*e, N, { { 0, 40, 80 } });
          delete e; return brightness(a, 12000, 16800); };
      const double s = bright(0), l = bright(100);
      check(l - s > 10, "decay 0%% -> 100%%: brightness a quarter second into the note %.1f -> %.1f dB", s, l); }

    // 6. accent: louder and brighter
    { Engine* e = make({ { kAccent, 100 } });
      const std::vector<float> a = render(*e, N, { { 0, 40, 80 }, { 9600, 40, 0 }, { 24000, 40, 127 }, { 33600, 40, 0 } });
      const float pn = peakAbs(a, 0, 9600), pa = peakAbs(a, 24000, 33600);
      check(pa / pn > 1.4f, "accent: peak %.2f -> %.2f (%.1f dB louder)", pn, pa, 20 * std::log10(pa / pn)); delete e; }

    // 7. consecutive accents build up (high resonance): the filter rises further each time
    { Engine* e = make({ { kAccent, 100 }, { kResonance, 95 }, { kCutoff, 15 }, { kEnvMod, 40 }, { kDecay, 20 } });
      std::vector<Ev> ev; for (int k = 0; k < 6; ++k) { ev.push_back({ k * 6000, 40, 127 }); ev.push_back({ k * 6000 + 3000, 40, 0 }); }
      const std::vector<float> a = render(*e, 36000, ev);
      const double b1 = brightness(a, 6000, 8400), b5 = brightness(a, 5 * 6000, 5 * 6000 + 2400);
      Engine* f = make({ { kAccent, 0 }, { kResonance, 95 }, { kCutoff, 15 }, { kEnvMod, 40 }, { kDecay, 20 } });
      const std::vector<float> c = render(*f, 36000, ev); const double c1 = brightness(c, 6000, 8400), c5 = brightness(c, 5 * 6000, 5 * 6000 + 2400);
      check((b5 - b1) - (c5 - c1) > 1.5, "accent build-up: 2nd vs 6th accented note brightness %.1f -> %.1f dB (accent off: %.1f -> %.1f dB)", b1, b5, c1, c5);
      delete e; delete f; }

    // 8. slide: an overlapping note glides there without a new attack
    { Engine* e = make({ { kCutoff, 100 }, { kResonance, 0 }, { kEnvMod, 0 } });
      const std::vector<float> a = render(*e, N, { { 0, 45, 80 }, { 24000, 57, 80 }, { 24100, 45, 0 } });
      const float f0 = freqAt(a, 24000, 25000), f1 = freqAt(a, 24000 + 9600, 24000 + 14400);
      const float gap = peakAbs(a, 23500, 24000) > 0 ? peakAbs(a, 24000, 24480) / peakAbs(a, 23500, 24000) : 0;
      check(f0 < 180 && std::fabs(f1 - 220) < 4 && gap < 1.3f, "slide: 110 -> 220 Hz, first 20 ms at %.0f Hz, after 200 ms %.1f Hz, no new attack (level x%.2f)", f0, f1, gap); delete e; }

    // 9. the sequencer in FREE mode at 120 BPM: steps every 125 ms; rest is silent, tie holds the note
    { Engine* e = make({ { kSeqMode, kSeqFree }, { kTempo, 120 }, { kCutoff, 100 }, { kResonance, 0 }, { kEnvMod, 0 } });
      Pattern p; p.len = 4; for (Step& s : p.s) s = Step();
      p.s[0].note = 0; p.s[1].gate = kRest; p.s[2].note = 7; p.s[3].gate = kTie;
      setPattern(*e, patternIndex(0, 0, 0), p);
      const std::vector<float> a = render(*e, 48000);
      const int st = 6000;   // samples per step
      const float s0 = peakAbs(a, 100, st / 2 - 100), s0off = peakAbs(a, st / 2 + 1500, st), s1 = peakAbs(a, st + 600, 2 * st), s2 = peakAbs(a, 2 * st + 100, 3 * st - 100), s3 = peakAbs(a, 3 * st + 100, 4 * st - 600);
      const float f2 = freqAt(a, 2 * st + 300, 3 * st - 300);
      check(s0 > 0.05f && s0off < s0 * 0.05f && s1 < s0 * 0.05f && s3 > s2 * 0.5f && std::fabs(f2 - 98) < 2,
            "sequencer: note %.2f, released half way (%.3f), rest %.3f, G2 at %.1f Hz, tie still sounding (%.2f)", s0, s0off, s1, f2, s3); delete e; }

    // 10. following the host: steps line up with the host position; stopping releases the note
    { Engine* e = make({ { kSeqMode, kSeqHost }, { kCutoff, 100 }, { kResonance, 0 }, { kEnvMod, 0 } });
      std::vector<float> l(480), r(480); int firstStep = -2;
      e->setTransport(true, true, 3.875, 120.0);          // half a step before beat 4.0... step 15 of bar 1
      e->process(l.data(), r.data(), 480); firstStep = e->stepNow.load();
      e->setTransport(true, true, 4.0, 120.0); e->process(l.data(), r.data(), 480); const int s16 = e->stepNow.load();
      e->setTransport(true, false, 4.1, 120.0); e->process(l.data(), r.data(), 480); const bool stopped = !e->gateOn() && e->stepNow.load() == -1;
      check(firstStep == 15 && s16 == 0 && stopped, "host sync: position 3.875 beats plays step %d, 4.0 plays step %d, stop releases the note: %s", firstStep + 1, s16 + 1, stopped ? "yes" : "no");
      delete e; }

    // 11. pattern length, A+B chaining and MIDI transpose
    { Engine* e = make({ { kSeqMode, kSeqHost }, { kSection, kSecAB } });
      Pattern a, b; a.len = 3; b.len = 2;
      setPattern(*e, patternIndex(0, 0, 0), a); setPattern(*e, patternIndex(0, 0, 1), b);
      std::vector<float> l(16), r(16); std::vector<int> seen;
      for (int k = 0; k < 10; ++k) { e->setTransport(true, true, k * 0.25 + 0.01, 120.0); e->process(l.data(), r.data(), 16); seen.push_back(e->patNow.load() == patternIndex(0, 0, 1) ? 10 + e->stepNow.load() : e->stepNow.load()); }
      bool ok = true; const int want[10] = { 0, 1, 2, 10, 11, 0, 1, 2, 10, 11 }; for (int k = 0; k < 10; ++k) if (seen[k] != want[k]) ok = false;
      check(ok, "A (3 steps) + B (2 steps) chain: A1 A2 A3 B1 B2 A1 ...");
      Engine* t = make({ { kSeqMode, kSeqFree }, { kTempo, 120 }, { kCutoff, 100 }, { kResonance, 0 }, { kEnvMod, 0 } });
      Pattern c; c.len = 1; setPattern(*t, patternIndex(0, 0, 0), c);
      const std::vector<float> x = render(*t, 24000, { { 0, 67, 100 } });    // G3: up 7 semitones from C3
      const float fx = freqAt(x, 6000 + 300, 9000);
      check(std::fabs(fx - 98.0f) < 2, "MIDI transpose while the sequencer runs: C2 pattern + G3 key plays %.1f Hz (G2)", fx);
      delete e; delete t; }

    // 12. patterns survive their text form; damaged text is refused
    { static Pattern p[kNumPatterns], q[kNumPatterns]; defaultPatterns(p); p[5].s[3].accent = true; p[5].s[3].oct = -1; p[5].len = 7; p[63].s[15].gate = kTie;
      const std::string s = serializePatterns(p); const bool ok = parsePatterns(s.c_str(), q); const std::string t = serializePatterns(q);
      std::string bad = s; bad[100] = 'z';
      check(ok && s == t && !parsePatterns(bad.c_str(), q) && !parsePatterns("", q), "pattern text: %zu characters, round trip exact, damaged text refused", s.size()); }

    // 13. all presets finite and bounded, playing the factory pattern
    { bool ok = true; float worst = 0;
      for (uint32_t i = 0; i < kNumPresets; ++i) { Engine e; std::copy(presets()[i].v, presets()[i].v + kParamCount, e.p); e.p[kSeqMode] = kSeqFree; e.reset();
          std::vector<float> l(48000), r(48000); e.process(l.data(), r.data(), 48000);
          for (float v : l) { if (!std::isfinite(v)) ok = false; worst = std::max(worst, std::fabs(v)); } }
      check(ok && worst < 2.f, "all %u presets finite (peak %.2f)", kNumPresets, worst); }

    printf("\n%s (%d failure%s)\n", fails ? "FAILED" : "ALL PASSED", fails, fails == 1 ? "" : "s");
    return fails;
}
