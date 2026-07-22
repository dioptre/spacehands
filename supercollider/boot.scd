// Instrument integration — load into running SuperDirt instance.
// Run: load("/Users/andrewgrosser/Documents/spacehands/supercollider/boot.scd");

// Delete corrupt synthdef if it exists
if (File.exists(Platform.userAppSupportDir +/+ "synthdefs/dirt_convolve2.scsyndef")) {
    File.delete(Platform.userAppSupportDir +/+ "synthdefs/dirt_convolve2.scsyndef");
};

// Guard: only run once per SC session
if (~instrumentBooted != true) {
~instrumentBooted = true;

Routine({
    var instrumentRoot = "/Users/andrewgrosser/Documents/spacehands/supercollider";

    // Free any previously created instrument synths to avoid node conflicts
    if (~instrumentSynths.notNil) {
        ~instrumentSynths.do { |s| s.free };
        ~instrumentSynths = nil;
        "Freed previous instrument synths.".postln;
    };

    // Free previous OSC responders
    #[\hand, \hands_end, \instrument, \fx, \tempo,
      \mute_resp, \level_resp, \scale_resp,
      \hand_to_tidal, \state_to_tidal, \level_to_tidal,
      \hand_watchdog_update].do { |key|
        OSCdef(key).free;
    };
    if (~instrumentWatchdog.notNil) { ~instrumentWatchdog.stop; };

    // Load SynthDefs
    [
        "synthdefs/bass.scd",
        "synthdefs/pad.scd",
        "synthdefs/lead.scd",
        "synthdefs/percussion.scd"
    ].do { |path| load(instrumentRoot +/+ path) };

    // One-shot preview note for transformation sequence intro
    SynthDef(\preview_note, { |out=0, freq=440, amp=0.7, decay=0.8|
        var sig, env;
        env = EnvGen.kr(Env.perc(0.015, decay, 1, -4), doneAction: Done.freeSelf);
        sig = SinOsc.ar(freq + SinOsc.ar(freq*2.0, 0, freq*0.6*env));
        sig = sig + (Pulse.ar(freq, 0.3) * 0.25);
        sig = LPF.ar(sig * env * amp, 3200);
        sig = sig + FreeVerb.ar(sig, 0.4, 0.8, 0.3);
        Out.ar(out, Pan2.ar(sig, 0));
    }).add;

    s.sync;
    0.5.wait;

    "SynthDefs loaded:".postln;
    SynthDescLib.global.synthDescs.keys
        .select { |k| k.asString.beginsWith("instrument") }
        .do { |k| ("  " ++ k).postln };

    // Load OSC responders + create synths (stored in ~instrumentSynths)
    load(instrumentRoot +/+ "instruments.scd");

    // Load game responders (Stellar Offering)
    load(instrumentRoot +/+ "game.scd");

    "=== Instrument ready ===".postln;

}).play;
}; // end guard
