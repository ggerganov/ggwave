#include "ggwave/ggwave.h"

#include "reed-solomon/rs.hpp"

#include <chrono>
#include <cmath>
#include <algorithm>
//#include <random>
#include <stdexcept>
#include <map>

//
// C interface
//

namespace {
std::map<ggwave_Instance, GGWave *> g_instances;
}

extern "C"
ggwave_Parameters ggwave_getDefaultParameters(void) {
    return GGWave::getDefaultParameters();
}

extern "C"
ggwave_Instance ggwave_init(const ggwave_Parameters parameters) {
    static ggwave_Instance curId = 0;

    g_instances[curId] = new GGWave({
            parameters.sampleRateInp,
            parameters.sampleRateOut,
            parameters.samplesPerFrame,
            parameters.sampleFormatInp,
            parameters.sampleFormatOut});

    return curId++;
}

extern "C"
void ggwave_free(ggwave_Instance instance) {
    delete (GGWave *) g_instances[instance];
    g_instances.erase(instance);
}

extern "C"
int ggwave_encode(
        ggwave_Instance instance,
        const char * dataBuffer,
        int dataSize,
        ggwave_TxProtocolId txProtocolId,
        int volume,
        char * outputBuffer,
        int query) {
    GGWave * ggWave = (GGWave *) g_instances[instance];

    if (ggWave == nullptr) {
        fprintf(stderr, "Invalid GGWave instance %d\n", instance);
        return -1;
    }

    if (ggWave->init(dataSize, dataBuffer, ggWave->getTxProtocol(txProtocolId), volume) == false) {
        fprintf(stderr, "Failed to initialize GGWave instance %d\n", instance);
        return -1;
    }

    if (query != 0) {
        if (query == 1) {
            return ggWave->encodeSize_bytes();
        }

        return ggWave->encodeSize_samples();
    }

    int nSamples = 0;

    GGWave::CBWaveformOut cbWaveformOut = [&](const void * data, uint32_t nBytes) {
        char * p = (char *) data;
        std::copy(p, p + nBytes, outputBuffer);

        nSamples = nBytes/ggWave->getSampleSizeBytesOut();
    };

    if (ggWave->encode(cbWaveformOut) == false) {
        fprintf(stderr, "Failed to encode data - GGWave instance %d\n", instance);
        return -1;
    }

    return nSamples;
}

extern "C"
int ggwave_decode(
        ggwave_Instance instance,
        const char * dataBuffer,
        int dataSize,
        char * outputBuffer) {
    GGWave * ggWave = (GGWave *) g_instances[instance];

    GGWave::CBWaveformInp cbWaveformInp = [&](void * data, uint32_t nMaxBytes) -> uint32_t {
        uint32_t nCopied = std::min((uint32_t) dataSize, nMaxBytes);
        std::copy(dataBuffer, dataBuffer + nCopied, (char *) data);

        dataSize -= nCopied;
        dataBuffer += nCopied;

        return nCopied;
    };

    ggWave->decode(cbWaveformInp);

    // todo : avoid allocation
    GGWave::TxRxData rxData;

    auto rxDataLength = ggWave->takeRxData(rxData);
    if (rxDataLength == -1) {
        // failed to decode message
        return -1;
    } else if (rxDataLength > 0) {
        std::copy(rxData.begin(), rxData.end(), outputBuffer);
    }

    return rxDataLength;
}

//
// C++ implementation
//

namespace {

// FFT routines taken from https://stackoverflow.com/a/37729648/4039976

int log2(int N) {
    int k = N, i = 0;
    while(k) {
        k >>= 1;
        i++;
    }
    return i - 1;
}

int reverse(int N, int n) {
    int j, p = 0;
    for(j = 1; j <= log2(N); j++) {
        if(n & (1 << (log2(N) - j)))
            p |= 1 << (j - 1);
    }
    return p;
}

void ordina(float * f1, int N) {
    float f2[2*GGWave::kMaxSamplesPerFrame];
    for (int i = 0; i < N; i++) {
        int ir = reverse(N, i);
        f2[2*i + 0] = f1[2*ir + 0];
        f2[2*i + 1] = f1[2*ir + 1];
    }
    for (int j = 0; j < N; j++) {
        f1[2*j + 0] = f2[2*j + 0];
        f1[2*j + 1] = f2[2*j + 1];
    }
}

void transform(float * f, int N) {
    ordina(f, N);    //first: reverse order
    float * W;
    W = (float *)malloc(N*sizeof(float));
    W[2*1 + 0] = cos(-2.*M_PI/N);
    W[2*1 + 1] = sin(-2.*M_PI/N);
    W[2*0 + 0] = 1;
    W[2*0 + 1] = 0;
    for (int i = 2; i < N / 2; i++) {
        W[2*i + 0] = cos(-2.*i*M_PI/N);
        W[2*i + 1] = sin(-2.*i*M_PI/N);
    }
    int n = 1;
    int a = N / 2;
    for(int j = 0; j < log2(N); j++) {
        for(int i = 0; i < N; i++) {
            if(!(i & n)) {
                int wi = (i * a) % (n * a);
                int fi = i + n;
                float a = W[2*wi + 0];
                float b = W[2*wi + 1];
                float c = f[2*fi + 0];
                float d = f[2*fi + 1];
                float temp[2] = { f[2*i + 0], f[2*i + 1] };
                float Temp[2] = { a*c - b*d, b*c + a*d };
                f[2*i + 0]  = temp[0] + Temp[0];
                f[2*i + 1]  = temp[1] + Temp[1];
                f[2*fi + 0] = temp[0] - Temp[0];
                f[2*fi + 1] = temp[1] - Temp[1];
            }
        }
        n *= 2;
        a = a / 2;
    }
    free(W);
}

void FFT(float * f, int N, float d) {
    transform(f, N);
    for (int i = 0; i < N; i++) {
        f[2*i + 0] *= d;
        f[2*i + 1] *= d;
    }
}

void FFT(float * src, float * dst, int N, float d) {
    for (int i = 0; i < N; ++i) {
        dst[2*i + 0] = src[i];
        dst[2*i + 1] = 0.0f;
    }
    FFT(dst, N, d);
}

inline void addAmplitudeSmooth(
        const GGWave::AmplitudeData & src,
        GGWave::AmplitudeData & dst,
        float scalar, int startId, int finalId, int cycleMod, int nPerCycle) {
    int nTotal = nPerCycle*finalId;
    float frac = 0.15f;
    float ds = frac*nTotal;
    float ids = 1.0f/ds;
    int nBegin = frac*nTotal;
    int nEnd = (1.0f - frac)*nTotal;
    for (int i = startId; i < finalId; i++) {
        float k = cycleMod*finalId + i;
        if (k < nBegin) {
            dst[i] += scalar*src[i]*(k*ids);
        } else if (k > nEnd) {
            dst[i] += scalar*src[i]*(((float)(nTotal) - k)*ids);
        } else {
            dst[i] += scalar*src[i];
        }
    }
}

template <class T>
float getTime_ms(const T & tStart, const T & tEnd) {
    return ((float)(std::chrono::duration_cast<std::chrono::microseconds>(tEnd - tStart).count()))/1000.0;
}

int getECCBytesForLength(int len) {
    return std::max(4, 2*(len/5));
}

int bytesForSampleFormat(GGWave::SampleFormat sampleFormat) {
    switch (sampleFormat) {
        case GGWAVE_SAMPLE_FORMAT_UNDEFINED:    return 0;                   break;
        case GGWAVE_SAMPLE_FORMAT_U8:           return sizeof(uint8_t);     break;
        case GGWAVE_SAMPLE_FORMAT_I8:           return sizeof(int8_t);      break;
        case GGWAVE_SAMPLE_FORMAT_U16:          return sizeof(uint16_t);    break;
        case GGWAVE_SAMPLE_FORMAT_I16:          return sizeof(int16_t);     break;
        case GGWAVE_SAMPLE_FORMAT_F32:          return sizeof(float);       break;
    };

    fprintf(stderr, "Invalid sample format: %d\n", (int) sampleFormat);

    return 0;
}

}

const GGWave::Parameters & GGWave::getDefaultParameters() {
    static ggwave_Parameters result {
        GGWave::kBaseSampleRate,
        GGWave::kBaseSampleRate,
        GGWave::kDefaultSamplesPerFrame,
        GGWAVE_SAMPLE_FORMAT_F32,
        GGWAVE_SAMPLE_FORMAT_F32,
    };

    return result;
}

GGWave::GGWave(const Parameters & parameters) :
    m_sampleRateInp(parameters.sampleRateInp),
    m_sampleRateOut(parameters.sampleRateOut),
    m_samplesPerFrame(parameters.samplesPerFrame),
    m_isamplesPerFrame(1.0f/m_samplesPerFrame),
    m_sampleSizeBytesInp(bytesForSampleFormat(parameters.sampleFormatInp)),
    m_sampleSizeBytesOut(bytesForSampleFormat(parameters.sampleFormatOut)),
    m_sampleFormatInp(parameters.sampleFormatInp),
    m_sampleFormatOut(parameters.sampleFormatOut),
    m_hzPerSample(m_sampleRateInp/parameters.samplesPerFrame),
    m_ihzPerSample(1.0f/m_hzPerSample),
    m_freqDelta_bin(1),
    m_freqDelta_hz(2*m_hzPerSample),
    m_nBitsInMarker(16),
    m_nMarkerFrames(16),
    m_nPostMarkerFrames(0),
    m_encodedDataOffset(3),
    m_samplesNeeded(m_samplesPerFrame),
    m_fftInp(kMaxSamplesPerFrame),
    m_fftOut(2*kMaxSamplesPerFrame),
    m_hasNewSpectrum(false),
    m_sampleSpectrum(kMaxSamplesPerFrame),
    m_sampleAmplitude(kMaxSamplesPerFrame),
    m_sampleAmplitudeTmp(kMaxSamplesPerFrame*m_sampleSizeBytesInp),
    m_hasNewRxData(false),
    m_lastRxDataLength(0),
    m_rxData(kMaxDataSize),
    m_sampleAmplitudeAverage(kMaxSamplesPerFrame),
    m_sampleAmplitudeHistory(kMaxSpectrumHistory),
    m_recordedAmplitude(kMaxRecordedFrames*kMaxSamplesPerFrame),
    m_txData(kMaxDataSize),
    m_txDataEncoded(kMaxDataSize),
    m_outputBlock(kMaxSamplesPerFrame),
    m_outputBlockTmp(kMaxRecordedFrames*kMaxSamplesPerFrame*m_sampleSizeBytesOut),
    m_outputBlockI16(kMaxRecordedFrames*kMaxSamplesPerFrame) {

    if (m_sampleSizeBytesInp == 0) {
        throw std::runtime_error("Invalid or unsupported capture sample format");
    }

    if (m_sampleSizeBytesOut == 0) {
        throw std::runtime_error("Invalid or unsupported playback sample format");
    }

    if (parameters.samplesPerFrame > kMaxSamplesPerFrame) {
        throw std::runtime_error("Invalid samples per frame");
    }

    if (m_sampleRateInp < m_sampleRateOut) {
        fprintf(stderr, "Error: capture sample rate (%d Hz) must be >= playback sample rate (%d Hz)\n", (int) m_sampleRateInp, (int) m_sampleRateOut);
        throw std::runtime_error("Invalid capture/playback sample rate");
    }

    init("", getDefaultTxProtocol(), 0);
}

GGWave::~GGWave() {
}

bool GGWave::init(const std::string & text, const int volume) {
    return init(text.size(), text.data(), getDefaultTxProtocol(), volume);
}

bool GGWave::init(const std::string & text, const TxProtocol & txProtocol, const int volume) {
    return init(text.size(), text.data(), txProtocol, volume);
}

bool GGWave::init(int dataSize, const char * dataBuffer, const int volume) {
    return init(dataSize, dataBuffer, getDefaultTxProtocol(), volume);
}

bool GGWave::init(int dataSize, const char * dataBuffer, const TxProtocol & txProtocol, const int volume) {
    if (dataSize < 0) {
        fprintf(stderr, "Negative data size: %d\n", dataSize);
        return false;
    }

    if (dataSize > kMaxLength) {
        fprintf(stderr, "Truncating data from %d to 140 bytes\n", dataSize);
        dataSize = kMaxLength;
    }

    if (volume < 0 || volume > 100) {
        fprintf(stderr, "Invalid volume: %d\n", volume);
        return false;
    }

    m_txProtocol = txProtocol;
    m_txDataLength = dataSize;
    m_sendVolume = ((double)(volume))/100.0f;

    const uint8_t * text = reinterpret_cast<const uint8_t *>(dataBuffer);

    m_hasNewTxData = false;
    std::fill(m_txData.begin(), m_txData.end(), 0);
    std::fill(m_txDataEncoded.begin(), m_txDataEncoded.end(), 0);

    if (m_txDataLength > 0) {
        m_txData[0] = m_txDataLength;
        for (int i = 0; i < m_txDataLength; ++i) m_txData[i + 1] = text[i];

        m_hasNewTxData = true;
    }

    // Rx
    m_receivingData = false;
    m_analyzingData = false;

    m_framesToAnalyze = 0;
    m_framesLeftToAnalyze = 0;
    m_framesToRecord = 0;
    m_framesLeftToRecord = 0;

    std::fill(m_sampleSpectrum.begin(), m_sampleSpectrum.end(), 0);
    std::fill(m_sampleAmplitude.begin(), m_sampleAmplitude.end(), 0);
    for (auto & s : m_sampleAmplitudeHistory) {
        s.resize(kMaxSamplesPerFrame);
        std::fill(s.begin(), s.end(), 0);
    }

    std::fill(m_rxData.begin(), m_rxData.end(), 0);

    for (int i = 0; i < m_samplesPerFrame; ++i) {
        m_fftOut[2*i + 0] = 0.0f;
        m_fftOut[2*i + 1] = 0.0f;
    }

    return true;
}

uint32_t GGWave::encodeSize_bytes() const {
    return encodeSize_samples()*m_sampleSizeBytesOut;
}

uint32_t GGWave::encodeSize_samples() const {
    if (m_hasNewTxData == false) {
        return 0;
    }

    int samplesPerFrameOut = (m_sampleRateOut/m_sampleRateInp)*m_samplesPerFrame;
    int nECCBytesPerTx = getECCBytesForLength(m_txDataLength);
    int sendDataLength = m_txDataLength + m_encodedDataOffset;
    int totalBytes = sendDataLength + nECCBytesPerTx;
    int totalDataFrames = ((totalBytes + m_txProtocol.bytesPerTx - 1)/m_txProtocol.bytesPerTx)*m_txProtocol.framesPerTx;

    return (
            m_nMarkerFrames + m_nPostMarkerFrames + totalDataFrames + m_nMarkerFrames
           )*samplesPerFrameOut;
}

bool GGWave::encode(const CBWaveformOut & cbWaveformOut) {
    int samplesPerFrameOut = (m_sampleRateOut/m_sampleRateInp)*m_samplesPerFrame;

    if (m_sampleRateOut != m_sampleRateInp) {
        fprintf(stderr, "Resampling from %d Hz to %d Hz\n", (int) m_sampleRateInp, (int) m_sampleRateOut);
    }

    int frameId = 0;

    std::vector<double> phaseOffsets(kMaxDataBits);

    for (int k = 0; k < (int) phaseOffsets.size(); ++k) {
        phaseOffsets[k] = (M_PI*k)/(m_txProtocol.nDataBitsPerTx());
    }

    // note : what is the purpose of this shuffle ? I forgot .. :(
    //std::random_device rd;
    //std::mt19937 g(rd());

    //std::shuffle(phaseOffsets.begin(), phaseOffsets.end(), g);

    std::vector<bool> dataBits(kMaxDataBits);

    std::vector<AmplitudeData> bit1Amplitude(kMaxDataBits);
    std::vector<AmplitudeData> bit0Amplitude(kMaxDataBits);

    for (int k = 0; k < (int) dataBits.size(); ++k) {
        double freq = bitFreq(m_txProtocol, k);

        bit1Amplitude[k].resize(kMaxSamplesPerFrame);
        bit0Amplitude[k].resize(kMaxSamplesPerFrame);

        double phaseOffset = phaseOffsets[k];
        double curHzPerSample = m_sampleRateOut/m_samplesPerFrame;
        double curIHzPerSample = 1.0/curHzPerSample;
        for (int i = 0; i < m_samplesPerFrame; i++) {
            double curi = i;
            bit1Amplitude[k][i] = std::sin((2.0*M_PI)*(curi*m_isamplesPerFrame)*(freq*curIHzPerSample) + phaseOffset);
        }
        for (int i = 0; i < m_samplesPerFrame; i++) {
            double curi = i;
            bit0Amplitude[k][i] = std::sin((2.0*M_PI)*(curi*m_isamplesPerFrame)*((freq + m_hzPerSample*m_freqDelta_bin)*curIHzPerSample) + phaseOffset);
        }
    }

    int nECCBytesPerTx = getECCBytesForLength(m_txDataLength);
    int sendDataLength = m_txDataLength + m_encodedDataOffset;
    int totalBytes = sendDataLength + nECCBytesPerTx;
    int totalDataFrames = ((totalBytes + m_txProtocol.bytesPerTx - 1)/m_txProtocol.bytesPerTx)*m_txProtocol.framesPerTx;

    RS::ReedSolomon rsData = RS::ReedSolomon(m_txDataLength, nECCBytesPerTx);
    RS::ReedSolomon rsLength(1, m_encodedDataOffset - 1);

    rsLength.Encode(m_txData.data(), m_txDataEncoded.data());
    rsData.Encode(m_txData.data() + 1, m_txDataEncoded.data() + m_encodedDataOffset);

    while (m_hasNewTxData) {
        std::fill(m_outputBlock.begin(), m_outputBlock.end(), 0.0f);

        if (m_sampleRateOut != m_sampleRateInp) {
            for (int k = 0; k < m_txProtocol.nDataBitsPerTx(); ++k) {
                double freq = bitFreq(m_txProtocol, k);

                double phaseOffset = phaseOffsets[k];
                double curHzPerSample = m_sampleRateOut/m_samplesPerFrame;
                double curIHzPerSample = 1.0/curHzPerSample;
                for (int i = 0; i < samplesPerFrameOut; i++) {
                    double curi = (i + frameId*samplesPerFrameOut);
                    bit1Amplitude[k][i] = std::sin((2.0*M_PI)*(curi*m_isamplesPerFrame)*(freq*curIHzPerSample) + phaseOffset);
                }
                for (int i = 0; i < samplesPerFrameOut; i++) {
                    double curi = (i + frameId*samplesPerFrameOut);
                    bit0Amplitude[k][i] = std::sin((2.0*M_PI)*(curi*m_isamplesPerFrame)*((freq + m_hzPerSample*m_freqDelta_bin)*curIHzPerSample) + phaseOffset);
                }
            }
        }

        std::uint16_t nFreq = 0;
        if (frameId < m_nMarkerFrames) {
            nFreq = m_nBitsInMarker;

            for (int i = 0; i < m_nBitsInMarker; ++i) {
                if (i%2 == 0) {
                    ::addAmplitudeSmooth(bit1Amplitude[i], m_outputBlock, m_sendVolume, 0, samplesPerFrameOut, frameId, m_nMarkerFrames);
                } else {
                    ::addAmplitudeSmooth(bit0Amplitude[i], m_outputBlock, m_sendVolume, 0, samplesPerFrameOut, frameId, m_nMarkerFrames);
                }
            }
        } else if (frameId < m_nMarkerFrames + m_nPostMarkerFrames) {
            nFreq = m_nBitsInMarker;

            for (int i = 0; i < m_nBitsInMarker; ++i) {
                if (i%2 == 0) {
                    ::addAmplitudeSmooth(bit0Amplitude[i], m_outputBlock, m_sendVolume, 0, samplesPerFrameOut, frameId - m_nMarkerFrames, m_nPostMarkerFrames);
                } else {
                    ::addAmplitudeSmooth(bit1Amplitude[i], m_outputBlock, m_sendVolume, 0, samplesPerFrameOut, frameId - m_nMarkerFrames, m_nPostMarkerFrames);
                }
            }
        } else if (frameId < m_nMarkerFrames + m_nPostMarkerFrames + totalDataFrames) {
            int dataOffset = frameId - m_nMarkerFrames - m_nPostMarkerFrames;
            int cycleModMain = dataOffset%m_txProtocol.framesPerTx;
            dataOffset /= m_txProtocol.framesPerTx;
            dataOffset *= m_txProtocol.bytesPerTx;

            std::fill(dataBits.begin(), dataBits.end(), 0);

            for (int j = 0; j < m_txProtocol.bytesPerTx; ++j) {
                {
                    uint8_t d = m_txDataEncoded[dataOffset + j] & 15;
                    dataBits[(2*j + 0)*16 + d] = 1;
                }
                {
                    uint8_t d = m_txDataEncoded[dataOffset + j] & 240;
                    dataBits[(2*j + 1)*16 + (d >> 4)] = 1;
                }
            }

            for (int k = 0; k < 2*m_txProtocol.bytesPerTx*16; ++k) {
                if (dataBits[k] == 0) continue;

                ++nFreq;
                if (k%2) {
                    ::addAmplitudeSmooth(bit0Amplitude[k/2], m_outputBlock, m_sendVolume, 0, samplesPerFrameOut, cycleModMain, m_txProtocol.framesPerTx);
                } else {
                    ::addAmplitudeSmooth(bit1Amplitude[k/2], m_outputBlock, m_sendVolume, 0, samplesPerFrameOut, cycleModMain, m_txProtocol.framesPerTx);
                }
            }
        } else if (frameId < m_nMarkerFrames + m_nPostMarkerFrames + totalDataFrames + m_nMarkerFrames) {
            nFreq = m_nBitsInMarker;

            int fId = frameId - (m_nMarkerFrames + m_nPostMarkerFrames + totalDataFrames);
            for (int i = 0; i < m_nBitsInMarker; ++i) {
                if (i%2 == 0) {
                    addAmplitudeSmooth(bit0Amplitude[i], m_outputBlock, m_sendVolume, 0, samplesPerFrameOut, fId, m_nMarkerFrames);
                } else {
                    addAmplitudeSmooth(bit1Amplitude[i], m_outputBlock, m_sendVolume, 0, samplesPerFrameOut, fId, m_nMarkerFrames);
                }
            }
        } else {
            m_hasNewTxData = false;
            break;
        }

        if (nFreq == 0) nFreq = 1;
        float scale = 1.0f/nFreq;
        for (int i = 0; i < samplesPerFrameOut; ++i) {
            m_outputBlock[i] *= scale;
        }

        uint32_t offset = frameId*samplesPerFrameOut;

        // default output is in 16-bit signed int so we always compute it
        for (int i = 0; i < samplesPerFrameOut; ++i) {
            m_outputBlockI16[offset + i] = 32768*m_outputBlock[i];
        }

        // convert from 32-bit float
        switch (m_sampleFormatOut) {
            case GGWAVE_SAMPLE_FORMAT_UNDEFINED: break;
            case GGWAVE_SAMPLE_FORMAT_U8:
                {
                    auto p = reinterpret_cast<uint8_t *>(m_outputBlockTmp.data());
                    for (int i = 0; i < samplesPerFrameOut; ++i) {
                        p[offset + i] = 128*(m_outputBlock[i] + 1.0f);
                    }
                } break;
            case GGWAVE_SAMPLE_FORMAT_I8:
                {
                    auto p = reinterpret_cast<uint8_t *>(m_outputBlockTmp.data());
                    for (int i = 0; i < samplesPerFrameOut; ++i) {
                        p[offset + i] = 128*m_outputBlock[i];
                    }
                } break;
            case GGWAVE_SAMPLE_FORMAT_U16:
                {
                    auto p = reinterpret_cast<uint16_t *>(m_outputBlockTmp.data());
                    for (int i = 0; i < samplesPerFrameOut; ++i) {
                        p[offset + i] = 32768*(m_outputBlock[i] + 1.0f);
                    }
                } break;
            case GGWAVE_SAMPLE_FORMAT_I16:
                {
                    // skip because we already have the data in m_outputBlockI16
                    //auto p = reinterpret_cast<uint16_t *>(m_outputBlockTmp.data());
                    //for (int i = 0; i < samplesPerFrameOut; ++i) {
                    //    p[offset + i] = 32768*m_outputBlock[i];
                    //}
                } break;
            case GGWAVE_SAMPLE_FORMAT_F32:
                {
                    auto p = reinterpret_cast<float *>(m_outputBlockTmp.data());
                    for (int i = 0; i < samplesPerFrameOut; ++i) {
                        p[offset + i] = m_outputBlock[i];
                    }
                } break;
        }

        ++frameId;
    }

    switch (m_sampleFormatOut) {
        case GGWAVE_SAMPLE_FORMAT_UNDEFINED: break;
        case GGWAVE_SAMPLE_FORMAT_I16:
            {
                cbWaveformOut(m_outputBlockI16.data(), frameId*samplesPerFrameOut*m_sampleSizeBytesOut);
            } break;
        case GGWAVE_SAMPLE_FORMAT_U8:
        case GGWAVE_SAMPLE_FORMAT_I8:
        case GGWAVE_SAMPLE_FORMAT_U16:
        case GGWAVE_SAMPLE_FORMAT_F32:
            {
                cbWaveformOut(m_outputBlockTmp.data(), frameId*samplesPerFrameOut*m_sampleSizeBytesOut);
            } break;
    }

    m_txAmplitudeDataI16.resize(frameId*samplesPerFrameOut);
    for (int i = 0; i < frameId*samplesPerFrameOut; ++i) {
        m_txAmplitudeDataI16[i] = m_outputBlockI16[i];
    }

    return true;
}

void GGWave::decode(const CBWaveformInp & cbWaveformInp) {
    while (m_hasNewTxData == false) {
        // read capture data
        uint32_t nBytesNeeded = m_samplesNeeded*m_sampleSizeBytesInp;
        uint32_t nBytesRecorded = 0;
        uint32_t offset = m_samplesPerFrame - m_samplesNeeded;

        switch (m_sampleFormatInp) {
            case GGWAVE_SAMPLE_FORMAT_UNDEFINED: break;
            case GGWAVE_SAMPLE_FORMAT_U8:
            case GGWAVE_SAMPLE_FORMAT_I8:
            case GGWAVE_SAMPLE_FORMAT_U16:
            case GGWAVE_SAMPLE_FORMAT_I16:
                {
                    nBytesRecorded = cbWaveformInp(m_sampleAmplitudeTmp.data() + offset, nBytesNeeded);
                } break;
            case GGWAVE_SAMPLE_FORMAT_F32:
                {
                    nBytesRecorded = cbWaveformInp(m_sampleAmplitude.data() + offset, nBytesNeeded);
                } break;
        }

        if (nBytesRecorded % m_sampleSizeBytesInp != 0) {
            fprintf(stderr, "Failure during capture - provided bytes (%d) are not multiple of sample size (%d)\n",
                    nBytesRecorded, m_sampleSizeBytesInp);
            m_samplesNeeded = m_samplesPerFrame;
            break;
        }

        if (nBytesRecorded > nBytesNeeded) {
            fprintf(stderr, "Failure during capture - more samples were provided (%d) than requested (%d)\n",
                    nBytesRecorded/m_sampleSizeBytesInp, nBytesNeeded/m_sampleSizeBytesInp);
            m_samplesNeeded = m_samplesPerFrame;
            break;
        }

        // convert to 32-bit float
        switch (m_sampleFormatInp) {
            case GGWAVE_SAMPLE_FORMAT_UNDEFINED: break;
            case GGWAVE_SAMPLE_FORMAT_U8:
                {
                    constexpr float scale = 1.0f/128;
                    int nSamplesRecorded = nBytesRecorded/m_sampleSizeBytesInp;
                    auto p = reinterpret_cast<uint8_t *>(m_sampleAmplitudeTmp.data());
                    for (int i = 0; i < nSamplesRecorded; ++i) {
                        m_sampleAmplitude[offset + i] = float(int16_t(*(p + offset + i)) - 128)*scale;
                    }
                } break;
            case GGWAVE_SAMPLE_FORMAT_I8:
                {
                    constexpr float scale = 1.0f/128;
                    int nSamplesRecorded = nBytesRecorded/m_sampleSizeBytesInp;
                    auto p = reinterpret_cast<int8_t *>(m_sampleAmplitudeTmp.data());
                    for (int i = 0; i < nSamplesRecorded; ++i) {
                        m_sampleAmplitude[offset + i] = float(*(p + offset + i))*scale;
                    }
                } break;
            case GGWAVE_SAMPLE_FORMAT_U16:
                {
                    constexpr float scale = 1.0f/32768;
                    int nSamplesRecorded = nBytesRecorded/m_sampleSizeBytesInp;
                    auto p = reinterpret_cast<uint16_t *>(m_sampleAmplitudeTmp.data());
                    for (int i = 0; i < nSamplesRecorded; ++i) {
                        m_sampleAmplitude[offset + i] = float(int32_t(*(p + offset + i)) - 32768)*scale;
                    }
                } break;
            case GGWAVE_SAMPLE_FORMAT_I16:
                {
                    constexpr float scale = 1.0f/32768;
                    int nSamplesRecorded = nBytesRecorded/m_sampleSizeBytesInp;
                    auto p = reinterpret_cast<int16_t *>(m_sampleAmplitudeTmp.data());
                    for (int i = 0; i < nSamplesRecorded; ++i) {
                        m_sampleAmplitude[offset + i] = float(*(p + offset + i))*scale;
                    }
                } break;
            case GGWAVE_SAMPLE_FORMAT_F32: break;
        }

        // we have enough bytes to do analysis
        if (nBytesRecorded == nBytesNeeded) {
            m_samplesNeeded = m_samplesPerFrame;
            m_sampleAmplitudeHistory[m_historyId] = m_sampleAmplitude;

            if (++m_historyId >= kMaxSpectrumHistory) {
                m_historyId = 0;
            }

            if (m_historyId == 0 || m_receivingData) {
                m_hasNewSpectrum = true;

                std::fill(m_sampleAmplitudeAverage.begin(), m_sampleAmplitudeAverage.end(), 0.0f);
                for (auto & s : m_sampleAmplitudeHistory) {
                    for (int i = 0; i < m_samplesPerFrame; ++i) {
                        m_sampleAmplitudeAverage[i] += s[i];
                    }
                }

                float norm = 1.0f/kMaxSpectrumHistory;
                for (int i = 0; i < m_samplesPerFrame; ++i) {
                    m_sampleAmplitudeAverage[i] *= norm;
                }

                // calculate spectrum
                std::copy(m_sampleAmplitudeAverage.begin(), m_sampleAmplitudeAverage.begin() + m_samplesPerFrame, m_fftInp.data());

                FFT(m_fftInp.data(), m_fftOut.data(), m_samplesPerFrame, 1.0);

                for (int i = 0; i < m_samplesPerFrame; ++i) {
                    m_sampleSpectrum[i] = (m_fftOut[2*i + 0]*m_fftOut[2*i + 0] + m_fftOut[2*i + 1]*m_fftOut[2*i + 1]);
                }
                for (int i = 1; i < m_samplesPerFrame/2; ++i) {
                    m_sampleSpectrum[i] += m_sampleSpectrum[m_samplesPerFrame - i];
                }
            }

            if (m_framesLeftToRecord > 0) {
                std::copy(m_sampleAmplitude.begin(),
                          m_sampleAmplitude.begin() + m_samplesPerFrame,
                          m_recordedAmplitude.data() + (m_framesToRecord - m_framesLeftToRecord)*m_samplesPerFrame);

                if (--m_framesLeftToRecord <= 0) {
                    m_analyzingData = true;
                }
            }

            if (m_analyzingData) {
                fprintf(stderr, "Analyzing captured data ..\n");
                auto tStart = std::chrono::high_resolution_clock::now();

                const int stepsPerFrame = 16;
                const int step = m_samplesPerFrame/stepsPerFrame;

                bool isValid = false;
                for (int rxProtocolId = 0; rxProtocolId < (int) getTxProtocols().size(); ++rxProtocolId) {
                    const auto & rxProtocol = getTxProtocol(rxProtocolId);

                    // skip Rx protocol if start frequency is different from detected one
                    if (rxProtocol.freqStart != m_markerFreqStart) {
                        continue;
                    }

                    std::fill(m_sampleSpectrum.begin(), m_sampleSpectrum.end(), 0.0f);

                    m_framesToAnalyze = m_nMarkerFrames*stepsPerFrame;
                    m_framesLeftToAnalyze = m_framesToAnalyze;
                    for (int ii = m_nMarkerFrames*stepsPerFrame - 1; ii >= m_nMarkerFrames*stepsPerFrame/2; --ii) {
                        bool knownLength = false;

                        const int offsetStart = ii;
                        for (int itx = 0; itx < 1024; ++itx) {
                            int offsetTx = offsetStart + itx*rxProtocol.framesPerTx*stepsPerFrame;
                            if (offsetTx >= m_recvDuration_frames*stepsPerFrame || (itx + 1)*rxProtocol.bytesPerTx >= (int) m_txDataEncoded.size()) {
                                break;
                            }

                            std::copy(
                                    m_recordedAmplitude.begin() + offsetTx*step,
                                    m_recordedAmplitude.begin() + offsetTx*step + m_samplesPerFrame, m_fftInp.data());

                            for (int k = 1; k < rxProtocol.framesPerTx - 1; ++k) {
                                for (int i = 0; i < m_samplesPerFrame; ++i) {
                                    m_fftInp[i] += m_recordedAmplitude[(offsetTx + k*stepsPerFrame)*step + i];
                                }
                            }

                            FFT(m_fftInp.data(), m_fftOut.data(), m_samplesPerFrame, 1.0);

                            for (int i = 0; i < m_samplesPerFrame; ++i) {
                                m_sampleSpectrum[i] = (m_fftOut[2*i + 0]*m_fftOut[2*i + 0] + m_fftOut[2*i + 1]*m_fftOut[2*i + 1]);
                            }
                            for (int i = 1; i < m_samplesPerFrame/2; ++i) {
                                m_sampleSpectrum[i] += m_sampleSpectrum[m_samplesPerFrame - i];
                            }

                            uint8_t curByte = 0;
                            for (int i = 0; i < 2*rxProtocol.bytesPerTx; ++i) {
                                double freq = m_hzPerSample*rxProtocol.freqStart;
                                int bin = std::round(freq*m_ihzPerSample) + 16*i;

                                int kmax = 0;
                                double amax = 0.0;
                                for (int k = 0; k < 16; ++k) {
                                    if (m_sampleSpectrum[bin + k] > amax) {
                                        kmax = k;
                                        amax = m_sampleSpectrum[bin + k];
                                    }
                                }

                                if (i%2) {
                                    curByte += (kmax << 4);
                                    m_txDataEncoded[itx*rxProtocol.bytesPerTx + i/2] = curByte;
                                    curByte = 0;
                                } else {
                                    curByte = kmax;
                                }
                            }

                            if (itx*rxProtocol.bytesPerTx > m_encodedDataOffset && knownLength == false) {
                                RS::ReedSolomon rsLength(1, m_encodedDataOffset - 1);
                                if ((rsLength.Decode(m_txDataEncoded.data(), m_rxData.data()) == 0) && (m_rxData[0] > 0 && m_rxData[0] <= 140)) {
                                    knownLength = true;
                                } else {
                                    break;
                                }
                            }

                            if (knownLength && itx*rxProtocol.bytesPerTx > m_encodedDataOffset + m_rxData[0] + ::getECCBytesForLength(m_rxData[0]) + 1) {
                                break;
                            }
                        }

                        if (knownLength) {
                            int decodedLength = m_rxData[0];

                            RS::ReedSolomon rsData(decodedLength, ::getECCBytesForLength(decodedLength));

                            if (rsData.Decode(m_txDataEncoded.data() + m_encodedDataOffset, m_rxData.data()) == 0) {
                                if (m_rxData[0] != 0) {
                                    std::string s((char *) m_rxData.data(), decodedLength);

                                    fprintf(stderr, "Decoded length = %d\n", decodedLength);
                                    fprintf(stderr, "Received sound data successfully: '%s'\n", s.c_str());

                                    isValid = true;
                                    m_hasNewRxData = true;
                                    m_lastRxDataLength = decodedLength;
                                    m_rxProtocol = rxProtocol;
                                    m_rxProtocolId = TxProtocolId(rxProtocolId);
                                }
                            }
                        }

                        if (isValid) {
                            break;
                        }
                        --m_framesLeftToAnalyze;
                    }

                    if (isValid) break;
                }

                m_framesToRecord = 0;

                if (isValid == false) {
                    fprintf(stderr, "Failed to capture sound data. Please try again\n");
                    m_lastRxDataLength = -1;
                    m_framesToRecord = -1;
                }

                m_receivingData = false;
                m_analyzingData = false;

                std::fill(m_sampleSpectrum.begin(), m_sampleSpectrum.end(), 0.0f);

                m_framesToAnalyze = 0;
                m_framesLeftToAnalyze = 0;

                auto tEnd = std::chrono::high_resolution_clock::now();
                fprintf(stderr, "Time to analyze: %g ms\n", getTime_ms(tStart, tEnd));
            }

            // check if receiving data
            if (m_receivingData == false) {
                bool isReceiving = false;

                for (const auto & rxProtocol : getTxProtocols()) {
                    int nDetectedMarkerBits = m_nBitsInMarker;

                    for (int i = 0; i < m_nBitsInMarker; ++i) {
                        double freq = bitFreq(rxProtocol.second, i);
                        int bin = std::round(freq*m_ihzPerSample);

                        if (i%2 == 0) {
                            if (m_sampleSpectrum[bin] <= 3.0f*m_sampleSpectrum[bin + m_freqDelta_bin]) --nDetectedMarkerBits;
                        } else {
                            if (m_sampleSpectrum[bin] >= 3.0f*m_sampleSpectrum[bin + m_freqDelta_bin]) --nDetectedMarkerBits;
                        }
                    }

                    if (nDetectedMarkerBits == m_nBitsInMarker) {
                        m_markerFreqStart = rxProtocol.second.freqStart;
                        isReceiving = true;
                        break;
                    }
                }

                if (isReceiving) {
                    std::time_t timestamp = std::time(nullptr);
                    fprintf(stderr, "%sReceiving sound data ...\n", std::asctime(std::localtime(&timestamp)));

                    m_receivingData = true;
                    std::fill(m_rxData.begin(), m_rxData.end(), 0);

                    // max recieve duration
                    m_recvDuration_frames =
                        2*m_nMarkerFrames + m_nPostMarkerFrames +
                        maxFramesPerTx()*((kMaxLength + ::getECCBytesForLength(kMaxLength))/minBytesPerTx() + 1);

                    m_framesToRecord = m_recvDuration_frames;
                    m_framesLeftToRecord = m_recvDuration_frames;
                }
            } else {
                bool isEnded = false;

                for (const auto & rxProtocol : getTxProtocols()) {
                    int nDetectedMarkerBits = m_nBitsInMarker;

                    for (int i = 0; i < m_nBitsInMarker; ++i) {
                        double freq = bitFreq(rxProtocol.second, i);
                        int bin = std::round(freq*m_ihzPerSample);

                        if (i%2 == 0) {
                            if (m_sampleSpectrum[bin] >= 3.0f*m_sampleSpectrum[bin + m_freqDelta_bin]) nDetectedMarkerBits--;
                        } else {
                            if (m_sampleSpectrum[bin] <= 3.0f*m_sampleSpectrum[bin + m_freqDelta_bin]) nDetectedMarkerBits--;
                        }
                    }

                    if (nDetectedMarkerBits == m_nBitsInMarker) {
                        isEnded = true;
                        break;
                    }
                }

                if (isEnded && m_framesToRecord > 1) {
                    std::time_t timestamp = std::time(nullptr);
                    fprintf(stderr, "%sReceived end marker. Frames left = %d\n", std::asctime(std::localtime(&timestamp)), m_framesLeftToRecord);
                    m_recvDuration_frames -= m_framesLeftToRecord - 1;
                    m_framesLeftToRecord = 1;
                }
            }
        } else {
            m_samplesNeeded -= nBytesRecorded/m_sampleSizeBytesInp;
            break;
        }
    }
}

int GGWave::takeRxData(TxRxData & dst) {
    if (m_lastRxDataLength == 0) return 0;

    auto res = m_lastRxDataLength;
    m_lastRxDataLength = 0;

    if (res != -1) {
        dst = m_rxData;
    }

    return res;
}

int GGWave::takeTxAmplitudeDataI16(AmplitudeDataI16 & dst) {
    if (m_txAmplitudeDataI16.size() == 0) return 0;

    int res = (int) m_txAmplitudeDataI16.size();
    dst = std::move(m_txAmplitudeDataI16);

    return res;
}

bool GGWave::takeSpectrum(SpectrumData & dst) {
    if (m_hasNewSpectrum == false) return false;

    m_hasNewSpectrum = false;
    dst = m_sampleSpectrum;

    return true;
}

int GGWave::maxFramesPerTx() const {
    int res = 0;
    for (const auto & protocol : getTxProtocols()) {
        res = std::max(res, protocol.second.framesPerTx);
    }
    return res;
}

int GGWave::minBytesPerTx() const {
    int res = getTxProtocols().begin()->second.bytesPerTx;
    for (const auto & protocol : getTxProtocols()) {
        res = std::min(res, protocol.second.bytesPerTx);
    }
    return res;
}

