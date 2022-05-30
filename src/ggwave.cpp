#include "ggwave/ggwave.h"

#include "reed-solomon/rs.hpp"

#include <cstdio>
#include <cmath>
#include <map>
#include <ctime>
//#include <random>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define ggprintf(...) \
    g_fptr && fprintf(g_fptr, __VA_ARGS__)

//
// C interface
//

namespace {

FILE * g_fptr = stderr;
std::map<ggwave_Instance, GGWave *> g_instances;
std::map<ggwave_Instance, GGWave::RxProtocols> g_rxProtocols;

double linear_interp(double first_number, double second_number, double fraction) {
    return (first_number + ((second_number - first_number)*fraction));
}

}

extern "C"
void ggwave_setLogFile(void * fptr) {
    GGWave::setLogFile((FILE *) fptr);
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
            parameters.sampleRate,
            parameters.samplesPerFrame,
            parameters.soundMarkerThreshold,
            parameters.sampleFormatInp,
            parameters.sampleFormatOut,
            parameters.operatingMode});

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
        ggprintf("Invalid GGWave instance %d\n", instance);
        return -1;
    }

    if (ggWave->init(dataSize, dataBuffer, ggWave->getTxProtocol(txProtocolId), volume) == false) {
        ggprintf("Failed to initialize GGWave instance %d\n", instance);
        return -1;
    }

    if (query != 0) {
        if (query == 1) {
            return ggWave->encodeSize_bytes();
        }

        return ggWave->encodeSize_samples();
    }

    const int nBytes = ggWave->encode();
    if (nBytes == 0) {
        ggprintf("Failed to encode data - GGWave instance %d\n", instance);
        return -1;
    }

    const auto p = (char *) ggWave->txData();
    std::copy(p, p + nBytes, outputBuffer);

    return nBytes;
}

extern "C"
int ggwave_decode(
        ggwave_Instance instance,
        const char * dataBuffer,
        int dataSize,
        char * outputBuffer) {
    GGWave * ggWave = (GGWave *) g_instances[instance];

    if (ggWave->decode(dataBuffer, dataSize) == false) {
        ggprintf("Failed to decode data - GGWave instance %d\n", instance);
        return -1;
    }

    // TODO : avoid allocation
    GGWave::TxRxData rxData;

    auto rxDataLength = ggWave->takeRxData(rxData);
    if (rxDataLength == -1) {
        // failed to decode message
        return -1;
    } else if (rxDataLength > 0) {
        memcpy(outputBuffer, rxData.data(), rxDataLength);
    }

    return rxDataLength;
}

extern "C"
int ggwave_ndecode(
        ggwave_Instance instance,
        const char * dataBuffer,
        int dataSize,
        char * outputBuffer,
        int outputSize) {
    // TODO : avoid duplicated code
    GGWave * ggWave = (GGWave *) g_instances[instance];

    if (ggWave->decode(dataBuffer, dataSize) == false) {
        ggprintf("Failed to decode data - GGWave instance %d\n", instance);
        return -1;
    }

    // TODO : avoid allocation
    GGWave::TxRxData rxData;

    auto rxDataLength = ggWave->takeRxData(rxData);
    if (rxDataLength == -1) {
        // failed to decode message
        return -1;
    } else if (rxDataLength > outputSize) {
        // the outputBuffer is not big enough to store the data
        return -2;
    } else if (rxDataLength > 0) {
        memcpy(outputBuffer, rxData.data(), rxDataLength);
    }

    return rxDataLength;
}

extern "C"
void ggwave_toggleRxProtocol(
        ggwave_Instance instance,
        ggwave_TxProtocolId rxProtocolId,
        int state) {
    // if never called - initialize with all available protocols
    if (g_rxProtocols.find(instance) == g_rxProtocols.end()) {
        g_rxProtocols[instance] = GGWave::getTxProtocols();
    }

    if (state == 0) {
        // disable Rx protocol
        g_rxProtocols[instance][rxProtocolId].enabled = false;
    } else if (state == 1) {
        // enable Rx protocol
        g_rxProtocols[instance][rxProtocolId].enabled = true;
    }

    g_instances[instance]->setRxProtocols(g_rxProtocols[instance]);
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
    const int nTotal = nPerCycle*finalId;
    const float frac = 0.15f;
    const float ds = frac*nTotal;
    const float ids = 1.0f/ds;
    const int nBegin = frac*nTotal;
    const int nEnd = (1.0f - frac)*nTotal;

    for (int i = startId; i < finalId; i++) {
        const float k = cycleMod*finalId + i;
        if (k < nBegin) {
            dst[i] += scalar*src[i]*(k*ids);
        } else if (k > nEnd) {
            dst[i] += scalar*src[i]*(((float)(nTotal) - k)*ids);
        } else {
            dst[i] += scalar*src[i];
        }
    }
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

    ggprintf("Invalid sample format: %d\n", (int) sampleFormat);

    return 0;
}

}

struct GGWave::Rx {
    bool receivingData = false;
    bool analyzingData = false;

    int nMarkersSuccess     = 0;
    int markerFreqStart     = 0;
    int recvDuration_frames = 0;

    int framesLeftToAnalyze = 0;
    int framesLeftToRecord  = 0;
    int framesToAnalyze     = 0;
    int framesToRecord      = 0;
    int samplesNeeded       = 0;

    std::vector<float> fftInp; // real
    std::vector<float> fftOut; // complex

    bool hasNewSpectrum  = false;
    bool hasNewAmplitude = false;

    SpectrumData  sampleSpectrum;
    AmplitudeData sampleAmplitude;
    AmplitudeData sampleAmplitudeResampled;
    TxRxData      sampleAmplitudeTmp;

    bool hasNewRxData = false;

    int lastRxDataLength = 0;

    TxRxData     rxData;
    RxProtocol   rxProtocol;
    RxProtocolId rxProtocolId;
    RxProtocols  rxProtocols;

    int historyId = 0;

    AmplitudeData              sampleAmplitudeAverage;
    std::vector<AmplitudeData> sampleAmplitudeHistory;

    RecordedData recordedAmplitude;

    int historyIdFixed = 0;

    std::vector<std::vector<uint8_t>> spectrumHistoryFixed;
    std::vector<uint8_t> detectedBins;
    std::vector<uint8_t> detectedTones;
};

struct GGWave::Tx {
    bool hasNewTxData = false;

    float sendVolume = 0.1f;

    int txDataLength = 0;
    int lastAmplitudeSize = 0;

    std::vector<bool> dataBits;
    std::vector<double> phaseOffsets;

    std::vector<AmplitudeData> bit1Amplitude;
    std::vector<AmplitudeData> bit0Amplitude;

    TxRxData   txData;
    TxProtocol txProtocol;

    AmplitudeData    outputBlock;
    AmplitudeData    outputBlockResampled;
    TxRxData         outputBlockTmp;
    AmplitudeDataI16 outputBlockI16;

    Tones tones;
};

void GGWave::setLogFile(FILE * fptr) {
    g_fptr = fptr;
}

const GGWave::Parameters & GGWave::getDefaultParameters() {
    static ggwave_Parameters result {
        -1, // vaiable payload length
        kDefaultSampleRate,
        kDefaultSampleRate,
        kDefaultSampleRate,
        kDefaultSamplesPerFrame,
        kDefaultSoundMarkerThreshold,
        GGWAVE_SAMPLE_FORMAT_F32,
        GGWAVE_SAMPLE_FORMAT_F32,
        (ggwave_OperatingMode) (GGWAVE_OPERATING_MODE_RX | GGWAVE_OPERATING_MODE_TX),
    };

    return result;
}

GGWave::GGWave(const Parameters & parameters) :
    m_sampleRateInp       (parameters.sampleRateInp),
    m_sampleRateOut       (parameters.sampleRateOut),
    m_sampleRate          (parameters.sampleRate),
    m_samplesPerFrame     (parameters.samplesPerFrame),
    m_isamplesPerFrame    (1.0f/m_samplesPerFrame),
    m_sampleSizeBytesInp  (bytesForSampleFormat(parameters.sampleFormatInp)),
    m_sampleSizeBytesOut  (bytesForSampleFormat(parameters.sampleFormatOut)),
    m_sampleFormatInp     (parameters.sampleFormatInp),
    m_sampleFormatOut     (parameters.sampleFormatOut),
    m_hzPerSample         (m_sampleRate/m_samplesPerFrame),
    m_ihzPerSample        (1.0f/m_hzPerSample),
    m_freqDelta_bin       (1),
    m_freqDelta_hz        (2*m_hzPerSample),
    m_nBitsInMarker       (16),
    m_nMarkerFrames       (parameters.payloadLength > 0 ? 0 : kDefaultMarkerFrames),
    m_encodedDataOffset   (parameters.payloadLength > 0 ? 0 : kDefaultEncodedDataOffset),
    m_soundMarkerThreshold(parameters.soundMarkerThreshold),
    m_isFixedPayloadLength(parameters.payloadLength > 0),
    m_payloadLength       (parameters.payloadLength),
    m_isRxEnabled         (parameters.operatingMode & GGWAVE_OPERATING_MODE_RX),
    m_isTxEnabled         (parameters.operatingMode & GGWAVE_OPERATING_MODE_TX),
    m_needResampling      (m_sampleRateInp != m_sampleRate || m_sampleRateOut != m_sampleRate),
    m_txOnlyTones         (parameters.operatingMode & GGWAVE_OPERATING_MODE_TX_ONLY_TONES),

    // common
    m_dataEncoded         (kMaxDataSize),

    m_rx(nullptr),
    m_tx(nullptr),
    m_resampler(nullptr) {

    if (m_sampleSizeBytesInp == 0) {
        ggprintf("Invalid or unsupported capture sample format: %d\n", (int) parameters.sampleFormatInp);
        return;
    }

    if (m_sampleSizeBytesOut == 0) {
        ggprintf("Invalid or unsupported playback sample format: %d\n", (int) parameters.sampleFormatOut);
        return;
    }

    if (parameters.samplesPerFrame > kMaxSamplesPerFrame) {
        ggprintf("Invalid samples per frame: %d, max: %d\n", parameters.samplesPerFrame, kMaxSamplesPerFrame);
        return;
    }

    if (m_sampleRateInp < kSampleRateMin) {
        ggprintf("Error: capture sample rate (%g Hz) must be >= %g Hz\n", m_sampleRateInp, kSampleRateMin);
        return;
    }

    if (m_sampleRateInp > kSampleRateMax) {
        ggprintf("Error: capture sample rate (%g Hz) must be <= %g Hz\n", m_sampleRateInp, kSampleRateMax);
        return;
    }

    if (m_isRxEnabled) {
        m_rx = new Rx();

        m_rx->samplesNeeded = m_samplesPerFrame;

        m_rx->fftInp.resize(m_samplesPerFrame);
        m_rx->fftOut.resize(2*m_samplesPerFrame);

        m_rx->sampleSpectrum.resize(m_samplesPerFrame);
        m_rx->sampleAmplitude.resize(m_needResampling ? m_samplesPerFrame + 128 : m_samplesPerFrame); // small extra space because sometimes resampling needs a few more samples
        m_rx->sampleAmplitudeResampled.resize(m_needResampling ? 8*m_samplesPerFrame : m_samplesPerFrame); // min input sampling rate is 0.125*m_sampleRate
        m_rx->sampleAmplitudeTmp.resize(m_needResampling ? 8*m_samplesPerFrame*m_sampleSizeBytesInp : m_samplesPerFrame*m_sampleSizeBytesInp);

        m_rx->rxData.resize(kMaxDataSize);

        m_rx->rxProtocol   = getDefaultTxProtocol();
        m_rx->rxProtocolId = getDefaultTxProtocolId();
        m_rx->rxProtocols  = getTxProtocols();

        if (m_isFixedPayloadLength) {
            if (m_payloadLength > kMaxLengthFixed) {
                ggprintf("Invalid payload legnth: %d, max: %d\n", m_payloadLength, kMaxLengthFixed);
                return;
            }

            const int totalLength = m_payloadLength + getECCBytesForLength(m_payloadLength);
            const int totalTxs = (totalLength + minBytesPerTx() - 1)/minBytesPerTx();

            // TODO: factor of 2 due to Mono-tone protocols
            m_rx->spectrumHistoryFixed.resize(2*totalTxs*maxFramesPerTx());
            m_rx->detectedBins.resize(2*totalLength);
            m_rx->detectedTones.resize(2*16*maxBytesPerTx());
        } else {
            // variable payload length
            m_rx->recordedAmplitude.resize(kMaxRecordedFrames*m_samplesPerFrame);
            m_rx->sampleAmplitudeAverage.resize(m_samplesPerFrame);
            m_rx->sampleAmplitudeHistory.resize(kMaxSpectrumHistory);
        }

        for (auto & s : m_rx->sampleAmplitudeHistory) {
            s.resize(m_samplesPerFrame);
        }

        for (auto & s : m_rx->spectrumHistoryFixed) {
            s.resize(m_samplesPerFrame);
        }
    }

    if (m_isTxEnabled) {
        m_tx = new Tx();

        const int maxDataBits = 2*16*maxBytesPerTx();

        m_tx->txData.resize(kMaxDataSize);
        m_tx->dataBits.resize(maxDataBits);

        if (m_txOnlyTones == false) {
            m_tx->phaseOffsets.resize(maxDataBits);
            m_tx->bit0Amplitude.resize(maxDataBits);
            for (auto & a : m_tx->bit0Amplitude) {
                a.resize(m_samplesPerFrame);
            }
            m_tx->bit1Amplitude.resize(maxDataBits);
            for (auto & a : m_tx->bit1Amplitude) {
                a.resize(m_samplesPerFrame);
            }

            m_tx->outputBlock.resize(m_samplesPerFrame);
            m_tx->outputBlockResampled.resize(2*m_samplesPerFrame);
            m_tx->outputBlockTmp.resize(kMaxRecordedFrames*m_samplesPerFrame*m_sampleSizeBytesOut);
            m_tx->outputBlockI16.resize(kMaxRecordedFrames*m_samplesPerFrame);
        }

        // TODO
        // m_tx->tones;
    }

    // pre-allocate Reed-Solomon memory buffers
    {
        const auto maxLength = m_isFixedPayloadLength ? m_payloadLength : kMaxLengthVariable;

        if (m_isFixedPayloadLength == false) {
            m_workRSLength.resize(RS::ReedSolomon::getWorkSize_bytes(1, m_encodedDataOffset - 1));
        }
        m_workRSData.resize(RS::ReedSolomon::getWorkSize_bytes(maxLength, getECCBytesForLength(maxLength)));
    }

    if (m_needResampling) {
        m_resampler = new Resampler();
    }

    init("", getDefaultTxProtocol(), 0);
}

GGWave::~GGWave() {
    if (m_rx) {
        delete m_rx;
        m_rx = nullptr;
    }

    if (m_tx) {
        delete m_tx;
        m_tx = nullptr;
    }

    if (m_resampler) {
        delete m_resampler;
        m_resampler = nullptr;
    }
}

bool GGWave::init(const char * text, const int volume) {
    return init(strlen(text), text, getDefaultTxProtocol(), volume);
}

bool GGWave::init(const char * text, const TxProtocol & txProtocol, const int volume) {
    return init(strlen(text), text, txProtocol, volume);
}

bool GGWave::init(int dataSize, const char * dataBuffer, const int volume) {
    return init(dataSize, dataBuffer, getDefaultTxProtocol(), volume);
}

bool GGWave::init(int dataSize, const char * dataBuffer, const TxProtocol & txProtocol, const int volume) {
    if (dataSize < 0) {
        ggprintf("Negative data size: %d\n", dataSize);
        return false;
    }

    // Tx
    if (m_isTxEnabled) {
        const auto maxLength = m_isFixedPayloadLength ? m_payloadLength : kMaxLengthVariable;
        if (dataSize > maxLength) {
            ggprintf("Truncating data from %d to %d bytes\n", dataSize, maxLength);
            dataSize = maxLength;
        }

        if (volume < 0 || volume > 100) {
            ggprintf("Invalid volume: %d\n", volume);
            return false;
        }

        if (txProtocol.extra == 2 && m_isFixedPayloadLength == false) {
            ggprintf("Mono-tone protocols with variable length are not supported\n");
            return false;
        }

        m_tx->txProtocol = txProtocol;
        m_tx->txDataLength = dataSize;
        m_tx->sendVolume = ((double)(volume))/100.0f;

        m_tx->hasNewTxData = false;
        std::fill(m_tx->txData.begin(), m_tx->txData.end(), 0);
        std::fill(m_dataEncoded.begin(), m_dataEncoded.end(), 0);

        if (m_tx->txDataLength > 0) {
            m_tx->txData[0] = m_tx->txDataLength;
            for (int i = 0; i < m_tx->txDataLength; ++i) m_tx->txData[i + 1] = dataBuffer[i];

            m_tx->hasNewTxData = true;
        }

        if (m_isFixedPayloadLength) {
            m_tx->txDataLength = m_payloadLength;
        }
    } else {
        if (dataSize > 0) {
            ggprintf("Tx is disabled - cannot transmit data with this ggwave instance\n");
        }
    }

    // Rx
    if (m_isRxEnabled) {
        m_rx->receivingData = false;
        m_rx->analyzingData = false;

        m_rx->framesToAnalyze = 0;
        m_rx->framesLeftToAnalyze = 0;
        m_rx->framesToRecord = 0;
        m_rx->framesLeftToRecord = 0;

        std::fill(m_rx->sampleSpectrum.begin(), m_rx->sampleSpectrum.end(), 0);
        std::fill(m_rx->sampleAmplitude.begin(), m_rx->sampleAmplitude.end(), 0);
        for (auto & s : m_rx->sampleAmplitudeHistory) {
            std::fill(s.begin(), s.end(), 0);
        }

        std::fill(m_rx->rxData.begin(), m_rx->rxData.end(), 0);

        for (int i = 0; i < m_samplesPerFrame; ++i) {
            m_rx->fftOut[2*i + 0] = 0.0f;
            m_rx->fftOut[2*i + 1] = 0.0f;
        }

        for (auto & s : m_rx->spectrumHistoryFixed) {
            std::fill(s.begin(), s.end(), 0);
        }
    }

    return true;
}

uint32_t GGWave::encodeSize_bytes() const {
    return encodeSize_samples()*m_sampleSizeBytesOut;
}

uint32_t GGWave::encodeSize_samples() const {
    if (m_tx->hasNewTxData == false) {
        return 0;
    }

    float factor = 1.0f;
    int samplesPerFrameOut = m_samplesPerFrame;
    if (m_sampleRateOut != m_sampleRate) {
        factor = m_sampleRate/m_sampleRateOut;
        // note : +1 extra sample in order to overestimate the buffer size
        samplesPerFrameOut = m_resampler->resample(factor, m_samplesPerFrame, m_tx->outputBlock.data(), nullptr) + 1;
    }
    const int nECCBytesPerTx = getECCBytesForLength(m_tx->txDataLength);
    const int sendDataLength = m_tx->txDataLength + m_encodedDataOffset;
    const int totalBytes = sendDataLength + nECCBytesPerTx;
    const int totalDataFrames = m_tx->txProtocol.extra*((totalBytes + m_tx->txProtocol.bytesPerTx - 1)/m_tx->txProtocol.bytesPerTx)*m_tx->txProtocol.framesPerTx;

    return (
            m_nMarkerFrames + totalDataFrames + m_nMarkerFrames
           )*samplesPerFrameOut;
}

uint32_t GGWave::encode() {
    if (m_isTxEnabled == false) {
        ggprintf("Tx is disabled - cannot transmit data with this ggwave instance\n");
        return 0;
    }

    if (m_resampler) {
        m_resampler->reset();
    }

    const int nECCBytesPerTx = getECCBytesForLength(m_tx->txDataLength);
    const int sendDataLength = m_tx->txDataLength + m_encodedDataOffset;
    const int totalBytes = sendDataLength + nECCBytesPerTx;
    const int totalDataFrames = m_tx->txProtocol.extra*((totalBytes + m_tx->txProtocol.bytesPerTx - 1)/m_tx->txProtocol.bytesPerTx)*m_tx->txProtocol.framesPerTx;

    if (m_isFixedPayloadLength == false) {
        RS::ReedSolomon rsLength(1, m_encodedDataOffset - 1, m_workRSLength.data());
        rsLength.Encode(m_tx->txData.data(), m_dataEncoded.data());
    }

    // first byte of m_tx->txData contains the length of the payload, so we skip it:
    RS::ReedSolomon rsData = RS::ReedSolomon(m_tx->txDataLength, nECCBytesPerTx, m_workRSData.data());
    rsData.Encode(m_tx->txData.data() + 1, m_dataEncoded.data() + m_encodedDataOffset);

    // generate tones
    {
        int frameId = 0;
        bool hasNewData = m_tx->hasNewTxData;

        m_tx->tones.clear();
        while (hasNewData) {
            m_tx->tones.push_back({});

            if (frameId < m_nMarkerFrames) {
                for (int i = 0; i < m_nBitsInMarker; ++i) {
                    m_tx->tones.back().push_back({});
                    m_tx->tones.back().back().duration_ms = (1000.0*m_samplesPerFrame)/m_sampleRate;
                    if (i%2 == 0) {
                        m_tx->tones.back().back().freq_hz = bitFreq(m_tx->txProtocol, i);
                    } else {
                        m_tx->tones.back().back().freq_hz = bitFreq(m_tx->txProtocol, i) + m_hzPerSample;
                    }
                }
            } else if (frameId < m_nMarkerFrames + totalDataFrames) {
                int dataOffset = frameId - m_nMarkerFrames;
                dataOffset /= m_tx->txProtocol.framesPerTx;
                dataOffset *= m_tx->txProtocol.bytesPerTx;

                std::fill(m_tx->dataBits.begin(), m_tx->dataBits.end(), 0);

                for (int j = 0; j < m_tx->txProtocol.bytesPerTx; ++j) {
                    if (m_tx->txProtocol.extra == 1) {
                        {
                            uint8_t d = m_dataEncoded[dataOffset + j] & 15;
                            m_tx->dataBits[(2*j + 0)*16 + d] = 1;
                        }
                        {
                            uint8_t d = m_dataEncoded[dataOffset + j] & 240;
                            m_tx->dataBits[(2*j + 1)*16 + (d >> 4)] = 1;
                        }
                    } else {
                        if (dataOffset % m_tx->txProtocol.extra == 0) {
                            uint8_t d = m_dataEncoded[dataOffset/m_tx->txProtocol.extra + j] & 15;
                            m_tx->dataBits[(2*j + 0)*16 + d] = 1;
                        } else {
                            uint8_t d = m_dataEncoded[dataOffset/m_tx->txProtocol.extra + j] & 240;
                            m_tx->dataBits[(2*j + 0)*16 + (d >> 4)] = 1;
                        }
                    }
                }

                for (int k = 0; k < 2*m_tx->txProtocol.bytesPerTx*16; ++k) {
                    if (m_tx->dataBits[k] == 0) continue;

                    m_tx->tones.back().push_back({});
                    m_tx->tones.back().back().duration_ms = (1000.0*m_samplesPerFrame)/m_sampleRate;
                    if (k%2) {
                        m_tx->tones.back().back().freq_hz = bitFreq(m_tx->txProtocol, k/2) + m_hzPerSample;
                    } else {
                        m_tx->tones.back().back().freq_hz = bitFreq(m_tx->txProtocol, k/2);
                    }
                }
            } else if (frameId < m_nMarkerFrames + totalDataFrames + m_nMarkerFrames) {
                for (int i = 0; i < m_nBitsInMarker; ++i) {
                    m_tx->tones.back().push_back({});
                    m_tx->tones.back().back().duration_ms = (1000.0*m_samplesPerFrame)/m_sampleRate;
                    if (i%2 == 0) {
                        m_tx->tones.back().back().freq_hz = bitFreq(m_tx->txProtocol, i) + m_hzPerSample;
                    } else {
                        m_tx->tones.back().back().freq_hz = bitFreq(m_tx->txProtocol, i);
                    }
                }
            } else {
                hasNewData = false;
                break;
            }

            ++frameId;
        }

        if (m_txOnlyTones) {
            m_tx->hasNewTxData = false;
            return true;
        }
    }

    // compute Tx data
    {
        for (int k = 0; k < (int) m_tx->phaseOffsets.size(); ++k) {
            m_tx->phaseOffsets[k] = (M_PI*k)/(m_tx->txProtocol.nDataBitsPerTx());
        }

        // note : what is the purpose of this shuffle ? I forgot .. :(
        //std::random_device rd;
        //std::mt19937 g(rd());

        //std::shuffle(phaseOffsets.begin(), phaseOffsets.end(), g);

        for (int k = 0; k < (int) m_tx->dataBits.size(); ++k) {
            const double freq = bitFreq(m_tx->txProtocol, k);

            const double phaseOffset = m_tx->phaseOffsets[k];
            const double curHzPerSample = m_hzPerSample;
            const double curIHzPerSample = 1.0/curHzPerSample;

            for (int i = 0; i < m_samplesPerFrame; i++) {
                const double curi = i;
                m_tx->bit1Amplitude[k][i] = std::sin((2.0*M_PI)*(curi*m_isamplesPerFrame)*(freq*curIHzPerSample) + phaseOffset);
            }

            for (int i = 0; i < m_samplesPerFrame; i++) {
                const double curi = i;
                m_tx->bit0Amplitude[k][i] = std::sin((2.0*M_PI)*(curi*m_isamplesPerFrame)*((freq + m_hzPerSample*m_freqDelta_bin)*curIHzPerSample) + phaseOffset);
            }
        }
    }

    int frameId = 0;
    uint32_t offset = 0;
    const float factor = m_sampleRate/m_sampleRateOut;

    while (m_tx->hasNewTxData) {
        std::fill(m_tx->outputBlock.begin(), m_tx->outputBlock.end(), 0.0f);

        uint16_t nFreq = 0;
        if (frameId < m_nMarkerFrames) {
            nFreq = m_nBitsInMarker;

            for (int i = 0; i < m_nBitsInMarker; ++i) {
                if (i%2 == 0) {
                    ::addAmplitudeSmooth(m_tx->bit1Amplitude[i], m_tx->outputBlock, m_tx->sendVolume, 0, m_samplesPerFrame, frameId, m_nMarkerFrames);
                } else {
                    ::addAmplitudeSmooth(m_tx->bit0Amplitude[i], m_tx->outputBlock, m_tx->sendVolume, 0, m_samplesPerFrame, frameId, m_nMarkerFrames);
                }
            }
        } else if (frameId < m_nMarkerFrames + totalDataFrames) {
            int dataOffset = frameId - m_nMarkerFrames;
            int cycleModMain = dataOffset%m_tx->txProtocol.framesPerTx;
            dataOffset /= m_tx->txProtocol.framesPerTx;
            dataOffset *= m_tx->txProtocol.bytesPerTx;

            std::fill(m_tx->dataBits.begin(), m_tx->dataBits.end(), 0);

            for (int j = 0; j < m_tx->txProtocol.bytesPerTx; ++j) {
                if (m_tx->txProtocol.extra == 1) {
                    {
                        uint8_t d = m_dataEncoded[dataOffset + j] & 15;
                        m_tx->dataBits[(2*j + 0)*16 + d] = 1;
                    }
                    {
                        uint8_t d = m_dataEncoded[dataOffset + j] & 240;
                        m_tx->dataBits[(2*j + 1)*16 + (d >> 4)] = 1;
                    }
                } else {
                    if (dataOffset % m_tx->txProtocol.extra == 0) {
                        uint8_t d = m_dataEncoded[dataOffset/m_tx->txProtocol.extra + j] & 15;
                        m_tx->dataBits[(2*j + 0)*16 + d] = 1;
                    } else {
                        uint8_t d = m_dataEncoded[dataOffset/m_tx->txProtocol.extra + j] & 240;
                        m_tx->dataBits[(2*j + 0)*16 + (d >> 4)] = 1;
                    }
                }
            }

            for (int k = 0; k < 2*m_tx->txProtocol.bytesPerTx*16; ++k) {
                if (m_tx->dataBits[k] == 0) continue;

                ++nFreq;
                if (k%2) {
                    ::addAmplitudeSmooth(m_tx->bit0Amplitude[k/2], m_tx->outputBlock, m_tx->sendVolume, 0, m_samplesPerFrame, cycleModMain, m_tx->txProtocol.framesPerTx);
                } else {
                    ::addAmplitudeSmooth(m_tx->bit1Amplitude[k/2], m_tx->outputBlock, m_tx->sendVolume, 0, m_samplesPerFrame, cycleModMain, m_tx->txProtocol.framesPerTx);
                }
            }
        } else if (frameId < m_nMarkerFrames + totalDataFrames + m_nMarkerFrames) {
            nFreq = m_nBitsInMarker;

            const int fId = frameId - (m_nMarkerFrames + totalDataFrames);
            for (int i = 0; i < m_nBitsInMarker; ++i) {
                if (i%2 == 0) {
                    addAmplitudeSmooth(m_tx->bit0Amplitude[i], m_tx->outputBlock, m_tx->sendVolume, 0, m_samplesPerFrame, fId, m_nMarkerFrames);
                } else {
                    addAmplitudeSmooth(m_tx->bit1Amplitude[i], m_tx->outputBlock, m_tx->sendVolume, 0, m_samplesPerFrame, fId, m_nMarkerFrames);
                }
            }
        } else {
            m_tx->hasNewTxData = false;
            break;
        }

        if (nFreq == 0) nFreq = 1;
        const float scale = 1.0f/nFreq;
        for (int i = 0; i < m_samplesPerFrame; ++i) {
            m_tx->outputBlock[i] *= scale;
        }

        int samplesPerFrameOut = m_samplesPerFrame;
        if (m_sampleRateOut != m_sampleRate) {
            samplesPerFrameOut = m_resampler->resample(factor, m_samplesPerFrame, m_tx->outputBlock.data(), m_tx->outputBlockResampled.data());
        } else {
            m_tx->outputBlockResampled = m_tx->outputBlock;
        }

        // default output is in 16-bit signed int so we always compute it
        for (int i = 0; i < samplesPerFrameOut; ++i) {
            m_tx->outputBlockI16[offset + i] = 32768*m_tx->outputBlockResampled[i];
        }

        // convert from 32-bit float
        switch (m_sampleFormatOut) {
            case GGWAVE_SAMPLE_FORMAT_UNDEFINED: break;
            case GGWAVE_SAMPLE_FORMAT_U8:
                {
                    auto p = reinterpret_cast<uint8_t *>(m_tx->outputBlockTmp.data());
                    for (int i = 0; i < samplesPerFrameOut; ++i) {
                        p[offset + i] = 128*(m_tx->outputBlockResampled[i] + 1.0f);
                    }
                } break;
            case GGWAVE_SAMPLE_FORMAT_I8:
                {
                    auto p = reinterpret_cast<uint8_t *>(m_tx->outputBlockTmp.data());
                    for (int i = 0; i < samplesPerFrameOut; ++i) {
                        p[offset + i] = 128*m_tx->outputBlockResampled[i];
                    }
                } break;
            case GGWAVE_SAMPLE_FORMAT_U16:
                {
                    auto p = reinterpret_cast<uint16_t *>(m_tx->outputBlockTmp.data());
                    for (int i = 0; i < samplesPerFrameOut; ++i) {
                        p[offset + i] = 32768*(m_tx->outputBlockResampled[i] + 1.0f);
                    }
                } break;
            case GGWAVE_SAMPLE_FORMAT_I16:
                {
                    // skip because we already have the data in m_tx->outputBlockI16
                    //auto p = reinterpret_cast<uint16_t *>(m_tx->outputBlockTmp.data());
                    //for (int i = 0; i < samplesPerFrameOut; ++i) {
                    //    p[offset + i] = 32768*m_tx->outputBlockResampled[i];
                    //}
                } break;
            case GGWAVE_SAMPLE_FORMAT_F32:
                {
                    auto p = reinterpret_cast<float *>(m_tx->outputBlockTmp.data());
                    for (int i = 0; i < samplesPerFrameOut; ++i) {
                        p[offset + i] = m_tx->outputBlockResampled[i];
                    }
                } break;
        }

        ++frameId;
        offset += samplesPerFrameOut;
    }

    m_tx->lastAmplitudeSize = offset;

    // the encoded waveform can be accessed via the txData() method
    // we return the size of the waveform in bytes:
    return offset*m_sampleSizeBytesOut;
}

const void * GGWave::txData() const {
    if (m_tx == nullptr) {
        ggprintf("Tx is disabled - cannot transmit data with this ggwave instance\n");
        return nullptr;
    }

    switch (m_sampleFormatOut) {
        case GGWAVE_SAMPLE_FORMAT_UNDEFINED: break;
        case GGWAVE_SAMPLE_FORMAT_I16:
            {
                return m_tx->outputBlockI16.data();
            } break;
        case GGWAVE_SAMPLE_FORMAT_U8:
        case GGWAVE_SAMPLE_FORMAT_I8:
        case GGWAVE_SAMPLE_FORMAT_U16:
        case GGWAVE_SAMPLE_FORMAT_F32:
            {
                return m_tx->outputBlockTmp.data();
            } break;
    }

    return nullptr;
}

bool GGWave::decode(const void * data, uint32_t nBytes) {
    if (m_isRxEnabled == false) {
        ggprintf("Rx is disabled - cannot receive data with this ggwave instance\n");
        return false;
    }

    if (m_tx && m_tx->hasNewTxData) {
        ggprintf("Cannot decode while transmitting\n");
        return false;
    }

    auto dataBuffer = (uint8_t *) data;
    const float factor = m_sampleRateInp/m_sampleRate;

    while (true) {
        // read capture data
        uint32_t nBytesNeeded = m_rx->samplesNeeded*m_sampleSizeBytesInp;

        if (m_sampleRateInp != m_sampleRate) {
            // note : predict 4 extra samples just to make sure we have enough data
            nBytesNeeded = (m_resampler->resample(1.0f/factor, m_rx->samplesNeeded, m_rx->sampleAmplitudeResampled.data(), nullptr) + 4)*m_sampleSizeBytesInp;
        }

        const uint32_t nBytesRecorded = std::min(nBytes, nBytesNeeded);

        if (nBytesRecorded == 0) {
            break;
        }

        switch (m_sampleFormatInp) {
            case GGWAVE_SAMPLE_FORMAT_UNDEFINED: break;
            case GGWAVE_SAMPLE_FORMAT_U8:
            case GGWAVE_SAMPLE_FORMAT_I8:
            case GGWAVE_SAMPLE_FORMAT_U16:
            case GGWAVE_SAMPLE_FORMAT_I16:
                {
                    std::copy(dataBuffer, dataBuffer + nBytesRecorded, m_rx->sampleAmplitudeTmp.data());
                } break;
            case GGWAVE_SAMPLE_FORMAT_F32:
                {
                    std::copy(dataBuffer, dataBuffer + nBytesRecorded, (uint8_t *) m_rx->sampleAmplitudeResampled.data());
                } break;
        }

        dataBuffer += nBytesRecorded;
        nBytes -= nBytesRecorded;

        if (nBytesRecorded % m_sampleSizeBytesInp != 0) {
            ggprintf("Failure during capture - provided bytes (%d) are not multiple of sample size (%d)\n",
                    nBytesRecorded, m_sampleSizeBytesInp);
            m_rx->samplesNeeded = m_samplesPerFrame;
            break;
        }

        // convert to 32-bit float
        int nSamplesRecorded = nBytesRecorded/m_sampleSizeBytesInp;
        switch (m_sampleFormatInp) {
            case GGWAVE_SAMPLE_FORMAT_UNDEFINED: break;
            case GGWAVE_SAMPLE_FORMAT_U8:
                {
                    constexpr float scale = 1.0f/128;
                    auto p = reinterpret_cast<uint8_t *>(m_rx->sampleAmplitudeTmp.data());
                    for (int i = 0; i < nSamplesRecorded; ++i) {
                        m_rx->sampleAmplitudeResampled[i] = float(int16_t(*(p + i)) - 128)*scale;
                    }
                } break;
            case GGWAVE_SAMPLE_FORMAT_I8:
                {
                    constexpr float scale = 1.0f/128;
                    auto p = reinterpret_cast<int8_t *>(m_rx->sampleAmplitudeTmp.data());
                    for (int i = 0; i < nSamplesRecorded; ++i) {
                        m_rx->sampleAmplitudeResampled[i] = float(*(p + i))*scale;
                    }
                } break;
            case GGWAVE_SAMPLE_FORMAT_U16:
                {
                    constexpr float scale = 1.0f/32768;
                    auto p = reinterpret_cast<uint16_t *>(m_rx->sampleAmplitudeTmp.data());
                    for (int i = 0; i < nSamplesRecorded; ++i) {
                        m_rx->sampleAmplitudeResampled[i] = float(int32_t(*(p + i)) - 32768)*scale;
                    }
                } break;
            case GGWAVE_SAMPLE_FORMAT_I16:
                {
                    constexpr float scale = 1.0f/32768;
                    auto p = reinterpret_cast<int16_t *>(m_rx->sampleAmplitudeTmp.data());
                    for (int i = 0; i < nSamplesRecorded; ++i) {
                        m_rx->sampleAmplitudeResampled[i] = float(*(p + i))*scale;
                    }
                } break;
            case GGWAVE_SAMPLE_FORMAT_F32: break;
        }

        uint32_t offset = m_samplesPerFrame - m_rx->samplesNeeded;

        if (m_sampleRateInp != m_sampleRate) {
            if (nSamplesRecorded <= 2*Resampler::kWidth) {
                m_rx->samplesNeeded = m_samplesPerFrame;
                break;
            }

            // reset resampler state every minute
            if (!m_rx->receivingData && m_resampler->nSamplesTotal() > 60.0f*factor*m_sampleRate) {
                m_resampler->reset();
            }

            int nSamplesResampled = offset + m_resampler->resample(factor, nSamplesRecorded, m_rx->sampleAmplitudeResampled.data(), m_rx->sampleAmplitude.data() + offset);
            nSamplesRecorded = nSamplesResampled;
        } else {
            for (int i = 0; i < nSamplesRecorded; ++i) {
                m_rx->sampleAmplitude[offset + i] = m_rx->sampleAmplitudeResampled[i];
            }
        }

        // we have enough bytes to do analysis
        if (nSamplesRecorded >= m_samplesPerFrame) {
            m_rx->hasNewAmplitude = true;

            if (m_isFixedPayloadLength) {
                decode_fixed();
            } else {
                decode_variable();
            }

            int nExtraSamples = nSamplesRecorded - m_samplesPerFrame;
            for (int i = 0; i < nExtraSamples; ++i) {
                m_rx->sampleAmplitude[i] = m_rx->sampleAmplitude[m_samplesPerFrame + i];
            }

            m_rx->samplesNeeded = m_samplesPerFrame - nExtraSamples;
        } else {
            m_rx->samplesNeeded = m_samplesPerFrame - nSamplesRecorded;
            break;
        }
    }

    return true;
}

//
// instance state
//

bool GGWave::hasTxData() const { return m_tx && m_tx->hasNewTxData; }

int GGWave::getSamplesPerFrame()    const { return m_samplesPerFrame; }
int GGWave::getSampleSizeBytesInp() const { return m_sampleSizeBytesInp; }
int GGWave::getSampleSizeBytesOut() const { return m_sampleSizeBytesOut; }

float GGWave::getSampleRateInp() const { return m_sampleRateInp; }
float GGWave::getSampleRateOut() const { return m_sampleRateOut; }
GGWave::SampleFormat GGWave::getSampleFormatInp() const { return m_sampleFormatInp; }
GGWave::SampleFormat GGWave::getSampleFormatOut() const { return m_sampleFormatOut; }

//
// Tx
//

const GGWave::Tones & GGWave::txTones() const { return m_tx->tones; }

bool GGWave::takeTxAmplitudeI16(AmplitudeDataI16 & dst) {
    if (m_tx->lastAmplitudeSize == 0) return false;

    if ((int) dst.size() < m_tx->lastAmplitudeSize) {
        dst.resize(m_tx->lastAmplitudeSize);
    }
    std::copy(m_tx->outputBlockI16.begin(), m_tx->outputBlockI16.begin() + m_tx->lastAmplitudeSize, dst.begin());
    m_tx->lastAmplitudeSize = 0;

    return true;
}

//
// Rx
//

bool GGWave::isReceiving() const { return m_rx->receivingData; }
bool GGWave::isAnalyzing() const { return m_rx->analyzingData; }

int GGWave::getFramesToRecord()      const { return m_rx->framesToRecord; }
int GGWave::getFramesLeftToRecord()  const { return m_rx->framesLeftToRecord; }
int GGWave::getFramesToAnalyze()     const { return m_rx->framesToAnalyze; }
int GGWave::getFramesLeftToAnalyze() const { return m_rx->framesLeftToAnalyze; }

bool GGWave::stopReceiving() {
    if (m_rx->receivingData == false) {
        return false;
    }

    m_rx->receivingData = false;

    return true;
}

void GGWave::setRxProtocols(const RxProtocols & rxProtocols) { m_rx->rxProtocols = rxProtocols; }
const GGWave::RxProtocols & GGWave::getRxProtocols() const { return m_rx->rxProtocols; }

int GGWave::lastRxDataLength() const { return m_rx->lastRxDataLength; }

const GGWave::TxRxData &     GGWave::getRxData()       const { return m_rx->rxData; }
const GGWave::RxProtocol &   GGWave::getRxProtocol()   const { return m_rx->rxProtocol; }
const GGWave::RxProtocolId & GGWave::getRxProtocolId() const { return m_rx->rxProtocolId; }

int GGWave::takeRxData(TxRxData & dst) {
    if (m_rx->lastRxDataLength == 0) return 0;

    auto res = m_rx->lastRxDataLength;
    m_rx->lastRxDataLength = 0;

    if (res != -1) {
        dst = m_rx->rxData;
    }

    return res;
}

bool GGWave::takeRxSpectrum(SpectrumData & dst) {
    if (m_rx->hasNewSpectrum == false) return false;

    m_rx->hasNewSpectrum = false;
    dst = m_rx->sampleSpectrum;

    return true;
}

bool GGWave::takeRxAmplitude(AmplitudeData & dst) {
    if (m_rx->hasNewAmplitude == false) return false;

    m_rx->hasNewAmplitude = false;
    dst = m_rx->sampleAmplitude;

    return true;
}

bool GGWave::computeFFTR(const float * src, float * dst, int N, float d) {
    if (N > kMaxSamplesPerFrame) {
        ggprintf("computeFFTR: N (%d) must be <= %d\n", N, GGWave::kMaxSamplesPerFrame);
        return false;
    }

    FFT(src, dst, N, d);

    return true;
}

//
// GGWave::Resampler
//

GGWave::Resampler::Resampler() :
    m_sincTable(kWidth*kSamplesPerZeroCrossing),
    m_delayBuffer(3*kWidth),
    m_edgeSamples(kWidth),
    m_samplesInp(2048) {
    makeSinc();
    reset();
}

void GGWave::Resampler::reset() {
    m_state = {};
    std::fill(m_edgeSamples.begin(), m_edgeSamples.end(), 0.0f);
    std::fill(m_delayBuffer.begin(), m_delayBuffer.end(), 0.0f);
    std::fill(m_samplesInp.begin(), m_samplesInp.end(), 0.0f);
}

int GGWave::Resampler::resample(
        float factor,
        int nSamples,
        const float * samplesInp,
        float * samplesOut) {
    int idxInp = -1;
    int idxOut = 0;
    int notDone = 1;
    float data_in = 0.0f;
    float data_out = 0.0f;
    double one_over_factor = 1.0;

    auto stateSave = m_state;

    m_state.nSamplesTotal += nSamples;

    if (samplesOut) {
        assert(nSamples > kWidth);
        if ((int) m_samplesInp.size() < nSamples + kWidth) {
            m_samplesInp.resize(nSamples + kWidth);
        }
        for (int i = 0; i < kWidth; ++i) {
            m_samplesInp[i] = m_edgeSamples[i];
            m_edgeSamples[i] = samplesInp[nSamples - kWidth + i];
        }
        for (int i = 0; i < nSamples; ++i) {
            m_samplesInp[i + kWidth] = samplesInp[i];
        }
        samplesInp = m_samplesInp.data();
    }

    while (notDone) {
        while (m_state.timeLast < m_state.timeInt) {
            if (++idxInp >= nSamples) {
                notDone = 0;
                break;
            } else {
                data_in = samplesInp[idxInp];
            }
            //printf("xxxx idxInp = %d\n", idxInp);
            if (samplesOut) newData(data_in);
            m_state.timeLast += 1;
        }

        if (notDone == false) break;

        double temp1 = 0.0;
        int left_limit = m_state.timeNow - kWidth + 1; /* leftmost neighboring sample used for interp.*/
        int right_limit = m_state.timeNow + kWidth;    /* rightmost leftmost neighboring sample used for interp.*/
        if (left_limit < 0) left_limit = 0;
        if (right_limit > m_state.nSamplesTotal + kWidth) right_limit = m_state.nSamplesTotal + kWidth;
        if (factor < 1.0) {
            for (int j = left_limit; j < right_limit; j++) {
                temp1 += getData(j - m_state.timeInt)*sinc(m_state.timeNow - (double) j);
            }
            data_out = temp1;
        }
        else {
            one_over_factor = 1.0 / factor;
            for (int j = left_limit; j < right_limit; j++) {
                temp1 += getData(j - m_state.timeInt)*one_over_factor*sinc(one_over_factor*(m_state.timeNow - (double) j));
            }
            data_out = temp1;
        }

        if (samplesOut) {
            //printf("inp = %d, l = %d, r = %d, n = %d, a = %d, b = %d\n", idxInp, left_limit, right_limit, m_state.nSamplesTotal, left_limit - m_state.timeInt, right_limit - m_state.timeInt - 1);
            samplesOut[idxOut] = data_out;
        }
        ++idxOut;

        m_state.timeNow += factor;
        m_state.timeLast = m_state.timeInt;
        m_state.timeInt = m_state.timeNow;
        while (m_state.timeLast < m_state.timeInt) {
            if (++idxInp >= nSamples) {
                notDone = 0;
                break;
            } else {
                data_in = samplesInp[idxInp];
            }
            if (samplesOut) newData(data_in);
            m_state.timeLast += 1;
        }
        //printf("last idxInp = %d, nSamples = %d\n", idxInp, nSamples);
    }

    if (samplesOut == nullptr) {
        m_state = stateSave;
    }

    return idxOut;
}

float GGWave::Resampler::getData(int j) const {
    return m_delayBuffer[(int) j + kWidth];
}

void GGWave::Resampler::newData(float data) {
    for (int i = 0; i < kDelaySize - 5; i++) {
        m_delayBuffer[i] = m_delayBuffer[i + 1];
    }
    m_delayBuffer[kDelaySize - 5] = data;
}

void GGWave::Resampler::makeSinc() {
    double temp, win_freq, win;
    win_freq = M_PI/kWidth/kSamplesPerZeroCrossing;
    m_sincTable[0] = 1.0;
    for (int i = 1; i < kWidth*kSamplesPerZeroCrossing; i++) {
        temp = (double) i*M_PI/kSamplesPerZeroCrossing;
        m_sincTable[i] = sin(temp)/temp;
        win = 0.5 + 0.5*cos(win_freq*i);
        m_sincTable[i] *= win;
    }
}

double GGWave::Resampler::sinc(double x) const {
    int low;
    double temp, delta;
    if (fabs(x) >= kWidth - 1) {
        return 0.0;
    } else {
        temp = fabs(x)*(double) kSamplesPerZeroCrossing;
        low = temp;          /* these are interpolation steps */
        delta = temp - low;  /* and can be ommited if desired */
        return linear_interp(m_sincTable[low], m_sincTable[low + 1], delta);
    }
}

//
// Variable payload length
//

void GGWave::decode_variable() {
    m_rx->sampleAmplitudeHistory[m_rx->historyId] = m_rx->sampleAmplitude;

    if (++m_rx->historyId >= kMaxSpectrumHistory) {
        m_rx->historyId = 0;
    }

    if (m_rx->historyId == 0 || m_rx->receivingData) {
        m_rx->hasNewSpectrum = true;

        std::fill(m_rx->sampleAmplitudeAverage.begin(), m_rx->sampleAmplitudeAverage.end(), 0.0f);
        for (auto & s : m_rx->sampleAmplitudeHistory) {
            for (int i = 0; i < m_samplesPerFrame; ++i) {
                m_rx->sampleAmplitudeAverage[i] += s[i];
            }
        }

        float norm = 1.0f/kMaxSpectrumHistory;
        for (int i = 0; i < m_samplesPerFrame; ++i) {
            m_rx->sampleAmplitudeAverage[i] *= norm;
        }

        // calculate spectrum
        FFT(m_rx->sampleAmplitudeAverage.data(), m_rx->fftOut.data(), m_samplesPerFrame, 1.0);

        for (int i = 0; i < m_samplesPerFrame; ++i) {
            m_rx->sampleSpectrum[i] = (m_rx->fftOut[2*i + 0]*m_rx->fftOut[2*i + 0] + m_rx->fftOut[2*i + 1]*m_rx->fftOut[2*i + 1]);
        }
        for (int i = 1; i < m_samplesPerFrame/2; ++i) {
            m_rx->sampleSpectrum[i] += m_rx->sampleSpectrum[m_samplesPerFrame - i];
        }
    }

    if (m_rx->framesLeftToRecord > 0) {
        std::copy(m_rx->sampleAmplitude.begin(),
                  m_rx->sampleAmplitude.begin() + m_samplesPerFrame,
                  m_rx->recordedAmplitude.data() + (m_rx->framesToRecord - m_rx->framesLeftToRecord)*m_samplesPerFrame);

        if (--m_rx->framesLeftToRecord <= 0) {
            m_rx->analyzingData = true;
        }
    }

    if (m_rx->analyzingData) {
        ggprintf("Analyzing captured data ..\n");

        const int stepsPerFrame = 16;
        const int step = m_samplesPerFrame/stepsPerFrame;

        bool isValid = false;
        for (int rxProtocolId = 0; rxProtocolId < (int) m_rx->rxProtocols.size(); ++rxProtocolId) {
            const auto & rxProtocol = m_rx->rxProtocols[rxProtocolId];
            if (rxProtocol.enabled == false) {
                continue;
            }

            // skip Rx protocol if it is mono-tone
            if (rxProtocol.extra == 2) {
                continue;
            }

            // skip Rx protocol if start frequency is different from detected one
            if (rxProtocol.freqStart != m_rx->markerFreqStart) {
                continue;
            }

            std::fill(m_rx->sampleSpectrum.begin(), m_rx->sampleSpectrum.end(), 0.0f);

            m_rx->framesToAnalyze = m_nMarkerFrames*stepsPerFrame;
            m_rx->framesLeftToAnalyze = m_rx->framesToAnalyze;

            // note : not sure if looping backwards here is more meaningful than looping forwards
            for (int ii = m_nMarkerFrames*stepsPerFrame - 1; ii >= 0; --ii) {
                bool knownLength = false;

                int decodedLength = 0;
                const int offsetStart = ii;
                for (int itx = 0; itx < 1024; ++itx) {
                    int offsetTx = offsetStart + itx*rxProtocol.framesPerTx*stepsPerFrame;
                    if (offsetTx >= m_rx->recvDuration_frames*stepsPerFrame || (itx + 1)*rxProtocol.bytesPerTx >= (int) m_dataEncoded.size()) {
                        break;
                    }

                    std::copy(
                            m_rx->recordedAmplitude.begin() + offsetTx*step,
                            m_rx->recordedAmplitude.begin() + offsetTx*step + m_samplesPerFrame, m_rx->fftInp.data());

                    // note : should we skip the first and last frame here as they are amplitude-smoothed?
                    for (int k = 1; k < rxProtocol.framesPerTx; ++k) {
                        for (int i = 0; i < m_samplesPerFrame; ++i) {
                            m_rx->fftInp[i] += m_rx->recordedAmplitude[(offsetTx + k*stepsPerFrame)*step + i];
                        }
                    }

                    FFT(m_rx->fftInp.data(), m_rx->fftOut.data(), m_samplesPerFrame, 1.0);

                    for (int i = 0; i < m_samplesPerFrame; ++i) {
                        m_rx->sampleSpectrum[i] = (m_rx->fftOut[2*i + 0]*m_rx->fftOut[2*i + 0] + m_rx->fftOut[2*i + 1]*m_rx->fftOut[2*i + 1]);
                    }
                    for (int i = 1; i < m_samplesPerFrame/2; ++i) {
                        m_rx->sampleSpectrum[i] += m_rx->sampleSpectrum[m_samplesPerFrame - i];
                    }

                    uint8_t curByte = 0;
                    for (int i = 0; i < 2*rxProtocol.bytesPerTx; ++i) {
                        double freq = m_hzPerSample*rxProtocol.freqStart;
                        int bin = round(freq*m_ihzPerSample) + 16*i;

                        int kmax = 0;
                        double amax = 0.0;
                        for (int k = 0; k < 16; ++k) {
                            if (m_rx->sampleSpectrum[bin + k] > amax) {
                                kmax = k;
                                amax = m_rx->sampleSpectrum[bin + k];
                            }
                        }

                        if (i%2) {
                            curByte += (kmax << 4);
                            m_dataEncoded[itx*rxProtocol.bytesPerTx + i/2] = curByte;
                            curByte = 0;
                        } else {
                            curByte = kmax;
                        }
                    }

                    if (itx*rxProtocol.bytesPerTx > m_encodedDataOffset && knownLength == false) {
                        RS::ReedSolomon rsLength(1, m_encodedDataOffset - 1, m_workRSLength.data());
                        if ((rsLength.Decode(m_dataEncoded.data(), m_rx->rxData.data()) == 0) && (m_rx->rxData[0] > 0 && m_rx->rxData[0] <= 140)) {
                            knownLength = true;
                            decodedLength = m_rx->rxData[0];

                            const int nTotalBytesExpected = m_encodedDataOffset + decodedLength + ::getECCBytesForLength(decodedLength);
                            const int nTotalFramesExpected = 2*m_nMarkerFrames + ((nTotalBytesExpected + rxProtocol.bytesPerTx - 1)/rxProtocol.bytesPerTx)*rxProtocol.framesPerTx;
                            if (m_rx->recvDuration_frames > nTotalFramesExpected ||
                                m_rx->recvDuration_frames < nTotalFramesExpected - 2*m_nMarkerFrames) {
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
                    RS::ReedSolomon rsData(decodedLength, ::getECCBytesForLength(decodedLength), m_workRSData.data());

                    if (rsData.Decode(m_dataEncoded.data() + m_encodedDataOffset, m_rx->rxData.data()) == 0) {
                        if (m_rx->rxData[0] != 0) {
                            ggprintf("Decoded length = %d, protocol = '%s' (%d)\n", decodedLength, rxProtocol.name, rxProtocolId);
                            ggprintf("Received sound data successfully: '%s'\n", m_rx->rxData.data());

                            isValid = true;
                            m_rx->hasNewRxData = true;
                            m_rx->lastRxDataLength = decodedLength;
                            m_rx->rxProtocol = rxProtocol;
                            m_rx->rxProtocolId = TxProtocolId(rxProtocolId);
                        }
                    }
                }

                if (isValid) {
                    break;
                }
                --m_rx->framesLeftToAnalyze;
            }

            if (isValid) break;
        }

        m_rx->framesToRecord = 0;

        if (isValid == false) {
            ggprintf("Failed to capture sound data. Please try again (length = %d)\n", m_rx->rxData[0]);
            m_rx->lastRxDataLength = -1;
            m_rx->framesToRecord = -1;
        }

        m_rx->receivingData = false;
        m_rx->analyzingData = false;

        std::fill(m_rx->sampleSpectrum.begin(), m_rx->sampleSpectrum.end(), 0.0f);

        m_rx->framesToAnalyze = 0;
        m_rx->framesLeftToAnalyze = 0;
    }

    // check if receiving data
    if (m_rx->receivingData == false) {
        bool isReceiving = false;

        for (const auto & rxProtocol : m_rx->rxProtocols) {
            if (rxProtocol.enabled == false) {
                continue;
            }

            int nDetectedMarkerBits = m_nBitsInMarker;

            for (int i = 0; i < m_nBitsInMarker; ++i) {
                double freq = bitFreq(rxProtocol, i);
                int bin = round(freq*m_ihzPerSample);

                if (i%2 == 0) {
                    if (m_rx->sampleSpectrum[bin] <= m_soundMarkerThreshold*m_rx->sampleSpectrum[bin + m_freqDelta_bin]) --nDetectedMarkerBits;
                } else {
                    if (m_rx->sampleSpectrum[bin] >= m_soundMarkerThreshold*m_rx->sampleSpectrum[bin + m_freqDelta_bin]) --nDetectedMarkerBits;
                }
            }

            if (nDetectedMarkerBits == m_nBitsInMarker) {
                m_rx->markerFreqStart = rxProtocol.freqStart;
                isReceiving = true;
                break;
            }
        }

        if (isReceiving) {
            if (++m_rx->nMarkersSuccess >= 1) {
            } else {
                isReceiving = false;
            }
        } else {
            m_rx->nMarkersSuccess = 0;
        }

        if (isReceiving) {
            std::time_t timestamp = std::time(nullptr);
            ggprintf("%sReceiving sound data ...\n", std::asctime(std::localtime(&timestamp)));

            m_rx->receivingData = true;
            std::fill(m_rx->rxData.begin(), m_rx->rxData.end(), 0);

            // max recieve duration
            m_rx->recvDuration_frames =
                2*m_nMarkerFrames +
                maxFramesPerTx()*((kMaxLengthVariable + ::getECCBytesForLength(kMaxLengthVariable))/minBytesPerTx() + 1);

            m_rx->nMarkersSuccess = 0;
            m_rx->framesToRecord = m_rx->recvDuration_frames;
            m_rx->framesLeftToRecord = m_rx->recvDuration_frames;
        }
    } else {
        bool isEnded = false;

        for (const auto & rxProtocol : m_rx->rxProtocols) {
            if (rxProtocol.enabled == false) {
                continue;
            }

            int nDetectedMarkerBits = m_nBitsInMarker;

            for (int i = 0; i < m_nBitsInMarker; ++i) {
                double freq = bitFreq(rxProtocol, i);
                int bin = round(freq*m_ihzPerSample);

                if (i%2 == 0) {
                    if (m_rx->sampleSpectrum[bin] >= m_soundMarkerThreshold*m_rx->sampleSpectrum[bin + m_freqDelta_bin]) nDetectedMarkerBits--;
                } else {
                    if (m_rx->sampleSpectrum[bin] <= m_soundMarkerThreshold*m_rx->sampleSpectrum[bin + m_freqDelta_bin]) nDetectedMarkerBits--;
                }
            }

            if (nDetectedMarkerBits == m_nBitsInMarker) {
                isEnded = true;
                break;
            }
        }

        if (isEnded) {
            if (++m_rx->nMarkersSuccess >= 1) {
            } else {
                isEnded = false;
            }
        } else {
            m_rx->nMarkersSuccess = 0;
        }

        if (isEnded && m_rx->framesToRecord > 1) {
            std::time_t timestamp = std::time(nullptr);
            m_rx->recvDuration_frames -= m_rx->framesLeftToRecord - 1;
            ggprintf("%sReceived end marker. Frames left = %d, recorded = %d\n", std::asctime(std::localtime(&timestamp)), m_rx->framesLeftToRecord, m_rx->recvDuration_frames);
            m_rx->nMarkersSuccess = 0;
            m_rx->framesLeftToRecord = 1;
        }
    }
}

//
// Fixed payload length

void GGWave::decode_fixed() {
    m_rx->hasNewSpectrum = true;

    // calculate spectrum
    FFT(m_rx->sampleAmplitude.data(), m_rx->fftOut.data(), m_samplesPerFrame, 1.0);

    float amax = 0.0f;
    for (int i = 0; i < m_samplesPerFrame; ++i) {
        m_rx->sampleSpectrum[i] = (m_rx->fftOut[2*i + 0]*m_rx->fftOut[2*i + 0] + m_rx->fftOut[2*i + 1]*m_rx->fftOut[2*i + 1]);
    }
    for (int i = 1; i < m_samplesPerFrame/2; ++i) {
        m_rx->sampleSpectrum[i] += m_rx->sampleSpectrum[m_samplesPerFrame - i];
        amax = std::max(amax, m_rx->sampleSpectrum[i]);
    }

    // float -> uint8_t
    //m_rx->spectrumHistoryFixed[m_rx->historyIdFixed] = m_rx->sampleSpectrum;
    for (int i = 0; i < m_samplesPerFrame; ++i) {
        m_rx->spectrumHistoryFixed[m_rx->historyIdFixed][i] = std::min(255.0, std::max(0.0, round(m_rx->sampleSpectrum[i]/amax*255.0f)));
    }

    if (++m_rx->historyIdFixed >= (int) m_rx->spectrumHistoryFixed.size()) {
        m_rx->historyIdFixed = 0;
    }

    bool isValid = false;
    for (int rxProtocolId = 0; rxProtocolId < (int) m_rx->rxProtocols.size(); ++rxProtocolId) {
        const auto & rxProtocol = m_rx->rxProtocols[rxProtocolId];
        if (rxProtocol.enabled == false) {
            continue;
        }

        const int binStart = rxProtocol.freqStart;
        const int binDelta = 16;
        const int binOffset = rxProtocol.extra == 1 ? binDelta : 0;

        if (binStart > m_samplesPerFrame) {
            continue;
        }

        const int totalLength = m_payloadLength + getECCBytesForLength(m_payloadLength);
        const int totalTxs = rxProtocol.extra*((totalLength + rxProtocol.bytesPerTx - 1)/rxProtocol.bytesPerTx);

        int historyStartId = m_rx->historyIdFixed - totalTxs*rxProtocol.framesPerTx;
        if (historyStartId < 0) {
            historyStartId += m_rx->spectrumHistoryFixed.size();
        }

        const int nTones = 2*rxProtocol.bytesPerTx;
        std::fill(m_rx->detectedBins.begin(), m_rx->detectedBins.end(), 0);

        int txNeededTotal   = 0;
        int txDetectedTotal = 0;
        bool detectedSignal = true;

        for (int k = 0; k < totalTxs; ++k) {
            if (k % rxProtocol.extra == 0) {
                std::fill(m_rx->detectedTones.begin(), m_rx->detectedTones.begin() + 16*nTones, 0);
            }

            for (int i = 0; i < rxProtocol.framesPerTx; ++i) {
                int historyId = historyStartId + k*rxProtocol.framesPerTx + i;
                if (historyId >= (int) m_rx->spectrumHistoryFixed.size()) {
                    historyId -= m_rx->spectrumHistoryFixed.size();
                }

                for (int j = 0; j < rxProtocol.bytesPerTx; ++j) {
                    int f0bin = -1;
                    int f1bin = -1;

                    double f0max = 0.0;
                    double f1max = 0.0;

                    for (int b = 0; b < 16; ++b) {
                        {
                            const auto & v = m_rx->spectrumHistoryFixed[historyId][binStart + 2*j*binDelta + b];

                            if (f0max <= v) {
                                f0max = v;
                                f0bin = b;
                            }
                        }

                        {
                            const auto & v = m_rx->spectrumHistoryFixed[historyId][binStart + 2*j*binDelta + binOffset + b];

                            if (f1max <= v) {
                                f1max = v;
                                f1bin = b;
                            }
                        }
                    }

                    if ((k + 0)%rxProtocol.extra == 0) m_rx->detectedTones[(2*j + 0)*16 + f0bin]++;
                    if ((k + 1)%rxProtocol.extra == 0) m_rx->detectedTones[(2*j + 1)*16 + f1bin]++;
                }
            }

            if (rxProtocol.extra > 1 && (k % rxProtocol.extra == 0)) continue;

            int txNeeded = 0;
            int txDetected = 0;
            for (int j = 0; j < rxProtocol.bytesPerTx; ++j) {
                if ((k/rxProtocol.extra)*rxProtocol.bytesPerTx + j >= totalLength) break;
                txNeeded += 2;
                for (int b = 0; b < 16; ++b) {
                    if (m_rx->detectedTones[(2*j + 0)*16 + b] > rxProtocol.framesPerTx/2) {
                        m_rx->detectedBins[2*((k/rxProtocol.extra)*rxProtocol.bytesPerTx + j) + 0] = b;
                        txDetected++;
                    }
                    if (m_rx->detectedTones[(2*j + 1)*16 + b] > rxProtocol.framesPerTx/2) {
                        m_rx->detectedBins[2*((k/rxProtocol.extra)*rxProtocol.bytesPerTx + j) + 1] = b;
                        txDetected++;
                    }
                }
            }

            txDetectedTotal += txDetected;
            txNeededTotal += txNeeded;
        }

        if (txDetectedTotal < 0.75*txNeededTotal) {
            detectedSignal = false;
        }

        if (detectedSignal) {
            RS::ReedSolomon rsData(m_payloadLength, getECCBytesForLength(m_payloadLength), m_workRSData.data());

            for (int j = 0; j < totalLength; ++j) {
                m_dataEncoded[j] = (m_rx->detectedBins[2*j + 1] << 4) + m_rx->detectedBins[2*j + 0];
            }

            if (rsData.Decode(m_dataEncoded.data(), m_rx->rxData.data()) == 0) {
                if (m_rx->rxData[0] != 0) {
                    ggprintf("Decoded length = %d, protocol = '%s' (%d)\n", m_rx->rxData[0], rxProtocol.name, rxProtocolId);
                    ggprintf("Received sound data successfully: '%s'\n", m_rx->rxData.data());

                    isValid = true;
                    m_rx->hasNewRxData = true;
                    m_rx->lastRxDataLength = m_payloadLength;
                    m_rx->rxProtocol = rxProtocol;
                    m_rx->rxProtocolId = TxProtocolId(rxProtocolId);
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
        if (protocol.enabled == false) {
            continue;
        }
        res = std::max(res, protocol.framesPerTx);
    }
    return res;
}

int GGWave::minBytesPerTx() const {
    int res = 1;
    for (const auto & protocol : getTxProtocols()) {
        if (protocol.enabled == false) {
            continue;
        }
        res = std::min(res, protocol.bytesPerTx);
    }
    return res;
}

int GGWave::maxBytesPerTx() const {
    int res = 1;
    for (const auto & protocol : getTxProtocols()) {
        if (protocol.enabled == false) {
            continue;
        }
        res = std::max(res, protocol.bytesPerTx);
    }
    return res;
}
