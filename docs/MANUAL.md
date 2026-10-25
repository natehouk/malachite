# Malachite 1.0 — User Manual

Malachite is a monophonic acid bass synthesizer with a built-in step sequencer. One oscillator goes through a squelchy resonant filter. Accents and slides give a line its bite and glide, and 64 patterns play in time with your DAW. Typical uses:

- acid basslines and squelchy leads
- tight, punchy synth bass played from MIDI
- rising, accent-driven filter sweeps for builds

Malachite runs as VST3, AU, CLAP and VST2 on macOS, Windows and Linux.

**Contents**

1. [Installing](#1-installing)
2. [A quick tour](#2-a-quick-tour)
3. [The sound](#3-the-sound)
4. [Accent and slide](#4-accent-and-slide)
5. [The sequencer](#5-the-sequencer)
6. [Programming patterns](#6-programming-patterns)
7. [Playing from MIDI](#7-playing-from-midi)
8. [Knobs](#8-knobs)
9. [A/B, undo and redo](#9-ab-undo-and-redo)
10. [MIDI Learn](#10-midi-learn)
11. [Presets](#11-presets)
12. [Help and hover tips](#12-help-and-hover-tips)
13. [Recipes](#13-recipes)
14. [Parameter reference](#14-parameter-reference)
15. [Troubleshooting](#15-troubleshooting)

---

## 1. Installing

Download the zip for your system from the project's **Releases** page and copy the plugin files:

| System | Copy | To |
|---|---|---|
| macOS | `Malachite.vst3`, `Malachite.component` (AU), `Malachite.clap` | `~/Library/Audio/Plug-Ins/VST3`, `.../Components`, `.../CLAP` |
| Windows | the `Malachite.vst3` folder, `Malachite.clap` | `C:\Program Files\Common Files\VST3\`, `...\CLAP\` |
| Linux | `Malachite.vst3`, `Malachite.clap` | `~/.vst3/`, `~/.clap/` |

**macOS:** if the build isn't notarized and macOS blocks it, open Terminal and run:

```
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/*/Malachite*
```

Rescan plugins in your DAW. Malachite appears under **Epsilon Audio** as an instrument. Put it on an instrument track and press play: the sequencer starts with the transport.

---

## 2. A quick tour

![Malachite](../screenshot.png)

- **Header:** sound preset browser, A/B and COPY, UNDO/REDO, LEARN (MIDI Learn) and MIDI on/off, HELP, and the output level.
- **Synth panel** (top):
  - WAVEFORM (SAW or SQUARE)
  - GATE and ACCENT lights
  - the knobs: TUNING, CUTOFF, RESONANCE, ENV MOD, DECAY, ACCENT, VOLUME
  - a scope of the output
- **Sequencer panel** (bottom):
  - top row: SEQUENCER mode (OFF, HOST, FREE), FREE TEMPO, GROUP (I-IV), PATTERN (1-8) and PLAY (A, B, A+B)
  - tool row: which half you are EDITing, LENGTH, CLEAR, RANDOM, SHIFT, TRANSPOSE, COPY and PASTE
  - the step grid: a note grid (C to C) above lanes for OCT, ACCENT, SLIDE and GATE, sixteen steps across; the playing step is highlighted

---

## 3. The sound

Malachite has one voice: an oscillator, a filter and an amplifier.

- **WAVEFORM:** SAW is bright and buzzy; SQUARE is hollower and woodier.
- **TUNING** moves the pitch up or down by up to 12 semitones, in fine steps.
- **CUTOFF** sets where the low-pass filter starts. Low settings are dark; each note's filter envelope opens it up from there.
- **RESONANCE** emphasises the cutoff: the squelch. Around two thirds and up is the classic acid sound. It gets very vocal near the top but never whistles on its own.
- **ENV MOD** sets how far the filter envelope opens the filter on each note.
- **DECAY** sets how long the filter envelope takes to close again: 0.2 to 2.5 seconds. Accented notes always use a short decay.
- **ACCENT** sets how much accented steps stand out (section 4).
- **VOLUME** sets the level.

The amplifier has a fixed shape: a quick attack, then a long fade while the note is held, and a quick release when the step ends.

---

## 4. Accent and slide

- **Accent** makes a step louder (up to about 5 dB) and gives it a stronger, snappier filter sweep. The ACCENT knob sets how much.
  - Accents charge up. Several accented steps close together make the filter rise further each time, then it settles back. Resonance makes this stronger and slower to fade.
- **Slide** on a step glides the pitch into the next step and keeps the note sounding, without a new attack. The glide takes a fixed time, about a tenth of a second.
  - Slide into an octave jump for the classic acid gesture.

---

## 5. The sequencer

- **SEQUENCER:**
  - **HOST** plays the pattern in time with your DAW while its transport runs. Steps line up with the DAW's bars and beats, so a pattern always starts on the beat.
  - **FREE** runs on its own at **FREE TEMPO** (40-300 BPM), whether or not the DAW is playing.
  - **OFF** stops the sequencer: MIDI notes play the synth instead (section 7).
- **Patterns:** each pattern has up to 16 sixteenth-note steps. They are kept in four **GROUPS** (I-IV) of eight patterns, and every pattern has an **A** and a **B** half: 64 in all.
- **PLAY** chooses the A half, the B half, or **A+B**: A then B, for lines of up to 32 steps.
- While the sequencer plays, **MIDI notes transpose** the pattern: C3 (MIDI note 60) plays it as written, D3 two semitones up, and so on.
- Patterns are saved with your project. GROUP, PATTERN and PLAY can be automated, so your DAW can switch patterns as the song goes.
- Group I comes with ten original example lines. Every other slot starts as sixteen plain C notes.

---

## 6. Programming patterns

The grid shows the pattern being edited: the one selected by GROUP and PATTERN, and its A or B half (EDIT). When PLAY is A or B, EDIT follows it; with A+B, choose the half with EDIT.

- **Note grid:** click a cell to set that step's note (C to the C above). Drag up and down to change it. Turn the mouse wheel over a step to nudge it. **Right-click** a step to make it a rest (and again to bring it back).
- **OCT:** click to move a step an octave **UP**, **DOWN**, or back to the middle.
- **ACCENT** and **SLIDE:** click to switch on or off; drag across to set several steps.
- **GATE:** click to cycle:
  - **NOTE** plays the step
  - **TIE** keeps the previous note sounding through this step, without a new attack
  - **REST** is silence
- Steps past the pattern length are greyed out, and a copper line marks the end.

**Tools:**

| Tool | Does |
|---|---|
| EDIT A / B | Which half you edit (when PLAY is A+B) |
| LENGTH - / + | Pattern length, 1 to 16 steps. Odd lengths drift against a 4/4 beat |
| CLEAR | Resets the pattern to sixteen plain C notes |
| RANDOM | Writes a random acid line: notes from a minor scale, octave jumps, accents, slides, ties and rests |
| SHIFT < / > | Rotates the pattern one step earlier or later |
| TRANSPOSE - / + | Moves every note up or down a semitone |
| COPY / PASTE | Copies a pattern and pastes it into another slot |

Every edit is an undo step.

---

## 7. Playing from MIDI

Set SEQUENCER to **OFF** to play Malachite like any monophonic synth:

- The last note held sounds; release it and the previous held note comes back.
- **Velocity 100 or more** accents the note.
- A note played while another is still held **slides** to it, without a new attack.

---

## 8. Knobs

- **Drag** up or down; **Shift** for fine adjustment. **Grab the rim** of a knob to turn it in a circle.
- **Mouse wheel** turns a knob.
- **Double-click** to type a value.
- **Cmd/Ctrl-click** resets to the default.

---

## 9. A/B, undo and redo

- **A / B** hold two complete settings, **patterns included**. Click the other letter to switch; the first time, B starts as a copy of A.
- **COPY** copies the current setting into the other slot.
- Every edit is an undo step, including pattern edits, preset loads and A/B switches.
  - Undo: Cmd/Ctrl-Z
  - Redo: Cmd/Ctrl-Shift-Z or Cmd/Ctrl-Y

---

## 10. MIDI Learn

1. Click **LEARN**.
2. Click a knob or switch.
3. Move a knob or fader on your controller.
4. Click **LEARN** again (or press Esc) to finish.

Right-click a control while learning to unassign it. **CLEAR ALL ASSIGNMENTS** removes them all. Assignments are saved with your project. The **MIDI** button switches all MIDI input off: notes, transposing and learned controllers.

---

## 11. Presets

Presets change the **sound only**: the sequencer settings and your patterns stay as they are, so you can audition sounds while a line plays. There are 22 factory sounds in four groups, plus your own:

| Group | Presets |
|---|---|
| Acid | Classic Acid, Squelchy, Rubber, Hard Acid, Square Acid, Slow Sweep, Chirp |
| Bass | Round Bass, Square Sub, Pluck Bass, Deep Bass, Funk Bass |
| Leads | Bright Lead, Square Lead, Reed Lead, High Acid |
| Squelch & FX | Zap, Wobble Tail, Closed Squelch, Open Wide, Slightly Off |

- **SAVE CURRENT AS…** stores your own sounds, in one of these folders:
  - macOS: `~/Library/Audio/Presets/Epsilon Audio/Malachite`
  - Windows: `%APPDATA%\Epsilon Audio\Malachite\Presets`
  - Linux: `~/.config/epsilon-audio/malachite/presets`
- To delete one of your presets, right-click it in the browser and click again to confirm.

---

## 12. Help and hover tips

- Rest the pointer on any control, grid lane, tool, the scope or the lights to see what it does and its current value.
- **HELP** opens the in-plugin pages. The switch at the bottom of the help window turns tips on or off.
- **FULL MANUAL (WEB)** opens this manual.

---

## 13. Recipes

- **Classic squelch:** SAW, CUTOFF 20-30%, RESONANCE 75-90%, ENV MOD 60-70%, DECAY 30-45%, ACCENT 70%+. Ride CUTOFF and ENV MOD while the line plays.
- **A line that builds:** put accents on several steps in a row, raise RESONANCE, and the accents ramp the filter up.
- **The octave bounce:** alternate a root note and the same note UP, with SLIDE on the low step.
- **12 against 4:** set LENGTH to 12 over a straight kick: the line shifts against the beat every bar.
- **Sub bass:** SQUARE, CUTOFF low, RESONANCE low, ENV MOD low, TUNING -12, ties for long notes.
- **Song arrangement:** automate PATTERN (and GROUP or PLAY) in your DAW to switch lines in time.

---

## 14. Parameter reference

There are 14 parameters, all automatable. New parameters are only ever added at the end, so sessions and automation keep working across versions.

- **Sound:** Waveform (Saw, Square), Tuning (±12 semitones), Cutoff, Resonance, Env Mod, Decay, Accent, Volume (all 0-100%).
- **Sequencer:** Sequencer (Off, Host, Free), Pattern Group (I-IV), Pattern (1-8), Section (A, B, A+B), Free Tempo (40-300 BPM).
- **MIDI:** MIDI on/off.

The patterns themselves are saved as part of the plugin's state. Malachite also reports its output level to hosts that show plugin outputs.

---

## 15. Troubleshooting

- **Malachite doesn't appear in my DAW.** Check the install folders above and rescan plugins. On macOS, run the `xattr` command from section 1.
- **No sound when I press play.** SEQUENCER must be HOST (or FREE), and the DAW must send its transport to plugins. Check VOLUME, and that the pattern has NOTE steps.
- **MIDI notes don't play the synth.** With the sequencer on, MIDI notes transpose the pattern; set SEQUENCER to OFF to play it directly. Check that the MIDI button is on.
- **The line plays a different pitch.** A MIDI note may have transposed it: play C3 to return to the written pitch, or stop and restart the sequencer.
- **Latency:** 16 samples (0.33 ms at 48 kHz), from running the oscillator and filter at twice the sample rate. Malachite reports it, so the DAW compensates automatically.
