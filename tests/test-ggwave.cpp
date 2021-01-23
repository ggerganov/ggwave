#include "ggwave/ggwave.h"

#include <limits>
#include <string>
#include <typeinfo>
#include <typeindex>
#include <vector>
#include <set>

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
GGWave::CBEnqueueAudio getCBEnqueueAudio(uint32_t & nSamples, std::vector<T> & buffer) {
    return [&nSamples, &buffer](const void * data, uint32_t nBytes) {
        nSamples = nBytes/sizeof(T);
        CHECK(nSamples*sizeof(T) == nBytes);
        buffer.resize(nSamples);
        std::copy((char *) data, (char *) data + nBytes, (char *) buffer.data());
    };
}

template <typename T>
GGWave::CBDequeueAudio getCBDequeueAudio(uint32_t & nSamples, std::vector<T> & buffer) {
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

int main() {
    std::vector<uint8_t>  bufferU8;
    std::vector<int8_t>   bufferI8;
    std::vector<uint16_t> bufferU16;
    std::vector<int16_t>  bufferI16;
    std::vector<float>    bufferF32;

    auto convertHelper = [&](GGWave::SampleFormat formatOut, GGWave::SampleFormat formatIn) {
        switch (formatOut) {
            case GGWAVE_SAMPLE_FORMAT_UNDEFINED: CHECK(false); break;
            case GGWAVE_SAMPLE_FORMAT_U8:
                {
                    switch (formatIn) {
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
                    switch (formatIn) {
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
                    switch (formatIn) {
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
                    switch (formatIn) {
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
                    switch (formatIn) {
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

    uint32_t nSamples = 0;

    const std::map<GGWave::SampleFormat, GGWave::CBEnqueueAudio> kCBEnqueueAudio = {
        { GGWAVE_SAMPLE_FORMAT_U8,  getCBEnqueueAudio(nSamples, bufferU8)  },
        { GGWAVE_SAMPLE_FORMAT_I8,  getCBEnqueueAudio(nSamples, bufferI8)  },
        { GGWAVE_SAMPLE_FORMAT_U16, getCBEnqueueAudio(nSamples, bufferU16) },
        { GGWAVE_SAMPLE_FORMAT_I16, getCBEnqueueAudio(nSamples, bufferI16) },
        { GGWAVE_SAMPLE_FORMAT_F32, getCBEnqueueAudio(nSamples, bufferF32) },
    };

    const std::map<GGWave::SampleFormat, GGWave::CBDequeueAudio> kCBDequeueAudio = {
        { GGWAVE_SAMPLE_FORMAT_U8,  getCBDequeueAudio(nSamples, bufferU8)  },
        { GGWAVE_SAMPLE_FORMAT_I8,  getCBDequeueAudio(nSamples, bufferI8)  },
        { GGWAVE_SAMPLE_FORMAT_U16, getCBDequeueAudio(nSamples, bufferU16) },
        { GGWAVE_SAMPLE_FORMAT_I16, getCBDequeueAudio(nSamples, bufferI16) },
        { GGWAVE_SAMPLE_FORMAT_F32, getCBDequeueAudio(nSamples, bufferF32) },
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

    for (const auto & txProtocol : GGWave::getTxProtocols()) {
        for (const auto & formatOut : kFormats) {
            for (const auto & formatIn : kFormats) {
                printf("Testing: protocol = %s, in = %d, out = %d\n", txProtocol.second.name, formatIn, formatOut);

                auto parameters = GGWave::getDefaultParameters();
                parameters.sampleFormatIn = formatIn;
                parameters.sampleFormatOut = formatOut;
                GGWave instance(parameters);

                std::string payload = "test message xxxxxxxxxxxx";

                instance.init(payload, txProtocol.second, 25);
                instance.encode(kCBEnqueueAudio.at(formatOut));
                convertHelper(formatOut, formatIn);
                instance.decode(kCBDequeueAudio.at(formatIn));

                {
                    GGWave::TxRxData result;
                    CHECK(instance.takeRxData(result) == (int) payload.size());
                    for (int i = 0; i < (int) payload.size(); ++i) {
                        CHECK(payload[i] == result[i]);
                    }
                }
            }
        }
    }

    return 0;
}
