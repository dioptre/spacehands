(
SynthDef(\instrument_bass, { |out=0, freq=60, amp=0, pan=0, gate=0, cutoff=400, resonance=0.3, reverb=0.3, detune=0.02, drive=0.4|
    var sig, env, osc1, osc2, wet;
    env  = Lag.kr(amp * gate, 0.08);
    // Pause UGens when silent to save CPU
    sig  = K2A.ar(env) * (
        VarSaw.ar(freq * (1 + detune)) +
        VarSaw.ar(freq * (1 - detune))
    ) * 0.5;
    sig  = (sig * (1 + drive * 2)).tanh * 0.6;
    sig  = LPF.ar(sig, cutoff.clip(30, 18000));  // cheap LPF not MoogFF
    wet  = FreeVerb.ar(sig, reverb * env.clip(0,1), 0.8, 0.4);
    sig  = sig * (1 - reverb * 0.5) + wet;
    Out.ar(out, Pan2.ar(sig, pan));
}).add;
);
