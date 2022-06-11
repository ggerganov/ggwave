var factory = require('../bindings/javascript/ggwave.js')

factory().then(function(ggwave) {
    // create ggwave instance with default parameters
    var parameters = ggwave.getDefaultParameters();

    parameters.operatingMode |= ggwave.GGWAVE_OPERATING_MODE_USE_DSS;

    var instance = ggwave.init(parameters);
    console.log('instance: ' + instance);

    var payload = 'hello js';

    // generate audio waveform for string "hello js"
    var waveform = ggwave.encode(instance, payload, ggwave.ProtocolId.GGWAVE_PROTOCOL_AUDIBLE_FAST, 10);

    // decode the audio waveform back to text
    var res = ggwave.decode(instance, waveform);

    if (new TextDecoder("utf-8").decode(res) != payload) {
        process.exit(1);
    }
});
