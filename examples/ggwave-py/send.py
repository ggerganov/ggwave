import ggwave
import pyaudio
import numpy as np

p = pyaudio.PyAudio()

# generate audio waveform for string "hello python"
waveform = ggwave.encode("hello python", txProtocolId = 1, volume = 20)

print("Transmitting text 'hello python' ...")
stream = p.open(format=pyaudio.paInt16, channels=1, rate=48000, output=True, frames_per_buffer=4096)
stream.write(np.array(waveform).astype(np.int16), len(waveform))
stream.stop_stream()
stream.close()

p.terminate()
