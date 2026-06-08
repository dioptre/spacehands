// STELLAR OFFERING — game OSC responders

var orbSynths, climaxRoutine, orbActive;
var orbFreqs, orbSynthNames, orbReverbs, orbAmps;

orbActive    = Array.fill(4, { false });
orbFreqs     = [40, 55, 67, 72].collect { |m| m.midicps };
orbSynthNames= [\instrument_bass, \instrument_pad, \instrument_lead, \instrument_lead];
orbReverbs   = [0.4, 0.7, 0.5, 0.3];
orbAmps      = [0.8, 0.6, 0.5, 0.4];

orbSynths = Array.fill(4, { |i|
    Synth(orbSynthNames[i], [
        \freq, orbFreqs[i], \amp, 0, \gate, 0, \reverb, orbReverbs[i]
    ])
});

OSCdef(\orb_touch, { |msg|
    var orbId, held;
    orbId = msg[1].asInteger;
    held  = msg[2].asInteger;
    if ((orbId >= 0) && (orbId < 4)) {
        orbActive[orbId] = (held == 1);
        if (held == 1) {
            orbSynths[orbId].set(\amp, orbAmps[orbId], \gate, 1, \freq, orbFreqs[orbId]);
            ("Orb " ++ orbId ++ " held").postln;
        } {
            orbSynths[orbId].set(\gate, 0);
            ("Orb " ++ orbId ++ " released").postln;
        };
    };
}, '/orb');

OSCdef(\orb_climax, { |msg|
    "=== WORMHOLE OPENS — DEMIGOD ARRIVES ===".postln;
    if (climaxRoutine.notNil) { climaxRoutine.stop };
    climaxRoutine = Routine({
        var drones;
        orbSynths.do { |s, i| s.set(\amp, orbAmps[i] * 1.5, \gate, 1, \reverb, 0.95) };
        drones = [
            Synth(\instrument_bass, [\freq, 40.midicps, \amp, 1.0, \gate, 1, \reverb, 0.9]),
            Synth(\instrument_pad,  [\freq, 52.midicps, \amp, 0.8, \gate, 1, \reverb, 0.95]),
            Synth(\instrument_lead, [\freq, 64.midicps, \amp, 0.7, \gate, 1, \reverb, 0.9]),
            Synth(\instrument_lead, [\freq, 71.midicps, \amp, 0.5, \gate, 1, \reverb, 0.95]),
        ];
        3.wait;
        orbSynths.do { |s| s.set(\gate, 0) };
        drones.do    { |s| s.set(\gate, 0) };
        2.wait;
        if (~dirt.notNil) {
            var sd;
            sd = NetAddr.new("127.0.0.1", 57120);
            sd.sendMsg("/dirt/play",
                \cps, 0.25, \cycle, 0, \delta, 8.0, \orbit, 0,
                \s, "cosmicg", \n, rrand(0, 14),
                \verbwet, 0.99, \verbtime, 12.0, \verbdamp, 0.05,
                \gain, 1.8, \speed, 0.4
            );
        };
        8.wait;
        orbSynths.do { |s| s.set(\amp, 0, \gate, 0) };
        orbActive = Array.fill(4, { false });
        "=== RITUAL COMPLETE — WORMHOLE CLOSED ===".postln;
    }).play;
}, '/orb_climax');

"Game OSC responders active — Stellar Offering ready".postln;
