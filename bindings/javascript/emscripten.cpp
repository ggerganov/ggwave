#include "ggwave/ggwave.h"

#include <emscripten.h>
#include <emscripten/bind.h>

EMSCRIPTEN_BINDINGS(ggwave) {
    emscripten::enum_<ggwave_SampleFormat>("SampleFormat")
        .value("GGWAVE_SAMPLE_FORMAT_UNDEFINED", GGWAVE_SAMPLE_FORMAT_UNDEFINED)
        .value("GGWAVE_SAMPLE_FORMAT_U8",        GGWAVE_SAMPLE_FORMAT_U8)
        .value("GGWAVE_SAMPLE_FORMAT_I8",        GGWAVE_SAMPLE_FORMAT_I8)
        .value("GGWAVE_SAMPLE_FORMAT_U16",       GGWAVE_SAMPLE_FORMAT_U16)
        .value("GGWAVE_SAMPLE_FORMAT_I16",       GGWAVE_SAMPLE_FORMAT_I16)
        .value("GGWAVE_SAMPLE_FORMAT_F32",       GGWAVE_SAMPLE_FORMAT_F32)
        ;

    emscripten::enum_<ggwave_ProtocolId>("ProtocolId")
        .value("GGWAVE_PROTOCOL_AUDIBLE_NORMAL",     GGWAVE_PROTOCOL_AUDIBLE_NORMAL)
        .value("GGWAVE_PROTOCOL_AUDIBLE_FAST",       GGWAVE_PROTOCOL_AUDIBLE_FAST)
        .value("GGWAVE_PROTOCOL_AUDIBLE_FASTEST",    GGWAVE_PROTOCOL_AUDIBLE_FASTEST)
        .value("GGWAVE_PROTOCOL_ULTRASOUND_NORMAL",  GGWAVE_PROTOCOL_ULTRASOUND_NORMAL)
        .value("GGWAVE_PROTOCOL_ULTRASOUND_FAST",    GGWAVE_PROTOCOL_ULTRASOUND_FAST)
        .value("GGWAVE_PROTOCOL_ULTRASOUND_FASTEST", GGWAVE_PROTOCOL_ULTRASOUND_FASTEST)
        .value("GGWAVE_PROTOCOL_DT_NORMAL",          GGWAVE_PROTOCOL_DT_NORMAL)
        .value("GGWAVE_PROTOCOL_DT_FAST",            GGWAVE_PROTOCOL_DT_FAST)
        .value("GGWAVE_PROTOCOL_DT_FASTEST",         GGWAVE_PROTOCOL_DT_FASTEST)
        .value("GGWAVE_PROTOCOL_MT_NORMAL",          GGWAVE_PROTOCOL_MT_NORMAL)
        .value("GGWAVE_PROTOCOL_MT_FAST",            GGWAVE_PROTOCOL_MT_FAST)
        .value("GGWAVE_PROTOCOL_MT_FASTEST",         GGWAVE_PROTOCOL_MT_FASTEST)

        .value("GGWAVE_PROTOCOL_CUSTOM_0", GGWAVE_PROTOCOL_CUSTOM_0)
        .value("GGWAVE_PROTOCOL_CUSTOM_1", GGWAVE_PROTOCOL_CUSTOM_1)
        .value("GGWAVE_PROTOCOL_CUSTOM_2", GGWAVE_PROTOCOL_CUSTOM_2)
        .value("GGWAVE_PROTOCOL_CUSTOM_3", GGWAVE_PROTOCOL_CUSTOM_3)
        .value("GGWAVE_PROTOCOL_CUSTOM_4", GGWAVE_PROTOCOL_CUSTOM_4)
        .value("GGWAVE_PROTOCOL_CUSTOM_5", GGWAVE_PROTOCOL_CUSTOM_5)
        .value("GGWAVE_PROTOCOL_CUSTOM_6", GGWAVE_PROTOCOL_CUSTOM_6)
        .value("GGWAVE_PROTOCOL_CUSTOM_7", GGWAVE_PROTOCOL_CUSTOM_7)
        .value("GGWAVE_PROTOCOL_CUSTOM_8", GGWAVE_PROTOCOL_CUSTOM_8)
        .value("GGWAVE_PROTOCOL_CUSTOM_9", GGWAVE_PROTOCOL_CUSTOM_9)
        ;

    emscripten::constant("GGWAVE_OPERATING_MODE_RX",            (int) GGWAVE_OPERATING_MODE_RX);
    emscripten::constant("GGWAVE_OPERATING_MODE_TX",            (int) GGWAVE_OPERATING_MODE_TX);
    emscripten::constant("GGWAVE_OPERATING_MODE_RX_AND_TX",     (int) GGWAVE_OPERATING_MODE_RX | GGWAVE_OPERATING_MODE_TX);
    emscripten::constant("GGWAVE_OPERATING_MODE_TX_ONLY_TONES", (int) GGWAVE_OPERATING_MODE_TX_ONLY_TONES);
    emscripten::constant("GGWAVE_OPERATING_MODE_USE_DSS",       (int) GGWAVE_OPERATING_MODE_USE_DSS);

    emscripten::value_object<ggwave_Parameters>("Parameters")
        .field("payloadLength",        & ggwave_Parameters::payloadLength)
        .field("sampleRateInp",        & ggwave_Parameters::sampleRateInp)
        .field("sampleRateOut",        & ggwave_Parameters::sampleRateOut)
        .field("sampleRate",           & ggwave_Parameters::sampleRate)
        .field("samplesPerFrame",      & ggwave_Parameters::samplesPerFrame)
        .field("soundMarkerThreshold", & ggwave_Parameters::soundMarkerThreshold)
        .field("sampleFormatInp",      & ggwave_Parameters::sampleFormatInp)
        .field("sampleFormatOut",      & ggwave_Parameters::sampleFormatOut)
        .field("operatingMode",        & ggwave_Parameters::operatingMode)
        ;

    emscripten::function("getDefaultParameters", & ggwave_getDefaultParameters);
    emscripten::function("init", & ggwave_init);
    emscripten::function("free", & ggwave_free);

    emscripten::function("encode", emscripten::optional_override(
                    [](ggwave_Instance instance,
                       const std::string & data,
                       ggwave_ProtocolId protocolId,
                       int volume) {
                        auto n = ggwave_encode(instance, data.data(), data.size(), protocolId, volume, nullptr, 1);

                        // TODO: how to return the waveform data?
                        //       for now using this static vector and returning a pointer to it
                        static std::vector<char> result(n);
                        result.resize(n);

                        int nActual = ggwave_encode(instance, data.data(), data.size(), protocolId, volume, result.data(), 0);

                        // printf("n = %d, nActual = %d\n", n, nActual);
                        return emscripten::val(emscripten::typed_memory_view(nActual, result.data()));
                    }));

    emscripten::function("decode", emscripten::optional_override(
                    [](ggwave_Instance instance,
                       const std::string & data) {
                        // TODO: how to return the result?
                        //       again using a static array and returning a pointer to it
                        static char output[256];

                        auto n = ggwave_decode(instance, data.data(), data.size(), output);

                        if (n > 0) {
                            return emscripten::val(emscripten::typed_memory_view(n, output));
                        }

                        return emscripten::val(emscripten::typed_memory_view(0, output));
                    }));

    emscripten::function("disableLog", emscripten::optional_override(
                    []() {
                        ggwave_setLogFile(NULL);
                    }));

    emscripten::function("enableLog", emscripten::optional_override(
                    []() {
                        ggwave_setLogFile(stderr);
                    }));

    emscripten::function("rxToggleProtocol", emscripten::optional_override(
                    [](ggwave_ProtocolId protocolId,
                       int state) {
                        ggwave_rxToggleProtocol(protocolId, state);
                    }));

    emscripten::function("txToggleProtocol", emscripten::optional_override(
                    [](ggwave_ProtocolId protocolId,
                       int state) {
                        ggwave_txToggleProtocol(protocolId, state);
                    }));

    emscripten::function("rxProtocolSetFreqStart", emscripten::optional_override(
                    [](ggwave_ProtocolId protocolId,
                       int freqStart) {
                        ggwave_rxProtocolSetFreqStart(protocolId, freqStart);
                    }));

    emscripten::function("txProtocolSetFreqStart", emscripten::optional_override(
                    [](ggwave_ProtocolId protocolId,
                       int freqStart) {
                        ggwave_txProtocolSetFreqStart(protocolId, freqStart);
                    }));

    emscripten::function("rxDurationFrames", emscripten::optional_override(
                    [](ggwave_Instance instance) {
                        return ggwave_rxDurationFrames(instance);
                    }));
}
