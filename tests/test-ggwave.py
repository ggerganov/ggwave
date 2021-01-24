import sys
import ggwave

# create ggwave instance with default parameters
instance = ggwave.init()

payload = 'hello python'

# generate audio waveform for string "hello python"
waveform = ggwave.encode(payload, txProtocolId = 1, volume = 20, instance = instance)

# decode the audio waveform back to text
res = ggwave.decode(instance, waveform)

if res != payload.encode():
    sys.exit(1)
