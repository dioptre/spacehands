(
SynthDef(\instrument_pad, { |out=0, freq=220, amp=0, pan=0, gate=0, cutoff=2000, resonance=0.15, reverb=0.5, chorDepth=0.008, chorRate=0.25|
    var sig, env, wet;
    env = Lag.kr(amp * gate, 0.2);
    sig = Mix(Array.fill(3, { |i|
        Saw.ar(freq * (1 + (i-1) * chorDepth * 0.5))
    })) * 0.33;
    sig = LPF.ar(sig, cutoff.clip(100, 18000));
    sig = sig * env;
    wet = FreeVerb.ar(sig, reverb * env.clip(0,1), 0.9, 0.4);
    sig = sig * (1 - reverb * 0.5) + wet;
    Out.ar(out, Pan2.ar(sig, pan));
}).add;
);
