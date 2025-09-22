#include <jni.h>
#include <vector>
#include <cstdint>
#include "ggwave/ggwave.h"

extern "C"
JNIEXPORT jshortArray JNICALL
Java_com_echopass_aat_audio_GGWaveBridge_encodeToken(
        JNIEnv *env,
        jclass,
        jbyteArray tokenArray,
        jint sampleRate,
        jint protocolId,
        jint volume) {
    const jsize tokenLength = env->GetArrayLength(tokenArray);
    if (tokenLength <= 0) {
        return env->NewShortArray(0);
    }

    std::vector<uint8_t> token(static_cast<size_t>(tokenLength));
    env->GetByteArrayRegion(tokenArray, 0, tokenLength, reinterpret_cast<jbyte *>(token.data()));

    ggwave_Parameters params = ggwave_getDefaultParameters();
    params.payloadLength = tokenLength;
    params.sampleRateInp = static_cast<float>(sampleRate);
    params.sampleRateOut = static_cast<float>(sampleRate);
    params.sampleRate = static_cast<float>(sampleRate);
    params.sampleFormatOut = GGWAVE_SAMPLE_FORMAT_I16;
    params.operatingMode = GGWAVE_OPERATING_MODE_TX;

    ggwave_Instance instance = ggwave_init(params);
    if (instance < 0) {
        return env->NewShortArray(0);
    }

    const int estimatedSize = ggwave_encode(
            instance,
            token.data(),
            static_cast<int>(token.size()),
            static_cast<ggwave_ProtocolId>(protocolId),
            volume,
            nullptr,
            1);

    if (estimatedSize <= 0) {
        ggwave_free(instance);
        return env->NewShortArray(0);
    }

    std::vector<int16_t> waveform(static_cast<size_t>(estimatedSize) / sizeof(int16_t));
    const int generated = ggwave_encode(
            instance,
            token.data(),
            static_cast<int>(token.size()),
            static_cast<ggwave_ProtocolId>(protocolId),
            volume,
            waveform.data(),
            0);

    ggwave_free(instance);

    if (generated <= 0) {
        return env->NewShortArray(0);
    }

    const int sampleCount = generated / static_cast<int>(sizeof(int16_t));
    jshortArray result = env->NewShortArray(sampleCount);
    if (!result) {
        return env->NewShortArray(0);
    }

    env->SetShortArrayRegion(result, 0, sampleCount, reinterpret_cast<const jshort *>(waveform.data()));
    return result;
}
