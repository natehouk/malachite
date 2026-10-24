// Parameter metadata shared by the DSP wrapper and the UI. License: MIT.
#pragma once
#include "MalachiteEngine.hpp"
#include <cstdio>

namespace malachite {

// kHintNoPreset: sequencer settings, which sound presets leave alone
static constexpr uint32_t kHintInteger = 1, kHintBoolean = 2, kHintLog = 4, kHintOutput = 8, kHintNoAuto = 16, kHintNoPreset = 32;
struct ParamDef { const char* name; const char* sym; const char* unit; float min, max; uint32_t hints; };

inline const ParamDef& paramDef(uint32_t i) {
    static ParamDef t[kParamCount];
    static bool done = false;
    if (!done) {
        done = true;
        auto set = [](uint32_t i, const char* n, const char* s, const char* u, float lo, float hi, uint32_t h) { t[i] = { n, s, u, lo, hi, h }; };
        const uint32_t I = kHintInteger, B = kHintBoolean | kHintInteger, O = kHintOutput, N = kHintNoPreset;
        set(kWave,      "Waveform",        "wave",     "",    0, 1, I);
        set(kTune,      "Tuning",          "tune",     "st",  -12, 12, 0);
        set(kCutoff,    "Cutoff",          "cutoff",   "%",   0, 100, 0);
        set(kResonance, "Resonance",       "reso",     "%",   0, 100, 0);
        set(kEnvMod,    "Env Mod",         "envmod",   "%",   0, 100, 0);
        set(kDecay,     "Decay",           "decay",    "%",   0, 100, 0);
        set(kAccent,    "Accent",          "accent",   "%",   0, 100, 0);
        set(kVolume,    "Volume",          "volume",   "%",   0, 100, 0);
        set(kSeqMode,   "Sequencer",       "seqmode",  "",    0, 2, I | N);
        set(kGroup,     "Pattern Group",   "group",    "",    0, kGroups - 1, I | N);
        set(kPattern,   "Pattern",         "pattern",  "",    0, kPatternsPerGroup - 1, I | N);
        set(kSection,   "Section",         "section",  "",    0, 2, I | N);
        set(kTempo,     "Free Tempo",      "tempo",    "BPM", 40, 300, N);
        set(kMidiOn,    "MIDI",            "midion",   "",    0, 1, B);
        set(kOutLearn,  "MIDI Learn",      "o_learn",  "",    0, 10000000, O);
        set(kOutLevel,  "Output Level",    "o_level",  "dB",  -120, 24, O);
    }
    return t[i];
}

inline const char* const* enumLabels(uint32_t p, int& count) {
    static const char* const wave[] = { "Saw", "Square" };
    static const char* const seq[] = { "Off (MIDI)", "Host", "Free" };
    static const char* const grp[] = { "I", "II", "III", "IV" };
    static const char* const pat[] = { "1", "2", "3", "4", "5", "6", "7", "8" };
    static const char* const sec[] = { "A", "B", "A+B" };
    switch (p) {
    case kWave: count = 2; return wave;
    case kSeqMode: count = 3; return seq;
    case kGroup: count = 4; return grp;
    case kPattern: count = 8; return pat;
    case kSection: count = 3; return sec;
    default: count = 0; return nullptr;
    }
}

inline float toNorm(uint32_t p, float v) {
    const ParamDef& d = paramDef(p);
    if (d.hints & kHintLog) return std::log(clampf(v, d.min, d.max) / d.min) / std::log(d.max / d.min);
    return (clampf(v, d.min, d.max) - d.min) / (d.max - d.min);
}
inline float fromNorm(uint32_t p, float n) {
    const ParamDef& d = paramDef(p);
    n = clampf(n, 0.f, 1.f);
    float v = (d.hints & kHintLog) ? d.min * std::pow(d.max / d.min, n) : d.min + n * (d.max - d.min);
    if (d.hints & kHintInteger) v = std::round(v);
    return v;
}

} // namespace malachite
