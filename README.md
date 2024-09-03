# AzAudio
C/C++ cross-platform audio library for games

### Development Phase
Move fast and break things! The solo developer is currently exploring this problem space, so expect things to change quickly and profoundly. As of now I don't recommend trying to use this library for the above stated reasons (unless you want to contribute to it, of course).

### Broad-Strokes Goal
- Provide high-quality 3D audio for games with more control surfaces than other free libraries.

### Desired Features
- Simple spacial attenuation for speakers
- Advanced filtering for headphones (binaural modeling)
- Realtime control over effects and samplers
- The ability to trigger events at a more granular precision than per-update (scheduling)
- DSP optimized for realtime games (possibly at the cost of some quality, also preferring lower latency if feasible)
- Plugin chain latency compensation with regards to video latency (adding delays where necessary to match timing of busses to visual outputs)

[Documentation (Currently a Stub)](https://singularityazure.github.io/AzAudio)
