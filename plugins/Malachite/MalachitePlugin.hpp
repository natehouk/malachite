// Malachite — DPF plugin wrapper: parameters, programs, patterns (saved as state), host transport, MIDI notes and
// MIDI Learn, and direct access for the UI (scope and sequencer position). License: MIT.
#pragma once
#include "DistrhoPlugin.hpp"
#include "ParamDefs.hpp"
#include "Presets.hpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>

START_NAMESPACE_DISTRHO

using namespace malachite;

class MalachitePlugin : public Plugin {
public:
    MalachitePlugin() : Plugin(kParamCount, kNumPresets, 3) {
        for (int& m : ccMap) m = -1;
        engine.setSampleRate((float)getSampleRate());
        setLatency(Engine::latency());
        patternText = serializePatterns(engine.pats);
    }

protected:
    const char* getLabel() const override { return "Malachite"; }
    const char* getDescription() const override {
        return "Acid bass synthesizer: saw or square oscillator, resonant 4-pole filter with envelope, accent and slide, "
               "and a 16-step pattern sequencer with 64 patterns that follows the host tempo.";
    }
    const char* getMaker() const override { return "Epsilon Audio"; }
    const char* getHomePage() const override { return "https://epsilonrecords.com"; }
    const char* getLicense() const override { return "MIT"; }
    uint32_t getVersion() const override { return d_version(1, 0, 0); }

    void initAudioPort(bool input, uint32_t index, AudioPort& port) override {
        port.groupId = kPortGroupStereo;
        Plugin::initAudioPort(input, index, port);
    }

    void initParameter(uint32_t i, Parameter& prm) override {
        const ParamDef& d = paramDef(i);
        prm.hints = (d.hints & kHintOutput) ? kParameterIsOutput
                  : (((d.hints & kHintNoAuto) ? 0 : kParameterIsAutomatable)
                     | ((d.hints & kHintInteger) ? kParameterIsInteger : 0)
                     | ((d.hints & kHintBoolean) ? kParameterIsBoolean : 0)
                     | ((d.hints & kHintLog) ? kParameterIsLogarithmic : 0));
        prm.name = d.name; prm.symbol = d.sym; prm.unit = d.unit;
        prm.ranges.min = d.min; prm.ranges.max = d.max;
        Engine* def = new Engine(); prm.ranges.def = std::min(d.max, std::max(d.min, def->p[i])); delete def;
        int n = 0;
        if (const char* const* labels = enumLabels(i, n)) {
            ParameterEnumerationValue* ev = new ParameterEnumerationValue[n];
            for (int k = 0; k < n; ++k) { ev[k].value = (float)k; ev[k].label = labels[k]; }
            prm.enumValues.count = (uint8_t)n; prm.enumValues.restrictedMode = true; prm.enumValues.values = ev;
        }
    }

    // state: "patterns" and "midimap" are saved with the session; "cmd" carries one-shot UI requests
    void initState(uint32_t index, State& state) override {
        if (index == 0)      { state.key = "patterns"; state.defaultValue = String(serializePatterns(engine.pats).c_str()); state.label = "Patterns"; state.hints = kStateIsHostReadable; }
        else if (index == 1) { state.key = "midimap";  state.defaultValue = ""; state.label = "MIDI Learn map"; state.hints = kStateIsHostReadable; }
        else                 { state.key = "cmd";      state.defaultValue = ""; state.label = "UI command";     state.hints = kStateIsOnlyForDSP; }
    }
    String getState(const char* key) const override {
        if (std::strcmp(key, "patterns") == 0) return String(patternText.c_str());
        if (std::strcmp(key, "midimap") == 0) return String(serializeMap().c_str());
        return String();
    }
    void setState(const char* key, const char* value) override {
        if (std::strcmp(key, "patterns") == 0) {
            if (!value || !*value) return;
            Pattern* tmp = new Pattern[kNumPatterns];
            const bool ok = parsePatterns(value, tmp);
            delete[] tmp;
            if (!ok) return;
            patternText = value;
            // hand over to the audio thread; wait briefly if it hasn't picked up the previous edit yet
            for (int i = 0; i < 100 && !engine.queuePatterns(value); ++i) std::this_thread::sleep_for(std::chrono::milliseconds(1));
            return;
        }
        if (std::strcmp(key, "midimap") == 0) { parseMap(value); return; }
        if (std::strcmp(key, "cmd") == 0 && value && *value) {
            if (std::strncmp(value, "learn:", 6) == 0)      learnTarget = std::atoi(value + 6);
            else if (std::strcmp(value, "cancel") == 0)     learnTarget = -1;
            else if (std::strncmp(value, "unmap:", 6) == 0) { const int p = std::atoi(value + 6); for (int& m : ccMap) if (m == p) m = -1; }
            else if (std::strcmp(value, "clear") == 0)      { for (int& m : ccMap) m = -1; }
        }
    }

    void initProgramName(uint32_t i, String& name) override { name = presets()[i].name; }
    void loadProgram(uint32_t i) override {      // sound only: the sequencer settings and patterns stay
        for (uint32_t k = 0; k < kParamCount; ++k) if (!(paramDef(k).hints & (kHintOutput | kHintNoAuto | kHintNoPreset))) engine.p[k] = presets()[i].v[k];
    }

    float getParameterValue(uint32_t i) const override {
        if (i == kOutLearn) return learnEcho;
        if (i == kOutLevel) return clampf(20.f * std::log10(std::max(engine.level(), 1e-6f)), -120.f, 24.f);
        return engine.p[i];
    }
    void setParameterValue(uint32_t i, float v) override { if (!(paramDef(i).hints & kHintOutput)) engine.p[i] = v; }

    void activate() override { engine.setSampleRate((float)getSampleRate()); setLatency(Engine::latency()); }
    void sampleRateChanged(double sr) override { engine.setSampleRate((float)sr); }

    void run(const float**, float** outputs, uint32_t frames, const MidiEvent* ev, uint32_t evCount) override {
        const TimePosition& tp = getTimePosition();
        double ppq = 0;
        if (tp.bbt.valid) ppq = (tp.bbt.bar - 1) * (double)tp.bbt.beatsPerBar + (tp.bbt.beat - 1) + tp.bbt.tick / std::max(1.0, tp.bbt.ticksPerBeat);
        engine.setTransport(tp.bbt.valid, tp.playing, ppq, tp.bbt.valid ? tp.bbt.beatsPerMinute : 120.0);
        const bool midiOn = engine.p[kMidiOn] > 0.5f;
        uint32_t pos = 0;
        for (uint32_t e = 0; e <= evCount; ++e) {
            const uint32_t until = e < evCount ? std::min(ev[e].frame, frames) : frames;
            if (until > pos) {
                engine.process(outputs[0] + pos, outputs[1] + pos, until - pos);
                pos = until;
            }
            if (e == evCount) break;
            if (!midiOn || ev[e].size > 3) continue;
            const uint8_t* d = ev[e].data;
            const uint8_t st = d[0] & 0xF0;
            if (st == 0x90 && d[2] > 0) engine.midiNoteOn(d[1] & 0x7F, d[2] & 0x7F);
            else if (st == 0x80 || (st == 0x90 && d[2] == 0)) engine.midiNoteOff(d[1] & 0x7F);
            else if (st == 0xB0) {
                const int cc = d[1] & 0x7F, val = d[2] & 0x7F;
                if (cc == 120 || cc == 123) { engine.allNotesOff(); continue; }
                if (learnTarget >= 0 && learnTarget < (int)kParamCount) {          // MIDI learn: bind this controller
                    for (int& m : ccMap) if (m == learnTarget) m = -1;
                    ccMap[cc] = learnTarget;
                    learnSerial = (learnSerial + 1) % 8;
                    learnEcho = (float)(learnSerial * 10000 + learnTarget * 128 + cc + 1);
                    learnTarget = -1;
                }
                const int prm = ccMap[cc];
                if (prm >= 0) {
                    const float v = fromNorm((uint32_t)prm, val / 127.f);
                    engine.p[prm] = v;
                    requestParameterValueChange((uint32_t)prm, v);
                }
            }
        }
    }

private:
    std::string serializeMap() const {
        std::string s; char b[24];
        for (int cc = 0; cc < 128; ++cc) if (ccMap[cc] >= 0) { std::snprintf(b, sizeof b, "%d:%d;", cc, ccMap[cc]); s += b; }
        return s;
    }
    void parseMap(const char* v) {
        for (int& m : ccMap) m = -1;
        if (!v) return;
        const char* q = v;
        while (*q) {
            int cc = -1, prm = -1;
            if (std::sscanf(q, "%d:%d", &cc, &prm) == 2 && cc >= 0 && cc < 128 && prm >= 0 && prm < (int)kParamCount) ccMap[cc] = prm;
            const char* semi = std::strchr(q, ';');
            if (!semi) break;
            q = semi + 1;
        }
    }

public:
    Engine engine;                 // public: the UI reads the scope and the sequencer position directly
private:
    std::string patternText;
    int ccMap[128];
    int learnTarget = -1, learnSerial = 0;
    float learnEcho = 0;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MalachitePlugin)
};

END_NAMESPACE_DISTRHO
