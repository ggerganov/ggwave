import ggwave
import wave
import numpy as np

# Parameters
volume_ = 20
sample_rate_ = 48000
filename = "output.wav"

# Generate audio waveform for string "hello python"
waveform = ggwave.encode("hello python", protocolId=5, volume=volume_)

# Convert byte data into float32
waveform_float32 = np.frombuffer(waveform, dtype=np.float32)

# Normalize the float32 data to the range of int16
waveform_int16 = np.int16(waveform_float32 * 32767)

# Save the waveform to a .wav file
with wave.open(filename, "wb") as wf:
    wf.setnchannels(1)                  # mono audio
    wf.setsampwidth(2)                  # 2 bytes per sample (16-bit PCM)
    wf.setframerate(sample_rate_)       # sample rate
    wf.writeframes(waveform_int16.tobytes())  # write the waveform as bytes