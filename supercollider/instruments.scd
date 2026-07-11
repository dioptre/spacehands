// OSC responder — interactive hand instrument with ring buffer of 12 notes.
// Auto-calibrates input range (min/max) for x, y, and z.
// X = speed of playback (cycle)
// Y = pitch (major pentatonic scale)
// Z = reverb room size + sustain time

(
var scale, melodyInstruments;

// 20-note pentatonic scale spanning 4 octaves
scale = [48, 50, 52, 55, 57, 60, 62, 64, 67, 69, 72, 74, 76, 79, 81, 84, 86, 89, 91, 93];

melodyInstruments = [\superpiano, \supervibe, \superhammond, \supermandolin, \supersaw, \supersquare, \supergong];

~instrumentMuted = false;

// Global calibration bounds
~xMin = ~xMin ? 0.2;
~xMax = ~xMax ? 0.8;
~yMin = ~yMin ? 0.3;
~yMax = ~yMax ? 0.7;
~zMin = ~zMin ? 400.0;
~zMax = ~zMax ? 1500.0;

// Dictionary to store active hand states
~handsState = ~handsState ? Dictionary.new;

// Free previous instrument synths and loops if they exist
~handsState.keysValuesDo { |handId, hand|
    if (hand[\routine].notNil) {
        hand[\routine].stop;
    };
};
~handsState.clear;

if (~instrumentWatchdog.notNil) { ~instrumentWatchdog.stop; };

// Play loop routine generator
~startHandLoop = { |handId|
    var hand = ~handsState[handId];
    if (hand[\routine].isNil) {
        hand[\routine] = Routine({
            var waitTime;
            loop {
                var noteIdx = hand[\readIndex];
                var note = hand[\ringBuffer][noteIdx];
                var inst = hand[\instrument];
                var orbit = (handId % 11) + 1; // Map to orbit 1-11, leaving 0 reserved
                var x = hand[\x];
                var y = hand[\y];
                var z = hand[\z];
                var distFactor = hand[\distFactor] ? 1.0;
                
                if ((~dirt.notNil) && (~instrumentMuted != true)) {
                    var sd = NetAddr.new("127.0.0.1", 57120);
                    sd.sendMsg("/dirt/play",
                        \cps, 0.5, \cycle, 0, \delta, 0.5, \orbit, orbit,
                        \s, inst, \note, note - 60,
                        \gain, 1.2,
                        \sustain, z.linexp(0, 1, 0.1, 2.5),
                        \room, z.linlin(0, 1, 0.1, 0.9),
                        \verbwet, z.linlin(0, 1, 0.1, 0.9),
                        \pan, x.linlin(0, 1, -0.8, 0.8),
                        \cutoff, x.linexp(0, 1, 150, 15000),
                        \resonance, x.linlin(0, 1, 0.0, 0.8)
                    );
                };
                
                hand[\readIndex] = (noteIdx + 1) % 12;
                // Speed (tempo) is controlled by distFactor (closer = faster playback)
                waitTime = distFactor.linexp(0, 1, 0.08, 1.2);
                waitTime.wait;
            };
        }).play;
    };
};

OSCdef(\hand, { |msg|
    var handId, xRaw, yRaw, zRaw, zVel, gesture, distFactor;
    var xNorm, yNorm, zNorm;
    var noteIndex, pitch;
    var now, hand;

    handId     = msg[1].asInteger;
    xRaw       = msg[2].asFloat;
    yRaw       = msg[3].asFloat;
    zRaw       = msg[4].asFloat;
    zVel       = msg[5].asFloat;
    gesture    = msg[6].asInteger;
    distFactor = msg[7].asFloat;

    now = Main.elapsedTime;

    // Dynamically adjust auto-calibration bounds
    if (xRaw < ~xMin) { ~xMin = xRaw };
    if (xRaw > ~xMax) { ~xMax = xRaw };
    if (yRaw < ~yMin) { ~yMin = yRaw };
    if (yRaw > ~yMax) { ~yMax = yRaw };
    if (zRaw < ~zMin) { ~zMin = zRaw };
    if (zRaw > ~zMax) { ~zMax = zRaw };

    // Normalize coordinates
    xNorm = ((xRaw - ~xMin) / (~xMax - ~xMin + 0.001)).clip(0.0, 1.0);
    yNorm = ((yRaw - ~yMin) / (~yMax - ~yMin + 0.001)).clip(0.0, 1.0);
    zNorm = ((zRaw - ~zMin) / (~zMax - ~zMin + 0.001)).clip(0.0, 1.0);

    // Map Y to pentatonic scale pitch
    noteIndex = (yNorm * (scale.size - 0.01)).floor.asInteger;
    pitch = scale[noteIndex];

    // Find or initialize hand state
    if (~handsState.includesKey(handId).not) {
        ~handsState[handId] = Dictionary.newFrom([
            \instrument, melodyInstruments.choose,
            \ringBuffer, Array.fill(12, { pitch }), // initialize ring buffer with first pitch
            \writeIndex, 0,
            \readIndex, 0,
            \x, xNorm,
            \y, yNorm,
            \z, zNorm,
            \distFactor, distFactor,
            \lastNote, nil,
            \lastWriteTime, 0,
            \lastSeenTime, now,
            \routine, nil
        ]);
        if (~instrumentMuted != true) {
            ~startHandLoop.(handId);
        };
        ("Hand " ++ handId ++ " detected. Instrument: " ++ ~handsState[handId][\instrument]).postln;
    };

    hand = ~handsState[handId];
    hand[\lastSeenTime] = now;
    hand[\x] = xNorm;
    hand[\y] = yNorm;
    hand[\z] = zNorm;
    hand[\distFactor] = distFactor;

    // Push note to ring buffer if it changes (with debounce)
    if ((pitch != hand[\lastNote]) && ((now - hand[\lastWriteTime]) > 0.15)) {
        var wIdx = hand[\writeIndex];
        hand[\ringBuffer][wIdx] = pitch;
        hand[\writeIndex] = (wIdx + 1) % 12;
        hand[\lastNote] = pitch;
        hand[\lastWriteTime] = now;

        // Trigger note immediately on movement for high responsiveness
        if ((~dirt.notNil) && (~instrumentMuted != true)) {
            var sd = NetAddr.new("127.0.0.1", 57120);
            var orbit = (handId % 11) + 1;
            sd.sendMsg("/dirt/play",
                \cps, 0.5, \cycle, 0, \delta, 0.5, \orbit, orbit,
                \s, hand[\instrument], \note, pitch - 60,
                \gain, 1.2,
                \sustain, zNorm.linexp(0, 1, 0.1, 2.5),
                \room, zNorm.linlin(0, 1, 0.1, 0.9),
                \verbwet, zNorm.linlin(0, 1, 0.1, 0.9),
                \pan, xNorm.linlin(0, 1, -0.8, 0.8),
                \cutoff, xNorm.linexp(0, 1, 150, 15000),
                \resonance, xNorm.linlin(0, 1, 0.0, 0.8)
            );
        };
    };
}, '/hand');

OSCdef(\mute_resp, { |msg|
    var val = msg[1].asInteger;
    var wasMuted = ~instrumentMuted;
    ~instrumentMuted = (val == 1);
    if (~instrumentMuted == true) {
        ~handsState.keysValuesDo { |handId, hand|
            if (hand[\routine].notNil) {
                hand[\routine].stop;
                hand[\routine] = nil;
            };
        };
    } {
        if (wasMuted == true) {
            ~handsState.keysValuesDo { |handId, hand|
                if (hand[\routine].isNil) {
                    ~startHandLoop.(handId);
                };
            };
        };
    };
    ("Instrument mute state: " ++ ~instrumentMuted).postln;
}, '/mute');

// Watchdog — check for stale hands and stop their playback routines
~instrumentWatchdog = Task({
    loop {
        var now = Main.elapsedTime;
        ~handsState.keysValuesDo { |handId, hand|
            if ((now - hand[\lastSeenTime]) > 1.5) {
                ("Hand " ++ handId ++ " timed-out. Releasing slot.").postln;
                if (hand[\routine].notNil) {
                    hand[\routine].stop;
                };
                ~handsState.removeAt(handId);
            };
        };
        0.1.wait;
    };
}).play;

"=== New Hand Instrument Responders Active ===".postln;
)
