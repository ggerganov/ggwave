## ggwave-from-file

Decode GGWave messages from an input WAV file

```
Usage: ./bin/ggwave-from-file [-lN] [-d]
    -lN - fixed payload length of size N, N in [1, 64]
    -d  - use Direct Sequence Spread (DSS)
```

### Examples

- Basic usage with auto-detection of frequency and speed:

  ```bash
  echo "Hello world" | ./bin/ggwave-to-file > example.wav
  ./bin/ggwave-from-file example.wav

  Usage: ./bin/ggwave-from-file audio.wav [-lN] [-d]
      -lN - fixed payload length of size N, N in [1, 64]
      -d  - use Direct Sequence Spread (DSS)

  [+] Number of channels: 1
  [+] Sample rate: 48000
  [+] Bits per sample: 16
  [+] Total samples: 69632
  [+] Decoding ..

  [+] Decoded message with length 11: 'Hello world'

  [+] Done
  ```

- Decoding fixed-length payload with DSS enabled:

  ```bash
  echo "Hello world" | ./bin/ggwave-to-file -l16 -d > example.wav
  ./bin/ggwave-from-file example.wav -l16 -d
  ```
