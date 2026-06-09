(
SynthDef(\instrument_perc, { |out=0, freq=200, amp=0, pan=0, gate=0,
                               decay=0.8, tone=0.5, noise=0.2, reverb=0.4|
    var sig, env, metal, noise_sig, wet;
    env       = Lag.kr(amp * gate, 0.005);
    metal     = Klank.ar(`[[1, 2.756, 5.179, 7.23], [1, 0.6, 0.4, 0.2],
                           [decay, decay*0.6, decay*0.4, decay*0.2]],
                          Impulse.ar(0), freq * tone);
    noise_sig = WhiteNoise.ar(noise) * EnvGen.kr(Env.perc(0.001, 0.15));
    sig = (metal + noise_sig) * env;
    wet = FreeVerb.ar(sig, reverb, 0.75, 0.35);
    sig = sig * (1 - reverb * 0.5) + wet * reverb;
    Out.ar(out, Pan2.ar(sig, pan));
}).add;

SynthDef(\instrument_kick, { |out=0, freq=55, amp=0.9, pan=0, decay=0.5|
    var sig, env, pitch_env;
    env       = EnvGen.kr(Env.perc(0.001, decay), doneAction: Done.freeSelf);
    pitch_env = EnvGen.kr(Env.perc(0.001, 0.1)) * freq * 4;
    sig = SinOsc.ar(freq + pitch_env) * env * amp;
    sig = (sig * 2.5).tanh * 0.7;
    Out.ar(out, Pan2.ar(sig, pan));
}).add;
);
