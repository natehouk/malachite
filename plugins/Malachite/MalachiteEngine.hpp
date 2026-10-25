// Malachite — monophonic acid bass synthesizer with a 16-step pattern sequencer.
// Voice: one band-limited saw or square oscillator -> resonant 4-pole low-pass (zero-delay-feedback ladder with a
// saturating input and a high-passed resonance path, so it squelches without self-oscillating) -> VCA.
// A fixed amp envelope, a filter envelope with decay and depth, accent (louder, shorter and stronger filter sweep,
// with a charge that builds up over consecutive accents) and a fixed-time slide.
// The oscillator and ladder run at 2x the sample rate, and the ladder's saturation uses first-order antiderivative
// anti-aliasing (ADAA), which keeps the saturation's harmonics from folding back into the audible range.
// Clean-room implementation from public, generic techniques (polyBLEP oscillators, Zavalishin TPT ladder,
// halfband oversampling, ADAA after Parker, Zavalishin and Le Bivic, DAFx 2016). No
// third-party product code, assets, firmware or disassembly were used. License: MIT (see LICENSE).
#pragma once
#include "common/Halfband.hpp"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

namespace malachite {

static constexpr float kPi = 3.14159265358979f;
inline float clampf(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }
inline float dbToGain(float db) { return std::pow(10.f, db / 20.f); }

// ------------------------------------------------------------------ params (append-only)
enum Param : uint32_t {
    kWave, kTune, kCutoff, kResonance, kEnvMod, kDecay, kAccent, kVolume,
    kSeqMode, kGroup, kPattern, kSection, kTempo, kMidiOn,
    kOutLearn, kOutLevel,
    kParamCount
};
enum SeqMode { kSeqOff, kSeqHost, kSeqFree };
enum Section { kSecA, kSecB, kSecAB };

// ------------------------------------------------------------------ patterns
enum Gate { kRest, kNote, kTie };
struct Step { int8_t note = 0, oct = 0; bool accent = false, slide = false; int8_t gate = kNote; };   // note 0-12 (C..C), oct -1/0/+1
struct Pattern { int len = 16; Step s[16]; };
static constexpr int kGroups = 4, kPatternsPerGroup = 8, kNumPatterns = kGroups * kPatternsPerGroup * 2;
inline int patternIndex(int group, int pattern, int section) { return ((group * kPatternsPerGroup) + pattern) * 2 + section; }

// text form: per pattern one hex digit (length-1) and 16 x 3 hex digits per step; patterns separated by '.'
inline std::string serializePatterns(const Pattern* pats) {
    static const char* hx = "0123456789abcdef";
    std::string s; s.reserve(kNumPatterns * 50);
    for (int p = 0; p < kNumPatterns; ++p) {
        if (p) s += '.';
        s += hx[(pats[p].len - 1) & 15];
        for (int i = 0; i < 16; ++i) {
            const Step& st = pats[p].s[i];
            const int v = (st.note & 15) | ((st.oct + 1) & 3) << 4 | (st.accent ? 64 : 0) | (st.slide ? 128 : 0) | (st.gate & 3) << 8;
            s += hx[(v >> 8) & 15]; s += hx[(v >> 4) & 15]; s += hx[v & 15];
        }
    }
    return s;
}
inline bool parsePatterns(const char* str, Pattern* out) {
    auto hv = [](char c) { return c >= '0' && c <= '9' ? c - '0' : (c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1); };
    const char* q = str; if (!q) return false;
    for (int p = 0; p < kNumPatterns; ++p) {
        if (p) { if (*q != '.') return false; ++q; }
        const int l = hv(*q++); if (l < 0) return false;
        out[p].len = l + 1;
        for (int i = 0; i < 16; ++i) {
            const int a = hv(q[0]), b = a >= 0 ? hv(q[1]) : -1, c = b >= 0 ? hv(q[2]) : -1;
            if (c < 0) return false;
            q += 3; const int v = a << 8 | b << 4 | c;
            Step& st = out[p].s[i];
            st.note = (int8_t)std::min(12, v & 15); st.oct = (int8_t)(((v >> 4) & 3) - 1); if (st.oct > 1) st.oct = 1;
            st.accent = (v & 64) != 0; st.slide = (v & 128) != 0; st.gate = (int8_t)std::min(2, (v >> 8) & 3);
        }
    }
    return true;
}

// the factory patterns in group I (original compositions); everything else starts as a run of C notes
inline void defaultPatterns(Pattern* pats) {
    for (int p = 0; p < kNumPatterns; ++p) { pats[p] = Pattern(); for (Step& s : pats[p].s) s = Step(); }
    // compact form: per step "note oct flags", flags: a = accent, s = slide, t = tie, r = rest
    struct Def { int pattern, section, len; const char* steps; };
    static const Def defs[] = {
        { 0, 0, 16, "0 0 a,0 0 ,0 1 s,0 0 ,3 0 ,0 0 a,0 0 r,10 0 s,0 1 ,0 0 ,0 0 a,7 0 ,0 0 r,3 0 s,5 0 ,0 0 a" },
        { 0, 1, 16, "0 0 a,0 0 ,0 1 s,0 0 ,3 0 ,0 0 a,0 0 r,10 0 s,0 1 ,0 0 ,12 0 as,0 0 ,0 0 r,10 -1 ,7 -1 s,0 0 a" },
        { 1, 0, 16, "5 0 a,5 0 ,5 1 s,5 0 ,5 0 t,8 0 a,5 0 ,3 0 s,5 0 ,5 1 a,5 0 r,5 0 ,0 0 s,3 0 ,5 0 a,8 0 " },
        { 1, 1, 16, "5 0 a,5 0 ,5 1 s,5 0 ,5 0 t,8 0 a,5 0 ,3 0 s,5 0 ,5 1 a,5 0 r,10 0 s,12 0 ,10 0 a,8 0 s,5 0 " },
        { 2, 0, 16, "0 -1 a,0 0 s,0 -1 ,0 -1 ,0 0 as,0 -1 ,0 -1 a,0 0 ,0 -1 ,3 0 s,0 -1 ,0 -1 a,7 0 ,0 -1 s,0 0 ,0 -1 a" },
        { 3, 0, 12, "7 0 a,7 0 s,7 1 ,7 0 ,10 0 a,7 0 r,7 0 ,5 0 s,7 0 a,3 0 ,7 0 s,7 1 a" },
        { 4, 0, 16, "0 0 a,3 0 ,7 0 ,10 0 s,12 0 a,10 0 ,7 0 ,3 0 s,0 0 a,3 0 t,7 0 ,10 0 ,0 1 as,10 0 ,7 0 s,3 0 " },
        { 5, 0, 16, "2 0 a,2 0 r,2 1 ,2 0 r,2 0 a,2 0 s,2 1 ,2 0 r,2 0 a,5 0 s,2 0 ,2 0 r,0 0 a,2 0 s,2 1 ,2 0 r" },
        { 6, 0, 16, "9 -1 a,9 -1 ,9 0 s,9 -1 ,9 -1 t,9 -1 t,0 0 a,9 -1 ,9 -1 ,7 -1 s,9 -1 a,9 0 ,9 -1 r,9 -1 ,4 0 as,9 -1 " },
        { 7, 0, 8,  "0 0 a,0 1 s,0 0 ,0 0 a,0 1 ,0 0 s,3 0 a,0 0 " },
    };
    for (const Def& d : defs) {
        Pattern& pt = pats[patternIndex(0, d.pattern, d.section)];
        pt.len = d.len; const char* q = d.steps;
        for (int i = 0; i < d.len && *q; ++i) {
            int n = 0, o = 0; char fl[8] = {}; int used = 0;
            std::sscanf(q, "%d %d %7[astr]%n", &n, &o, fl, &used);
            if (!used) std::sscanf(q, "%d %d%n", &n, &o, &used);
            Step& s = pt.s[i]; s.note = (int8_t)n; s.oct = (int8_t)o;
            s.accent = std::strchr(fl, 'a') != nullptr; s.slide = std::strchr(fl, 's') != nullptr;
            s.gate = std::strchr(fl, 'r') ? kRest : (std::strchr(fl, 't') ? kTie : kNote);
            q += used; while (*q == ' ') ++q; if (*q == ',') ++q;
        }
    }
}

// ------------------------------------------------------------------ engine
class Engine {
public:
    float p[kParamCount];
    Pattern pats[kNumPatterns];

    Engine() {
        std::fill(p, p + kParamCount, 0.f);
        p[kWave] = 0; p[kTune] = 0; p[kCutoff] = 30; p[kResonance] = 70; p[kEnvMod] = 55; p[kDecay] = 45; p[kAccent] = 60; p[kVolume] = 70;
        p[kSeqMode] = kSeqHost; p[kGroup] = 0; p[kPattern] = 0; p[kSection] = kSecA; p[kTempo] = 126; p[kMidiOn] = 1;
        defaultPatterns(pats);
        std::fill(scope, scope + kScope, 0.f);
        setSampleRate(48000.f);
    }
    static constexpr int kOs = 2;                                         // oscillator + filter oversampling
    static int latency() { return epsilon::Oversampler::latency(kOs) / 2; } // only the down-sampler adds delay
    void setSampleRate(float sr) { fs = std::max(8000.f, sr); reset(); }
    void reset() {
        phase = 0; pitch = curNote = 36; ampEnv = 0; attacking = false; filtEnv = 0; accCap = 0; accentOn = false;
        std::memset(z, 0, sizeof z); os.reset(); uPrev = 0; hpZ = 0; dcY = 0; gate = false; held = 0; midiNote = -1;
        ppqFree = 0; lastStep = -1; gateOffAt = -1; transpose = 0; running = false; lastSlide = false;
        mLevel = 0; stepNow.store(-1); patNow.store(-1);
        sCut = p[kCutoff] / 100.f; sRes = p[kResonance] / 100.f; sEnv = p[kEnvMod] / 100.f; sVol = volGain();
    }

    // --- control from the host wrapper
    void setTransport(bool valid, bool playing, double ppqStart, double bpm) { tValid = valid; tPlaying = playing; tPpq = ppqStart; tBpm = bpm > 1 ? bpm : 120.0; }
    void midiNoteOn(int note, int vel) {
        if (p[kMidiOn] < 0.5f) return;
        const int mode = seqMode();
        if (mode != kSeqOff) { transpose = note - 60; return; }        // while the sequencer plays, keys transpose it
        const bool legato = held > 0 && gate;
        noteStack[held < 16 ? held : 15] = note; held = std::min(16, held + 1);
        play(note, vel >= 100, legato);                                   // overlapping notes slide; high velocity accents
    }
    void midiNoteOff(int note) {
        if (seqMode() != kSeqOff) return;
        int k = -1; for (int i = 0; i < held; ++i) if (noteStack[i] == note) k = i;
        if (k < 0) return;
        for (int i = k; i < held - 1; ++i) noteStack[i] = noteStack[i + 1];
        --held;
        if (held > 0) { if (note == midiNote) play(noteStack[held - 1], accentOn, true); }
        else gate = false;
    }
    void allNotesOff() { held = 0; gate = false; }

    // the UI hands over pattern edits as text; the audio thread picks them up at the start of the next block
    // returns false while the previous edit is still waiting to be picked up
    bool queuePatterns(const char* s) {
        if (pendingFlag.load()) return false;
        if (parsePatterns(s, pending)) pendingFlag.store(true);
        return true;
    }

    // --- UI read-outs
    static constexpr int kScope = 4096;
    float scope[kScope];
    std::atomic<uint32_t> scopeWrite { 0 };
    std::atomic<int> stepNow { -1 }, patNow { -1 };
    float sampleRate() const { return fs; }
    float level() const { return mLevel; }
    bool gateOn() const { return gate; }
    bool accentLit() const { return gate && accentOn; }

    void process(float* outL, float* outR, uint32_t n) {
        if (pendingFlag.load()) { std::copy(pending, pending + kNumPatterns, pats); pendingFlag.store(false); }
        const int mode = seqMode();
        // where the sequencer is (in quarter notes) at the start of this block
        double ppq = 0, beatsPerSample = 0; bool seq = false;
        if (mode == kSeqHost && tValid && tPlaying) { ppq = tPpq; beatsPerSample = tBpm / 60.0 / fs; seq = true; }
        else if (mode == kSeqFree) { if (!running) { ppqFree = 0; lastStep = -1; } ppq = ppqFree; beatsPerSample = clampf(p[kTempo], 40.f, 300.f) / 60.0 / fs; seq = true; }
        if (seq != running) {                                             // started or stopped
            running = seq; lastStep = -1; gateOffAt = -1;
            if (!seq) { gate = false; stepNow.store(-1); patNow.store(-1); transpose = 0; }
        }
        if (!seq && mode != kSeqOff && held) held = 0;
        const float decayS = 0.2f * std::pow(12.5f, clampf(p[kDecay], 0.f, 100.f) / 100.f);   // 0.2 - 2.5 s
        const float fDecay = std::exp(-6.9f / (decayS * fs)), fDecayAcc = std::exp(-6.9f / (0.2f * fs));
        const float ampDecay = std::exp(-1.f / (1.6f * fs)), ampRel = std::exp(-1.f / (0.006f * fs)), ampAtt = 1.f - std::exp(-1.f / (0.0012f * fs));
        const float glide = 1.f - std::exp(-1.f / (0.035f * fs));
        const float acc = clampf(p[kAccent], 0.f, 100.f) / 100.f;
        // the accent charge fills with each accented sweep and drains slowly: back-to-back accents ramp up, more with resonance
        const float rz = clampf(p[kResonance], 0.f, 100.f) / 100.f;
        const float accUp = 1.f - std::exp(-1.f / ((0.04f + 0.08f * rz) * fs)), accDown = 1.f - std::exp(-1.f / ((0.12f + 0.6f * rz) * fs));
        const float kSm = 1.f - std::exp(-1.f / (0.01f * fs));
        const bool square = p[kWave] > 0.5f;
        const float tune = clampf(p[kTune], -12.f, 12.f);
        const float hpK = 1.f - std::exp(-2.f * kPi * 120.f / (fs * kOs)); // resonance path high-pass (oversampled rate)
        const float dcK = 1.f - std::exp(-2.f * kPi * 25.f / fs);
        uint32_t sw = scopeWrite.load(std::memory_order_relaxed);
        const float meterFall = std::exp(-1.f / (0.3f * fs));
        for (uint32_t i = 0; i < n; ++i) {
            if (seq) {
                const long stepAbs = (long)std::floor(ppq * 4.0 + 1e-9);
                if (stepAbs != lastStep) { onStep(stepAbs); lastStep = stepAbs; }
                if (gateOffAt >= 0 && ppq >= gateOffAt) { gate = false; gateOffAt = -1; }
                ppq += beatsPerSample;
            }
            // smoothed knobs
            sCut += (clampf(p[kCutoff], 0.f, 100.f) / 100.f - sCut) * kSm;
            sRes += (clampf(p[kResonance], 0.f, 100.f) / 100.f - sRes) * kSm;
            sEnv += (clampf(p[kEnvMod], 0.f, 100.f) / 100.f - sEnv) * kSm;
            sVol += (volGain() - sVol) * kSm;
            // pitch (slides glide; plain notes jump)
            pitch += (curNote - pitch) * glide;
            const float hz = 440.f * std::exp2((pitch + tune - 69.f) / 12.f);
            const float dt = std::min(0.45f, hz / fs);
            // oscillator, at kOs times the sample rate
            float osc[kOs]; const float dto = dt / kOs;
            for (int j = 0; j < kOs; ++j) {
                phase += dto; if (phase >= 1.f) phase -= 1.f;
                if (!square) osc[j] = 2.f * phase - 1.f - blep(phase, dto);
                else { float ph2 = phase + 0.5f; if (ph2 >= 1.f) ph2 -= 1.f; osc[j] = ((phase < 0.5f ? 1.f : -1.f) + blep(phase, dto) - blep(ph2, dto)) * 0.85f; }
            }
            // envelopes
            filtEnv *= accentOn ? fDecayAcc : fDecay;
            if (gate) {
                if (attacking) { ampEnv += (1.02f - ampEnv) * ampAtt; if (ampEnv >= 1.f) { ampEnv = 1.f; attacking = false; } }
                else ampEnv *= ampDecay;
            } else ampEnv *= ampRel;
            { const float drive = accentOn ? filtEnv : 0.f; accCap += (drive - accCap) * (drive > accCap ? accUp : accDown); }
            // filter
            const float oct = sEnv * filtEnv * 4.2f + acc * accCap * 3.f;
            const float fc = clampf(28.f * std::exp2(sCut * 7.4f + oct), 20.f, 0.42f * fs);
            const float k = 3.75f * std::pow(sRes, 0.8f);
            for (int j = 0; j < kOs; ++j) osc[j] = ladder(osc[j] * 0.9f, fc, k, hpK, fs * kOs);
            float y = os.down(osc, kOs);
            // VCA, output
            const float vel = 1.f + (accentOn ? acc * 0.9f : 0.f);
            float out = y * ampEnv * vel * sVol;
            dcY += (out - dcY) * dcK; out -= dcY;                             // output coupling
            if (!(std::fabs(out) < 8.f)) { out = std::isfinite(out) ? std::copysign(8.f, out) : 0.f; std::memset(z, 0, sizeof z); }
            outL[i] = outR[i] = out;
            scope[sw & (kScope - 1)] = out; ++sw;
            mLevel = std::max(mLevel * meterFall, std::fabs(out));
        }
        if (mode == kSeqFree && seq) ppqFree = ppq;
        if (mode == kSeqHost && seq) tPpq = ppq;                             // a later sub-block of the same host block continues here
        scopeWrite.store(sw, std::memory_order_release);
    }

    int seqMode() const { return (int)clampf(std::round(p[kSeqMode]), 0, 2); }
    const Pattern& patternAt(int idx) const { return pats[idx]; }

private:
    float volGain() const { const float v = clampf(p[kVolume], 0.f, 100.f) / 100.f; return v * v * 2.0f; }
    // antiderivative of tanh(1.4x) / 1.4, i.e. ln(cosh(1.4x)) / 1.96, in a form that does not overflow
    static float satAD(float x) { const float a = std::fabs(1.4f * x); return (a + std::log1p(std::exp(-2.f * a)) - 0.69314718f) / 1.96f; }
    static float blep(float t, float dt) {
        if (t < dt) { t /= dt; return t + t - t * t - 1.f; }
        if (t > 1.f - dt) { t = (t - 1.f) / dt; return t * t + t + t + 1.f; }
        return 0.f;
    }
    // zero-delay-feedback 4-pole ladder (Zavalishin's TPT form). The resonance feedback is high-passed (the low end
    // stays when resonance is up), and the input to the ladder is soft-saturated.
    float ladder(float x, float fc, float k, float hpK, float fsl) {
        const float g = std::tan(kPi * fc / fsl), G = g / (1.f + g), B = 1.f / (1.f + g);
        const float G2 = G * G, G3 = G2 * G, G4 = G2 * G2;
        const float S = G3 * B * z[0] + G2 * B * z[1] + G * B * z[2] + B * z[3];   // y4 = G4 * u + S
        float u = (x - k * (S - hpZ)) / (1.f + k * G4);
        {   // u = tanh(1.4u) / 1.4, with first-order ADAA: the mean of the curve between this input and the last
            const float d = u - uPrev;
            const float s = std::fabs(d) > 1e-4f ? (satAD(u) - satAD(uPrev)) / d : std::tanh(0.7f * (u + uPrev)) / 1.4f;
            uPrev = u; u = s;
        }
        float v = u;
        for (int i = 0; i < 4; ++i) { const float w = (v - z[i]) * G, o = w + z[i]; z[i] = o + w; v = o; }
        hpZ += (v - hpZ) * hpK;
        return v;
    }

    void play(int note, bool accent, bool slide) {
        curNote = (float)note; midiNote = note;
        if (!slide) { pitch = curNote; filtEnv = 1.f; accentOn = accent; gate = true; attacking = true; }
        else { gate = true; }
    }
    void onStep(long stepAbs) {
        const int group = (int)clampf(std::round(p[kGroup]), 0, kGroups - 1), patn = (int)clampf(std::round(p[kPattern]), 0, kPatternsPerGroup - 1);
        const int secSel = (int)clampf(std::round(p[kSection]), 0, 2);
        const Pattern& pa = pats[patternIndex(group, patn, 0)];
        const Pattern& pb = pats[patternIndex(group, patn, 1)];
        // which section and step: A+B plays A then B
        int sec = secSel == kSecAB ? 0 : secSel; long idx = stepAbs;
        if (secSel == kSecAB) { const long cyc = pa.len + pb.len; long m = ((stepAbs % cyc) + cyc) % cyc; if (m >= pa.len) { sec = 1; m -= pa.len; } idx = m; }
        const Pattern& pt = sec == 0 ? pa : pb;
        const int len = std::max(1, pt.len);
        const int si = (int)(((idx % len) + len) % len);
        const Step& st = pt.s[si];
        // the following step (for ties and slides)
        const Step& nx = (si + 1 < len) ? pt.s[si + 1] : pt.s[0];
        stepNow.store(si); patNow.store(patternIndex(group, patn, sec));
        const double stepStart = (double)stepAbs / 4.0;
        const bool prevSlide = lastSlide && gate;
        if (st.gate == kRest) { gate = false; gateOffAt = -1; lastSlide = false; return; }
        if (st.gate == kNote) {
            const int note = 36 + st.note + 12 * st.oct + transpose;
            if (prevSlide) { curNote = (float)note; accentOn = st.accent ? true : accentOn; if (st.accent) filtEnv = std::max(filtEnv, 1.f); }
            else { curNote = pitch = (float)note; filtEnv = 1.f; accentOn = st.accent; gate = true; attacking = true; }
            gate = true;
        } else if (!gate) { lastSlide = false; return; }                  // a tie after a rest stays silent
        // hold through to the next step when sliding into it or when it is a tie; otherwise release half way
        const bool hold = (st.slide && nx.gate != kRest) || nx.gate == kTie;
        gateOffAt = hold ? -1 : stepStart + 0.125;
        lastSlide = st.gate == kNote ? st.slide : lastSlide;
    }

    float fs = 48000.f;
    epsilon::Oversampler os; float uPrev = 0;
    float phase = 0, pitch = 36, curNote = 36, ampEnv = 0, filtEnv = 0, accCap = 0;
    bool accentOn = false, gate = false, lastSlide = false, attacking = false;
    float z[4] = {}, hpZ = 0, dcY = 0;
    float sCut = 0.3f, sRes = 0.7f, sEnv = 0.5f, sVol = 0.5f, mLevel = 0;
    int noteStack[16] = {}, held = 0, midiNote = -1, transpose = 0;
    bool tValid = false, tPlaying = false, running = false; double tPpq = 0, tBpm = 120, ppqFree = 0, gateOffAt = -1; long lastStep = -1;
    Pattern pending[kNumPatterns]; std::atomic<bool> pendingFlag { false };
};

} // namespace malachite
