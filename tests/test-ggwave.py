import ggwave
import numpy as np

instance = ggwave.init()

# generate audio waveform for string "hello python"
waveform = ggwave.encode("hello python", txProtocolId = 1, volume = 20, instance = instance)
