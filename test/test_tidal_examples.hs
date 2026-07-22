{-
========================================================================
TIDALCYCLES EXAMPLES FOR 3-SECOND DELAYED SUPERDIRT PLAYBACK
========================================================================

How it works:
- Process 1 (TidalCycles): Evaluates a pattern sent to OSC or MIDI with # gain 0 (silent).
- Process 2 (SuperCollider / Python): Intercepts the note ('c', 'a', 'f', 'e'), waits 3 seconds, and plays the note in SuperDirt with full gain!

------------------------------------------------------------------------
OPTION 1: OSC-based Delay (Recommended)
------------------------------------------------------------------------
Load in SuperCollider.app first:
  load("/Users/andrewgrosser/Documents/spacehands/test/test_osc_delay.scd");

Then run in TidalCycles / GHCI:
-}

-- 1. Pass track with 0 gain (silent) to sound "arpy":
d1 $ n "c a f e" # s "arpy" # gain 0

-- 2. OR use custom target instrument sound "delaynote":
d1 $ n "c a f e" # s "delaynote"

-- Stop Process 1:
hush

{-
------------------------------------------------------------------------
OPTION 2: MIDI-based Delay
------------------------------------------------------------------------
Load in SuperCollider.app first:
  load("/Users/andrewgrosser/Documents/spacehands/test/test_midi_delay.scd");

Then run in TidalCycles / GHCI:
-}

-- Send silent notes to SuperDirt MIDI device target:
d1 $ n "c a f e" # s "midi" # gain 0

-- Stop Process 1:
hush
