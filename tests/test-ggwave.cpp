#include "ggwave/ggwave.h"

#include <cstring>
#include <limits>
#include <string>
#include <typeinfo>
#include <typeindex>
#include <vector>
#include <set>
#include <cstdint>
#include <map>

constexpr float iRandMax = 1.0f/float(RAND_MAX);
float frand() { return float(rand()%RAND_MAX)*iRandMax; }

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

template <typename S, typename D>
void convert(std::vector<uint8_t> & src) {
    const int n = src.size()/sizeof(S);
    std::vector<D> dst(n);
    S v;
    for (int i = 0; i < n; ++i) {
        std::memcpy(&v, &src[i*sizeof(S)], sizeof(S));
        dst[i] = ((float(v) - kSampleOffset.at(typeid(S)))/kSampleScale.at(typeid(S)))*kSampleScale.at(typeid(D)) + kSampleOffset.at(typeid(D));
    }

    src.resize(n*sizeof(D));
    std::memcpy(&src[0], &dst[0], n*sizeof(D));
}

int main(int argc, char ** argv) {
    bool full = false;
    if (argc > 1) {
        if (strcmp(argv[1], "--full") == 0) {
            full = true;
        }
    }

    std::vector<uint8_t> buffer;

    auto convertHelper = [&](GGWave::SampleFormat formatOut, GGWave::SampleFormat formatInp) {
        switch (formatOut) {
            case GGWAVE_SAMPLE_FORMAT_UNDEFINED: CHECK(false); break;
            case GGWAVE_SAMPLE_FORMAT_U8:
                {
                    switch (formatInp) {
                        case GGWAVE_SAMPLE_FORMAT_UNDEFINED: CHECK(false); break;
                        case GGWAVE_SAMPLE_FORMAT_U8:        break;
                        case GGWAVE_SAMPLE_FORMAT_I8:        convert<uint8_t, int8_t>  (buffer); break;
                        case GGWAVE_SAMPLE_FORMAT_U16:       convert<uint8_t, uint16_t>(buffer); break;
                        case GGWAVE_SAMPLE_FORMAT_I16:       convert<uint8_t, int16_t> (buffer); break;
                        case GGWAVE_SAMPLE_FORMAT_F32:       convert<uint8_t, float>   (buffer); break;
                    };
                } break;
            case GGWAVE_SAMPLE_FORMAT_I8:
                {
                    switch (formatInp) {
                        case GGWAVE_SAMPLE_FORMAT_UNDEFINED: CHECK(false); break;
                        case GGWAVE_SAMPLE_FORMAT_U8:        convert<int8_t, uint8_t> (buffer); break;
                        case GGWAVE_SAMPLE_FORMAT_I8:        break;
                        case GGWAVE_SAMPLE_FORMAT_U16:       convert<int8_t, uint16_t>(buffer); break;
                        case GGWAVE_SAMPLE_FORMAT_I16:       convert<int8_t, int16_t> (buffer); break;
                        case GGWAVE_SAMPLE_FORMAT_F32:       convert<int8_t, float>   (buffer); break;
                    };
                } break;
            case GGWAVE_SAMPLE_FORMAT_U16:
                {
                    switch (formatInp) {
                        case GGWAVE_SAMPLE_FORMAT_UNDEFINED: CHECK(false); break;
                        case GGWAVE_SAMPLE_FORMAT_U8:        convert<uint16_t, uint8_t>(buffer); break;
                        case GGWAVE_SAMPLE_FORMAT_I8:        convert<uint16_t, int8_t> (buffer); break;
                        case GGWAVE_SAMPLE_FORMAT_U16:       break;
                        case GGWAVE_SAMPLE_FORMAT_I16:       convert<uint16_t, int16_t>(buffer); break;
                        case GGWAVE_SAMPLE_FORMAT_F32:       convert<uint16_t, float>  (buffer); break;
                    };
                } break;
            case GGWAVE_SAMPLE_FORMAT_I16:
                {
                    switch (formatInp) {
                        case GGWAVE_SAMPLE_FORMAT_UNDEFINED: CHECK(false); break;
                        case GGWAVE_SAMPLE_FORMAT_U8:        convert<int16_t, uint8_t> (buffer); break;
                        case GGWAVE_SAMPLE_FORMAT_I8:        convert<int16_t, int8_t>  (buffer); break;
                        case GGWAVE_SAMPLE_FORMAT_U16:       convert<int16_t, uint16_t>(buffer); break;
                        case GGWAVE_SAMPLE_FORMAT_I16:       break;
                        case GGWAVE_SAMPLE_FORMAT_F32:       convert<int16_t, float>   (buffer); break;
                    };
                } break;
            case GGWAVE_SAMPLE_FORMAT_F32:
                {
                    switch (formatInp) {
                        case GGWAVE_SAMPLE_FORMAT_UNDEFINED: CHECK(false); break;
                        case GGWAVE_SAMPLE_FORMAT_U8:        convert<float, uint8_t> (buffer); break;
                        case GGWAVE_SAMPLE_FORMAT_I8:        convert<float, int8_t>  (buffer); break;
                        case GGWAVE_SAMPLE_FORMAT_U16:       convert<float, uint16_t>(buffer); break;
                        case GGWAVE_SAMPLE_FORMAT_I16:       convert<float, int16_t> (buffer); break;
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
                    const int n = buffer.size()/sizeof(uint8_t);
                    auto p = (uint8_t *) buffer.data();
                    for (int i = 0; i < n; ++i) {
                        p[i] = std::max(0.0f, std::min(255.0f, (float) p[i] + (frand() - 0.5f)*(level*256)));
                    }
                } break;
            case GGWAVE_SAMPLE_FORMAT_I8:
                {
                    const int n = buffer.size()/sizeof(int8_t);
                    auto p = (int8_t *) buffer.data();
                    for (int i = 0; i < n; ++i) {
                        p[i] = std::max(-128.0f, std::min(127.0f, (float) p[i] + (frand() - 0.5f)*(level*256)));
                    }
                } break;
            case GGWAVE_SAMPLE_FORMAT_U16:
                {
                    const int n = buffer.size()/sizeof(uint16_t);
                    auto p = (uint16_t *) buffer.data();
                    for (int i = 0; i < n; ++i) {
                        p[i] = std::max(0.0f, std::min(65535.0f, (float) p[i] + (frand() - 0.5f)*(level*65536)));
                    }
                } break;
            case GGWAVE_SAMPLE_FORMAT_I16:
                {
                    const int n = buffer.size()/sizeof(int16_t);
                    auto p = (int16_t *) buffer.data();
                    for (int i = 0; i < n; ++i) {
                        p[i] = std::max(-32768.0f, std::min(32767.0f, (float) p[i] + (frand() - 0.5f)*(level*65536)));
                    }
                } break;
            case GGWAVE_SAMPLE_FORMAT_F32:
                {
                    const int n = buffer.size()/sizeof(float);
                    auto p = (float *) buffer.data();
                    for (int i = 0; i < n; ++i) {
                        p[i] = std::max(-1.0f, std::min(1.0f, p[i] + (frand() - 0.5f)*(level)));
                    }
                } break;
        };
    };

    {
        GGWave instance(GGWave::getDefaultParameters());

        std::string payload = "hello";

        CHECK(instance.init(payload.c_str(), GGWAVE_PROTOCOL_AUDIBLE_FAST));

        // data
        CHECK_F(instance.init(-1, "asd",   GGWAVE_PROTOCOL_AUDIBLE_FAST));
        CHECK_T(instance.init(0,  nullptr, GGWAVE_PROTOCOL_AUDIBLE_FAST));
        CHECK_T(instance.init(0,  "asd",   GGWAVE_PROTOCOL_AUDIBLE_FAST));
        CHECK_T(instance.init(1,  "asd",   GGWAVE_PROTOCOL_AUDIBLE_FAST));
        CHECK_T(instance.init(2,  "asd",   GGWAVE_PROTOCOL_AUDIBLE_FAST));
        CHECK_T(instance.init(3,  "asd",   GGWAVE_PROTOCOL_AUDIBLE_FAST));

        // volume
        CHECK_F(instance.init(payload.size(), payload.c_str(), GGWAVE_PROTOCOL_AUDIBLE_FAST, -1));
        CHECK_T(instance.init(payload.size(), payload.c_str(), GGWAVE_PROTOCOL_AUDIBLE_FAST, 0));
        CHECK_T(instance.init(payload.size(), payload.c_str(), GGWAVE_PROTOCOL_AUDIBLE_FAST, 50));
        CHECK_T(instance.init(payload.size(), payload.c_str(), GGWAVE_PROTOCOL_AUDIBLE_FAST, 100));
        CHECK_F(instance.init(payload.size(), payload.c_str(), GGWAVE_PROTOCOL_AUDIBLE_FAST, 101));
    }

    // playback / capture at different sample rates
    for (int srInp = GGWave::kDefaultSampleRate/6; srInp <= 2*GGWave::kDefaultSampleRate; srInp += 1371) {
        printf("Testing: sample rate = %d\n", srInp);

        auto parameters = GGWave::getDefaultParameters();
        parameters.soundMarkerThreshold = 3.0f;

        const std::string payload = "hello123";

        // encode
        {
            parameters.sampleRateOut = srInp;
            GGWave instanceOut(parameters);

            instanceOut.init(payload.c_str(), GGWAVE_PROTOCOL_DT_FASTEST, 25);
            const auto expectedSize = instanceOut.encodeSize_bytes();
            const auto nBytes = instanceOut.encode();
            printf("Expected = %d, actual = %d\n", expectedSize, nBytes);
            CHECK(expectedSize >= nBytes);
            { auto p = (const uint8_t *)(instanceOut.txWaveform()); buffer.resize(nBytes); memcpy(buffer.data(), p, nBytes); }
            addNoiseHelper(0.01, parameters.sampleFormatOut); // add some artificial noise
            convertHelper(parameters.sampleFormatOut, parameters.sampleFormatInp);
        }

        // decode
        {
            parameters.sampleRateInp = srInp;
            GGWave instanceInp(parameters);
            instanceInp.rxProtocols().only(GGWAVE_PROTOCOL_DT_FASTEST);

            instanceInp.decode(buffer.data(), buffer.size());

            GGWave::TxRxData result;
            CHECK(instanceInp.rxTakeData(result) == (int) payload.size());
            for (int i = 0; i < (int) payload.size(); ++i) {
                CHECK(payload[i] == result[i]);
            }
        }
    }

    const std::string payload = "a0Z5kR2g";

    // encode / decode using different sample formats and Tx protocols
    for (const auto & formatOut : kFormats) {
        for (const auto & formatInp : kFormats) {
            if (full == false) {
                if (formatOut != GGWAVE_SAMPLE_FORMAT_I16) continue;
                if (formatInp != GGWAVE_SAMPLE_FORMAT_F32) continue;
            }
            for (int protocolId = 0; protocolId < GGWAVE_PROTOCOL_COUNT; ++protocolId) {
                const auto & protocol = GGWave::Protocols::kDefault()[protocolId];
                if (protocol.enabled == false) continue;
                printf("Testing: protocol = %s, in = %d, out = %d\n", protocol.name, formatInp, formatOut);

                for (int length = 1; length <= (int) payload.size(); ++length) {
                    // mono-tone protocols with variable length are not supported
                    if (protocol.extra == 2) {
                        break;
                    }

                    // variable payload length
                    {
                        auto parameters = GGWave::getDefaultParameters();
                        parameters.sampleFormatInp = formatInp;
                        parameters.sampleFormatOut = formatOut;
                        // it seems DSS is not suitable for "variable-length" transmission
                        // sometimes, the decoder incorrectly detects an early "end" marker when DSS is enabled
                        //if (rand() % 2 == 0) parameters.operatingMode |= GGWAVE_OPERATING_MODE_USE_DSS;
                        GGWave instance(parameters);
                        instance.rxProtocols().only(GGWave::ProtocolId(protocolId));

                        instance.init(length, payload.data(), GGWave::ProtocolId(protocolId), 25);
                        const auto expectedSize = instance.encodeSize_bytes();
                        const auto nBytes = instance.encode();
                        printf("Expected = %d, actual = %d\n", expectedSize, nBytes);
                        CHECK(expectedSize == nBytes);
                        { auto p = (const uint8_t *)(instance.txWaveform()); buffer.resize(nBytes); memcpy(buffer.data(), p, nBytes); }
                        addNoiseHelper(0.02, parameters.sampleFormatOut); // add some artificial noise
                        convertHelper(formatOut, formatInp);
                        instance.decode(buffer.data(), buffer.size());

                        GGWave::TxRxData result;
                        CHECK(instance.rxTakeData(result) == length);
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
                        if (rand() % 2 == 0) parameters.operatingMode |= GGWAVE_OPERATING_MODE_USE_DSS;
                        GGWave instance(parameters);
                        instance.rxProtocols().only(GGWave::ProtocolId(protocolId));

                        instance.init(length, payload.data(), GGWave::ProtocolId(protocolId), 10);
                        const auto expectedSize = instance.encodeSize_bytes();
                        const auto nBytes = instance.encode();
                        printf("Expected = %d, actual = %d\n", expectedSize, nBytes);
                        CHECK(expectedSize == nBytes);
                        { auto p = (const uint8_t *)(instance.txWaveform()); buffer.resize(nBytes); memcpy(buffer.data(), p, nBytes); }
                        addNoiseHelper(0.10, parameters.sampleFormatOut); // add some artificial noise
                        convertHelper(formatOut, formatInp);
                        instance.decode(buffer.data(), buffer.size());

                        GGWave::TxRxData result;
                        CHECK(instance.rxTakeData(result) == length);
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
