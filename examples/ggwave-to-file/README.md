## ggwave-to-file

Output a generated waveform to an uncompressed WAV file.

```bash
echo "Hello world!" | ./bin/ggwave-to-file > example.wav
```

## HTTP service

Based on this tool, there is an HTTP service available on the following link:

https://ggwave-to-file.ggerganov.com/

You can use it to query audio waveforms by specifying the text message as a GET parameter to the HTTP request. Here are a few examples:

### curl

```bash
curl https://ggwave-to-file.ggerganov.com/?m=Hello\ world! --output hello.wav
```

### python

```python
import requests

def ggwave(message: str, protocolId: int = 1):

    url = 'https://ggwave-to-file.ggerganov.com/'

    params = {
        'm': message,       # message to encode
        'p': protocolId,    # transmission protocol to use
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
