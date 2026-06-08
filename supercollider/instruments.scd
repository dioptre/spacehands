// OSC responder — depth-bucket instrument selection + gesture expression.
// X=tempo  Y=pitch  Z=instrument bucket (300mm per bucket)
// Gestures: OPEN=play FIST=freeze POINT=solo PINCH=quantize SPREAD=arp
//           THUMBS_UP=octave+ THUMBS_DOWN=octave-  Z-vel=pitchbend

var bucketSize, numBuckets, zvelThresh, arpIntervals, scales;
var currentScale, synths, activeLevel, bucketActive, frozenFreq;
var bucketFrozen, soloBucket, arpRoutines, muted;
var fadeTimes, absenceFrames, targetFps;
var quantizeToScale, stopArp, startArp, applyLevel;
var tidalCtrl, lastHandTime, watchdog;

bucketSize   = 300;
numBuckets   = 5;
zvelThresh   = 30.0;
arpIntervals = #[0, 4, 7, 12];
scales = Dictionary.newFrom([
    \pentatonic, #[0,2,4,7,9],
    \minor,      #[0,2,3,5,7,8,10],
    \major,      #[0,2,4,5,7,9,11],
    \dorian,     #[0,2,3,5,7,9,10]
]);
currentScale  = \pentatonic;
activeLevel   = 0;
bucketActive  = Array.fill(numBuckets, { false });
frozenFreq    = Array.fill(numBuckets, { 220.0 });
bucketFrozen  = Array.fill(numBuckets, { false });
soloBucket    = -1;
arpRoutines   = Array.fill(numBuckets, { nil });
muted         = false;
fadeTimes     = #[0.8, 2.0, 0.4, 0.15, 0.5];
absenceFrames = Array.fill(numBuckets, { -1 });
targetFps     = 30;

synths = Array.fill(numBuckets, { nil });

// Create synths once server confirms SynthDefs are loaded
// Create synths now (server is already booted — called from boot.scd Routine after s.sync)
synths = [
    Synth(\instrument_bass, [\amp, 0, \gate, 0]),
    Synth(\instrument_pad,  [\amp, 0, \gate, 0]),
    Synth(\instrument_lead, [\amp, 0, \gate, 0]),
    Synth(\instrument_perc, [\amp, 0]),
    Synth(\instrument_lead, [\amp, 0, \gate, 0, \pan, 0.3])
];
~instrumentSynths = synths; // store globally so boot.scd can free on reload
applyLevel.();
"Instrument synths created.".postln;

quantizeToScale = { |midi, scaleKey|
    var intervals, root, oct, deg, closest;
    intervals = scales[scaleKey] ?? scales[\pentatonic];
    root = 60;
    oct  = ((midi - root) / 12).floor * 12 + root;
    deg  = midi - oct;
    closest = intervals.minIndex { |i| (i - deg).abs };
    oct + intervals[closest]
};

stopArp = { |bucket|
    if (arpRoutines[bucket].notNil) {
        arpRoutines[bucket].stop;
        arpRoutines[bucket] = nil;
    };
};

startArp = { |bucket, baseMidi, vel|
    var synth, idx;
    stopArp.(bucket);
    synth = synths[bucket];
    idx = 0;
    arpRoutines[bucket] = Routine({
        loop {
            var semi, freq;
            semi = arpIntervals[idx % arpIntervals.size];
            freq = (baseMidi + semi).midicps;
            synth.set(\freq, freq, \amp, vel * 0.8, \gate, 1);
            (0.25 / TempoClock.default.tempo).wait;
            idx = idx + 1;
        }
    }).play(TempoClock.default);
};

applyLevel = {
    numBuckets.do { |i|
        if (i > activeLevel) {
            synths[i].set(\gate, 0, \amp, 0);
            stopArp.(i);
        };
    };
};

OSCdef(\hand, { |msg|
    var handId, x, y, zMm, zVel, gesture, bucket, rawMidi, bpm, synth, vel;
    var midi, freq, bend;
    handId  = msg[1].asInteger;
    x       = msg[2].asFloat;
    y       = msg[3].asFloat;
    zMm     = msg[4].asFloat;
    zVel    = msg[5].asFloat;
    gesture = msg[6].asInteger;
    bucket  = (zMm / bucketSize).floor.asInteger.clip(0, numBuckets - 1);
    rawMidi = (36 + ((1 - y) * 48)).round.asInteger.clip(21, 108);
    bpm     = 60.0 * (2.0 ** (x * 2.0));
    TempoClock.default.tempo = bpm / 60.0;
    if ((bucket > activeLevel) || muted) { ^nil };
    synth = synths[bucket];
    if (synth.isNil) { ^nil };
    vel   = 0.7;
    case
    { gesture == 2 } {
        stopArp.(bucket);
        bucketFrozen[bucket] = true;
    }
    { gesture == 3 } {
        soloBucket = bucket;
        numBuckets.do { |i|
            if (i != bucket) { synths[i].set(\gate, 0, \amp, 0); stopArp.(i); };
        };
        midi = quantizeToScale.(rawMidi, currentScale);
        synth.set(\freq, midi.midicps, \amp, vel, \gate, 1);
        frozenFreq[bucket]   = midi.midicps;
        bucketFrozen[bucket] = false;
    }
    { gesture == 4 } {
        stopArp.(bucket);
        bucketFrozen[bucket] = false;
        soloBucket = -1;
        midi = quantizeToScale.(rawMidi, currentScale);
        synth.set(\freq, midi.midicps, \amp, vel, \gate, 1, \pan, (x * 2 - 1) * 0.5);
        frozenFreq[bucket] = midi.midicps;
    }
    { gesture == 5 } {
        if ((soloBucket != -1) && (soloBucket != bucket)) { ^nil };
        midi = quantizeToScale.(rawMidi, currentScale);
        startArp.(bucket, midi, vel);
        bucketFrozen[bucket] = false;
    }
    { gesture == 6 } {
        stopArp.(bucket);
        bucketFrozen[bucket] = false;
        if ((soloBucket != -1) && (soloBucket != bucket)) { ^nil };
        midi = rawMidi + 12;
        synth.set(\freq, midi.midicps, \amp, vel * 1.1, \gate, 1,
                  \pan, (x * 2 - 1) * 0.5, \cutoff, 6000);
        frozenFreq[bucket] = midi.midicps;
    }
    { gesture == 7 } {
        stopArp.(bucket);
        bucketFrozen[bucket] = false;
        if ((soloBucket != -1) && (soloBucket != bucket)) { ^nil };
        midi = (rawMidi - 12).max(21);
        synth.set(\freq, midi.midicps, \amp, vel, \gate, 1,
                  \pan, (x * 2 - 1) * 0.5, \cutoff, 300);
        frozenFreq[bucket] = midi.midicps;
    }
    { true } {
        if (bucketFrozen[bucket]) { ^nil };
        if ((soloBucket != -1) && (soloBucket != bucket)) { ^nil };
        stopArp.(bucket);
        freq = rawMidi.midicps;
        bend = if (zVel < zVel.neg) {
            if (zVel.abs > zvelThresh) { (zVel / 200.0).clip(-2, 0) } { 0 }
        } {
            if (zVel > zvelThresh) { (zVel / 200.0).clip(0, 2) } { 0 }
        };
        freq = freq * (2.0 ** (bend / 12.0));
        synth.set(\freq, freq, \amp, vel, \gate, 1,
                  \pan,    (x * 2 - 1) * 0.5,
                  \cutoff, (400 + (1 - y) * 3600).clip(80, 16000),
                  \reverb, (1 - zMm / 2000.0).clip(0, 0.8)); // closer = more reverb
        frozenFreq[bucket]   = freq;
        bucketFrozen[bucket] = false;
    };
    bucketActive[bucket] = true;
}, '/hand');

OSCdef(\hands_end, { |msg|
    var reported;
    reported = msg[1..].asArray;
    numBuckets.do { |i|
        if (reported.includes(i)) {
            absenceFrames[i] = -1;
        } {
            if (bucketFrozen[i].not && bucketActive[i]) {
                if (absenceFrames[i] == -1) {
                    synths[i].set(\gate, 0);
                    stopArp.(i);
                    if (soloBucket == i) { soloBucket = -1 };
                };
                absenceFrames[i] = absenceFrames[i] + 1;
                if (absenceFrames[i] > (fadeTimes[i] * targetFps).ceil) {
                    synths[i].set(\amp, 0);
                    absenceFrames[i] = -1;
                    bucketActive[i]  = false;
                };
            };
        };
    };
    bucketActive = Array.fill(numBuckets, { false });
}, '/hands_end');

OSCdef(\instrument, { |msg|
    activeLevel = msg[1].asInteger.clip(0, numBuckets - 1);
    applyLevel.();
    ("Instrument level: " ++ activeLevel).postln;
}, '/instrument');

OSCdef(\fx, { |msg|
    synths.do { |s| s.set(\reverb, msg[1].asFloat) };
}, '/fx');

OSCdef(\tempo, { |msg|
    TempoClock.default.tempo = msg[1].asFloat.clip(40, 300) / 60.0;
}, '/tempo');

OSCdef(\mute_resp, { |msg|
    muted = (msg[1].asInteger == 1);
    if (muted) {
        synths.do { |s| s.set(\gate, 0, \amp, 0) };
        numBuckets.do { |i| stopArp.(i) };
        bucketFrozen = Array.fill(numBuckets, { false });
    };
}, '/mute');

OSCdef(\level_resp, { |msg|
    var level;
    level = msg[1].asInteger;
    activeLevel = level.clip(0, numBuckets - 1);
    applyLevel.();
    Synth(\instrument_perc, [\freq, 600 + (level * 150), \amp, 0.7, \decay, 1.2]);
    ("=== LEVEL UP " ++ level ++ " ===").postln;
}, '/level');

OSCdef(\scale_resp, { |msg|
    var name;
    name = msg[1].asString.asSymbol;
    if (scales.includesKey(name)) {
        currentScale = name;
        ("Scale: " ++ name).postln;
    };
}, '/scale');

// --- Tidal /ctrl bridge ---
// Forwards hand position as Tidal control values so existing Tidal patterns
// can be modulated by hands. Use in Tidal: d1 $ s "bd" # speed (cF 1 "hand0_y")
tidalCtrl = NetAddr.new("127.0.0.1", 6010);

OSCdef(\hand_to_tidal, { |msg|
    var handId, x, y, z;
    handId = msg[1].asInteger;
    x = msg[2].asFloat;
    y = msg[3].asFloat;
    z = (msg[4].asFloat / 2000.0).clip(0, 1); // normalise mm→0-1
    if (handId < 4) {
        tidalCtrl.sendMsg("/ctrl", "hand" ++ handId ++ "_x", x);
        tidalCtrl.sendMsg("/ctrl", "hand" ++ handId ++ "_y", y);
        tidalCtrl.sendMsg("/ctrl", "hand" ++ handId ++ "_z", z);
    };
}, '/hand');

OSCdef(\state_to_tidal, { |msg|
    // Forward progress and level to Tidal on each /hands_end
    // (sent at 30Hz — use cF with smoothing in Tidal patterns)
}, '/hands_end');

// Also respond to /level to update Tidal
OSCdef(\level_to_tidal, { |msg|
    tidalCtrl.sendMsg("/ctrl", "instrument_level", msg[1].asFloat);
}, '/level');

// Watchdog — gate off all synths if no /hand message for 500ms
lastHandTime = Main.elapsedTime;

OSCdef(\hand_watchdog_update, { |msg|
    lastHandTime = Main.elapsedTime;
}, '/hand');

watchdog = ~instrumentWatchdog = Task({
    loop {
        0.1.wait;
        if ((Main.elapsedTime - lastHandTime) > 0.5) {
            numBuckets.do { |i|
                if (synths[i].notNil && bucketActive[i]) {
                    synths[i].set(\gate, 0);
                    bucketActive[i] = false;
                    absenceFrames[i] = -1;
                };
            };
        };
    };
}).play;

"OSC responders active — X=tempo Y=pitch Z=bucket".postln;
"Tidal /ctrl bridge active on port 6010".postln;
"  Use: d1 $ s \"bd\" # speed (cF 1 \"hand0_y\")".postln;
