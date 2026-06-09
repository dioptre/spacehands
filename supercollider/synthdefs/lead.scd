(
SynthDef(\instrument_lead, { |out=0, freq=440, amp=0, pan=0, gate=0, cutoff=4000, resonance=0.35, reverb=0.25, modIndex=2, modRatio=2|
    var sig, env, wet;
    env = Lag.kr(amp * gate, 0.03);
    sig = SinOsc.ar(freq + SinOsc.ar(freq * modRatio, 0, freq * modIndex * env));
    sig = LPF.ar(sig, cutoff.clip(200, 18000));
    sig = sig * env;
    wet = FreeVerb.ar(sig, reverb * env.clip(0,1), 0.75, 0.35);
    sig = sig * (1 - reverb * 0.4) + wet;
    Out.ar(out, Pan2.ar(sig, pan));
}).add;
);
