var factory = require('../bindings/javascript/ggwave.js')

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
