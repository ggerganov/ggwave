import sys
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

result = ggwave("Hello world!")

sys.stdout.buffer.write(result.content)
