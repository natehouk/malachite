// Malachite — hover tips and the in-plugin help pages. License: MIT.
#pragma once
#include "MalachiteEngine.hpp"

namespace malachite {

static const char* const kManualURL = "https://github.com/natehouk/malachite/blob/main/docs/MANUAL.md";

inline const char* paramTip(uint32_t p) {
    switch (p) {
    case kWave: return "The oscillator: SAW is bright and buzzy, SQUARE is hollower and woodier.";
    case kTune: return "Tunes the whole instrument up or down by up to an octave (12 semitones), in fine steps.";
    case kCutoff: return "The filter's starting point. Low settings are dark and muffled; the envelope opens it up on every note. For classic acid, start low and work ENV MOD and DECAY.";
    case kResonance: return "Emphasis at the cutoff: the squelch. Around two thirds and up is the classic acid sound. It gets very vocal near the top but never whistles on its own.";
    case kEnvMod: return "How far the filter envelope opens the filter on each note.";
    case kDecay: return "How long the filter envelope takes to close again, 0.2 to 2.5 seconds. Accented notes always use a short decay.";
    case kAccent: return "How much accented steps stand out: louder, with a stronger, snappier filter sweep. Several accents in a row build up a rising sweep, more so with high resonance.";
    case kVolume: return "Output volume.";
    case kSeqMode: return "OFF: play Malachite from MIDI notes (velocity 100 and up accents, overlapping notes slide). HOST: the pattern plays in time with your DAW while its transport runs. FREE: the pattern runs on its own at the FREE TEMPO. While the sequencer plays, MIDI notes transpose it (C3 = no change).";
    case kGroup: return "Patterns are kept in four groups (I to IV) of eight patterns, each with an A and a B half: 64 in all.";
    case kPattern: return "Which pattern of the group plays (and is shown for editing).";
    case kSection: return "Plays the A half, the B half, or A then B for a pattern of up to 32 steps.";
    case kTempo: return "Tempo for the FREE sequencer mode. HOST mode follows your DAW's tempo instead.";
    case kMidiOn: return "Turns MIDI input on or off: notes, transposing and learned controllers.";
    default: return "";
    }
}

enum TipTarget { kTipUndo, kTipRedo, kTipLearn, kTipMidi, kTipHelp, kTipA, kTipB, kTipCopy, kTipPrev, kTipPresets, kTipNext,
                 kTipRoll, kTipOct, kTipAccentRow, kTipSlideRow, kTipGateRow, kTipLength, kTipClear, kTipRandom, kTipShift,
                 kTipTranspose, kTipPatCopy, kTipPatPaste, kTipEditHalf, kTipScope, kTipLeds, kNumTipTargets };
inline const char* targetTitle(int t) {
    static const char* const n[kNumTipTargets] = { "Undo", "Redo", "MIDI Learn", "MIDI", "Help", "Setting A", "Setting B", "Copy A/B",
        "Previous preset", "Presets", "Next preset", "Notes", "Octave", "Accent", "Slide", "Note / tie / rest", "Pattern length", "Clear pattern",
        "Random pattern", "Shift pattern", "Transpose pattern", "Copy pattern", "Paste pattern", "Edit half", "Scope", "Gate and accent" };
    return n[t];
}
inline const char* targetTip(int t) {
    switch (t) {
    case kTipUndo: return "Steps back through your edits, including pattern edits, preset loads and A/B switches. Cmd/Ctrl-Z.";
    case kTipRedo: return "Re-applies an edit you undid. Cmd/Ctrl-Shift-Z or Cmd/Ctrl-Y.";
    case kTipLearn: return "Click, then click a knob and move a knob or fader on your MIDI controller to assign it. Right-click a control to unassign.";
    case kTipMidi: return paramTip(kMidiOn);
    case kTipHelp: return "Opens the help pages: the sound, accent and slide, the sequencer, programming patterns, MIDI and shortcuts.";
    case kTipA: case kTipB: return "Malachite keeps two complete settings, A and B, including the patterns. Click the other letter to switch and compare.";
    case kTipCopy: return "Copies the current setting to the other slot.";
    case kTipPrev: return "Load the previous sound preset. The mouse wheel over the preset name also steps through presets. Presets change the sound only, never your patterns.";
    case kTipPresets: return "Open the preset browser. A * after the name means you have changed the sound.";
    case kTipNext: return "Load the next sound preset.";
    case kTipRoll: return "Click a cell to set that step's note (C to C). Drag up and down to change it. Right-click a step to make it a rest. Steps past the pattern length are greyed out.";
    case kTipOct: return "Click to move a step an octave up, down, or back to the middle. Octave jumps with slides are the heart of acid lines.";
    case kTipAccentRow: return "Click to accent a step: louder, with a stronger filter sweep. Drag across to set several.";
    case kTipSlideRow: return "Click to slide from this step into the next: the pitch glides and the note keeps sounding. Drag across to set several.";
    case kTipGateRow: return "Click to cycle NOTE (play the step), TIE (keep the previous note sounding through this step) and REST (silence).";
    case kTipLength: return "Pattern length, 1 to 16 steps. Odd lengths (12, 7, 5...) drift against a 4/4 beat.";
    case kTipClear: return "Resets the pattern to sixteen plain C notes. Undo brings it back.";
    case kTipRandom: return "Writes a random acid line: notes from a minor scale, with octave jumps, accents, slides, ties and rests. Press again for another.";
    case kTipShift: return "Rotates the pattern one step earlier or later.";
    case kTipTranspose: return "Moves every note of the pattern up or down a semitone.";
    case kTipPatCopy: return "Copies this pattern, to paste it into another slot.";
    case kTipPatPaste: return "Pastes the copied pattern over this one.";
    case kTipEditHalf: return "Which half of the pattern you are editing, A or B. When SECTION is A or B this follows it.";
    case kTipScope: return "The output waveform.";
    case kTipLeds: return "GATE lights while a note sounds; ACCENT lights on accented notes.";
    default: return "";
    }
}

struct HelpPage { const char* title; const char* body; };
static const HelpPage kHelpPages[] = {
    { "Overview",
      "Malachite is a monophonic acid bass synthesizer with a built-in step sequencer.\n\n"
      "1. Press play in your DAW: the sequencer (HOST mode) plays the selected pattern in time.\n"
      "2. Turn CUTOFF, RESONANCE, ENV MOD and DECAY while it plays: that is where the sound lives.\n"
      "3. Pick other patterns (GROUP and 1-8, A or B), or write your own in the grid.\n"
      "4. Or set the sequencer to OFF and play it from a MIDI keyboard or clip.\n\n"
      "Hover over anything for a moment to see what it does." },
    { "The sound",
      "One oscillator (SAW or SQUARE) goes through a resonant low-pass filter and an amplifier.\n\n"
      "CUTOFF sets where the filter starts; every note, the filter envelope opens it by ENV MOD and closes it again over DECAY. "
      "RESONANCE adds the squelch. The amplifier has a fixed shape: a quick attack and a long fade while the note is held.\n\n"
      "TUNING moves the pitch by up to an octave either way. VOLUME sets the level." },
    { "Accent & slide",
      "ACCENT on a step makes it louder and gives it a stronger, shorter filter sweep; the ACCENT knob sets how much. "
      "Several accents in a row charge up and make the filter rise further each time, especially with high resonance.\n\n"
      "SLIDE on a step glides the pitch into the next step and keeps the note sounding, without a new attack. "
      "Slide into an octave jump for the classic sound.\n\n"
      "From MIDI: velocity 100 and above accents a note; a note played before the previous one is released slides." },
    { "The sequencer",
      "Patterns have up to 16 steps of sixteenth notes. They are kept in four GROUPS (I-IV) of eight patterns, each with an A and a B half: 64 patterns.\n\n"
      "SECTION plays A, B, or A then B. SEQUENCER: HOST follows your DAW's transport and tempo, FREE runs on its own at FREE TEMPO, OFF lets MIDI play the synth.\n\n"
      "While the sequencer plays, MIDI notes transpose the pattern (C3 plays it as written). Patterns are saved with your project." },
    { "Programming",
      "Each column is a step. In the note grid, click a cell to set the step's note; drag to change it; right-click for a rest.\n\n"
      "Below: OCT moves a step up or down an octave, ACC accents it, SLIDE glides into the next step, and the last row cycles NOTE, TIE (hold the previous note) and REST.\n\n"
      "Tools: LENGTH, CLEAR, RANDOM, shift left/right, transpose, COPY and PASTE between pattern slots. Every edit can be undone." },
    { "MIDI & shortcuts",
      "MIDI LEARN: click LEARN, click a knob, move a controller. Right-click unassigns.\n\n"
      "A/B holds two complete settings, patterns included; COPY copies the current one to the other slot. Every edit is an undo step (Cmd/Ctrl-Z, Cmd/Ctrl-Shift-Z).\n\n"
      "Knobs: drag, Shift-drag for fine control, double-click to type a value, Cmd/Ctrl-click to reset. Esc closes this window." },
};
static constexpr int kNumHelpPages = sizeof(kHelpPages) / sizeof(kHelpPages[0]);

} // namespace malachite
