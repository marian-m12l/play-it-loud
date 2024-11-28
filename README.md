# Play it loud!

A GB rom and a set of firmwares for Raspberry Pi Pico W to stream and play audio on the original Game Boy (DMG) or Game Boy Color (CGB).

The audio playback is largely based on jbshelton's [GBAudioPlayer](https://github.com/jbshelton/GBAudioPlayer).
The bluetooth A2DP sink is largely based on joba-1's [PicoW_A2DP](https://github.com/joba-1/PicoW_A2DP), itself based on pico-sdk and btstack example.


# Build instructions

Requires pico-sdk (tested with 2.0.0) and rgbds (tested with 0.8.0).

```
mkdir build
cd build
cmake -DPICO_SDK_PATH=/path/to/pico-sdk ..
make
```
