#include "ggwave/ggwave.h"

#include "resampler.h"

#include "reed-solomon/rs.hpp"

#include <chrono>
#include <cmath>
#include <algorithm>
#include <stdexcept>
#include <map>
//#include <random>

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
            parameters.payloadLength,
            parameters.sampleRateInp,
            parameters.sampleRateOut,
            parameters.samplesPerFrame,
            parameters.soundMarkerThreshold,
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
    static thread_local float f2[2*GGWave::kMaxSamplesPerFrame];
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

void FFT(const float * src, float * dst, int N, float d) {
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
    return len < 4 ? 2 : std::max(4, 2*(len/5));
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

struct GGWave::Impl {
    Resampler resampler;
};

const GGWave::Parameters & GGWave::getDefaultParameters() {
    static ggwave_Parameters result {
        -1, // vaiable payload length
        kBaseSampleRate,
        kBaseSampleRate,
        kDefaultSamplesPerFrame,
        kDefaultSoundMarkerThreshold,
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
    m_hzPerSample(kBaseSampleRate/parameters.samplesPerFrame),
    m_ihzPerSample(1.0f/m_hzPerSample),
    m_freqDelta_bin(1),
    m_freqDelta_hz(2*m_hzPerSample),
    m_nBitsInMarker(16),
    m_nMarkerFrames(parameters.payloadLength > 0 ? 0 : kDefaultMarkerFrames),
    m_encodedDataOffset(parameters.payloadLength > 0 ? 0 : kDefaultEncodedDataOffset),
    m_soundMarkerThreshold(parameters.soundMarkerThreshold),
    // common
    m_isFixedPayloadLength(parameters.payloadLength > 0),
    m_payloadLength(parameters.payloadLength),
    // Rx
    m_samplesNeeded(m_samplesPerFrame),
    m_fftInp(kMaxSamplesPerFrame),
    m_fftOut(2*kMaxSamplesPerFrame),
    m_hasNewSpectrum(false),
    m_hasNewAmplitude(false),
    m_sampleSpectrum(kMaxSamplesPerFrame),
    m_sampleAmplitude(kMaxSamplesPerFrame + 128), // small extra space because sometimes resampling needs a few more samples
    m_sampleAmplitudeResampled(8*kMaxSamplesPerFrame), // min input sampling rate is 0.125*kBaseSampleRate
    m_sampleAmplitudeTmp(8*kMaxSamplesPerFrame*m_sampleSizeBytesInp),
    m_hasNewRxData(false),
    m_lastRxDataLength(0),
    m_rxData(kMaxDataSize),
    m_rxProtocol(getDefaultTxProtocol()),
    m_rxProtocolId(getDefaultTxProtocolId()),
    m_rxProtocols(getTxProtocols()),
    m_historyId(0),
    m_sampleAmplitudeAverage(kMaxSamplesPerFrame),
    m_sampleAmplitudeHistory(kMaxSpectrumHistory),
    m_historyIdFixed(0),
    // Tx
    m_hasNewTxData(false),
    m_sendVolume(0.1),
    m_txDataLength(0),
    m_txData(kMaxDataSize),
    m_txDataEncoded(kMaxDataSize),
    m_outputBlock(kMaxSamplesPerFrame),
    m_outputBlockResampled(2*kMaxSamplesPerFrame),
    m_outputBlockTmp(kMaxRecordedFrames*kMaxSamplesPerFrame*m_sampleSizeBytesOut),
    m_outputBlockI16(kMaxRecordedFrames*kMaxSamplesPerFrame),
    m_impl(new Impl()) {

    if (m_payloadLength > 0) {
        // fixed payload length
        if (m_payloadLength > kMaxLengthFixed) {
            throw std::runtime_error("Invalid payload legnth");
        }

        m_txDataLength = m_payloadLength;

        int totalLength = m_txDataLength + getECCBytesForLength(m_txDataLength);
        int totalTxs = (totalLength + minBytesPerTx() - 1)/minBytesPerTx();

        m_spectrumHistoryFixed.resize(totalTxs*maxFramesPerTx());
    } else {
        // variable payload length
        m_recordedAmplitude.resize(kMaxRecordedFrames*kMaxSamplesPerFrame);
    }

    if (m_sampleSizeBytesInp == 0) {
        throw std::runtime_error("Invalid or unsupported capture sample format");
    }

    if (m_sampleSizeBytesOut == 0) {
        throw std::runtime_error("Invalid or unsupported playback sample format");
    }

    if (parameters.samplesPerFrame > kMaxSamplesPerFrame) {
        throw std::runtime_error("Invalid samples per frame");
    }

    if (m_sampleRateInp < kSampleRateMin) {
        fprintf(stderr, "Error: capture sample rate (%g Hz) must be >= %g Hz\n", m_sampleRateInp, kSampleRateMin);
        throw std::runtime_error("Invalid capture/playback sample rate");
    }

    if (m_sampleRateInp > kSampleRateMax) {
        fprintf(stderr, "Error: capture sample rate (%g Hz) must be <= %g Hz\n", m_sampleRateInp, kSampleRateMax);
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

    auto maxLength = m_isFixedPayloadLength ? m_payloadLength : kMaxLengthVarible;
    if (dataSize > maxLength) {
        fprintf(stderr, "Truncating data from %d to %d bytes\n", dataSize, maxLength);
        dataSize = maxLength;
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

    if (m_isFixedPayloadLength) {
        m_txDataLength = m_payloadLength;
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

    for (auto & s : m_spectrumHistoryFixed) {
        s.resize(kMaxSamplesPerFrame);
        std::fill(s.begin(), s.end(), 0);
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

    float factor = 1.0f;
    int samplesPerFrameOut = m_samplesPerFrame;
    if (m_sampleRateOut != kBaseSampleRate) {
        factor = kBaseSampleRate/m_sampleRateOut;
        // note : +1 extra sample in order to overestimate the buffer size
        samplesPerFrameOut = m_impl->resampler.resample(factor, m_samplesPerFrame, m_outputBlock.data(), nullptr) + 1;
    }
    int nECCBytesPerTx = getECCBytesForLength(m_txDataLength);
    int sendDataLength = m_txDataLength + m_encodedDataOffset;
    int totalBytes = sendDataLength + nECCBytesPerTx;
    int totalDataFrames = ((totalBytes + m_txProtocol.bytesPerTx - 1)/m_txProtocol.bytesPerTx)*m_txProtocol.framesPerTx;

    return (
            m_nMarkerFrames + totalDataFrames + m_nMarkerFrames
           )*samplesPerFrameOut;
}

bool GGWave::encode(const CBWaveformOut & cbWaveformOut) {
    int frameId = 0;

    m_impl->resampler.reset();

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
        double curHzPerSample = m_hzPerSample;
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

    if (m_isFixedPayloadLength == false) {
        RS::ReedSolomon rsLength(1, m_encodedDataOffset - 1);
        rsLength.Encode(m_txData.data(), m_txDataEncoded.data());
    }

    // first byte of m_txData contains the length of the payload, so we skip it:
    RS::ReedSolomon rsData = RS::ReedSolomon(m_txDataLength, nECCBytesPerTx);
    rsData.Encode(m_txData.data() + 1, m_txDataEncoded.data() + m_encodedDataOffset);

    float factor = kBaseSampleRate/m_sampleRateOut;
    uint32_t offset = 0;

    m_waveformTones.clear();

    while (m_hasNewTxData) {
        std::fill(m_outputBlock.begin(), m_outputBlock.end(), 0.0f);

        std::uint16_t nFreq = 0;
        m_waveformTones.push_back({});

        if (frameId < m_nMarkerFrames) {
            nFreq = m_nBitsInMarker;

            for (int i = 0; i < m_nBitsInMarker; ++i) {
                m_waveformTones.back().push_back({});
                m_waveformTones.back().back().duration_ms = (1000.0*m_samplesPerFrame)/kBaseSampleRate;
                if (i%2 == 0) {
                    ::addAmplitudeSmooth(bit1Amplitude[i], m_outputBlock, m_sendVolume, 0, m_samplesPerFrame, frameId, m_nMarkerFrames);
                    m_waveformTones.back().back().freq_hz = bitFreq(m_txProtocol, i);
                } else {
                    ::addAmplitudeSmooth(bit0Amplitude[i], m_outputBlock, m_sendVolume, 0, m_samplesPerFrame, frameId, m_nMarkerFrames);
                    m_waveformTones.back().back().freq_hz = bitFreq(m_txProtocol, i) + m_hzPerSample;
                }
            }
        } else if (frameId < m_nMarkerFrames + totalDataFrames) {
            int dataOffset = frameId - m_nMarkerFrames;
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
                m_waveformTones.back().push_back({});
                m_waveformTones.back().back().duration_ms = (1000.0*m_samplesPerFrame)/kBaseSampleRate;
                if (k%2) {
                    ::addAmplitudeSmooth(bit0Amplitude[k/2], m_outputBlock, m_sendVolume, 0, m_samplesPerFrame, cycleModMain, m_txProtocol.framesPerTx);
                    m_waveformTones.back().back().freq_hz = bitFreq(m_txProtocol, k/2) + m_hzPerSample;
                } else {
                    ::addAmplitudeSmooth(bit1Amplitude[k/2], m_outputBlock, m_sendVolume, 0, m_samplesPerFrame, cycleModMain, m_txProtocol.framesPerTx);
                    m_waveformTones.back().back().freq_hz = bitFreq(m_txProtocol, k/2);
                }
            }
        } else if (frameId < m_nMarkerFrames + totalDataFrames + m_nMarkerFrames) {
            nFreq = m_nBitsInMarker;

            int fId = frameId - (m_nMarkerFrames + totalDataFrames);
            for (int i = 0; i < m_nBitsInMarker; ++i) {
                m_waveformTones.back().push_back({});
                m_waveformTones.back().back().duration_ms = (1000.0*m_samplesPerFrame)/kBaseSampleRate;
                if (i%2 == 0) {
                    addAmplitudeSmooth(bit0Amplitude[i], m_outputBlock, m_sendVolume, 0, m_samplesPerFrame, fId, m_nMarkerFrames);
                    m_waveformTones.back().back().freq_hz = bitFreq(m_txProtocol, i) + m_hzPerSample;
                } else {
                    addAmplitudeSmooth(bit1Amplitude[i], m_outputBlock, m_sendVolume, 0, m_samplesPerFrame, fId, m_nMarkerFrames);
                    m_waveformTones.back().back().freq_hz = bitFreq(m_txProtocol, i);
                }
            }
        } else {
            m_hasNewTxData = false;
            break;
        }

        if (nFreq == 0) nFreq = 1;
        float scale = 1.0f/nFreq;
        for (int i = 0; i < m_samplesPerFrame; ++i) {
            m_outputBlock[i] *= scale;
        }

        int samplesPerFrameOut = m_samplesPerFrame;
        if (m_sampleRateOut != kBaseSampleRate) {
            samplesPerFrameOut = m_impl->resampler.resample(factor, m_samplesPerFrame, m_outputBlock.data(), m_outputBlockResampled.data());
        } else {
            m_outputBlockResampled = m_outputBlock;
        }

        // default output is in 16-bit signed int so we always compute it
        for (int i = 0; i < samplesPerFrameOut; ++i) {
            m_outputBlockI16[offset + i] = 32768*m_outputBlockResampled[i];
        }

        // convert from 32-bit float
        switch (m_sampleFormatOut) {
            case GGWAVE_SAMPLE_FORMAT_UNDEFINED: break;
            case GGWAVE_SAMPLE_FORMAT_U8:
                {
                    auto p = reinterpret_cast<uint8_t *>(m_outputBlockTmp.data());
                    for (int i = 0; i < samplesPerFrameOut; ++i) {
                        p[offset + i] = 128*(m_outputBlockResampled[i] + 1.0f);
                    }
                } break;
            case GGWAVE_SAMPLE_FORMAT_I8:
                {
                    auto p = reinterpret_cast<uint8_t *>(m_outputBlockTmp.data());
                    for (int i = 0; i < samplesPerFrameOut; ++i) {
                        p[offset + i] = 128*m_outputBlockResampled[i];
                    }
                } break;
            case GGWAVE_SAMPLE_FORMAT_U16:
                {
                    auto p = reinterpret_cast<uint16_t *>(m_outputBlockTmp.data());
                    for (int i = 0; i < samplesPerFrameOut; ++i) {
                        p[offset + i] = 32768*(m_outputBlockResampled[i] + 1.0f);
                    }
                } break;
            case GGWAVE_SAMPLE_FORMAT_I16:
                {
                    // skip because we already have the data in m_outputBlockI16
                    //auto p = reinterpret_cast<uint16_t *>(m_outputBlockTmp.data());
                    //for (int i = 0; i < samplesPerFrameOut; ++i) {
                    //    p[offset + i] = 32768*m_outputBlockResampled[i];
                    //}
                } break;
            case GGWAVE_SAMPLE_FORMAT_F32:
                {
                    auto p = reinterpret_cast<float *>(m_outputBlockTmp.data());
                    for (int i = 0; i < samplesPerFrameOut; ++i) {
                        p[offset + i] = m_outputBlockResampled[i];
                    }
                } break;
        }

        ++frameId;
        offset += samplesPerFrameOut;
    }

    switch (m_sampleFormatOut) {
        case GGWAVE_SAMPLE_FORMAT_UNDEFINED: break;
        case GGWAVE_SAMPLE_FORMAT_I16:
            {
                cbWaveformOut(m_outputBlockI16.data(), offset*m_sampleSizeBytesOut);
            } break;
        case GGWAVE_SAMPLE_FORMAT_U8:
        case GGWAVE_SAMPLE_FORMAT_I8:
        case GGWAVE_SAMPLE_FORMAT_U16:
        case GGWAVE_SAMPLE_FORMAT_F32:
            {
                cbWaveformOut(m_outputBlockTmp.data(), offset*m_sampleSizeBytesOut);
            } break;
    }

    m_txAmplitudeDataI16.resize(offset);
    for (uint32_t i = 0; i < offset; ++i) {
        m_txAmplitudeDataI16[i] = m_outputBlockI16[i];
    }

    return true;
}

void GGWave::decode(const CBWaveformInp & cbWaveformInp) {
    while (m_hasNewTxData == false) {
        // read capture data
        float factor = m_sampleRateInp/kBaseSampleRate;
        uint32_t nBytesNeeded = m_samplesNeeded*m_sampleSizeBytesInp;

        if (m_sampleRateInp != kBaseSampleRate) {
            // note : predict 4 extra samples just to make sure we have enough data
            nBytesNeeded = (m_impl->resampler.resample(1.0f/factor, m_samplesNeeded, m_sampleAmplitudeResampled.data(), nullptr) + 4)*m_sampleSizeBytesInp;
        }

        uint32_t nBytesRecorded = 0;

        switch (m_sampleFormatInp) {
            case GGWAVE_SAMPLE_FORMAT_UNDEFINED: break;
            case GGWAVE_SAMPLE_FORMAT_U8:
            case GGWAVE_SAMPLE_FORMAT_I8:
            case GGWAVE_SAMPLE_FORMAT_U16:
            case GGWAVE_SAMPLE_FORMAT_I16:
                {
                    nBytesRecorded = cbWaveformInp(m_sampleAmplitudeTmp.data(), nBytesNeeded);
                } break;
            case GGWAVE_SAMPLE_FORMAT_F32:
                {
                    nBytesRecorded = cbWaveformInp(m_sampleAmplitudeResampled.data(), nBytesNeeded);
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
        int nSamplesRecorded = nBytesRecorded/m_sampleSizeBytesInp;
        switch (m_sampleFormatInp) {
            case GGWAVE_SAMPLE_FORMAT_UNDEFINED: break;
            case GGWAVE_SAMPLE_FORMAT_U8:
                {
                    constexpr float scale = 1.0f/128;
                    auto p = reinterpret_cast<uint8_t *>(m_sampleAmplitudeTmp.data());
                    for (int i = 0; i < nSamplesRecorded; ++i) {
                        m_sampleAmplitudeResampled[i] = float(int16_t(*(p + i)) - 128)*scale;
                    }
                } break;
            case GGWAVE_SAMPLE_FORMAT_I8:
                {
                    constexpr float scale = 1.0f/128;
                    auto p = reinterpret_cast<int8_t *>(m_sampleAmplitudeTmp.data());
                    for (int i = 0; i < nSamplesRecorded; ++i) {
                        m_sampleAmplitudeResampled[i] = float(*(p + i))*scale;
                    }
                } break;
            case GGWAVE_SAMPLE_FORMAT_U16:
                {
                    constexpr float scale = 1.0f/32768;
                    auto p = reinterpret_cast<uint16_t *>(m_sampleAmplitudeTmp.data());
                    for (int i = 0; i < nSamplesRecorded; ++i) {
                        m_sampleAmplitudeResampled[i] = float(int32_t(*(p + i)) - 32768)*scale;
                    }
                } break;
            case GGWAVE_SAMPLE_FORMAT_I16:
                {
                    constexpr float scale = 1.0f/32768;
                    auto p = reinterpret_cast<int16_t *>(m_sampleAmplitudeTmp.data());
                    for (int i = 0; i < nSamplesRecorded; ++i) {
                        m_sampleAmplitudeResampled[i] = float(*(p + i))*scale;
                    }
                } break;
            case GGWAVE_SAMPLE_FORMAT_F32: break;
        }

        if (nSamplesRecorded == 0) {
            break;
        }

        uint32_t offset = m_samplesPerFrame - m_samplesNeeded;

        if (m_sampleRateInp != kBaseSampleRate) {
            if (nSamplesRecorded <= 2*Resampler::kWidth) {
                m_samplesNeeded = m_samplesPerFrame;
                break;
            }

            // reset resampler state every minute
            if (!m_receivingData && m_impl->resampler.nSamplesTotal() > 60.0f*factor*kBaseSampleRate) {
                m_impl->resampler.reset();
            }

            int nSamplesResampled = offset + m_impl->resampler.resample(factor, nSamplesRecorded, m_sampleAmplitudeResampled.data(), m_sampleAmplitude.data() + offset);
            nSamplesRecorded = nSamplesResampled;
        } else {
            for (int i = 0; i < nSamplesRecorded; ++i) {
                m_sampleAmplitude[offset + i] = m_sampleAmplitudeResampled[i];
            }
        }

        // we have enough bytes to do analysis
        if (nSamplesRecorded >= m_samplesPerFrame) {
            m_hasNewAmplitude = true;

            if (m_isFixedPayloadLength) {
                decode_fixed();
            } else {
                decode_variable();
            }

            int nExtraSamples = nSamplesRecorded - m_samplesPerFrame;
            for (int i = 0; i < nExtraSamples; ++i) {
                m_sampleAmplitude[i] = m_sampleAmplitude[m_samplesPerFrame + i];
            }

            m_samplesNeeded = m_samplesPerFrame - nExtraSamples;
        } else {
            m_samplesNeeded = m_samplesPerFrame - nSamplesRecorded;
            break;
        }
    }
}

bool GGWave::takeTxAmplitudeI16(AmplitudeDataI16 & dst) {
    if (m_txAmplitudeDataI16.size() == 0) return false;

    dst = std::move(m_txAmplitudeDataI16);

    return true;
}

bool GGWave::stopReceiving() {
    if (m_receivingData == false) {
        return false;
    }

    m_receivingData = false;

    return true;
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

bool GGWave::takeRxSpectrum(SpectrumData & dst) {
    if (m_hasNewSpectrum == false) return false;

    m_hasNewSpectrum = false;
    dst = m_sampleSpectrum;

    return true;
}

bool GGWave::takeRxAmplitude(AmplitudeData & dst) {
    if (m_hasNewAmplitude == false) return false;

    m_hasNewAmplitude = false;
    dst = m_sampleAmplitude;

    return true;
}

bool GGWave::computeFFTR(const float * src, float * dst, int N, float d) {
    if (N > kMaxSamplesPerFrame) {
        fprintf(stderr, "computeFFTR: N (%d) must be <= %d\n", N, GGWave::kMaxSamplesPerFrame);
        return false;
    }

    FFT(src, dst, N, d);

    return true;
}

//
// Variable payload length
//

void GGWave::decode_variable() {
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
        FFT(m_sampleAmplitudeAverage.data(), m_fftOut.data(), m_samplesPerFrame, 1.0);

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
        for (const auto & rxProtocolPair : m_rxProtocols) {
            const auto & rxProtocolId = rxProtocolPair.first;
            const auto & rxProtocol = rxProtocolPair.second;

            // skip Rx protocol if start frequency is different from detected one
            if (rxProtocol.freqStart != m_markerFreqStart) {
                continue;
            }

            std::fill(m_sampleSpectrum.begin(), m_sampleSpectrum.end(), 0.0f);

            m_framesToAnalyze = m_nMarkerFrames*stepsPerFrame;
            m_framesLeftToAnalyze = m_framesToAnalyze;

            // note : not sure if looping backwards here is more meaningful than looping forwards
            for (int ii = m_nMarkerFrames*stepsPerFrame - 1; ii >= 0; --ii) {
                bool knownLength = false;

                int decodedLength = 0;
                const int offsetStart = ii;
                for (int itx = 0; itx < 1024; ++itx) {
                    int offsetTx = offsetStart + itx*rxProtocol.framesPerTx*stepsPerFrame;
                    if (offsetTx >= m_recvDuration_frames*stepsPerFrame || (itx + 1)*rxProtocol.bytesPerTx >= (int) m_txDataEncoded.size()) {
                        break;
                    }

                    std::copy(
                            m_recordedAmplitude.begin() + offsetTx*step,
                            m_recordedAmplitude.begin() + offsetTx*step + m_samplesPerFrame, m_fftInp.data());

                    // note : should we skip the first and last frame here as they are amplitude-smoothed?
                    for (int k = 1; k < rxProtocol.framesPerTx; ++k) {
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
                            decodedLength = m_rxData[0];

                            const int nTotalBytesExpected = m_encodedDataOffset + decodedLength + ::getECCBytesForLength(decodedLength);
                            const int nTotalFramesExpected = 2*m_nMarkerFrames + ((nTotalBytesExpected + rxProtocol.bytesPerTx - 1)/rxProtocol.bytesPerTx)*rxProtocol.framesPerTx;
                            if (m_recvDuration_frames > nTotalFramesExpected ||
                                m_recvDuration_frames < nTotalFramesExpected - 2*m_nMarkerFrames) {
                                knownLength = false;
                                break;
                            }
                        } else {
                            break;
                        }
                    }

                    {
                        const int nTotalBytesExpected = m_encodedDataOffset + decodedLength + ::getECCBytesForLength(decodedLength);
                        if (knownLength && itx*rxProtocol.bytesPerTx > nTotalBytesExpected + 1) {
                            break;
                        }
                    }
                }

                if (knownLength) {
                    RS::ReedSolomon rsData(decodedLength, ::getECCBytesForLength(decodedLength));

                    if (rsData.Decode(m_txDataEncoded.data() + m_encodedDataOffset, m_rxData.data()) == 0) {
                        if (m_rxData[0] != 0) {
                            std::string s((char *) m_rxData.data(), decodedLength);

                            fprintf(stderr, "Decoded length = %d, protocol = '%s' (%d)\n", decodedLength, rxProtocol.name, rxProtocolId);
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
            fprintf(stderr, "Failed to capture sound data. Please try again (length = %d)\n", m_rxData[0]);
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
                    if (m_sampleSpectrum[bin] <= m_soundMarkerThreshold*m_sampleSpectrum[bin + m_freqDelta_bin]) --nDetectedMarkerBits;
                } else {
                    if (m_sampleSpectrum[bin] >= m_soundMarkerThreshold*m_sampleSpectrum[bin + m_freqDelta_bin]) --nDetectedMarkerBits;
                }
            }

            if (nDetectedMarkerBits == m_nBitsInMarker) {
                m_markerFreqStart = rxProtocol.second.freqStart;
                isReceiving = true;
                break;
            }
        }

        if (isReceiving) {
            if (++m_nMarkersSuccess >= 1) {
            } else {
                isReceiving = false;
            }
        } else {
            m_nMarkersSuccess = 0;
        }

        if (isReceiving) {
            std::time_t timestamp = std::time(nullptr);
            fprintf(stderr, "%sReceiving sound data ...\n", std::asctime(std::localtime(&timestamp)));

            m_receivingData = true;
            std::fill(m_rxData.begin(), m_rxData.end(), 0);

            // max recieve duration
            m_recvDuration_frames =
                2*m_nMarkerFrames +
                maxFramesPerTx()*((kMaxLengthVarible + ::getECCBytesForLength(kMaxLengthVarible))/minBytesPerTx() + 1);

            m_nMarkersSuccess = 0;
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
                    if (m_sampleSpectrum[bin] >= m_soundMarkerThreshold*m_sampleSpectrum[bin + m_freqDelta_bin]) nDetectedMarkerBits--;
                } else {
                    if (m_sampleSpectrum[bin] <= m_soundMarkerThreshold*m_sampleSpectrum[bin + m_freqDelta_bin]) nDetectedMarkerBits--;
                }
            }

            if (nDetectedMarkerBits == m_nBitsInMarker) {
                isEnded = true;
                break;
            }
        }

        if (isEnded) {
            if (++m_nMarkersSuccess >= 1) {
            } else {
                isEnded = false;
            }
        } else {
            m_nMarkersSuccess = 0;
        }

        if (isEnded && m_framesToRecord > 1) {
            std::time_t timestamp = std::time(nullptr);
            m_recvDuration_frames -= m_framesLeftToRecord - 1;
            fprintf(stderr, "%sReceived end marker. Frames left = %d, recorded = %d\n", std::asctime(std::localtime(&timestamp)), m_framesLeftToRecord, m_recvDuration_frames);
            m_nMarkersSuccess = 0;
            m_framesLeftToRecord = 1;
        }
    }
}

//
// Fixed payload length

void GGWave::decode_fixed() {
    m_hasNewSpectrum = true;

    // calculate spectrum
    FFT(m_sampleAmplitude.data(), m_fftOut.data(), m_samplesPerFrame, 1.0);

    for (int i = 0; i < m_samplesPerFrame; ++i) {
        m_sampleSpectrum[i] = (m_fftOut[2*i + 0]*m_fftOut[2*i + 0] + m_fftOut[2*i + 1]*m_fftOut[2*i + 1]);
    }
    for (int i = 1; i < m_samplesPerFrame/2; ++i) {
        m_sampleSpectrum[i] += m_sampleSpectrum[m_samplesPerFrame - i];
    }

    m_spectrumHistoryFixed[m_historyIdFixed] = m_sampleSpectrum;

    if (++m_historyIdFixed >= (int) m_spectrumHistoryFixed.size()) {
        m_historyIdFixed = 0;
    }

    bool isValid = false;
    for (const auto & rxProtocolPair : m_rxProtocols) {
        const auto & rxProtocolId = rxProtocolPair.first;
        const auto & rxProtocol = rxProtocolPair.second;

        const int binStart = rxProtocol.freqStart;
        const int binDelta = 16;

        const int totalLength = m_payloadLength + getECCBytesForLength(m_payloadLength);
        const int totalTxs = (totalLength + rxProtocol.bytesPerTx - 1)/rxProtocol.bytesPerTx;

        int historyStartId = m_historyIdFixed - totalTxs*rxProtocol.framesPerTx;
        if (historyStartId < 0) {
            historyStartId += m_spectrumHistoryFixed.size();
        }

        const int nTones = 2*rxProtocol.bytesPerTx;
        std::vector<int> detectedBins(2*totalLength);

        struct ToneData {
            int nMax[16];
        };

        std::vector<ToneData> tones(nTones);

        bool detectedSignal = true;
        int txDetectedTotal = 0;
        int txNeededTotal = 0;
        for (int k = 0; k < totalTxs; ++k) {
            for (auto & tone : tones) {
                std::fill(tone.nMax, tone.nMax + 16, 0);
            }

            for (int i = 0; i < rxProtocol.framesPerTx; ++i) {
                int historyId = historyStartId + k*rxProtocol.framesPerTx + i;
                if (historyId >= (int) m_spectrumHistoryFixed.size()) {
                    historyId -= m_spectrumHistoryFixed.size();
                }

                for (int j = 0; j < rxProtocol.bytesPerTx; ++j) {
                    int f0bin = -1;
                    int f1bin = -1;

                    double f0max = 0.0;
                    double f1max = 0.0;

                    for (int b = 0; b < 16; ++b) {
                        {
                            const auto & v = m_spectrumHistoryFixed[historyId][binStart + 2*j*binDelta + b];

                            if (f0max <= v) {
                                f0max = v;
                                f0bin = b;
                            }
                        }

                        {
                            const auto & v = m_spectrumHistoryFixed[historyId][binStart + 2*j*binDelta + binDelta + b];

                            if (f1max <= v) {
                                f1max = v;
                                f1bin = b;
                            }
                        }
                    }

                    tones[2*j + 0].nMax[f0bin]++;
                    tones[2*j + 1].nMax[f1bin]++;
                }
            }

            int txDetected = 0;
            int txNeeded = 0;
            for (int j = 0; j < rxProtocol.bytesPerTx; ++j) {
                if (k*rxProtocol.bytesPerTx + j >= totalLength) break;
                txNeeded += 2;
                for (int b = 0; b < 16; ++b) {
                    if (tones[2*j + 0].nMax[b] > rxProtocol.framesPerTx/2) {
                        detectedBins[2*(k*rxProtocol.bytesPerTx + j) + 0] = b;
                        txDetected++;
                    }
                    if (tones[2*j + 1].nMax[b] > rxProtocol.framesPerTx/2) {
                        detectedBins[2*(k*rxProtocol.bytesPerTx + j) + 1] = b;
                        txDetected++;
                    }
                }
            }

            txDetectedTotal += txDetected;
            txNeededTotal += txNeeded;
        }

        //if (rxProtocolId == GGWAVE_TX_PROTOCOL_DT_FAST) {
        //    printf("detected = %d, needed = %d\n", txDetectedTotal, txNeededTotal);
        //}

        if (txDetectedTotal < 0.75*txNeededTotal) {
            detectedSignal = false;
        }

        if (detectedSignal) {
            RS::ReedSolomon rsData(m_payloadLength, getECCBytesForLength(m_payloadLength));

            for (int j = 0; j < totalLength; ++j) {
                m_txDataEncoded[j] = (detectedBins[2*j + 1] << 4) + detectedBins[2*j + 0];
            }

            if (rsData.Decode(m_txDataEncoded.data(), m_rxData.data()) == 0) {
                if (m_rxData[0] != 0) {
                    fprintf(stderr, "Received sound data successfully: '%s'\n", m_rxData.data());

                    isValid = true;
                    m_hasNewRxData = true;
                    m_lastRxDataLength = m_payloadLength;
                    m_rxProtocol = rxProtocol;
                    m_rxProtocolId = TxProtocolId(rxProtocolId);
                }
            }
        }

        if (isValid) {
            break;
        }
    }
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

