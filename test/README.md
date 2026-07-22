# TidalCycles -> Delayed SuperDirt Playback Test Suite

This directory contains test scripts and configuration files for capturing silent TidalCycles tracks (via OSC or MIDI) and triggering delayed SuperDirt sample playback (e.g. 3.0s later).

---

## 🚀 How to Run in SuperCollider.app (Recommended)

Since **SuperCollider.app** is already running, you can execute the test scripts directly inside SuperCollider:

### Option A: OSC-based Setup (Simplest & Best)

1. In **SuperCollider.app**, execute:
   ```supercollider
   load("/Users/andrewgrosser/Documents/spacehands/test/test_osc_delay.scd");
   ```
2. In **TidalCycles / GHCI**, evaluate:
   ```haskell
   d1 $ n "c a f e" # s "arpy" # gain 0
   ```
   *(Or using custom instrument name: `d1 $ n "c a f e" # s "delaynote"`)*

**Result**:
- Tidal sends notes (`c`, `a`, `f`, `e`) silently (`gain 0`).
- Process 2 intercepts each note over OSC.
- Exactly 3.0 seconds later, SuperDirt plays the corresponding note on sample `"arpy"` with full gain!

---

### Option B: MIDI-based Setup

1. In **SuperCollider.app**, execute:
   ```supercollider
   load("/Users/andrewgrosser/Documents/spacehands/test/test_midi_delay.scd");
   ```
2. In **TidalCycles / GHCI**, evaluate:
   ```haskell
   d1 $ n "c a f e" # s "midi" # gain 0
   ```

**Result**:
- Tidal sends MIDI notes to SuperDirt's `\midi` target silently.
- SuperDirt outputs MIDI `noteOn` events to CoreMIDI.
- Process 2 (`MIDIFunc.noteOn`) catches the MIDI notes, waits 3.0s, and triggers SuperDirt sample playback!

---

## ⚡ Fast Startup File (`test/boot_lean.scd`)

To avoid long sample loading times on startup (which scans GBs of samples in the main `startup.scd`), you can use:
```supercollider
load("/Users/andrewgrosser/Documents/spacehands/test/boot_lean.scd");
```
This loads **only 1 sample bank** (`arpy`) into SuperDirt for near-instant boot times (<0.1s).

---

## 📁 Summary of Test Files

- `test/test_osc_delay.scd` — OSC delay responder + local simulation test.
- `test/test_midi_delay.scd` — MIDI delay responder + local simulation test.
- `test/boot_lean.scd` — Lean SuperDirt boot file (loads only `arpy` sample bank).
- `test/virtual_midi_hub.py` — Python script for creating virtual CoreMIDI ports & forwarding.
- `test/test_tidal_examples.hs` — TidalCycles code snippets for copy-pasting into GHCI.
