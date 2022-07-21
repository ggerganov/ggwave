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

    params: Dict[str, Union[str, int, float]] = {
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
        f.writeframes(response.content)


if __name__ == "__main__":
    ggwave("Hello world!", "hello_world.wav")
