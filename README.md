## Supercollider

```
load("/Users/andrewgrosser/Documents/ag/instrument/supercollider/boot.scd");

~instrumentSynths.postln

~instrumentSynths.do{|s| s.set(\gate, 0, \amp, 0)}

Server.default.freeAll;
```