# AzureAudio
C++ cross-platform audio library for games

This is meant to be a simple, extensible, easy-to-use API that you can include in your projects.
Feel free to write your own modifications for any missing features.
    I do ask that you send me your modifications so they may be added to the core library,
    though you are in no way required to do so.

I intend to write the boilerplate code C-style, while the high-level abstractions will all be C++.
This is so, should the desire to write wrappers for other languages arise, the majority of work will be portable.

A secondary goal will be to implement audio file I/O, but for now it will take raw PCM buffers.

Dependencies:
    Using PortAudio for hardware and OS interface
    For threading, full C++11 support will be required
