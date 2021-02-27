#include "ggwave/ggwave.h"

#include <cstring>
#include <limits>
#include <string>
#include <typeinfo>
#include <typeindex>
#include <vector>
#include <set>
#include <cstdint>

float frand() { return float(rand()%RAND_MAX)/RAND_MAX; }

#define CHECK(cond) \
    if (!(cond)) { \
        fprintf(stderr, "[%s:%d] Check failed: %s\n", __FILE__, __LINE__, #cond); \
        exit(1); \
    }

#define CHECK_T(cond) CHECK(cond)
#define CHECK_F(cond) CHECK(!(cond))

const std::map<std::type_index, float> kSampleScale = {
    { typeid(uint8_t),  std::numeric_limits<uint8_t>::max()  },
    { typeid(int8_t),   std::numeric_limits<int8_t>::max()   },
    { typeid(uint16_t), std::numeric_limits<uint16_t>::max() },
    { typeid(int16_t),  std::numeric_limits<int16_t>::max()  },
    { typeid(float),    1.0f                                 },
};

const std::map<std::type_index, float> kSampleOffset = {
    { typeid(uint8_t),  0.5f*std::numeric_limits<uint8_t>::max()  },
    { typeid(int8_t),   0.0f                                      },
    { typeid(uint16_t), 0.5f*std::numeric_limits<uint16_t>::max() },
    { typeid(int16_t),  0.0f                                      },
    { typeid(float),    0.0f                                      },
};

const std::set<GGWave::SampleFormat> kFormats = {
    GGWAVE_SAMPLE_FORMAT_U8,
    GGWAVE_SAMPLE_FORMAT_I8,
    GGWAVE_SAMPLE_FORMAT_U16,
    GGWAVE_SAMPLE_FORMAT_I16,
    GGWAVE_SAMPLE_FORMAT_F32,
};

template <typename T>
GGWave::CBWaveformOut getCBWaveformOut(uint32_t & nSamples, std::vector<T> & buffer) {
    return [&nSamples, &buffer](const void * data, uint32_t nBytes) {
        nSamples = nBytes/sizeof(T);
        CHECK(nSamples*sizeof(T) == nBytes);
        buffer.resize(nSamples);
        std::copy((char *) data, (char *) data + nBytes, (char *) buffer.data());
    };
}

template <typename T>
GGWave::CBWaveformInp getCBWaveformInp(uint32_t & nSamples, std::vector<T> & buffer) {
    return [&nSamples, &buffer](void * data, uint32_t nMaxBytes) {
        uint32_t nCopied = std::min((uint32_t) (nSamples*sizeof(T)), nMaxBytes);
        const char * p = (char *) (buffer.data() + buffer.size() - nSamples);
        std::copy(p, p + nCopied, (char *) data);
        nSamples -= nCopied/sizeof(T);

        return nCopied;
    };
}

template <typename S, typename D>
void convert(const std::vector<S> & src, std::vector<D> & dst) {
    int n = src.size();
    dst.resize(n);
    for (int i = 0; i < n; ++i) {
        dst[i] = ((float(src[i]) - kSampleOffset.at(typeid(S)))/kSampleScale.at(typeid(S)))*kSampleScale.at(typeid(D)) + kSampleOffset.at(typeid(D));
    }
}

int main(int argc, char ** argv) {
    bool full = false;
    if (argc > 1) {
        if (strcmp(argv[1], "--full") == 0) {
            full = true;
        }
    }

    std::vector<uint8_t>  bufferU8;
    std::vector<int8_t>   bufferI8;
    std::vector<uint16_t> bufferU16;
    std::vector<int16_t>  bufferI16;
    std::vector<float>    bufferF32;

    auto convertHelper = [&](GGWave::SampleFormat formatOut, GGWave::SampleFormat formatInp) {
        switch (formatOut) {
            case GGWAVE_SAMPLE_FORMAT_UNDEFINED: CHECK(false); break;
            case GGWAVE_SAMPLE_FORMAT_U8:
                {
                    switch (formatInp) {
                        case GGWAVE_SAMPLE_FORMAT_UNDEFINED: CHECK(false); break;
                        case GGWAVE_SAMPLE_FORMAT_U8:        break;
                        case GGWAVE_SAMPLE_FORMAT_I8:        convert(bufferU8, bufferI8);  break;
                        case GGWAVE_SAMPLE_FORMAT_U16:       convert(bufferU8, bufferU16); break;
                        case GGWAVE_SAMPLE_FORMAT_I16:       convert(bufferU8, bufferI16); break;
                        case GGWAVE_SAMPLE_FORMAT_F32:       convert(bufferU8, bufferF32); break;
                    };
                } break;
            case GGWAVE_SAMPLE_FORMAT_I8:
                {
                    switch (formatInp) {
                        case GGWAVE_SAMPLE_FORMAT_UNDEFINED: CHECK(false); break;
                        case GGWAVE_SAMPLE_FORMAT_U8:        convert(bufferI8, bufferU8);  break;
                        case GGWAVE_SAMPLE_FORMAT_I8:        break;
                        case GGWAVE_SAMPLE_FORMAT_U16:       convert(bufferI8, bufferU16); break;
                        case GGWAVE_SAMPLE_FORMAT_I16:       convert(bufferI8, bufferI16); break;
                        case GGWAVE_SAMPLE_FORMAT_F32:       convert(bufferI8, bufferF32); break;
                    };
                } break;
            case GGWAVE_SAMPLE_FORMAT_U16:
                {
                    switch (formatInp) {
                        case GGWAVE_SAMPLE_FORMAT_UNDEFINED: CHECK(false); break;
                        case GGWAVE_SAMPLE_FORMAT_U8:        convert(bufferU16, bufferU8);  break;
                        case GGWAVE_SAMPLE_FORMAT_I8:        convert(bufferU16, bufferI8);  break;
                        case GGWAVE_SAMPLE_FORMAT_U16:       break;
                        case GGWAVE_SAMPLE_FORMAT_I16:       convert(bufferU16, bufferI16); break;
                        case GGWAVE_SAMPLE_FORMAT_F32:       convert(bufferU16, bufferF32); break;
                    };
                } break;
            case GGWAVE_SAMPLE_FORMAT_I16:
                {
                    switch (formatInp) {
                        case GGWAVE_SAMPLE_FORMAT_UNDEFINED: CHECK(false); break;
                        case GGWAVE_SAMPLE_FORMAT_U8:        convert(bufferI16, bufferU8);  break;
                        case GGWAVE_SAMPLE_FORMAT_I8:        convert(bufferI16, bufferI8);  break;
                        case GGWAVE_SAMPLE_FORMAT_U16:       convert(bufferI16, bufferU16); break;
                        case GGWAVE_SAMPLE_FORMAT_I16:       break;
                        case GGWAVE_SAMPLE_FORMAT_F32:       convert(bufferI16, bufferF32); break;
                    };
                } break;
            case GGWAVE_SAMPLE_FORMAT_F32:
                {
                    switch (formatInp) {
                        case GGWAVE_SAMPLE_FORMAT_UNDEFINED: CHECK(false); break;
                        case GGWAVE_SAMPLE_FORMAT_U8:        convert(bufferF32, bufferU8);  break;
                        case GGWAVE_SAMPLE_FORMAT_I8:        convert(bufferF32, bufferI8);  break;
                        case GGWAVE_SAMPLE_FORMAT_U16:       convert(bufferF32, bufferU16); break;
                        case GGWAVE_SAMPLE_FORMAT_I16:       convert(bufferF32, bufferI16); break;
                        case GGWAVE_SAMPLE_FORMAT_F32:       break;
                    };
                } break;
        };
    };

    auto addNoiseHelper = [&](float level, GGWave::SampleFormat format) {
        switch (format) {
            case GGWAVE_SAMPLE_FORMAT_UNDEFINED: CHECK(false); break;
            case GGWAVE_SAMPLE_FORMAT_U8:
                {
                    for (auto & s : bufferU8) {
                        s = std::max(0.0f, std::min(255.0f, (float) s + (frand() - 0.5f)*(level*256)));
                    }
                } break;
            case GGWAVE_SAMPLE_FORMAT_I8:
                {
                    for (auto & s : bufferI8) {
                        s = std::max(-128.0f, std::min(127.0f, (float) s + (frand() - 0.5f)*(level*256)));
                    }
                } break;
            case GGWAVE_SAMPLE_FORMAT_U16:
                {
                    for (auto & s : bufferU16) {
                        s = std::max(0.0f, std::min(65535.0f, (float) s + (frand() - 0.5f)*(level*65536)));
                    }
                } break;
            case GGWAVE_SAMPLE_FORMAT_I16:
                {
                    for (auto & s : bufferI16) {
                        s = std::max(-32768.0f, std::min(32767.0f, (float) s + (frand() - 0.5f)*(level*65536)));
                    }
                } break;
            case GGWAVE_SAMPLE_FORMAT_F32:
                {
                    for (auto & s : bufferF32) {
                        s = std::max(-1.0f, std::min(1.0f, (float) s + (frand() - 0.5f)*(level)));
                    }
                } break;
        };
    };

    uint32_t nSamples = 0;

    const std::map<GGWave::SampleFormat, GGWave::CBWaveformOut> kCBWaveformOut = {
        { GGWAVE_SAMPLE_FORMAT_U8,  getCBWaveformOut(nSamples, bufferU8)  },
        { GGWAVE_SAMPLE_FORMAT_I8,  getCBWaveformOut(nSamples, bufferI8)  },
        { GGWAVE_SAMPLE_FORMAT_U16, getCBWaveformOut(nSamples, bufferU16) },
        { GGWAVE_SAMPLE_FORMAT_I16, getCBWaveformOut(nSamples, bufferI16) },
        { GGWAVE_SAMPLE_FORMAT_F32, getCBWaveformOut(nSamples, bufferF32) },
    };

    const std::map<GGWave::SampleFormat, GGWave::CBWaveformInp> kCBWaveformInp = {
        { GGWAVE_SAMPLE_FORMAT_U8,  getCBWaveformInp(nSamples, bufferU8)  },
        { GGWAVE_SAMPLE_FORMAT_I8,  getCBWaveformInp(nSamples, bufferI8)  },
        { GGWAVE_SAMPLE_FORMAT_U16, getCBWaveformInp(nSamples, bufferU16) },
        { GGWAVE_SAMPLE_FORMAT_I16, getCBWaveformInp(nSamples, bufferI16) },
        { GGWAVE_SAMPLE_FORMAT_F32, getCBWaveformInp(nSamples, bufferF32) },
    };

    {
        GGWave instance(GGWave::getDefaultParameters());

        std::string payload = "hello";

        CHECK(instance.init(payload));

        // data
        CHECK_F(instance.init(-1, "asd"));
        CHECK_T(instance.init(0, nullptr));
        CHECK_T(instance.init(0, "asd"));
        CHECK_T(instance.init(1, "asd"));
        CHECK_T(instance.init(2, "asd"));
        CHECK_T(instance.init(3, "asd"));

        // volume
        CHECK_F(instance.init(payload.size(), payload.c_str(), -1));
        CHECK_T(instance.init(payload.size(), payload.c_str(), 0));
        CHECK_T(instance.init(payload.size(), payload.c_str(), 50));
        CHECK_T(instance.init(payload.size(), payload.c_str(), 100));
        CHECK_F(instance.init(payload.size(), payload.c_str(), 101));
    }

    // playback / capture at different sample rates
    for (int srInp = GGWave::kBaseSampleRate/6; srInp <= 2*GGWave::kBaseSampleRate; srInp += 1371) {
        printf("Testing: sample rate = %d\n", srInp);

        auto parameters = GGWave::getDefaultParameters();
        parameters.soundMarkerThreshold = 3.0f;

        std::string payload = "hello123";

        // encode
        {
            parameters.sampleRateOut = srInp;
            GGWave instanceOut(parameters);

            instanceOut.init(payload, instanceOut.getTxProtocol(GGWAVE_TX_PROTOCOL_DT_FASTEST), 25);
            auto expectedSize = instanceOut.encodeSize_samples();
            instanceOut.encode(kCBWaveformOut.at(parameters.sampleFormatOut));
            printf("Expected = %d, actual = %d\n", expectedSize, nSamples);
            CHECK(expectedSize >= nSamples);
            addNoiseHelper(0.01, parameters.sampleFormatOut); // add some artificial noise
            convertHelper(parameters.sampleFormatOut, parameters.sampleFormatInp);
        }

        // decode
        {
            parameters.sampleRateInp = srInp;
            GGWave instanceInp(parameters);

            instanceInp.setRxProtocols({{GGWAVE_TX_PROTOCOL_DT_FASTEST, instanceInp.getTxProtocol(GGWAVE_TX_PROTOCOL_DT_FASTEST)}});
            instanceInp.decode(kCBWaveformInp.at(parameters.sampleFormatInp));

            GGWave::TxRxData result;
            CHECK(instanceInp.takeRxData(result) == (int) payload.size());
            for (int i = 0; i < (int) payload.size(); ++i) {
                CHECK(payload[i] == result[i]);
            }
        }
    }

    std::string payload = "a0Z5kR2g";

    // encode / decode using different sample formats and Tx protocols
    for (const auto & formatOut : kFormats) {
        for (const auto & formatInp : kFormats) {
            if (full == false) {
                if (formatOut != GGWAVE_SAMPLE_FORMAT_I16) continue;
                if (formatInp != GGWAVE_SAMPLE_FORMAT_F32) continue;
            }
            for (const auto & txProtocol : GGWave::getTxProtocols()) {
                printf("Testing: protocol = %s, in = %d, out = %d\n", txProtocol.second.name, formatInp, formatOut);

                for (int length = 1; length <= (int) payload.size(); ++length) {
                    // variable payload length
                    {
                        auto parameters = GGWave::getDefaultParameters();
                        parameters.sampleFormatInp = formatInp;
                        parameters.sampleFormatOut = formatOut;
                        GGWave instance(parameters);

                        instance.setRxProtocols({{txProtocol.first, txProtocol.second}});
                        instance.init(length, payload.data(), txProtocol.second, 25);
                        auto expectedSize = instance.encodeSize_samples();
                        instance.encode(kCBWaveformOut.at(formatOut));
                        printf("Expected = %d, actual = %d\n", expectedSize, nSamples);
                        CHECK(expectedSize == nSamples);
                        convertHelper(formatOut, formatInp);
                        instance.decode(kCBWaveformInp.at(formatInp));

                        GGWave::TxRxData result;
                        CHECK(instance.takeRxData(result) == length);
                        for (int i = 0; i < length; ++i) {
                            CHECK(payload[i] == result[i]);
                        }
                    }
                }

                for (int length = 1; length <= (int) payload.size(); ++length) {
                    // fixed payload length
                    {
                        auto parameters = GGWave::getDefaultParameters();
                        parameters.payloadLength = length;
                        parameters.sampleFormatInp = formatInp;
                        parameters.sampleFormatOut = formatOut;
                        GGWave instance(parameters);

                        instance.setRxProtocols({{txProtocol.first, txProtocol.second}});
                        instance.init(length, payload.data(), txProtocol.second, 10);
                        auto expectedSize = instance.encodeSize_samples();
                        instance.encode(kCBWaveformOut.at(formatOut));
                        printf("Expected = %d, actual = %d\n", expectedSize, nSamples);
                        CHECK(expectedSize == nSamples);
                        convertHelper(formatOut, formatInp);
                        instance.decode(kCBWaveformInp.at(formatInp));

                        GGWave::TxRxData result;
                        CHECK(instance.takeRxData(result) == length);
                        for (int i = 0; i < length; ++i) {
                            CHECK(payload[i] == result[i]);
                        }
                    }
                }
            }
        }
    }

    return 0;
}
