(
SynthDef(\instrument_lead, { |out=0, freq=440, amp=0, pan=0, gate=0,
                               cutoff=4000, resonance=0.35, reverb=0.35,
                               modIndex=2.5, modRatio=2|
    var sig, env, carrier, modulator, vib, tremolo, wet;
    vib     = SinOsc.kr(5.5 + LFNoise1.kr(0.4) * 1.2, 0, 0.012);
    tremolo = SinOsc.kr(6.5 + LFNoise1.kr(0.3) * 2, 0, 0.05) + 1.0;
    env       = Lag.kr(amp * gate, 0.03);
    modulator = SinOsc.ar(freq * modRatio * (1 + vib * 0.5), 0, freq * modIndex * env);
    carrier   = SinOsc.ar(freq * (1 + vib) + modulator);
    sig = carrier * 0.55 + (Pulse.ar(freq * (1 + vib * 0.8), 0.48) * 0.45);
    sig = RLPF.ar(sig, cutoff.clip(200, 18000), resonance.clip(0.01, 1));
    sig = sig * env * tremolo;
    wet = FreeVerb.ar(sig, reverb, 0.80, 0.4);
    sig = sig * (1 - reverb * 0.4) + wet * reverb;
    Out.ar(out, Pan2.ar(sig, pan));
}).add;
);
