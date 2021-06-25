## ggwave-to-file

Output a generated waveform to an uncompressed WAV file.

```
Usage: ./bin/ggwave-to-file [-vN] [-sN] [-pN] [-lN]
    -vN - output volume, N in (0, 100], (default: 50)
    -sN - output sample rate, N in [6000, 96000], (default: 48000)
    -pN - select the transmission protocol id (default: 1)
    -lN - fixed payload length of size N, N in [1, 16]

    Available protocols:
      0 - Normal
      1 - Fast
      2 - Fastest
      3 - [U] Normal
      4 - [U] Fast
      5 - [U] Fastest
      6 - [DT] Normal
      7 - [DT] Fast
      8 - [DT] Fastest
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


## HTTP service

Based on this tool, there is an HTTP service available on the following link:

https://ggwave-to-file.ggerganov.com/

You can use it to query audio waveforms by specifying the text message as a GET parameter to the HTTP request. Here are a few examples:

### terminal

```bash
# audible example
curl -sS 'https://ggwave-to-file.ggerganov.com/?m=Hello world!' --output hello.wav

# ultrasound example
curl -sS 'https://ggwave-to-file.ggerganov.com/?m=Hello world!&p=4' --output hello.wav
```

### browser

- Audible example

  https://ggwave-to-file.ggerganov.com/?m=Hello%20world%21

- Ultrasound example

  https://ggwave-to-file.ggerganov.com/?m=Hello%20world%21&p=4


### python

```python
import requests

def ggwave(message: str, protocolId: int = 1, sampleRate: float = 48000, volume: int = 50, payloadLength: int = -1):

    url = 'https://ggwave-to-file.ggerganov.com/'

    params = {
        'm': message,       # message to encode
        'p': protocolId,    # transmission protocol to use
        's': sampleRate,    # output sample rate
        'v': volume,        # output volume
        'l': payloadLength, # if positive - use fixed-length encoding
    }

    response = requests.get(url, params=params)

    if response == '':
        raise SyntaxError('Request failed')

    return response

```

...

```python
import sys

# query waveform from server
result = ggwave("Hello world!")

# dump wav file to stdout
sys.stdout.buffer.write(result.content)

```
