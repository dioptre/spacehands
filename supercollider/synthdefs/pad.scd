(
SynthDef(\instrument_pad, { |out=0, freq=220, amp=0, pan=0, gate=0,
                              cutoff=2000, resonance=0.15, reverb=0.6,
                              chorDepth=0.008, chorRate=0.25|
    var sig, env, voices, vib, wet;
    vib  = SinOsc.kr(4.1 + LFNoise1.kr(0.2) * 1.0, 0, chorDepth);
    env  = Lag.kr(amp * gate, 0.2);
    voices = Array.fill(4, { |i|
        var d = (i - 1.5) * chorDepth;
        Saw.ar(freq * (1 + d + SinOsc.kr(chorRate * (1 + i * 0.13), i * 0.7) * 0.003 + vib))
    });
    sig = Mix(voices) * 0.25;
    sig = RLPF.ar(sig, (cutoff + SinOsc.kr(0.15, 0, cutoff * 0.15)).clip(100, 18000),
                  resonance.clip(0.01, 1));
    sig = sig * env;
    wet = FreeVerb.ar(sig, reverb, 0.92, 0.5);
    sig = sig * (1 - reverb * 0.6) + wet * reverb;
    Out.ar(out, Pan2.ar(sig, pan));
}).add;
);
