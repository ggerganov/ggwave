## ggwave-to-file

Output a generated waveform to an uncompressed WAV file.

```bash
echo "Hello World!" | ./bin/ggwave-to-file > example.wav
```

Based on this tool, there is a REST service available on the following link: https://ggwave.ggerganov.com/ggwave-to-file.php

You can use it to query audio waveforms for different text messages.

### curl:

```bash
curl https://ggwave.ggerganov.com/ggwave-to-file.php?m=Hello --output hello.wav
```

### python

```python
import sys
import requests

def ggwave(message: str, protocolId: int = 1):

    url = 'https://ggwave.ggerganov.com/ggwave-to-file.php'

    params = {
        'm': message,       # message to encode
        'p': protocolId,    # transmission protocol to use
    }

    response = requests.get(url, params=params)

    if response == '':
        raise SyntaxError('Request failed')

    return response

...

# query waveform from server
result = ggwave("Hello world!")

# dump wav file to stdout
sys.stdout.buffer.write(result.content)

```
