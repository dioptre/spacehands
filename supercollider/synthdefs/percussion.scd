(
SynthDef(\instrument_perc, { |out=0, freq=200, amp=0, pan=0, gate=0, decay=0.8, tone=0.5, noise=0.2, reverb=0.3|
    var sig, env, wet;
    env = Lag.kr(amp * gate, 0.005);
    sig = SinOsc.ar(freq * tone) * env;
    sig = sig + (WhiteNoise.ar(noise * 0.1) * EnvGen.kr(Env.perc(0.001, 0.1)));
    wet = FreeVerb.ar(sig, reverb * env.clip(0,1), 0.7, 0.3);
    sig = sig * (1 - reverb * 0.4) + wet;
    Out.ar(out, Pan2.ar(sig, pan));
}).add;

SynthDef(\instrument_kick, { |out=0, freq=55, amp=0.9, pan=0, decay=0.5|
    var sig, env;
    env = EnvGen.kr(Env.perc(0.001, decay), doneAction: Done.freeSelf);
    sig = SinOsc.ar(freq + EnvGen.kr(Env.perc(0.001, 0.08)) * freq * 3) * env * amp;
    Out.ar(out, Pan2.ar(sig * 2.tanh * 0.7, pan));
}).add;
);
