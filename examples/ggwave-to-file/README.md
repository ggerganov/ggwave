## ggwave-to-file

Output a generated waveform to an uncompressed WAV file.

```
Usage: ./bin/ggwave-to-file [-vN] [-sN] [-pN]
    -vN - output volume, N in (0, 100], (default: 50)
    -sN - output sample rate, N in [1024, 48000], (default: 48000)
    -pN - select the transmission protocol (default: 1)

    Available protocols:
      0 - Normal
      1 - Fast
      2 - Fastest
      3 - [U] Normal
      4 - [U] Fast
      5 - [U] Fastest
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


## HTTP service

Based on this tool, there is an HTTP service available on the following link:

https://ggwave-to-file.ggerganov.com/

You can use it to query audio waveforms by specifying the text message as a GET parameter to the HTTP request. Here are a few examples:

### terminal

```bash
curl https://ggwave-to-file.ggerganov.com/?m=Hello\ world! --output hello.wav
```

### browser

https://ggwave-to-file.ggerganov.com/?m=Hello%20world%21

### python

```python
import requests

def ggwave(message: str, protocolId: int = 1, sampleRate: int = 48000, volume: int = 50):

    url = 'https://ggwave-to-file.ggerganov.com/'

    params = {
        'm': message,       # message to encode
        'p': protocolId,    # transmission protocol to use
        's': sampleRate,    # output sample rate
        'v': volume,        # output volume
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
