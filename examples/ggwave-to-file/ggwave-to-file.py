import sys
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

result = ggwave("Hello world!")

sys.stdout.buffer.write(result.content)
