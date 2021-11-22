import sys
import ggwave

# optionally disable logging
#ggwave.disableLog()

# create ggwave instance with default parameters
instance = ggwave.init()

payload = 'hello python'

# generate audio waveform for string "hello python"
waveform = ggwave.encode(payload, txProtocolId = 1, volume = 20, instance = instance)

# decode the audio waveform back to text
res = ggwave.decode(instance, waveform)

if res != payload.encode():
    sys.exit(1)

# disable the Rx protocol - the decoding should fail
ggwave.toggleRxProtocol(instance, rxProtocolId = 1, state = 0)
res = ggwave.decode(instance, waveform)

if res != None:
    sys.exit(1)

# re-enable the Rx protocol - the decoding should succeed
ggwave.toggleRxProtocol(instance, rxProtocolId = 1, state = 1)
res = ggwave.decode(instance, waveform)

if res != payload.encode():
    sys.exit(1)
