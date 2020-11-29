# ggwave

Tiny data-over-sound library.


## Details

This is a simple C++ library that allows communication of small amounts of data between air-gapped devices using sound. It implements a simple transmission protocol that can be easily integrated in various projects. The bandwidth rate is between 8-16 bytes/sec depending on the protocol parameters. Error correction codes (ECC) are used to improve demodulation robustness.

This library is used only to generate and analyze the RAW waveforms that are played and captured from your audio devices (speakers, microphones, etc.). You are free to use any audio backend (e.g. PulseAudio, ALSA, etc.) as long as you provide callbacks for queuing and dequeuing samples.

Browser demo: https://ggwave.ggerganov.com

### Modulation (Tx)

The current approach uses a multi-frequency [Frequency-Shift Keying (FSK)](https://en.wikipedia.org/wiki/Frequency-shift_keying) modulation scheme. The data to be transmitted is first split into 4-bit chunks. At each moment of time, 3 bytes are transmitted using 6 tones - one tone for each 4-bit chunk. The 6 tones are emitted in a 4.5kHz range divided in 96 equally-spaced frequencies:

| Freq, [Hz]   | Value, [bits]   | Freq, [Hz]   | Value, [bits]   | ... | Freq, [Hz]   | Value, [bits]   |
| ------------ | --------------- | ------------ | --------------- | --- | ------------ | --------------- |
| `F0 + 00*dF` | Chunk 0: `0000` | `F0 + 16*dF` | Chunk 1: `0000` | ... | `F0 + 80*dF` | Chunk 5: `0000` |
| `F0 + 01*dF` | Chunk 0: `0001` | `F0 + 17*dF` | Chunk 1: `0001` | ... | `F0 + 81*dF` | Chunk 5: `0001` |
| `F0 + 02*dF` | Chunk 0: `0010` | `F0 + 18*dF` | Chunk 1: `0010` | ... | `F0 + 82*dF` | Chunk 5: `0010` |
| ...          | ...             | ...          | ...             | ... | ...          | ...             |
| `F0 + 14*dF` | Chunk 0: `1110` | `F0 + 30*dF` | Chunk 1: `1110` | ... | `F0 + 94*dF` | Chunk 5: `1110` |
| `F0 + 15*dF` | Chunk 0: `1111` | `F0 + 31*dF` | Chunk 1: `1111` | ... | `F0 + 95*dF` | Chunk 5: `1111` |

For all protocols: `dF = 46.875 Hz`. For non-ultrasonic protocols: `F0 = 1875.000 Hz`. For ultrasonic protocols: `F0 = 15000.000 Hz`.

The original data is encoded using [Reed-Solomon error codes](https://github.com/ggerganov/ggwave/blob/master/src/reed-solomon). The number of ECC bytes is determined based on the length of the original data. The encoded data is the one being transmitted.

### Demodulation (Rx)

Beginning and ending of the transmission are marked with special sound markers. The receiver listens for these markers and records the in-between sound data. The recorded data is then Fourier transformed to obtain a frequency spectrum. The detected frequencies are decoded back to binary data in the same way they were encoded.

Reed-Solomon decoding is finally performed to obtain the original data.


## Examples

The [examples](https://github.com/ggerganov/ggwave/blob/master/examples/) folder contains several sample applications of the library:

- [ggwave-cli](https://github.com/ggerganov/ggwave/blob/master/examples/ggwave-cli) - a command line tool for sending/receiving data through sound
- [ggwave-wasm](https://github.com/ggerganov/ggwave/blob/master/examples/ggwave-wasm) - a WebAssembly module for web applications

Other projects using **ggwave** or one of its prototypes:

- [wave-gui](https://github.com/ggerganov/wave-gui) - a GUI for exploring different modulation protocols
- [wave-share](https://github.com/ggerganov/wave-share) - WebRTC file sharing with sound signaling


## Building

### Linux and Mac:

```bash
# build
git clone https://github.com/ggerganov/ggwave
cd ggwave && mkdir build && cd build
cmake ..
make

# running
./bin/wave-share-cli
```

### Emscripten:

```bash
git clone https://github.com/ggerganov/ggwave
cd ggwave
mkdir build && cd build
emcmake cmake ..
make
```

## Todo

  - [ ] Improve library interface
  - [ ] Support for non-float32 input and non-int16 output
  - [ ] Mobile app examples
