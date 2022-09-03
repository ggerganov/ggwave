## ggwave-to-file

Output a generated waveform to an uncompressed WAV file.

```
Usage: ./bin/ggwave-to-file [-vN] [-sN] [-pN] [-lN] [-d]
    -vN - output volume, N in (0, 100], (default: 50)
    -sN - output sample rate, N in [6000, 96000], (default: 48000)
    -pN - select the transmission protocol id (default: 1)
    -lN - fixed payload length of size N, N in [1, 16]
    -d  - use Direct Sequence Spread (DSS)

    Available protocols:
      0  - Normal
      1  - Fast
      2  - Fastest
      3  - [U] Normal
      4  - [U] Fast
      5  - [U] Fastest
      6  - [DT] Normal
      7  - [DT] Fast
      8  - [DT] Fastest
      9  - [MT] Normal
      10 - [MT] Fast
      11 - [MT] Fastest
```

### Examples

- Generate waveform with default parameters

  ```bash
  echo "Hello world!" | ./bin/ggwave-to-file > example.wav
  ```

- Generate waveform at 24 kHz sample rate

  ```bash
  echo "Hello world!" | ./bin/ggwave-to-file -s24000 > example.wav
  ```

- Generate ultrasound waveform using the `[U] Fast` protocol

  ```bash
  echo "Hello world!" | ./bin/ggwave-to-file -p4 > example.wav
  ```

- Use fixed-length encoding (i.e. no sound markers)

  ```bash
  echo "Hello world!" | ./bin/ggwave-to-file -l12 > example.wav
  ```

- Use DSS when encoding the text

  ```bash
  echo "aaaaaaaa" | ./bin/ggwave-to-file -l8 -d > example.wav
  ```

- Play the generated waveform directly through the speakers

  ```bash
  echo "Hello world!" | ./bin/ggwave-to-file | play --ignore-length -t wav -
  ```

## HTTP service

Based on this tool, there is an HTTP service available on the following link:

https://ggwave-to-file.ggerganov.com/

You can use it to query audio waveforms by specifying the text message as a GET parameter to the HTTP request. Here are a few examples:

### terminal

```bash
# audible example
curl -sS 'https://ggwave-to-file.ggerganov.com/?m=Hello%20world!' --output hello.wav

# ultrasound example
curl -sS 'https://ggwave-to-file.ggerganov.com/?m=Hello%20world!&p=4' --output hello.wav
```

### browser

- Audible example

  https://ggwave-to-file.ggerganov.com/?m=Hello%20world%21

- Ultrasound example

  https://ggwave-to-file.ggerganov.com/?m=Hello%20world%21&p=4


### python

```python
from typing import Dict, Union
import requests
import wave

def ggwave(message: str,
           file: str,
           protocolId: int = 1,
           sampleRate: float = 48000,
           volume: int = 50,
           payloadLength: int = -1,
           useDSS: int = 0) -> None:

    url = 'https://ggwave-to-file.ggerganov.com/'

    params: Dict[str, Union[str, int, float] = {
        'm': message,        # message to encode
        'p': protocolId,     # transmission protocol to use
        's': sampleRate,     # output sample rate
        'v': volume,         # output volume
        'l': payloadLength,  # if positive - use fixed-length encoding
        'd': useDSS,         # if positive - use DSS
    }

    response = requests.get(url, params=params)

    if response == '' or b'Usage: ggwave-to-file' in response.content:
        raise SyntaxError('Request failed')

    with wave.open(file, 'wb') as f:
        f.setnchannels(1)
        f.setframerate(sampleRate)
        f.setsampwidth(2)
        f.writeframes(response.context)

```

...

```python

# query waveform from server and write to file
ggwave("Hello world!", "hello_world.wav")


```
