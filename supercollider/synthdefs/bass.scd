(
SynthDef(\instrument_bass, { |out=0, freq=60, amp=0, pan=0, gate=0,
                               cutoff=400, resonance=0.3, reverb=0.3,
                               detune=0.02, drive=0.4|
    var sig, env, osc1, osc2, vib, wet;
    vib  = SinOsc.kr(3.2 + LFNoise1.kr(0.3) * 0.8, 0, 0.008);
    env  = Lag.kr(amp * gate, 0.08);
    osc1 = VarSaw.ar(freq * (1 + detune + vib));
    osc2 = VarSaw.ar(freq * (1 - detune + vib * 0.7));
    sig  = (osc1 + osc2) * 0.5;
    sig  = (sig * (1 + drive * 2)).tanh * 0.6;
    sig  = MoogFF.ar(sig, cutoff.clip(30, 18000), resonance.clip(0, 3.5));
    sig  = sig * env;
    wet  = FreeVerb.ar(sig, reverb, 0.85, 0.4);
    sig  = sig * (1 - reverb * 0.5) + wet * reverb;
    Out.ar(out, Pan2.ar(sig, pan));
}).add;
);
