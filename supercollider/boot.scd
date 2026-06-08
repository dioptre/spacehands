// Instrument integration — load into running SuperDirt instance.
// Run: load("/Users/andrewgrosser/Documents/ag/instrument/supercollider/boot.scd");

Routine({
    var instrumentRoot = thisProcess.nowExecutingPath.dirname;

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

    s.sync;
    0.5.wait;

    "SynthDefs loaded:".postln;
    SynthDescLib.global.synthDescs.keys
        .select { |k| k.asString.beginsWith("instrument") }
        .do { |k| ("  " ++ k).postln };

    // Load OSC responders + create synths (stored in ~instrumentSynths)
    load(instrumentRoot +/+ "instruments.scd");

    "=== Instrument ready ===".postln;

}).play;
