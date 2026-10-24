// Factory sound presets (each lists only what it changes from the engine defaults). Presets set the sound only:
// the sequencer settings and the patterns stay as they are. License: MIT.
// Engine defaults: saw, tuning 0, cutoff 30%, resonance 70%, env mod 55%, decay 45%, accent 60%, volume 70%.
#pragma once
#include "MalachiteEngine.hpp"
#include <algorithm>

namespace malachite {

struct PV { uint32_t id; float v; };
struct PresetDef { const char* name; const char* category; const PV* pv; };
struct Preset { const char* name; const char* category; float v[kParamCount]; };

static const char* const kCategories[] = { "Acid", "Bass", "Leads", "Squelch & FX" };
static constexpr int kNumCategories = 4;

#define END { kParamCount, 0 }
namespace pv {
static const PV init[] = { END };
// ---------------------------------------------------------------- Acid
static const PV classic[]   = { {kCutoff,28}, {kResonance,78}, {kEnvMod,60}, {kDecay,40}, {kAccent,70}, END };
static const PV squelchy[]  = { {kCutoff,22}, {kResonance,92}, {kEnvMod,72}, {kDecay,35}, {kAccent,85}, END };
static const PV rubber[]    = { {kCutoff,34}, {kResonance,66}, {kEnvMod,40}, {kDecay,62}, {kAccent,45}, END };
static const PV hardAcid[]  = { {kCutoff,40}, {kResonance,88}, {kEnvMod,80}, {kDecay,25}, {kAccent,100}, {kVolume,75}, END };
static const PV squareAcid[]= { {kWave,1}, {kCutoff,30}, {kResonance,80}, {kEnvMod,62}, {kDecay,38}, {kAccent,75}, END };
static const PV slowAcid[]  = { {kCutoff,18}, {kResonance,84}, {kEnvMod,70}, {kDecay,85}, {kAccent,55}, END };
static const PV chirp[]     = { {kCutoff,45}, {kResonance,95}, {kEnvMod,50}, {kDecay,10}, {kAccent,90}, END };
// ---------------------------------------------------------------- Bass
static const PV roundBass[] = { {kCutoff,20}, {kResonance,25}, {kEnvMod,30}, {kDecay,55}, {kAccent,40}, END };
static const PV squareSub[] = { {kWave,1}, {kCutoff,12}, {kResonance,15}, {kEnvMod,20}, {kDecay,70}, {kAccent,30}, {kTune,-12}, END };
static const PV pluckBass[] = { {kCutoff,15}, {kResonance,45}, {kEnvMod,65}, {kDecay,15}, {kAccent,50}, END };
static const PV deepBass[]  = { {kCutoff,8}, {kResonance,35}, {kEnvMod,45}, {kDecay,60}, {kAccent,35}, {kTune,-12}, END };
static const PV funkBass[]  = { {kWave,1}, {kCutoff,26}, {kResonance,55}, {kEnvMod,55}, {kDecay,22}, {kAccent,65}, END };
// ---------------------------------------------------------------- Leads
static const PV brightLead[]= { {kCutoff,62}, {kResonance,60}, {kEnvMod,45}, {kDecay,55}, {kAccent,60}, {kTune,12}, END };
static const PV squareLead[]= { {kWave,1}, {kCutoff,58}, {kResonance,50}, {kEnvMod,35}, {kDecay,70}, {kTune,12}, END };
static const PV reedLead[]  = { {kWave,1}, {kCutoff,42}, {kResonance,70}, {kEnvMod,25}, {kDecay,90}, {kAccent,40}, {kTune,12}, END };
static const PV highAcid[]  = { {kCutoff,36}, {kResonance,90}, {kEnvMod,68}, {kDecay,30}, {kAccent,80}, {kTune,12}, END };
// ---------------------------------------------------------------- Squelch & FX
static const PV zap[]       = { {kCutoff,10}, {kResonance,98}, {kEnvMod,100}, {kDecay,5}, {kAccent,100}, END };
static const PV wobble[]    = { {kCutoff,25}, {kResonance,97}, {kEnvMod,85}, {kDecay,100}, {kAccent,95}, END };
static const PV closed[]    = { {kCutoff,4}, {kResonance,90}, {kEnvMod,90}, {kDecay,50}, {kAccent,100}, END };
static const PV openWide[]  = { {kCutoff,80}, {kResonance,40}, {kEnvMod,20}, {kDecay,60}, {kAccent,30}, END };
static const PV detuned[]   = { {kCutoff,30}, {kResonance,82}, {kEnvMod,60}, {kDecay,40}, {kAccent,70}, {kTune,-0.4f}, END };
} // namespace pv
#undef END

static const PresetDef kPresetDefs[] = {
    { "Init", "Init", pv::init },
    { "Classic Acid", "Acid", pv::classic }, { "Squelchy", "Acid", pv::squelchy }, { "Rubber", "Acid", pv::rubber }, { "Hard Acid", "Acid", pv::hardAcid },
    { "Square Acid", "Acid", pv::squareAcid }, { "Slow Sweep", "Acid", pv::slowAcid }, { "Chirp", "Acid", pv::chirp },
    { "Round Bass", "Bass", pv::roundBass }, { "Square Sub", "Bass", pv::squareSub }, { "Pluck Bass", "Bass", pv::pluckBass },
    { "Deep Bass", "Bass", pv::deepBass }, { "Funk Bass", "Bass", pv::funkBass },
    { "Bright Lead", "Leads", pv::brightLead }, { "Square Lead", "Leads", pv::squareLead }, { "Reed Lead", "Leads", pv::reedLead }, { "High Acid", "Leads", pv::highAcid },
    { "Zap", "Squelch & FX", pv::zap }, { "Wobble Tail", "Squelch & FX", pv::wobble }, { "Closed Squelch", "Squelch & FX", pv::closed },
    { "Open Wide", "Squelch & FX", pv::openWide }, { "Slightly Off", "Squelch & FX", pv::detuned },
};
static constexpr uint32_t kNumPresets = sizeof(kPresetDefs) / sizeof(kPresetDefs[0]);

inline const Preset* presets() {
    static Preset table[kNumPresets];
    static bool done = false;
    if (done) return table;
    done = true;
    Engine* e = new Engine();
    for (uint32_t i = 0; i < kNumPresets; ++i) {
        table[i].name = kPresetDefs[i].name; table[i].category = kPresetDefs[i].category;
        std::copy(e->p, e->p + kParamCount, table[i].v);
        for (const PV* x = kPresetDefs[i].pv; x->id != kParamCount; ++x) table[i].v[x->id] = x->v;
    }
    delete e;
    return table;
}

} // namespace malachite
