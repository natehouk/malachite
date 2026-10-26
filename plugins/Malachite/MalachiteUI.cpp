// Malachite — custom vector UI (DPF/NanoVG). Original design. License: MIT.
#include "DistrhoUI.hpp"
#include "MalachitePlugin.hpp"
#include "UserPresets.hpp"
#include "ResizeHandle.hpp"
#include "Help.hpp"
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>
#include <vector>

#if defined(_WIN32)
extern "C" __declspec(dllimport) void* __stdcall ShellExecuteA(void*, const char*, const char*, const char*, const char*, int);
#endif

START_NAMESPACE_DISTRHO

static void openURL(const char* url) {
#if defined(_WIN32)
    ShellExecuteA(nullptr, "open", url, nullptr, nullptr, 1);
#elif defined(__APPLE__)
    const std::string c = std::string("open '") + url + "' &"; if (std::system(c.c_str())) {}
#else
    const std::string c = std::string("xdg-open '") + url + "' >/dev/null 2>&1 &"; if (std::system(c.c_str())) {}
#endif
}
static bool gTipsOn = true;
static double nowSec() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }

using namespace malachite;
using DGL_NAMESPACE::ResizeHandle;

static constexpr float W = 1060.f, H = 720.f;

#define C_BG        Color(0x0c, 0x12, 0x10)
#define C_PANEL     Color(0x13, 0x1c, 0x18)
#define C_PANEL_HI  Color(0x1e, 0x2a, 0x24)
#define C_LINE      Color(0x2a, 0x3a, 0x32)
#define C_TEXT      Color(0xe6, 0xee, 0xe8)
#define C_DIM       Color(0x8c, 0x9c, 0x93)
#define C_FAINT     Color(0x4b, 0x5a, 0x52)
#define C_ACC       Color(0x3c, 0xd0, 0x8c)       // malachite green
#define C_DEEP      Color(0x0e, 0x5c, 0x3c)
#define C_COPPER    Color(0xe0, 0x93, 0x52)       // accents
#define C_SLIDE     Color(0x84, 0xe4, 0xd6)
#define C_DISPLAY   Color(0x08, 0x0d, 0x0b)
#define C_FAV       Color(0xff, 0xc8, 0x5a)       // favourite star

static Color withAlpha(const Color& c, float a) { Color r(c); r.alpha = a; return r; }

enum WType { Knob, Segment };
struct Ctl { WType type; uint32_t param; float x, y, r; const char* label; bool sideLabel; };   // Segment: r = width

// ---------------------------------------------------------------- layout
static constexpr float SX = 16, SY = 64, SW = 1028, SH = 206;          // synth panel
static constexpr float QX = 16, QY = 280, QW = 1028, QH = 424;         // sequencer panel
static constexpr float SCX = SX + 852, SCY = SY + 16, SCW = 160, SCH = 174;   // scope
static constexpr float SEG_H = 24;
static constexpr float GL = QX + 64, GR = QX + QW - 12, COLW = (GR - GL) / 16.f;   // step grid
static constexpr float ROLL_Y = QY + 114, ROW_H = 13.f, ROLL_H = 13 * ROW_H;
static constexpr float OCT_Y = ROLL_Y + ROLL_H + 8, ACC_Y = OCT_Y + 24, SLD_Y = ACC_Y + 24, GATE_Y = SLD_Y + 24, LANE_H = 19, NUM_Y = GATE_Y + 34;
static constexpr float TOOL_Y = QY + 72, TOOL_H = 24;

static std::vector<Ctl> buildControls() {
    std::vector<Ctl> c;
    c.push_back({ Segment, kWave, SX + 86, SY + 58, 132, "", false });
    static const uint32_t kn[7] = { kTune, kCutoff, kResonance, kEnvMod, kDecay, kAccent, kVolume };
    static const char* const lb[7] = { "TUNING", "CUTOFF", "RESONANCE", "ENV MOD", "DECAY", "ACCENT", "VOLUME" };
    for (int i = 0; i < 7; ++i) c.push_back({ Knob, kn[i], SX + 222 + i * 92.f, SY + 96, i == 1 || i == 2 ? 36.f : 31.f, lb[i], false });
    c.push_back({ Segment, kSeqMode, QX + 126, QY + 36, 210, "", false });
    c.push_back({ Knob, kTempo, QX + 268, QY + 36, 15, "FREE TEMPO", true });
    c.push_back({ Segment, kGroup, QX + 470, QY + 36, 156, "", false });
    c.push_back({ Segment, kPattern, QX + 708, QY + 36, 272, "", false });
    c.push_back({ Segment, kSection, QX + 940, QY + 36, 138, "", false });
    return c;
}
static const std::vector<Ctl>& controls() { static std::vector<Ctl> c = buildControls(); return c; }
static const char* const* segLabels(uint32_t p, int& n) {
    static const char* const wave[] = { "SAW", "SQUARE" };
    static const char* const seq[] = { "OFF", "HOST", "FREE" };
    static const char* const grp[] = { "I", "II", "III", "IV" };
    static const char* const pat[] = { "1", "2", "3", "4", "5", "6", "7", "8" };
    static const char* const sec[] = { "A", "B", "A+B" };
    switch (p) {
    case kWave: n = 2; return wave;
    case kSeqMode: n = 3; return seq;
    case kGroup: n = 4; return grp;
    case kPattern: n = 8; return pat;
    case kSection: n = 3; return sec;
    default: n = 0; return nullptr;
    }
}

// tool buttons in the sequencer panel
enum Tool { TEditA, TEditB, TLenDown, TLen, TLenUp, TClear, TRandom, TShiftL, TShiftR, TTrDown, TTrUp, TCopy, TPaste, kNumTools };
struct ToolRect { float x, w; const char* label; };
static const ToolRect kTools[kNumTools] = {
    { QX + 58, 28, "A" }, { QX + 88, 28, "B" },
    { QX + 192, 24, "-" }, { QX + 218, 34, "" }, { QX + 254, 24, "+" },
    { QX + 298, 62, "CLEAR" }, { QX + 366, 72, "RANDOM" },
    { QX + 482, 28, "<" }, { QX + 512, 28, ">" },
    { QX + 628, 28, "-" }, { QX + 658, 28, "+" },
    { QX + 712, 58, "COPY" }, { QX + 776, 62, "PASTE" },
};

static constexpr float PS_X = 318.f, PS_Y = 20.f, PS_W = 220.f, PS_H = 28.f, PS_ARROW = 28.f;
static constexpr float A_X = 548.f, AB_W = 30.f, COPY_X = 612.f, COPY_W = 46.f;
static constexpr float UNDO_X = 668.f, REDO_X = 720.f, UR_W = 48.f, LEARN_X = 778.f, LEARN_W = 66.f, MIDI_X = 848.f, MIDI_W = 48.f, HELP_X = 902.f, HELP_W = 48.f;
static constexpr float PM_COLW = 160.f, PM_ROW = 23.f, PM_HEAD = 22.f, PM_PAD = 8.f;

class MalachiteUI : public UI {
public:
    MalachiteUI() : UI((uint)W, (uint)H), resizeHandle(this), rng((unsigned)std::chrono::steady_clock::now().time_since_epoch().count()) {
        loadSharedResources();
        Engine* def = new Engine();
        for (uint32_t i = 0; i < kParamCount; ++i) { val[i] = def->p[i]; defv[i] = def->p[i]; }
        std::copy(def->pats, def->pats + kNumPatterns, pats);
        delete def;
        if (MalachitePlugin* const dsp = (MalachitePlugin*)getPluginInstancePointer()) std::copy(dsp->engine.pats, dsp->engine.pats + kNumPatterns, pats);
        patText = serializePatterns(pats);
        val[kOutLevel] = -120;
        for (int& m : paramCC) m = -1;
        const double sf = getScaleFactor();
        if (sf != 1.0) setSize((uint)(W * sf), (uint)(H * sf));
        setGeometryConstraints((uint)(W * 0.6 * sf), (uint)(H * 0.6 * sf), true);
        if (isResizable()) resizeHandle.hide();
        userPresets = listUserPresets();
        before = snapshot();
    }

protected:
    float uiScale() const { return (float)getWidth() / W; }
    static bool isOut(uint32_t p) { return (paramDef(p).hints & kHintOutput) != 0; }

    // ---- values -------------------------------------------------------------
    void setValue(uint32_t p, float v) {
        const ParamDef& d = paramDef(p);
        v = clampf(v, d.min, d.max);
        if (d.hints & kHintInteger) v = std::round(v);
        if (v == val[p]) return;
        val[p] = v; setParameterValue(p, v); repaint();
    }
    void commit(uint32_t p, float v) { editParameter(p, true); setValue(p, v); editParameter(p, false); }
    void parameterChanged(uint32_t i, float v) override {
        if (i >= kParamCount) return;
        if (i == kOutLearn) {
            if (v != lastLearnEcho && v > 0) {
                const int code = (int)v % 10000 - 1, prm = code / 128, cc = code % 128;
                for (int& m : paramCC) if (m == cc) m = -1;
                if (prm >= 0 && prm < (int)kParamCount) paramCC[prm] = cc;
                learnWaiting = -1;
            }
            lastLearnEcho = v; repaint(); return;
        }
        if (isOut(i)) { val[i] = v; return; }
        if (val[i] != v) { val[i] = v; if (!gesture) before.v[i] = v; repaint(); }
    }
    void stateChanged(const char* key, const char* value) override {
        if (std::strcmp(key, "patterns") == 0) {
            if (value && *value && patText != value && parsePatterns(value, pats)) { patText = value; if (!gesture) before.pat = patText; repaint(); }
            return;
        }
        if (std::strcmp(key, "midimap") != 0) return;
        for (int& m : paramCC) m = -1;
        const char* q = value;
        while (q && *q) {
            int cc = -1, prm = -1;
            if (std::sscanf(q, "%d:%d", &cc, &prm) == 2 && prm >= 0 && prm < (int)kParamCount) paramCC[prm] = cc;
            const char* s = std::strchr(q, ';'); if (!s) break; q = s + 1;
        }
        repaint();
    }

    // ---- patterns -----------------------------------------------------------
    int editHalf() const { const int s = (int)(val[kSection] + 0.5f); return s == kSecAB ? editSec : s; }
    int editIndex() const { return patternIndex((int)(val[kGroup] + .5f), (int)(val[kPattern] + .5f), editHalf()); }
    Pattern& editPat() { return pats[editIndex()]; }
    const Pattern& editPat() const { return pats[editIndex()]; }
    void pushPatterns() { patText = serializePatterns(pats); setState("patterns", patText.c_str()); repaint(); }

    // ---- undo / redo / A-B: the parameters and the patterns ------------------------
    struct Snap { std::vector<float> v; std::string pat; int ab = 0; bool empty() const { return v.empty(); } };
    Snap snapshot() const { Snap s; s.v.assign(val, val + kParamCount); s.pat = patText; s.ab = abSlot; return s; }
    bool differs(const Snap& a, const Snap& b) const {
        if (a.pat != b.pat) return true;
        for (uint32_t i = 0; i < kParamCount; ++i) if (!isOut(i) && a.v[i] != b.v[i]) return true;
        return false;
    }
    void beginGesture() { gesture = true; before = snapshot(); }
    void endGesture() {
        gesture = false;
        const Snap now = snapshot();
        if (differs(before, now)) { undoStack.push_back(before); if (undoStack.size() > 200) undoStack.erase(undoStack.begin()); redoStack.clear(); }
        before = now; repaint();
    }
    void applySnap(const Snap& s) {
        for (uint32_t i = 0; i < kParamCount; ++i) if (!isOut(i) && val[i] != s.v[i]) commit(i, s.v[i]);
        if (s.pat != patText && parsePatterns(s.pat.c_str(), pats)) pushPatterns();
    }
    void restoreSnap(const Snap& s) {
        if (s.ab != abSlot) { abStore[abSlot] = snapshot(); abSlot = s.ab; }
        applySnap(s); before = snapshot();
    }
    void undo() { if (undoStack.empty()) return; redoStack.push_back(snapshot()); restoreSnap(undoStack.back()); undoStack.pop_back(); flash("Undo"); }
    void redo() { if (redoStack.empty()) return; undoStack.push_back(snapshot()); restoreSnap(redoStack.back()); redoStack.pop_back(); flash("Redo"); }
    void switchAB(int to) {
        if (to == abSlot) return;
        abStore[abSlot] = snapshot(); if (abStore[to].empty()) { abStore[to] = snapshot(); abStore[to].ab = to; }
        beginGesture(); applySnap(abStore[to]); endGesture();
        abSlot = to; flash(to == 0 ? "Setting A" : "Setting B");
    }
    void copyAB() { abStore[1 - abSlot] = snapshot(); abStore[1 - abSlot].ab = 1 - abSlot; flash(abSlot == 0 ? "Copied A to B" : "Copied B to A"); }

    // ---- formatting ---------------------------------------------------------
    void fmt(uint32_t p, char* b, size_t n) const {
        const float v = val[p];
        int ne = 0;
        if (const char* const* l = enumLabels(p, ne)) { snprintf(b, n, "%s", l[std::max(0, std::min(ne - 1, (int)std::round(v)))]); return; }
        if (paramDef(p).hints & kHintBoolean) { snprintf(b, n, "%s", v > 0.5f ? "On" : "Off"); return; }
        switch (p) {
        case kTune: if (std::fabs(v) < 0.005f) snprintf(b, n, "0 st"); else snprintf(b, n, "%+.2f st", v); return;
        case kTempo: snprintf(b, n, "%.1f BPM", v); return;
        default: snprintf(b, n, "%.0f%%", v);
        }
    }
    bool parse(uint32_t, const std::string& in, float& out) const {
        std::string s; for (char ch : in) if (!std::isspace((unsigned char)ch)) s += (char)std::tolower((unsigned char)ch);
        if (s.empty()) return false;
        char* end = nullptr; const double x = std::strtod(s.c_str(), &end);
        if (end == s.c_str()) return false;
        out = (float)x; return true;
    }
    static const char* noteName(int n) { static const char* const t[13] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B", "C" }; return t[n < 0 ? 0 : (n > 12 ? 12 : n)]; }

    // ---- drawing ------------------------------------------------------------
    void onNanoDisplay() override {
        const float s = uiScale();
        save(); scale(s, s);
        beginPath(); rect(0, 0, W, H); fillColor(C_BG); fill();
        fontFace(NANOVG_DEJAVU_SANS_TTF);
        drawHeader();
        drawSynthPanel();
        drawSeqPanel();
        const std::vector<Ctl>& cs = controls();
        for (int i = 0; i < (int)cs.size(); ++i) drawCtl(cs[i], i == active || i == hover);
        if (editing >= 0) drawEditBox();
        drawStatus();
        if (menuOpen) drawPresetMenu();
        if (saving) drawSaveBox();
        if (helpOpen) drawHelp();
        else if (tipShown && gTipsOn) drawTooltip();
        restore();
    }
    void panel(float x, float y, float w, float h) { beginPath(); roundedRect(x, y, w, h, 6); fillColor(C_PANEL); fill(); strokeColor(C_LINE); strokeWidth(1); stroke(); }
    void caption(float cx, float y, const char* t, int align = ALIGN_CENTER) { fontSize(7.5f); textLetterSpacing(1.8f); textAlign(align | ALIGN_MIDDLE); fillColor(C_FAINT); text(cx, y, t, nullptr); }
    void drawHeaderButton(float x, float w, const char* label, bool on, bool hot, bool disabled) {
        beginPath(); roundedRect(x, PS_Y, w, PS_H, 5); fillColor(on ? withAlpha(C_ACC, 0.2f) : C_PANEL); fill();
        strokeColor(on ? C_ACC : (hot && !disabled ? C_DIM : C_LINE)); strokeWidth(1); stroke();
        fontSize(9); textLetterSpacing(1.6f); textAlign(ALIGN_CENTER | ALIGN_MIDDLE); fillColor(disabled ? C_FAINT : (on ? C_TEXT : C_DIM));
        text(x + w / 2, PS_Y + PS_H / 2 + 0.5f, label, nullptr);
    }
    void drawHeader() {
        // logo: a polished malachite pebble, banded
        const float cx = 34, cy = 34;
        beginPath(); ellipse(cx, cy, 17, 14); fillColor(C_DEEP); fill();
        for (int k = 0; k < 4; ++k) { beginPath(); ellipse(cx + 3, cy + 2, 13.5f - k * 3.2f, 10.5f - k * 2.6f); strokeColor(withAlpha(C_ACC, 0.55f + 0.1f * k)); strokeWidth(1.4f); stroke(); }
        fontSize(21); textLetterSpacing(4.5f); fillColor(C_TEXT); textAlign(ALIGN_LEFT | ALIGN_MIDDLE); text(60, 34, "MALACHITE", nullptr);
        fontSize(9.5f); textLetterSpacing(2.5f); fillColor(C_DIM); text(222, 29, "ACID BASS", nullptr);
        fontSize(7); textLetterSpacing(1.1f); fillColor(C_FAINT); text(222, 41, "EPSILON AUDIO 1.0", nullptr);
        drawPresetBar();
        for (int k = 0; k < 2; ++k) {
            const float x = A_X + k * AB_W; const bool on = abSlot == k, hot = hoverHeader == 6 + k;
            beginPath(); roundedRect(x, PS_Y, AB_W - 2, PS_H, 5); fillColor(on ? withAlpha(C_ACC, 0.25f) : C_PANEL); fill();
            strokeColor(on ? C_ACC : (hot ? C_DIM : C_LINE)); strokeWidth(1); stroke();
            fontSize(10); textLetterSpacing(0); textAlign(ALIGN_CENTER | ALIGN_MIDDLE); fillColor(on ? C_TEXT : C_DIM);
            text(x + (AB_W - 2) / 2, PS_Y + PS_H / 2 + 0.5f, k == 0 ? "A" : "B", nullptr);
        }
        drawHeaderButton(COPY_X, COPY_W, "COPY", false, hoverHeader == 8, false);
        drawHeaderButton(UNDO_X, UR_W, "UNDO", false, hoverHeader == 3, undoStack.empty());
        drawHeaderButton(REDO_X, UR_W, "REDO", false, hoverHeader == 4, redoStack.empty());
        drawHeaderButton(LEARN_X, LEARN_W, learnMode ? "LEARNING" : "LEARN", learnMode, hoverHeader == 1, false);
        drawHeaderButton(MIDI_X, MIDI_W, "MIDI", val[kMidiOn] > 0.5f, hoverHeader == 2, false);
        drawHeaderButton(HELP_X, HELP_W, "HELP", helpOpen, hoverHeader == 5, false);
        // output level
        const float x0 = 966, w = 78, lv = clampf((val[kOutLevel] + 48.f) / 48.f, 0.f, 1.f);
        caption(x0, 28, "LEVEL", ALIGN_LEFT);
        beginPath(); rect(x0, 36, w, 5); fillColor(C_PANEL_HI); fill();
        if (lv > 0) { beginPath(); rect(x0, 36, w * lv, 5); fillColor(val[kOutLevel] > -0.1f ? Color(0xe0, 0x50, 0x48) : C_ACC); fill(); }
    }
    void drawSynthPanel() {
        panel(SX, SY, SW, SH);
        // malachite banding across the panel
        save(); scissor(SX + 1, SY + 1, SW - 2, SH - 2);
        for (int k = 0; k < 14; ++k) {
            beginPath(); ellipse(SX + 520, SY + 420, 260.f + k * 34.f + (k % 3) * 6.f, 230.f + k * 26.f + (k % 2) * 8.f);
            strokeColor(withAlpha(k % 2 ? C_ACC : C_DEEP, k % 2 ? 0.05f : 0.22f)); strokeWidth(k % 3 == 0 ? 9.f : 5.f); stroke();
        }
        restore();
        caption(SX + 86, SY + 32, "WAVEFORM");
        // LEDs
        const bool g = gateLit, a = accentLit;
        auto led = [&](float x, float y, bool on, const Color& c, const char* label) {
            if (on) { beginPath(); circle(x, y, 9); fillColor(withAlpha(c, 0.25f)); fill(); }
            beginPath(); circle(x, y, 5); fillColor(on ? c : C_PANEL_HI); fill(); strokeColor(C_LINE); strokeWidth(1); stroke();
            fontSize(8); textLetterSpacing(1.6f); textAlign(ALIGN_LEFT | ALIGN_MIDDLE); fillColor(on ? C_TEXT : C_DIM); text(x + 12, y + 0.5f, label, nullptr);
        };
        led(SX + 40, SY + 118, g, C_ACC, "GATE");
        led(SX + 40, SY + 144, a, C_COPPER, "ACCENT");
        // scope
        beginPath(); roundedRect(SCX, SCY, SCW, SCH, 6); fillColor(C_DISPLAY); fill(); strokeColor(C_LINE); strokeWidth(1); stroke();
        beginPath(); moveTo(SCX + 6, SCY + SCH / 2); lineTo(SCX + SCW - 6, SCY + SCH / 2); strokeColor(Color(255, 255, 255, 14 / 255.f)); stroke();
        if (!scopeBuf.empty()) {
            scissor(SCX + 1, SCY + 1, SCW - 2, SCH - 2);
            beginPath();
            const int nShow = (int)scopeBuf.size();
            for (int i = 0; i < nShow; ++i) {
                const float x = SCX + 6 + (SCW - 12) * i / (nShow - 1.f), y = SCY + SCH / 2 - clampf(scopeBuf[i] * 1.2f, -1.f, 1.f) * (SCH / 2 - 10);
                if (!i) moveTo(x, y); else lineTo(x, y);
            }
            strokeColor(C_ACC); strokeWidth(1.4f); stroke();
            resetScissor();
        }
        caption(SCX + 8, SCY + 10, "SCOPE", ALIGN_LEFT);
    }
    void drawSeqPanel() {
        panel(QX, QY, QW, QH);
        caption(QX + 126, QY + 14, "SEQUENCER"); caption(QX + 470, QY + 14, "GROUP"); caption(QX + 708, QY + 14, "PATTERN"); caption(QX + 940, QY + 14, "PLAY");
        beginPath(); moveTo(QX + 12, QY + 58); lineTo(QX + QW - 12, QY + 58); strokeColor(C_LINE); strokeWidth(1); stroke();
        // tools
        const Pattern& pt = editPat();
        caption(QX + 14, TOOL_Y + TOOL_H / 2, "EDIT", ALIGN_LEFT); caption(QX + 136, TOOL_Y + TOOL_H / 2, "LENGTH", ALIGN_LEFT);
        caption(QX + 444, TOOL_Y + TOOL_H / 2, "SHIFT", ALIGN_LEFT); caption(QX + 556, TOOL_Y + TOOL_H / 2, "TRANSPOSE", ALIGN_LEFT);
        const bool secFixed = (int)(val[kSection] + .5f) != kSecAB;
        for (int t = 0; t < kNumTools; ++t) {
            const ToolRect& r = kTools[t]; const bool hot = hoverTool == t;
            bool on = false, dim = false;
            if (t == TEditA || t == TEditB) { on = editHalf() == (t == TEditA ? 0 : 1); dim = secFixed && !on; }
            if (t == TPaste) dim = !haveClip;
            if (t == TLen) {
                beginPath(); roundedRect(r.x, TOOL_Y, r.w, TOOL_H, 4); fillColor(C_DISPLAY); fill(); strokeColor(C_LINE); strokeWidth(1); stroke();
                char b[8]; snprintf(b, sizeof b, "%d", pt.len);
                fontSize(11); textLetterSpacing(0.3f); textAlign(ALIGN_CENTER | ALIGN_MIDDLE); fillColor(C_TEXT); text(r.x + r.w / 2, TOOL_Y + TOOL_H / 2 + 0.5f, b, nullptr);
                continue;
            }
            beginPath(); roundedRect(r.x, TOOL_Y, r.w, TOOL_H, 4); fillColor(on ? withAlpha(C_ACC, 0.85f) : (hot && !dim ? C_PANEL_HI : C_PANEL)); fill();
            strokeColor(on ? C_ACC : (hot && !dim ? C_DIM : C_LINE)); strokeWidth(1); stroke();
            fontSize(t == TShiftL || t == TShiftR || t == TLenDown || t == TLenUp || t == TTrDown || t == TTrUp ? 12.f : 8.5f); textLetterSpacing(1.4f);
            textAlign(ALIGN_CENTER | ALIGN_MIDDLE); fillColor(on ? C_BG : (dim ? C_FAINT : C_DIM));
            text(r.x + r.w / 2, TOOL_Y + TOOL_H / 2 + 0.5f, r.label, nullptr);
        }
        { char b[64]; static const char* const g[] = { "I", "II", "III", "IV" };
          snprintf(b, sizeof b, "EDITING  %s-%d %s  ·  %d STEP%s", g[(int)(val[kGroup] + .5f)], (int)(val[kPattern] + .5f) + 1, editHalf() ? "B" : "A", pt.len, pt.len == 1 ? "" : "S");
          fontSize(8.5f); textLetterSpacing(1.6f); textAlign(ALIGN_RIGHT | ALIGN_MIDDLE); fillColor(C_DIM); text(QX + QW - 14, TOOL_Y + TOOL_H / 2, b, nullptr); }
        drawGrid();
    }
    float colX(int i) const { return GL + i * COLW; }
    void drawGrid() {
        const Pattern& pt = editPat();
        const bool playingHere = playPat == editIndex() && playStep >= 0;
        // roll background: black-key rows darker
        static const bool blackKey[13] = { false, true, false, true, false, false, true, false, true, false, true, false, false };
        for (int r = 0; r < 13; ++r) {
            const int note = 12 - r; const float y = ROLL_Y + r * ROW_H;
            beginPath(); rect(GL, y, GR - GL, ROW_H); fillColor(blackKey[note] ? C_DISPLAY : Color(0x10, 0x18, 0x14)); fill();
            fontSize(7.5f); textLetterSpacing(0.6f); textAlign(ALIGN_RIGHT | ALIGN_MIDDLE); fillColor(blackKey[note] ? C_FAINT : C_DIM);
            text(GL - 8, y + ROW_H / 2 + 0.5f, noteName(note), nullptr);
        }
        caption(GL - 8, OCT_Y + LANE_H / 2, "OCT", ALIGN_RIGHT); caption(GL - 8, ACC_Y + LANE_H / 2, "ACCENT", ALIGN_RIGHT);
        caption(GL - 8, SLD_Y + LANE_H / 2, "SLIDE", ALIGN_RIGHT); caption(GL - 8, GATE_Y + LANE_H / 2, "GATE", ALIGN_RIGHT);
        // beat lines
        beginPath(); for (int i = 0; i <= 16; ++i) { const float x = colX(i); moveTo(x, ROLL_Y); lineTo(x, GATE_Y + LANE_H); }
        strokeColor(Color(255, 255, 255, 10 / 255.f)); strokeWidth(1); stroke();
        beginPath(); for (int i = 0; i <= 16; i += 4) { const float x = colX(i); moveTo(x, ROLL_Y); lineTo(x, GATE_Y + LANE_H); }
        strokeColor(Color(255, 255, 255, 26 / 255.f)); stroke();
        // playhead
        if (playingHere) { beginPath(); rect(colX(playStep), ROLL_Y, COLW, GATE_Y + LANE_H - ROLL_Y); fillColor(withAlpha(C_ACC, 0.10f)); fill(); }
        // notes; a tie continues the note before it
        int lastNote = -1, lastOct = 0;
        auto rowY = [&](int note) { return ROLL_Y + (12 - note) * ROW_H; };
        float prevX = -1, prevY = -1; bool prevSlide = false;
        for (int i = 0; i < 16; ++i) {
            const Step& s = pt.s[i]; const bool in = i < pt.len;
            const float x = colX(i);
            float a = in ? 1.f : 0.25f;
            if (s.gate == kNote) { lastNote = s.note; lastOct = s.oct; }
            if (s.gate != kRest && lastNote >= 0) {
                const bool tie = s.gate == kTie;
                const int note = tie ? lastNote : s.note, oc = tie ? lastOct : s.oct;
                const float y = rowY(note);
                const Color c = s.accent && !tie ? C_COPPER : C_ACC;
                beginPath(); roundedRect(x + (tie ? 0.f : 3.f), y + 1.5f, COLW - (tie ? 3.f : 6.f), ROW_H - 3, 3);
                fillColor(withAlpha(c, a * (tie ? 0.55f : 0.95f))); fill();
                if (oc != 0) {
                    const float ax = x + COLW - 12, ay = y + ROW_H / 2;
                    beginPath(); if (oc > 0) { moveTo(ax - 3.5f, ay + 2.5f); lineTo(ax + 3.5f, ay + 2.5f); lineTo(ax, ay - 3); } else { moveTo(ax - 3.5f, ay - 2.5f); lineTo(ax + 3.5f, ay - 2.5f); lineTo(ax, ay + 3); }
                    closePath(); fillColor(withAlpha(C_BG, a)); fill();
                }
                if (prevSlide && prevX >= 0 && !tie) {
                    beginPath(); moveTo(prevX, prevY); bezierTo(prevX + COLW * 0.4f, prevY, x - COLW * 0.2f, y + ROW_H / 2, x + 3, y + ROW_H / 2);
                    strokeColor(withAlpha(C_SLIDE, a)); strokeWidth(2); stroke();
                }
                prevX = x + COLW - 3; prevY = y + ROW_H / 2;
            } else { prevX = -1; }
            prevSlide = s.slide && s.gate != kRest;
            // lanes
            auto lane = [&](float y, bool on, const Color& c, const char* label) {
                beginPath(); roundedRect(x + 3, y, COLW - 6, LANE_H, 4); fillColor(on ? withAlpha(c, a * 0.9f) : withAlpha(C_PANEL_HI, in ? 1.f : 0.4f)); fill();
                if (label && *label) { fontSize(8); textLetterSpacing(1.2f); textAlign(ALIGN_CENTER | ALIGN_MIDDLE); fillColor(on ? withAlpha(C_BG, a) : withAlpha(C_DIM, a)); text(x + COLW / 2, y + LANE_H / 2 + 0.5f, label, nullptr); }
            };
            lane(OCT_Y, s.oct != 0, C_DIM, s.oct > 0 ? "UP" : (s.oct < 0 ? "DOWN" : ""));
            lane(ACC_Y, s.accent, C_COPPER, s.accent ? "ACC" : "");
            lane(SLD_Y, s.slide, C_SLIDE, s.slide ? "SLIDE" : "");
            lane(GATE_Y, s.gate != kRest, s.gate == kTie ? C_DIM : C_ACC, s.gate == kNote ? "NOTE" : (s.gate == kTie ? "TIE" : "REST"));
            char b[4]; snprintf(b, sizeof b, "%d", i + 1);
            fontSize(8.5f); textLetterSpacing(0.4f); textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
            fillColor(playingHere && i == playStep ? C_ACC : (in ? C_DIM : C_FAINT)); text(x + COLW / 2, NUM_Y, b, nullptr);
        }
        // the length marker
        if (pt.len < 16) { beginPath(); moveTo(colX(pt.len), ROLL_Y - 4); lineTo(colX(pt.len), GATE_Y + LANE_H + 4); strokeColor(withAlpha(C_COPPER, 0.8f)); strokeWidth(1.5f); stroke(); }
        if (transposeShown != 0) { char b[32]; snprintf(b, sizeof b, "MIDI TRANSPOSE %+d", transposeShown); caption(GR, NUM_Y, b, ALIGN_RIGHT); }
    }

    void drawKnobBody(float x, float y, float r, float n, bool bipolar, bool hot) {
        const float a0 = 0.75f * kPi, a1 = 2.25f * kPi, ang = a0 + (a1 - a0) * n;
        const float tw = r > 28 ? 5.f : 3.5f;
        beginPath(); arc(x, y, r, a0, a1, NanoVG::CW); strokeColor(C_LINE); strokeWidth(tw); lineCap(ROUND); stroke();
        const float from = bipolar ? (a0 + a1) / 2 : a0;
        if (std::fabs(ang - from) > 0.001f) { beginPath(); arc(x, y, r, std::min(from, ang), std::max(from, ang), NanoVG::CW); strokeColor(C_ACC); stroke(); }
        const float cr = r - (r > 28 ? 9.f : 6.f);
        // copper rim, dark cap
        beginPath(); circle(x, y, cr); fillPaint(linearGradient(x, y - cr, x, y + cr, Color(0xe8, 0xa8, 0x6c), Color(0x7a, 0x44, 0x22))); fill();
        beginPath(); circle(x, y, cr - 2.5f);
        fillPaint(radialGradient(x - cr * 0.3f, y - cr * 0.4f, 0, cr * 1.3f, hot ? Color(0x2c, 0x3a, 0x33) : Color(0x1f, 0x2b, 0x25), Color(0x0b, 0x10, 0x0e))); fill();
        beginPath(); moveTo(x + std::cos(ang) * cr * 0.3f, y + std::sin(ang) * cr * 0.3f); lineTo(x + std::cos(ang) * (cr - 4), y + std::sin(ang) * (cr - 4));
        strokeColor(C_TEXT); strokeWidth(r > 28 ? 2.5f : 2.f); lineCap(ROUND); stroke();
    }
    void drawCtl(const Ctl& c, bool hot) {
        const uint32_t p = c.param; char buf[40];
        if (c.type == Knob) {
            drawKnobBody(c.x, c.y, c.r, toNorm(p, val[p]), p == kTune, hot);
            fmt(p, buf, sizeof buf);
            const bool dim = p == kTempo && (int)(val[kSeqMode] + .5f) != kSeqFree;
            if (c.sideLabel) {
                fontSize(7.5f); textLetterSpacing(1.6f); textAlign(ALIGN_LEFT | ALIGN_MIDDLE); fillColor(dim ? C_FAINT : C_DIM); text(c.x + c.r + 10, c.y - 6, c.label, nullptr);
                fontSize(10); textLetterSpacing(0.4f); fillColor(dim ? C_FAINT : (hot ? C_ACC : C_TEXT)); text(c.x + c.r + 10, c.y + 8, buf, nullptr);
            } else {
                fontSize(8.5f); textLetterSpacing(1.6f); textAlign(ALIGN_CENTER | ALIGN_TOP); fillColor(C_DIM); text(c.x, c.y + c.r + 7, c.label, nullptr);
                fontSize(11); textLetterSpacing(0.4f); fillColor(hot ? C_ACC : C_TEXT); text(c.x, c.y + c.r + 20, buf, nullptr);
            }
        } else {
            int n = 0; const char* const* names = segLabels(p, n);
            const int cur = (int)(val[p] + .5f); const float sw = c.r / n, x0 = c.x - c.r / 2;
            for (int i = 0; i < n; ++i) {
                const float x = x0 + sw * i;
                beginPath(); roundedRect(x + 1.5f, c.y - SEG_H / 2, sw - 3, SEG_H, 5); fillColor(i == cur ? C_ACC : C_PANEL_HI); fill();
                fontSize(9); textLetterSpacing(1.4f); textAlign(ALIGN_CENTER | ALIGN_MIDDLE); fillColor(i == cur ? C_BG : C_DIM); text(x + sw / 2, c.y + 0.5f, names[i], nullptr);
            }
        }
        if (learnMode) {
            const int cc = paramCC[p]; const bool waiting = learnWaiting == (int)p;
            if (cc >= 0 || waiting) {
                const float by = c.type == Knob ? c.y - c.r - 12 : c.y - SEG_H / 2 - 8;
                snprintf(buf, sizeof buf, waiting ? "?" : "CC%d", cc);
                fontSize(8.5f); textLetterSpacing(0.5f); textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
                const float bw = waiting ? 16 : 34;
                beginPath(); roundedRect(c.x - bw / 2, by - 7, bw, 14, 3); fillColor(waiting ? C_ACC : C_PANEL_HI); fill(); strokeColor(C_ACC); strokeWidth(1); stroke();
                fillColor(waiting ? C_BG : C_ACC); text(c.x, by + 0.5f, buf, nullptr);
            }
        }
    }
    void drawStatus() {
        std::string msg;
        if (statusFlash > 0) msg = statusText;
        else if (learnMode) msg = learnWaiting >= 0 ? std::string("Move a MIDI controller to assign: ") + paramDef(learnWaiting).name : "MIDI Learn: click a control, then move a controller.  Right-click a control to unassign.";
        if (!msg.empty()) { fontSize(10); textLetterSpacing(0.8f); textAlign(ALIGN_RIGHT | ALIGN_MIDDLE); fillColor(C_ACC); text(SCX - 14, SY + 186, msg.c_str(), nullptr); }
        if (learnMode && learnWaiting < 0) { fontSize(9); textAlign(ALIGN_LEFT | ALIGN_MIDDLE); fillColor(hoverClear ? C_TEXT : C_DIM); text(SX + 18, SY + 186, "CLEAR ALL ASSIGNMENTS", nullptr); }
        if (statusFlash > 0) { --statusFlash; repaint(); }
    }
    void flash(const std::string& s) { statusText = s; statusFlash = 60; repaint(); }
    void drawEditBox() {
        const Ctl& c = controls()[editing];
        const float w = 90, h = 22, x = c.x - w / 2, y = c.y + c.r + 6;
        beginPath(); roundedRect(x, y, w, h, 4); fillColor(C_DISPLAY); fill(); strokeColor(C_ACC); strokeWidth(1); stroke();
        fontSize(12); textLetterSpacing(0.5f); textAlign(ALIGN_CENTER | ALIGN_MIDDLE); fillColor(C_TEXT); text(c.x, y + h / 2 + 0.5f, (editBuf + "|").c_str(), nullptr);
    }

    // ---- presets (sound only) ----------------------------------------------------------
    static bool isSound(uint32_t i) { return !(paramDef(i).hints & (kHintOutput | kHintNoAuto | kHintNoPreset)) && i != kMidiOn; }
    bool matchesValues(const float* v) const {
        for (uint32_t i = 0; i < kParamCount; ++i) if (isSound(i) && std::fabs(val[i] - v[i]) > 1e-4f * std::max(1.f, std::fabs(v[i]))) return false;
        return true;
    }
    void applyValues(const float* v) { beginGesture(); for (uint32_t i = 0; i < kParamCount; ++i) if (isSound(i) && val[i] != v[i]) commit(i, v[i]); endGesture(); }
    void applyFactory(int idx) { idx = (idx % (int)kNumPresets + (int)kNumPresets) % (int)kNumPresets; applyValues(presets()[idx].v); curFactory = idx; curUser.clear(); repaint(); }
    void applyUser(const std::string& name) { float v[kParamCount]; std::copy(defv, defv + kParamCount, v); if (!loadUserPreset(name, v)) return; applyValues(v); std::copy(v, v + kParamCount, userVals); curUser = name; curFactory = -1; repaint(); }
    void programLoaded(uint32_t index) override { curFactory = (int)index; curUser.clear(); repaint(); }
    void step(int d) {
        const int nf = (int)kNumPresets, nu = (int)userPresets.size(), total = nf + nu;
        int cur = curFactory; if (!curUser.empty()) for (int i = 0; i < nu; ++i) if (userPresets[i] == curUser) cur = nf + i;
        if (cur < 0) cur = 0;
        const int next = ((cur + d) % total + total) % total;
        if (next < nf) applyFactory(next); else applyUser(userPresets[next - nf]);
    }
    void drawPresetBar() {
        const float x = PS_X, y = PS_Y, w = PS_W, h = PS_H, a = PS_ARROW;
        beginPath(); roundedRect(x, y, w, h, 5); fillColor(C_PANEL); fill(); strokeColor(menuOpen ? C_ACC : C_LINE); strokeWidth(1); stroke();
        beginPath(); moveTo(x + a, y + 5); lineTo(x + a, y + h - 5); moveTo(x + w - a, y + 5); lineTo(x + w - a, y + h - 5); strokeColor(C_LINE); stroke();
        for (int side = 0; side < 2; ++side) {
            const bool hot = presetHover == (side == 0 ? 0 : 2);
            const float cx = side == 0 ? x + a / 2 : x + w - a / 2, cy = y + h / 2, d = side == 0 ? -1.f : 1.f;
            beginPath(); moveTo(cx - 3 * d, cy - 5); lineTo(cx + 3 * d, cy); lineTo(cx - 3 * d, cy + 5); closePath(); fillColor(hot ? C_ACC : C_DIM); fill();
        }
        std::string name; bool modified = false;
        if (!curUser.empty()) { name = curUser; modified = !matchesValues(userVals); }
        else if (curFactory >= 0) { name = presets()[curFactory].name; modified = !matchesValues(presets()[curFactory].v); }
        else name = "Custom";
        if (modified) name += " *";
        fontSize(11.5f); textLetterSpacing(0.6f); textAlign(ALIGN_CENTER | ALIGN_MIDDLE); fillColor(presetHover == 1 || menuOpen ? C_TEXT : Color(0xd0, 0xdc, 0xd4));
        text(x + w / 2, y + h / 2 + 0.5f, name.c_str(), nullptr);
    }
    // preset browser: categories, user presets and favourites in columns; typing searches names and categories.
    // kind: 0 factory, 1 user, 2 save button, 3 search field, 4 favourite (idx into favs)
    struct MRect { float x, y, w, h; int kind; int idx; };
    static std::string lower(const std::string& s) { std::string r(s); for (char& c : r) c = (char)std::tolower((unsigned char)c); return r; }
    std::string favKey(int kind, int idx) const { return kind == 0 ? std::string("f:") + presets()[idx].name : (kind == 1 ? "u:" + userPresets[idx] : (idx < (int)favs.size() ? favs[idx] : std::string())); }
    bool isFav(const std::string& k) const { return std::find(favs.begin(), favs.end(), k) != favs.end(); }
    void toggleFav(const std::string& k) {
        if (k.empty()) return;
        auto it = std::find(favs.begin(), favs.end(), k); if (it != favs.end()) favs.erase(it); else favs.push_back(k);
        saveFavourites(favs); repaint();
    }
    static constexpr int kMenuCols = kNumCategories + 2, kMenuRows = 16;
    void menuLayout(float& mx, float& my, float& mw, float& mh, std::vector<MRect>& items) const {
        mw = PM_COLW * kMenuCols + 2 * PM_PAD; mx = std::min(W - mw - 8, std::max(8.f, PS_X + PS_W / 2 - mw / 2)); my = PS_Y + PS_H + 4;
        items.clear();
        const float top = my + PM_PAD + PM_ROW + 6 + PM_HEAD;
        int maxRows = 1;
        items.push_back({ mx + PM_PAD + PM_COLW, my + PM_PAD, 300, PM_ROW, 3, 0 });
        items.push_back({ mx + mw - PM_PAD - 150, my + PM_PAD, 150, PM_ROW, 2, 0 });
        if (!query.empty()) {
            const std::string q = lower(query); int k = 0;
            auto place = [&](int kind, int idx) { const int col = k / kMenuRows, row = k % kMenuRows; if (col >= kMenuCols) return; items.push_back({ mx + PM_PAD + col * PM_COLW, top + row * PM_ROW, PM_COLW - 4, PM_ROW, kind, idx }); maxRows = std::max(maxRows, row + 1); ++k; };
            for (uint32_t i = 0; i < kNumPresets; ++i) if (lower(presets()[i].name).find(q) != std::string::npos || lower(presets()[i].category).find(q) != std::string::npos) place(0, (int)i);
            for (int u = 0; u < (int)userPresets.size(); ++u) if (lower(userPresets[u]).find(q) != std::string::npos || std::string("user").find(q) != std::string::npos) place(1, u);
        } else {
            int rows[kNumCategories] = {};
            for (uint32_t i = 0; i < kNumPresets; ++i) {
                const char* cat = presets()[i].category;
                if (std::strcmp(cat, "Init") == 0) { items.push_back({ mx + PM_PAD, my + PM_PAD, PM_COLW - 4, PM_ROW, 0, (int)i }); continue; }
                for (int c = 0; c < kNumCategories; ++c)
                    if (std::strcmp(cat, kCategories[c]) == 0) { items.push_back({ mx + PM_PAD + c * PM_COLW, top + rows[c] * PM_ROW, PM_COLW - 4, PM_ROW, 0, (int)i }); maxRows = std::max(maxRows, ++rows[c]); }
            }
            for (int u = 0; u < (int)userPresets.size() && u < kMenuRows; ++u) items.push_back({ mx + PM_PAD + kNumCategories * PM_COLW, top + u * PM_ROW, PM_COLW - 4, PM_ROW, 1, u });
            maxRows = std::max(maxRows, std::min(kMenuRows, (int)userPresets.size()));
            for (int f = 0; f < (int)favs.size() && f < kMenuRows; ++f) items.push_back({ mx + PM_PAD + (kNumCategories + 1) * PM_COLW, top + f * PM_ROW, PM_COLW - 4, PM_ROW, 4, f });
            maxRows = std::max(maxRows, std::min(kMenuRows, (int)favs.size()));
        }
        mh = PM_PAD + PM_ROW + 6 + PM_HEAD + maxRows * PM_ROW + PM_PAD;
    }
    std::string itemLabel(const MRect& r) const {
        if (r.kind == 0) return presets()[r.idx].name;
        if (r.kind == 1) return userPresets[r.idx];
        if (r.kind == 4 && r.idx < (int)favs.size()) return favs[r.idx].substr(2);
        return "";
    }
    void drawPresetMenu() {
        float mx, my, mw, mh; std::vector<MRect> items; menuLayout(mx, my, mw, mh, items);
        beginPath(); rect(mx - 12, my - 4, mw + 24, mh + 20); fillPaint(boxGradient(mx, my + 2, mw, mh, 6, 14, Color(0, 0, 0, 0.6f), Color(0, 0, 0, 0.f))); fill();
        beginPath(); roundedRect(mx, my, mw, mh, 6); fillColor(C_PANEL_HI); fill(); strokeColor(C_LINE); strokeWidth(1); stroke();
        beginPath(); moveTo(mx + PM_PAD, my + PM_PAD + PM_ROW + 3); lineTo(mx + mw - PM_PAD, my + PM_PAD + PM_ROW + 3); strokeColor(C_LINE); stroke();
        fontSize(8.5f); textLetterSpacing(1.8f); textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
        const float hy = my + PM_PAD + PM_ROW + 6 + PM_HEAD / 2;
        if (query.empty()) {
            for (int c = 0; c < kMenuCols; ++c) {
                std::string up = c < kNumCategories ? kCategories[c] : (c == kNumCategories ? "User" : "Favourites"); for (char& ch : up) ch = (char)std::toupper((unsigned char)ch);
                fillColor(c < kNumCategories ? C_ACC : C_FAV); text(mx + PM_PAD + c * PM_COLW + 12, hy, up.c_str(), nullptr);
            }
            fontSize(10); textLetterSpacing(0.4f); fillColor(C_FAINT);
            if (userPresets.empty()) text(mx + PM_PAD + kNumCategories * PM_COLW + 12, hy + PM_HEAD / 2 + PM_ROW / 2, "(none saved yet)", nullptr);
            if (favs.empty()) text(mx + PM_PAD + (kNumCategories + 1) * PM_COLW + 12, hy + PM_HEAD / 2 + PM_ROW / 2, "(click a star)", nullptr);
        } else {
            int found = 0; for (const MRect& r : items) if (r.kind == 0 || r.kind == 1) ++found;
            char b[64]; snprintf(b, sizeof b, found ? "%d MATCHING PRESET%s" : "NO PRESET MATCHES", found, found == 1 ? "" : "S"); fillColor(C_ACC); text(mx + PM_PAD + 12, hy, b, nullptr);
        }
        for (int i = 0; i < (int)items.size(); ++i) {
            const MRect& r = items[i]; const bool hot = i == menuHover;
            if (r.kind == 2) {
                beginPath(); roundedRect(r.x, r.y + 1, r.w, r.h - 2, 4); fillColor(hot ? withAlpha(C_ACC, 0.25f) : C_PANEL); fill(); strokeColor(C_ACC); strokeWidth(1); stroke();
                fontSize(9.5f); textLetterSpacing(1.6f); textAlign(ALIGN_CENTER | ALIGN_MIDDLE); fillColor(C_TEXT); text(r.x + r.w / 2, r.y + r.h / 2 + 0.5f, "SAVE CURRENT AS...", nullptr);
                continue;
            }
            if (r.kind == 3) {             // search field
                beginPath(); roundedRect(r.x, r.y + 1, r.w, r.h - 2, 4); fillColor(C_DISPLAY); fill(); strokeColor(query.empty() ? C_LINE : C_ACC); strokeWidth(1); stroke();
                beginPath(); circle(r.x + 12, r.y + r.h / 2, 4); moveTo(r.x + 15, r.y + r.h / 2 + 3); lineTo(r.x + 18, r.y + r.h / 2 + 6); strokeColor(C_DIM); strokeWidth(1.3f); stroke();
                fontSize(11); textLetterSpacing(0.3f); textAlign(ALIGN_LEFT | ALIGN_MIDDLE); fillColor(query.empty() ? C_FAINT : C_TEXT);
                text(r.x + 26, r.y + r.h / 2 + 0.5f, query.empty() ? "Type to search names and categories" : (query + "|").c_str(), nullptr);
                continue;
            }
            const std::string key = favKey(r.kind, r.idx);
            const bool isCur = r.kind == 0 ? (curUser.empty() && r.idx == curFactory) : (r.kind == 1 ? curUser == userPresets[r.idx] : false);
            const bool del = r.kind == 1 && pendingDelete == r.idx;
            if (hot || del) { beginPath(); roundedRect(r.x, r.y, r.w, r.h, 4); fillColor(del ? Color(0xd0, 0x50, 0x40, 0.35f) : withAlpha(C_ACC, 0.18f)); fill(); }
            if (isCur) { beginPath(); circle(r.x + 6, r.y + r.h / 2, 2.5f); fillColor(C_ACC); fill(); }
            fontSize(11); textLetterSpacing(0.3f); textAlign(ALIGN_LEFT | ALIGN_MIDDLE); fillColor(hot || isCur || del ? C_TEXT : C_DIM);
            const std::string label = del ? "Delete? click again" : itemLabel(r);
            text(r.x + 12, r.y + r.h / 2 + 0.5f, label.c_str(), nullptr);
            // the favourite star
            const bool fav = isFav(key);
            if (fav || hot) drawStar(r.x + r.w - 10, r.y + r.h / 2, 5.f, fav);
        }
    }
    void drawStar(float cx, float cy, float rr, bool filled) {
        beginPath();
        for (int k = 0; k < 10; ++k) { const float a = -3.14159265f / 2 + k * 3.14159265f / 5, q = k % 2 ? rr * 0.45f : rr; if (!k) moveTo(cx + q * std::cos(a), cy + q * std::sin(a)); else lineTo(cx + q * std::cos(a), cy + q * std::sin(a)); }
        closePath();
        if (filled) { fillColor(C_FAV); fill(); } else { strokeColor(C_DIM); strokeWidth(1); stroke(); }
    }
    int menuHit(float x, float y) const { float mx, my, mw, mh; std::vector<MRect> it; menuLayout(mx, my, mw, mh, it);
        for (int i = 0; i < (int)it.size(); ++i) if (x >= it[i].x && x < it[i].x + it[i].w && y >= it[i].y && y < it[i].y + it[i].h) return i;
        return -1; }
    MRect menuItem(int i) const { float mx, my, mw, mh; std::vector<MRect> it; menuLayout(mx, my, mw, mh, it); return it[i]; }
    void openMenu() { userPresets = listUserPresets(); favs = loadFavourites(); query.clear(); menuOpen = true; menuHover = -1; pendingDelete = -1; repaint(); }
    void applyFavourite(int idx) {
        if (idx >= (int)favs.size()) return;
        const std::string& k = favs[idx];
        if (k.compare(0, 2, "f:") == 0) { for (uint32_t i = 0; i < kNumPresets; ++i) if (k.substr(2) == presets()[i].name) { applyFactory((int)i); return; } }
        else applyUser(k.substr(2));
    }
    void drawSaveBox() {
        const float w = 360, h = 96, x = W / 2 - w / 2, y = 140;
        beginPath(); rect(0, 0, W, H); fillColor(Color(0, 0, 0, 0.45f)); fill();
        beginPath(); roundedRect(x, y, w, h, 8); fillColor(C_PANEL_HI); fill(); strokeColor(C_ACC); strokeWidth(1); stroke();
        fontSize(9.5f); textLetterSpacing(2.f); textAlign(ALIGN_LEFT | ALIGN_TOP); fillColor(C_DIM); text(x + 16, y + 14, "SAVE SOUND AS", nullptr);
        beginPath(); roundedRect(x + 16, y + 36, w - 32, 26, 4); fillColor(C_DISPLAY); fill(); strokeColor(C_LINE); stroke();
        fontSize(13); textLetterSpacing(0.4f); textAlign(ALIGN_LEFT | ALIGN_MIDDLE); fillColor(C_TEXT); text(x + 26, y + 49.5f, (saveName + "|").c_str(), nullptr);
        fontSize(8.5f); textLetterSpacing(1.2f); fillColor(C_FAINT); textAlign(ALIGN_RIGHT | ALIGN_TOP);
        text(x + w - 16, y + 72, saveError ? "COULD NOT SAVE - CHECK THE NAME" : "ENTER TO SAVE   ·   ESC TO CANCEL", nullptr);
    }

    // ---- pattern editing ------------------------------------------------------------
    void toolAction(int t) {
        if (t == TEditA || t == TEditB) { if ((int)(val[kSection] + .5f) == kSecAB) { editSec = t == TEditA ? 0 : 1; repaint(); } return; }
        if (t == TLen) return;
        if (t == TCopy) { clip = editPat(); haveClip = true; flash("Pattern copied"); return; }
        if (t == TPaste && !haveClip) return;
        beginGesture();
        Pattern& pt = editPat();
        switch (t) {
        case TLenDown: pt.len = std::max(1, pt.len - 1); break;
        case TLenUp: pt.len = std::min(16, pt.len + 1); break;
        case TClear: { const int len = pt.len; pt = Pattern(); pt.len = len; for (Step& s : pt.s) s = Step(); break; }
        case TRandom: randomize(pt); break;
        case TShiftL: case TShiftR: {
            const int len = pt.len; Step tmp[16]; for (int i = 0; i < len; ++i) tmp[i] = pt.s[i];
            for (int i = 0; i < len; ++i) pt.s[i] = tmp[(i + (t == TShiftL ? 1 : len - 1)) % len];
            break; }
        case TTrDown: case TTrUp:
            for (Step& s : pt.s) {
                int n = s.note + s.oct * 12 + (t == TTrUp ? 1 : -1);
                n = std::max(-12, std::min(24, n));
                int o = n < 0 ? -1 : (n > 12 ? 1 : 0); if (n == 12 && s.oct == 0) o = 0;
                s.oct = (int8_t)o; s.note = (int8_t)(n - o * 12);
                if (s.note > 12) { s.note = 12; } if (s.note < 0) s.note = 0;
            }
            break;
        case TPaste: pt = clip; break;
        default: break;
        }
        pushPatterns(); endGesture();
    }
    void randomize(Pattern& pt) {
        static const int scale[] = { 0, 2, 3, 5, 7, 8, 10, 12 };
        std::uniform_real_distribution<float> u(0.f, 1.f);
        for (int i = 0; i < 16; ++i) {
            Step& s = pt.s[i];
            s.note = (int8_t)scale[(int)(u(rng) * 8) % 8]; if (u(rng) < 0.35f) s.note = 0;
            const float o = u(rng); s.oct = (int8_t)(o < 0.18f ? 1 : (o < 0.26f ? -1 : 0));
            s.accent = u(rng) < 0.3f; s.slide = u(rng) < 0.22f;
            const float g = u(rng); s.gate = (int8_t)(g < 0.14f ? kRest : (g < 0.22f && i > 0 ? kTie : kNote));
        }
        flash("Random pattern");
    }
    // grid hit: lane 0 = roll (row 0..12 top = C above), 1 oct, 2 accent, 3 slide, 4 gate
    bool gridHit(float x, float y, int& col, int& lane, int& row) const {
        if (x < GL || x >= GR) return false;
        col = (int)((x - GL) / COLW); if (col < 0 || col > 15) return false;
        if (y >= ROLL_Y && y < ROLL_Y + ROLL_H) { lane = 0; row = (int)((y - ROLL_Y) / ROW_H); return true; }
        const float ys[4] = { OCT_Y, ACC_Y, SLD_Y, GATE_Y };
        for (int k = 0; k < 4; ++k) if (y >= ys[k] - 2 && y < ys[k] + LANE_H + 2) { lane = k + 1; row = -1; return true; }
        return false;
    }
    int toolHit(float x, float y) const {
        if (y < TOOL_Y || y > TOOL_Y + TOOL_H) return -1;
        for (int t = 0; t < kNumTools; ++t) if (x >= kTools[t].x && x < kTools[t].x + kTools[t].w) return t;
        return -1;
    }
    void gridPress(int col, int lane, int row, bool right) {
        Pattern& pt = editPat(); Step& s = pt.s[col];
        beginGesture(); gridDrag = true; dragLane = lane; dragCol = col;
        if (lane == 0) {
            if (right) { s.gate = s.gate == kRest ? (int8_t)kNote : (int8_t)kRest; gridDrag = false; }
            else { s.note = (int8_t)(12 - row); if (s.gate != kNote) s.gate = kNote; }
        } else if (lane == 1) { s.oct = (int8_t)(s.oct == 0 ? 1 : (s.oct == 1 ? -1 : 0)); gridDrag = false; }
        else if (lane == 2) { s.accent = !s.accent; paintVal = s.accent; }
        else if (lane == 3) { s.slide = !s.slide; paintVal = s.slide; }
        else { s.gate = (int8_t)(s.gate == kNote ? kTie : (s.gate == kTie ? kRest : kNote)); gridDrag = false; }
        pushPatterns();
        if (!gridDrag) endGesture();
    }
    void gridDragTo(float x, float y) {
        Pattern& pt = editPat(); int col, lane, row;
        if (dragLane == 0) {
            const int r = (int)clampf((y - ROLL_Y) / ROW_H, 0.f, 12.f);
            Step& s = pt.s[dragCol]; if (s.note != 12 - r) { s.note = (int8_t)(12 - r); pushPatterns(); }
            return;
        }
        if (!gridHit(x, y, col, lane, row) || lane != dragLane) return;
        Step& s = pt.s[col];
        bool& f = dragLane == 2 ? s.accent : s.slide;
        if (f != paintVal) { f = paintVal; pushPatterns(); }
    }

    // ---- idle: scope, sequencer position, tips ---------------------------------------
    void uiIdle() override {
        const double t = nowSec();
        if (t - lastPoll > 0.03) {
            lastPoll = t;
            if (MalachitePlugin* const dsp = (MalachitePlugin*)getPluginInstancePointer()) {
                const Engine& e = dsp->engine;
                const int ps = e.stepNow.load(), pp = e.patNow.load();
                if (ps != playStep || pp != playPat) { playStep = ps; playPat = pp; }
                gateLit = e.gateOn(); accentLit = e.accentLit();
                // scope: the latest cycle-aligned stretch
                const uint32_t w = e.scopeWrite.load(std::memory_order_acquire);
                const int span = 1400, look = 2400;
                uint32_t start = w - span;
                for (int k = 0; k < look - span; ++k) {
                    const uint32_t i0 = w - span - k - 1, i1 = i0 + 1;
                    if (e.scope[i0 & (Engine::kScope - 1)] < 0 && e.scope[i1 & (Engine::kScope - 1)] >= 0) { start = i1; break; }
                }
                scopeBuf.resize(360);
                for (int i = 0; i < 360; ++i) scopeBuf[i] = e.scope[(start + (uint32_t)(i * span / 360)) & (Engine::kScope - 1)];
            }
            repaint();
        }
        if (gTipsOn && !tipShown && tipKey >= 0 && tipKey != tipSuppressed && active < 0 && !gridDrag && !menuOpen && !saving && !helpOpen && editing < 0
            && t - tipSince > 0.6) { tipShown = true; repaint(); }
    }
    bool tipContent(std::string& title, std::string& value, std::string& body) const {
        if (tipKey < 0) return false;
        if (tipKey < 1000) {
            const Ctl& c = controls()[tipKey]; const uint32_t p = c.param; char b[40]; fmt(p, b, sizeof b);
            title = paramDef(p).name; value = b; body = paramTip(p);
            if (c.type == Knob) body += "\n\nDrag up/down or grab the rim and turn. Double-click to type a value, Cmd/Ctrl-click to reset.";
            if (paramCC[p] >= 0) { char cc[32]; snprintf(cc, sizeof cc, "   ·   CC%d", paramCC[p]); value += cc; }
            return true;
        }
        int t = -1;
        if (tipKey >= 3000) t = tipKey - 3000;
        else if (tipKey >= 2000) t = kTipPrev + (tipKey - 2000);
        else { const int hh = tipKey - 1000;
            t = hh == 1 ? kTipLearn : hh == 2 ? kTipMidi : hh == 3 ? kTipUndo : hh == 4 ? kTipRedo : hh == 5 ? kTipHelp : hh == 6 ? kTipA : hh == 7 ? kTipB : kTipCopy; }
        title = targetTitle(t); value.clear(); body = targetTip(t);
        if (t == kTipUndo || t == kTipRedo) { const size_t n = t == kTipUndo ? undoStack.size() : redoStack.size(); char b[32]; snprintf(b, sizeof b, "%d step%s", (int)n, n == 1 ? "" : "s"); value = b; }
        if (t == kTipLength) { char b[16]; snprintf(b, sizeof b, "%d", editPat().len); value = b; }
        return true;
    }
    void drawTooltip() {
        std::string title, value, body;
        if (!tipContent(title, value, body)) return;
        const float w = 320, pad = 12;
        fontSize(11); textLetterSpacing(0.2f); textLineHeight(1.3f);
        float bb[4]; textBoxBounds(0, 0, w - 2 * pad, body.c_str(), nullptr, bb);
        const float h = pad + 16 + 8 + (bb[3] - bb[1]) + pad;
        float x = tipX + 16, y = tipY + 20;
        if (x + w > W - 8) x = tipX - w - 12;
        if (y + h > H - 8) y = tipY - h - 12;
        x = clampf(x, 8, W - w - 8); y = clampf(y, 8, H - h - 8);
        beginPath(); rect(x - 10, y - 4, w + 20, h + 18); fillPaint(boxGradient(x, y + 3, w, h, 6, 14, Color(0, 0, 0, 0.55f), Color(0, 0, 0, 0.f))); fill();
        beginPath(); roundedRect(x, y, w, h, 6); fillColor(Color(0x18, 0x22, 0x1d)); fill(); strokeColor(C_LINE); strokeWidth(1); stroke();
        fontSize(10); textLetterSpacing(1.6f); textAlign(ALIGN_LEFT | ALIGN_TOP); fillColor(C_TEXT);
        std::string up = title; for (char& ch : up) ch = (char)std::toupper((unsigned char)ch);
        text(x + pad, y + pad + 1, up.c_str(), nullptr);
        if (!value.empty()) { fontSize(10.5f); textLetterSpacing(0.4f); textAlign(ALIGN_RIGHT | ALIGN_TOP); fillColor(C_ACC); text(x + w - pad, y + pad + 1, value.c_str(), nullptr); }
        fontSize(11); textLetterSpacing(0.2f); textLineHeight(1.3f); textAlign(ALIGN_LEFT | ALIGN_TOP); fillColor(C_DIM);
        textBox(x + pad, y + pad + 24, w - 2 * pad, body.c_str(), nullptr);
        textLineHeight(1.f);
    }

    // ---- help window ---------------------------------------------------------------
    static constexpr float HX = 150, HY = 80, HW = 760, HH = 560, TAB_W = 180, TAB_H = 34;
    int helpHit(float x, float y) const {
        if (x < HX || x > HX + HW || y < HY || y > HY + HH) return -2;
        for (int i = 0; i < kNumHelpPages; ++i) if (x >= HX + 12 && x < HX + 12 + TAB_W && y >= HY + 56 + i * TAB_H && y < HY + 56 + (i + 1) * TAB_H) return i;
        const float by = HY + HH - 44;
        if (y >= by && y < by + 28) {
            if (x >= HX + 12 && x < HX + 12 + TAB_W) return 100;
            if (x >= HX + HW - 330 && x < HX + HW - 140) return 101;
            if (x >= HX + HW - 124 && x < HX + HW - 16) return 102;
        }
        if (x >= HX + HW - 40 && y < HY + 40) return 102;
        return -1;
    }
    void helpClick(float x, float y) {
        const int h = helpHit(x, y);
        if (h >= 0 && h < kNumHelpPages) helpPage = h;
        else if (h == 100) gTipsOn = !gTipsOn;
        else if (h == 101) openURL(kManualURL);
        else if (h == 102 || h == -2) helpOpen = false;
        repaint();
    }
    void drawHelp() {
        beginPath(); rect(0, 0, W, H); fillColor(Color(0, 0, 0, 0.55f)); fill();
        beginPath(); roundedRect(HX, HY, HW, HH, 10); fillColor(C_PANEL); fill(); strokeColor(C_LINE); strokeWidth(1); stroke();
        fontSize(13); textLetterSpacing(4.f); textAlign(ALIGN_LEFT | ALIGN_MIDDLE); fillColor(C_TEXT); text(HX + 22, HY + 28, "MALACHITE HELP", nullptr);
        const float cx = HX + HW - 24, cy = HY + 26;
        beginPath(); moveTo(cx - 6, cy - 6); lineTo(cx + 6, cy + 6); moveTo(cx + 6, cy - 6); lineTo(cx - 6, cy + 6); strokeColor(helpHover == 102 ? C_TEXT : C_DIM); strokeWidth(1.6f); stroke();
        for (int i = 0; i < kNumHelpPages; ++i) {
            const float ty = HY + 56 + i * TAB_H;
            if (i == helpPage || i == helpHover) { beginPath(); roundedRect(HX + 12, ty + 2, TAB_W, TAB_H - 4, 5); fillColor(i == helpPage ? withAlpha(C_ACC, 0.16f) : C_PANEL_HI); fill(); }
            if (i == helpPage) { beginPath(); roundedRect(HX + 12, ty + 8, 3, TAB_H - 16, 1.5f); fillColor(C_ACC); fill(); }
            fontSize(11.5f); textLetterSpacing(0.6f); textAlign(ALIGN_LEFT | ALIGN_MIDDLE); fillColor(i == helpPage ? C_TEXT : C_DIM);
            text(HX + 28, ty + TAB_H / 2 + 0.5f, kHelpPages[i].title, nullptr);
        }
        beginPath(); moveTo(HX + TAB_W + 26, HY + 56); lineTo(HX + TAB_W + 26, HY + HH - 56); strokeColor(C_LINE); strokeWidth(1); stroke();
        const float bx = HX + TAB_W + 46, bw = HW - TAB_W - 70;
        fontSize(15); textLetterSpacing(0.8f); textAlign(ALIGN_LEFT | ALIGN_TOP); fillColor(C_ACC); text(bx, HY + 60, kHelpPages[helpPage].title, nullptr);
        fontSize(12); textLetterSpacing(0.2f); textLineHeight(1.38f); fillColor(Color(0xc6, 0xd2, 0xca));
        textBox(bx, HY + 92, bw, kHelpPages[helpPage].body, nullptr);
        textLineHeight(1.f);
        const float by = HY + HH - 44;
        beginPath(); moveTo(HX + 12, by - 10); lineTo(HX + HW - 12, by - 10); strokeColor(C_LINE); stroke();
        const float tw = 34, th = 18, tx = HX + 16, tyy = by + 14 - th / 2;
        beginPath(); roundedRect(tx, tyy, tw, th, th / 2); fillColor(gTipsOn ? withAlpha(C_ACC, 0.25f) : C_PANEL_HI); fill(); strokeColor(gTipsOn ? C_ACC : C_LINE); stroke();
        beginPath(); circle(gTipsOn ? tx + tw - th / 2 : tx + th / 2, by + 14, 6); fillColor(gTipsOn ? C_ACC : C_DIM); fill();
        fontSize(10); textLetterSpacing(1.6f); textAlign(ALIGN_LEFT | ALIGN_MIDDLE); fillColor(helpHover == 100 ? C_TEXT : C_DIM); text(tx + tw + 10, by + 14.5f, "HOVER TIPS", nullptr);
        auto btn = [&](float x0, float w0, const char* label, bool hot, bool primary) {
            beginPath(); roundedRect(x0, by, w0, 28, 5); fillColor(primary ? (hot ? withAlpha(C_ACC, 0.32f) : withAlpha(C_ACC, 0.16f)) : (hot ? C_PANEL_HI : C_PANEL)); fill();
            strokeColor(primary ? C_ACC : C_LINE); strokeWidth(1); stroke();
            fontSize(9.5f); textLetterSpacing(1.8f); textAlign(ALIGN_CENTER | ALIGN_MIDDLE); fillColor(C_TEXT); text(x0 + w0 / 2, by + 14.5f, label, nullptr);
        };
        btn(HX + HW - 330, 190, "FULL MANUAL (WEB)", helpHover == 101, false);
        btn(HX + HW - 124, 108, "CLOSE", helpHover == 102, true);
    }

    // ---- interaction --------------------------------------------------------------
    static bool inRect(float x, float y, float rx, float ry, float rw, float rh) { return x >= rx && x < rx + rw && y >= ry && y < ry + rh; }
    int hit(float x, float y, int* seg = nullptr) const {
        const std::vector<Ctl>& cs = controls();
        for (int i = 0; i < (int)cs.size(); ++i) {
            const Ctl& c = cs[i];
            if (c.type == Knob) { const float rr = c.r + 6; if ((x - c.x) * (x - c.x) + (y - c.y) * (y - c.y) <= rr * rr) return i; }
            else { int n = 0; segLabels(c.param, n); const float x0 = c.x - c.r / 2;
                if (x >= x0 && x < x0 + c.r && std::fabs(y - c.y) <= SEG_H / 2 + 1) { if (seg) *seg = (int)((x - x0) / (c.r / n)); return i; } }
        }
        return -1;
    }
    int headerHit(float x, float y) const {
        if (y < PS_Y || y > PS_Y + PS_H) return 0;
        if (x >= LEARN_X && x <= LEARN_X + LEARN_W) return 1;
        if (x >= MIDI_X && x <= MIDI_X + MIDI_W) return 2;
        if (x >= UNDO_X && x <= UNDO_X + UR_W) return 3;
        if (x >= REDO_X && x <= REDO_X + UR_W) return 4;
        if (x >= HELP_X && x <= HELP_X + HELP_W) return 5;
        if (x >= A_X && x < A_X + AB_W) return 6;
        if (x >= A_X + AB_W && x < A_X + 2 * AB_W) return 7;
        if (x >= COPY_X && x <= COPY_X + COPY_W) return 8;
        return 0;
    }
    int presetBarHit(float x, float y) const {
        if (y < PS_Y || y > PS_Y + PS_H || x < PS_X || x > PS_X + PS_W) return -1;
        return x < PS_X + PS_ARROW ? 0 : (x > PS_X + PS_W - PS_ARROW ? 2 : 1);
    }
    bool clearHit(float x, float y) const { return learnMode && learnWaiting < 0 && inRect(x, y, SX + 14, SY + 178, 180, 16); }

    void setLearn(bool on) { learnMode = on; learnWaiting = -1; if (!on) setState("cmd", "cancel"); repaint(); }
    void endEdit(bool apply) {
        if (editing < 0) return;
        const uint32_t p = controls()[editing].param; float v;
        if (apply && parse(p, editBuf, v)) { beginGesture(); commit(p, v); endGesture(); }
        editing = -1; editBuf.clear(); repaint();
    }
    static float angleNorm(const Ctl& c, float x, float y) {
        float a = std::atan2(y - c.y, x - c.x);
        const float a0 = 0.75f * kPi;
        float rel = a - a0; while (rel < 0) rel += 2 * kPi;
        if (rel > 1.5f * kPi) rel = rel > 1.75f * kPi ? 0.f : 1.5f * kPi;
        return rel / (1.5f * kPi);
    }

    bool onKeyboard(const KeyboardEvent& ev) override {
        if (!ev.press) return false;
        std::string* buf = saving ? &saveName : (editing >= 0 ? &editBuf : nullptr);
        if (!buf && menuOpen) {                              // typing in the preset browser searches
            if (ev.key == kKeyEscape) { if (!query.empty()) query.clear(); else menuOpen = false; }
            else if (ev.key == kKeyBackspace || ev.key == kKeyDelete) { if (!query.empty()) query.pop_back(); }
            else if (ev.key >= 32 && ev.key < 127 && query.size() < 32) query += (char)ev.key;
            else return false;
            menuHover = -1; pendingDelete = -1; repaint(); return true;
        }
        if (!buf) {
            const bool cmd = (ev.mod & (kModifierControl | kModifierSuper)) != 0;
            if (cmd && (ev.key == 'z' || ev.key == 'Z')) { if (ev.mod & kModifierShift) redo(); else undo(); return true; }
            if (cmd && (ev.key == 'y' || ev.key == 'Y')) { redo(); return true; }
            if (ev.key == kKeyEscape) { if (helpOpen) helpOpen = false; else if (menuOpen) menuOpen = false; else if (learnMode) setLearn(false); repaint(); return true; }
            return false;
        }
        if (ev.key == kKeyEnter) {
            if (saving) {
                if (saveUserPreset(saveName, val)) { userPresets = listUserPresets(); curUser = sanitizeName(saveName); curFactory = -1; std::copy(val, val + kParamCount, userVals); saving = false; saveError = false; }
                else saveError = true;
            } else endEdit(true);
            repaint(); return true;
        }
        if (ev.key == kKeyEscape) { if (saving) saving = false; else endEdit(false); repaint(); return true; }
        if (ev.key == kKeyBackspace || ev.key == kKeyDelete) { if (!buf->empty()) buf->pop_back(); repaint(); return true; }
        if (ev.key >= 32 && ev.key < 127 && buf->size() < (saving ? 40u : 12u)) { *buf += (char)ev.key; repaint(); return true; }
        return true;
    }

    bool onMouse(const MouseEvent& ev) override {
        const float s = uiScale(), x = ev.pos.getX() / s, y = ev.pos.getY() / s;
        if (saving) return true;
        if (ev.press) { statusFlash = 0; tipShown = false; tipSuppressed = tipKey; }
        if (helpOpen) { if (ev.press && ev.button == 1) helpClick(x, y); return true; }
        if (ev.button == 2 && ev.press) {                  // right-click (DPF numbers buttons 1 left, 2 right, 3 middle)
            if (menuOpen) { const int m = menuHit(x, y); if (m >= 0 && menuItem(m).kind == 1) { pendingDelete = menuItem(m).idx; repaint(); } return true; }
            if (learnMode) { const int i = hit(x, y); if (i >= 0) { const uint32_t p = controls()[i].param; char b[24]; snprintf(b, sizeof b, "unmap:%u", p); setState("cmd", b); paramCC[p] = -1; repaint(); } return true; }
            int col, lane, row; if (gridHit(x, y, col, lane, row) && lane == 0) { gridPress(col, lane, row, true); return true; }
            return false;
        }
        if (ev.button != 1) return false;
        if (!ev.press) {
            if (active >= 0) { editParameter(controls()[active].param, false); active = -1; endGesture(); return true; }
            if (gridDrag) { gridDrag = false; endGesture(); return true; }
            return false;
        }
        if (editing >= 0) endEdit(true);
        if (menuOpen) {
            const int m = menuHit(x, y);
            if (m >= 0) {
                const MRect r = menuItem(m);
                if (r.kind == 3) return true;                       // the search field: just type
                if ((r.kind == 0 || r.kind == 1 || r.kind == 4) && x >= r.x + r.w - 20) { toggleFav(favKey(r.kind, r.idx)); return true; }
                if (r.kind == 2) { menuOpen = false; saving = true; saveError = false; saveName = curUser; }
                else if (r.kind == 1 && pendingDelete == r.idx) { deleteUserPreset(userPresets[r.idx]); if (curUser == userPresets[r.idx]) curUser.clear(); userPresets = listUserPresets(); pendingDelete = -1; repaint(); return true; }
                else { menuOpen = false; if (r.kind == 0) applyFactory(r.idx); else if (r.kind == 1) applyUser(userPresets[r.idx]); else applyFavourite(r.idx); }
            } else menuOpen = false;
            pendingDelete = -1; menuHover = -1; repaint(); return true;
        }
        const int hh = headerHit(x, y);
        if (hh == 1) { setLearn(!learnMode); return true; }
        if (hh == 2) { beginGesture(); commit(kMidiOn, val[kMidiOn] > 0.5f ? 0.f : 1.f); endGesture(); return true; }
        if (hh == 3) { undo(); return true; }
        if (hh == 4) { redo(); return true; }
        if (hh == 5) { helpOpen = true; repaint(); return true; }
        if (hh == 6 || hh == 7) { switchAB(hh - 6); return true; }
        if (hh == 8) { copyAB(); return true; }
        const int pb = presetBarHit(x, y);
        if (pb == 0) { step(-1); return true; }
        if (pb == 2) { step(1); return true; }
        if (pb == 1) { openMenu(); return true; }
        if (clearHit(x, y)) { setState("cmd", "clear"); for (int& m : paramCC) m = -1; repaint(); return true; }

        int seg = 0; const int i = hit(x, y, &seg);
        if (i >= 0) {
            const Ctl& c = controls()[i]; const uint32_t p = c.param;
            if (learnMode) { learnWaiting = (int)p; char b[24]; snprintf(b, sizeof b, "learn:%u", p); setState("cmd", b); repaint(); return true; }
            if (c.type == Segment) { beginGesture(); commit(p, (float)seg); endGesture(); return true; }
            if (ev.mod & (kModifierControl | kModifierSuper)) { beginGesture(); commit(p, defv[p]); endGesture(); return true; }
            if (i == lastClick && ev.time - lastClickTime < 350) {
                lastClick = -1; editing = i;
                char b[40]; fmt(p, b, sizeof b); editBuf = b;
                const size_t sp = editBuf.find(' '); if (sp != std::string::npos) editBuf.resize(sp);
                if (!editBuf.empty() && editBuf.back() == '%') editBuf.pop_back();
                repaint(); return true;
            }
            lastClick = i; lastClickTime = ev.time;
            beginGesture();
            active = i; dragY = y; dragNorm = toNorm(p, val[p]);
            circular = std::sqrt((x - c.x) * (x - c.x) + (y - c.y) * (y - c.y)) > c.r * 0.62f && c.r > 20;
            editParameter(p, true);
            if (circular) setValue(p, fromNorm(p, angleNorm(c, x, y)));
            repaint(); return true;
        }
        if (learnMode) return false;
        const int t = toolHit(x, y);
        if (t >= 0) { toolAction(t); return true; }
        int col, lane, row;
        if (gridHit(x, y, col, lane, row)) { gridPress(col, lane, row, false); return true; }
        return false;
    }

    bool onMotion(const MotionEvent& ev) override {
        const float s = uiScale(), x = ev.pos.getX() / s, y = ev.pos.getY() / s;
        if (saving) return true;
        if (helpOpen) { const int h = helpHit(x, y); if (h != helpHover) { helpHover = h; repaint(); } return true; }
        if (active >= 0) {
            const Ctl& c = controls()[active]; const uint32_t p = c.param;
            if (circular) { setValue(p, fromNorm(p, angleNorm(c, x, y))); return true; }
            const float sens = (ev.mod & kModifierShift) ? 1200.f : 240.f;
            dragNorm = clampf(dragNorm + (dragY - y) / sens, 0.f, 1.f); dragY = y;
            setValue(p, fromNorm(p, dragNorm));
            return true;
        }
        if (gridDrag) { gridDragTo(x, y); return true; }
        if (menuOpen) { const int m = menuHit(x, y); if (m != menuHover) { menuHover = m; repaint(); } return true; }
        const int hh = headerHit(x, y); if (hh != hoverHeader) { hoverHeader = hh; repaint(); }
        const int pb = presetBarHit(x, y); if (pb != presetHover) { presetHover = pb; repaint(); }
        const bool hc = clearHit(x, y); if (hc != hoverClear) { hoverClear = hc; repaint(); }
        const int h = hit(x, y); if (h != hover) { hover = h; repaint(); }
        const int th = toolHit(x, y); if (th != hoverTool) { hoverTool = th; repaint(); }
        int key = -1, col, lane, row;
        if (h >= 0) key = h; else if (hh > 0) key = 1000 + hh; else if (pb >= 0) key = 2000 + pb;
        else if (th >= 0) { static const int m[kNumTools] = { kTipEditHalf, kTipEditHalf, kTipLength, kTipLength, kTipLength, kTipClear, kTipRandom, kTipShift, kTipShift, kTipTranspose, kTipTranspose, kTipPatCopy, kTipPatPaste }; key = 3000 + m[th]; }
        else if (gridHit(x, y, col, lane, row)) { static const int m[5] = { kTipRoll, kTipOct, kTipAccentRow, kTipSlideRow, kTipGateRow }; key = 3000 + m[lane]; }
        else if (inRect(x, y, SCX, SCY, SCW, SCH)) key = 3000 + kTipScope;
        else if (inRect(x, y, SX + 30, SY + 108, 90, 46)) key = 3000 + kTipLeds;
        tipX = x; tipY = y;
        if (key != tipKey) { const bool was = tipShown; tipKey = key; tipSince = nowSec(); tipShown = false; if (key != tipSuppressed) tipSuppressed = -2; if (was) repaint(); }
        return false;
    }

    bool onScroll(const ScrollEvent& ev) override {
        const float s = uiScale(), sx = ev.pos.getX() / s, sy = ev.pos.getY() / s;
        if (menuOpen || saving || helpOpen) return true;
        if (presetBarHit(sx, sy) >= 0) { step(ev.delta.getY() > 0 ? -1 : 1); return true; }
        const int i = hit(sx, sy);
        if (i >= 0 && controls()[i].type == Knob) {
            const uint32_t p = controls()[i].param; const float d = (ev.delta.getY() > 0 ? 1.f : -1.f) * ((ev.mod & kModifierShift) ? 0.004f : 0.02f);
            beginGesture(); commit(p, fromNorm(p, toNorm(p, val[p]) + d)); endGesture(); return true;
        }
        int col, lane, row;
        if (gridHit(sx, sy, col, lane, row) && lane == 0) {      // wheel over a step: its note
            beginGesture(); Step& st = editPat().s[col]; st.note = (int8_t)std::max(0, std::min(12, st.note + (ev.delta.getY() > 0 ? 1 : -1))); pushPatterns(); endGesture(); return true;
        }
        return false;
    }

private:
    float val[kParamCount], defv[kParamCount], userVals[kParamCount] = {};
    int paramCC[kParamCount];
    Pattern pats[kNumPatterns]; std::string patText; Pattern clip; bool haveClip = false; int editSec = 0;
    int active = -1, hover = -1, lastClick = -1, editing = -1, hoverTool = -1;
    uint lastClickTime = 0;
    float dragY = 0, dragNorm = 0; bool circular = false, gesture = false;
    bool gridDrag = false, paintVal = false; int dragLane = 0, dragCol = 0;
    Snap before; std::vector<Snap> undoStack, redoStack; Snap abStore[2]; int abSlot = 0;
    std::string editBuf, statusText; int statusFlash = 0;
    int curFactory = 0, presetHover = -1, menuHover = -1, pendingDelete = -1, hoverHeader = 0;
    std::string curUser, query; std::vector<std::string> userPresets, favs;
    bool menuOpen = false, saving = false, saveError = false; std::string saveName;
    bool learnMode = false, hoverClear = false; int learnWaiting = -1; float lastLearnEcho = 0;
    int tipKey = -1, tipSuppressed = -2; double tipSince = 0; bool tipShown = false; float tipX = 0, tipY = 0;
    bool helpOpen = false; int helpPage = 0, helpHover = -1;
    int playStep = -1, playPat = -1, transposeShown = 0; bool gateLit = false, accentLit = false; double lastPoll = 0;
    std::vector<float> scopeBuf;
    ResizeHandle resizeHandle;
    std::mt19937 rng;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MalachiteUI)
};

UI* createUI() { return new MalachiteUI(); }

END_NAMESPACE_DISTRHO
