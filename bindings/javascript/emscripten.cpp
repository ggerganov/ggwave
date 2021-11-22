#include "ggwave/ggwave.h"

#include <emscripten.h>
#include <emscripten/bind.h>

EMSCRIPTEN_BINDINGS(ggwave) {
    emscripten::enum_<ggwave_SampleFormat>("SampleFormat")
        .value("GGWAVE_SAMPLE_FORMAT_UNDEFINED", GGWAVE_SAMPLE_FORMAT_UNDEFINED)
        .value("GGWAVE_SAMPLE_FORMAT_U8",  GGWAVE_SAMPLE_FORMAT_U8)
        .value("GGWAVE_SAMPLE_FORMAT_I8",  GGWAVE_SAMPLE_FORMAT_I8)
        .value("GGWAVE_SAMPLE_FORMAT_U16", GGWAVE_SAMPLE_FORMAT_U16)
        .value("GGWAVE_SAMPLE_FORMAT_I16", GGWAVE_SAMPLE_FORMAT_I16)
        .value("GGWAVE_SAMPLE_FORMAT_F32", GGWAVE_SAMPLE_FORMAT_F32)
        ;

    emscripten::enum_<ggwave_TxProtocolId>("TxProtocolId")
        .value("GGWAVE_TX_PROTOCOL_AUDIBLE_NORMAL",     GGWAVE_TX_PROTOCOL_AUDIBLE_NORMAL)
        .value("GGWAVE_TX_PROTOCOL_AUDIBLE_FAST",       GGWAVE_TX_PROTOCOL_AUDIBLE_FAST)
        .value("GGWAVE_TX_PROTOCOL_AUDIBLE_FASTEST",    GGWAVE_TX_PROTOCOL_AUDIBLE_FASTEST)
        .value("GGWAVE_TX_PROTOCOL_ULTRASOUND_NORMAL",  GGWAVE_TX_PROTOCOL_ULTRASOUND_NORMAL)
        .value("GGWAVE_TX_PROTOCOL_ULTRASOUND_FAST",    GGWAVE_TX_PROTOCOL_ULTRASOUND_FAST)
        .value("GGWAVE_TX_PROTOCOL_ULTRASOUND_FASTEST", GGWAVE_TX_PROTOCOL_ULTRASOUND_FASTEST)
        .value("GGWAVE_TX_PROTOCOL_DT_NORMAL",          GGWAVE_TX_PROTOCOL_DT_NORMAL)
        .value("GGWAVE_TX_PROTOCOL_DT_FAST",            GGWAVE_TX_PROTOCOL_DT_FAST)
        .value("GGWAVE_TX_PROTOCOL_DT_FASTEST",         GGWAVE_TX_PROTOCOL_DT_FASTEST)

        .value("GGWAVE_TX_PROTOCOL_CUSTOM_0", GGWAVE_TX_PROTOCOL_CUSTOM_0)
        .value("GGWAVE_TX_PROTOCOL_CUSTOM_1", GGWAVE_TX_PROTOCOL_CUSTOM_1)
        .value("GGWAVE_TX_PROTOCOL_CUSTOM_2", GGWAVE_TX_PROTOCOL_CUSTOM_2)
        .value("GGWAVE_TX_PROTOCOL_CUSTOM_3", GGWAVE_TX_PROTOCOL_CUSTOM_3)
        .value("GGWAVE_TX_PROTOCOL_CUSTOM_4", GGWAVE_TX_PROTOCOL_CUSTOM_4)
        .value("GGWAVE_TX_PROTOCOL_CUSTOM_5", GGWAVE_TX_PROTOCOL_CUSTOM_5)
        .value("GGWAVE_TX_PROTOCOL_CUSTOM_6", GGWAVE_TX_PROTOCOL_CUSTOM_6)
        .value("GGWAVE_TX_PROTOCOL_CUSTOM_7", GGWAVE_TX_PROTOCOL_CUSTOM_7)
        .value("GGWAVE_TX_PROTOCOL_CUSTOM_8", GGWAVE_TX_PROTOCOL_CUSTOM_8)
        .value("GGWAVE_TX_PROTOCOL_CUSTOM_9", GGWAVE_TX_PROTOCOL_CUSTOM_9)
        ;

    emscripten::class_<ggwave_Parameters>("Parameters")
        .constructor<>()
        .property("payloadLength", &ggwave_Parameters::payloadLength)
        .property("sampleRateInp", &ggwave_Parameters::sampleRateInp)
        .property("sampleRateOut", &ggwave_Parameters::sampleRateOut)
        .property("samplesPerFrame", &ggwave_Parameters::samplesPerFrame)
        .property("soundMarkerThreshold", &ggwave_Parameters::soundMarkerThreshold)
        .property("sampleFormatInp", &ggwave_Parameters::sampleFormatInp)
        .property("sampleFormatOut", &ggwave_Parameters::sampleFormatOut)
        ;

    emscripten::function("getDefaultParameters", &ggwave_getDefaultParameters);
    emscripten::function("init", &ggwave_init);
    emscripten::function("free", &ggwave_free);

    emscripten::function("encode", emscripten::optional_override(
                    [](ggwave_Instance instance,
                       const std::string & data,
                       ggwave_TxProtocolId txProtocolId,
                       int volume) {
                        auto n = ggwave_encode(instance, data.data(), data.size(), txProtocolId, volume, nullptr, 1);
                        std::vector<char> result(n);
                        result.resize(n);
                        ggwave_encode(instance, data.data(), data.size(), txProtocolId, volume, result.data(), 0);

                        return emscripten::val(
                                emscripten::typed_memory_view(result.size(),
                                                              result.data()));
                    }));

    emscripten::function("decode", emscripten::optional_override(
                    [](ggwave_Instance instance,
                       const std::string & data) {
                        char output[256];
                        auto n = ggwave_decode(instance, data.data(), data.size(), output);

                        if (n > 0) {
                            return std::string(output, n);
                        }

                        return std::string();
                    }));

    emscripten::function("disableLog", emscripten::optional_override(
                    []() {
                        ggwave_setLogFile(NULL);
                    }));

    emscripten::function("enableLog", emscripten::optional_override(
                    []() {
                        ggwave_setLogFile(stderr);
                    }));

    emscripten::function("toggleRxProtocol", emscripten::optional_override(
                    [](ggwave_Instance instance,
                       ggwave_TxProtocolId rxProtocolId,
                       int state) {
                        ggwave_toggleRxProtocol(instance, rxProtocolId, state);
                    }));
}
