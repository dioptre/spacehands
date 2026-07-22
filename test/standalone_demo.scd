// =====================================================================
// STANDALONE LIVE AUDIO DEMO: TIDAL SILENT TRACK -> 3S DELAYED SUPERDIRT
// =====================================================================

(
Routine({
    var s = Server.default;
    var delayTime = 3.0;
    var netDirt = NetAddr("127.0.0.1", 57120);

    "==================================================================".postln;
    "STARTING TIDAL -> SUPERDIRT 3.0s DELAY DEMO".postln;
    "==================================================================".postln;

    if (s.serverRunning.not) {
        "Booting SuperCollider audio engine (scsynth)...".postln;
        s.boot;
        s.bootSync;
    };

    s.volume.unmute;
    s.volume.volume = 0;

    if (~dirt.isNil) {
        "Loading SuperDirt with 'arpy' sample bank...".postln;
        ~dirt = SuperDirt(2, s);
        ~dirt.loadSoundFiles(Platform.userAppSupportDir +/+ "downloaded-quarks/Dirt-Samples/arpy");
        s.sync;

        try {
            ~dirt.start(57120, 0 ! 12);
        } { |err|
            "SuperDirt port 57120 already listening.".postln;
        };
        s.sync;
    };

    // Wait 3.0 seconds to guarantee spacehands/supercollider/boot.scd finishes
    3.0.wait;

    // Chain our receiveAction
    ~prevAction = ~dirt.receiveAction;
    ~dirt.receiveAction = { |event|
        var sName = (event[\s] ? "").asSymbol;
        var gainVal = (event[\gain] ? 1.0).asFloat;
        
        // Execute previous handler if present
        if (~prevAction.notNil) { ~prevAction.value(event); };

        // Intercept silent notes (gain == 0)
        if (sName == \arpy && gainVal == 0.0) {
            var nVal = event[\n] ? (event[\note] ? 0);
            var noteInt = if (nVal.isNumber) { nVal.asInteger } { 0 };
            var tReceived = Main.elapsedTime;

            (" >>> [PROCESS 2 INTERCEPTED] Silent note received: " ++ noteInt ++ " at " ++ tReceived.round(0.01) ++ "s").postln;
            (" >>> Scheduling delayed SuperDirt playback in " ++ delayTime ++ " seconds...").postln;

            SystemClock.sched(delayTime, {
                var tPlay = Main.elapsedTime;
                var elapsed = (tPlay - tReceived).round(0.01);
                (" >>> [AUDIO PLAYING NOW (+" ++ elapsed ++ "s)] Note " ++ noteInt ++ " ('arpy') played in SuperDirt!").postln;
                
                // Play out loud in SuperDirt
                netDirt.sendMsg("/dirt/play", "s", "arpy", "n", noteInt, "gain", 3.0, "sustain", 1.0);
                nil;
            });
        };
    };

    "=== OSC DELAY RESPONDER IS ACTIVE ===".postln;

    // Loop sending "c a f e" (notes 0, 9, 5, 4) with gain 0
    loop {
        [0, 9, 5, 4].do { |noteNum, idx|
            0.8.wait;
            ("------------------------------------------------------------------").postln;
            ("Process 1 (Tidal Track): Sending silent note " ++ noteNum ++ " (# s \"arpy\" # gain 0)...").postln;
            netDirt.sendMsg("/dirt/play", "s", "arpy", "n", noteNum, "gain", 0.0);
        };
        2.0.wait;
    };
}).play;
);
