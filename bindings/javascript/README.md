# ggwave

Tiny data-over-sound library.

- Audible and ultrasound transmissions available
- Bandwidth of 8-16 bytes/s (depending on the transmission protocol)
- Robust FSK modulation
- Reed-Solomon based error correction

## Example Usage

```js
var factory = require('ggwave')

factory().then(function(ggwave) {
    // create ggwave instance with default parameters
    var parameters = ggwave.getDefaultParameters();
    var instance = ggwave.init(parameters);

    var payload = 'hello js';

    // generate audio waveform for string "hello js"
    var waveform = ggwave.encode(instance, payload, ggwave.TxProtocolId.GGWAVE_TX_PROTOCOL_AUDIBLE_FAST, 10);

    // decode the audio waveform back to text
    var res = ggwave.decode(instance, waveform);

    if (res != payload) {
        process.exit(1);
    }
});
```
