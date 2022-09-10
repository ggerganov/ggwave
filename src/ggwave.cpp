#include "ggwave/ggwave.h"

#if !defined(ARDUINO) && !defined(PROGMEM)
#define PROGMEM
#endif

#include "fft.h"
#include "reed-solomon/rs.hpp"

#include <math.h>
#include <stdio.h>
//#include <random>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifdef GGWAVE_DISABLE_LOG
#define ggprintf(...)
#else
#ifdef ARDUINO
#define ggprintf(...)
#else
#define ggprintf(...) \
    g_fptr && fprintf(g_fptr, __VA_ARGS__)
#endif
#endif

#define GG_MIN(A, B) (((A) < (B)) ? (A) : (B))
#define GG_MAX(A, B) (((A) >= (B)) ? (A) : (B))

//
// C interface
//

namespace {

FILE * g_fptr = stderr;
GGWave * g_instances[GGWAVE_MAX_INSTANCES];

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
ggwave_Instance ggwave_init(ggwave_Parameters parameters) {
    for (ggwave_Instance id = 0; id < GGWAVE_MAX_INSTANCES; ++id) {
        if (g_instances[id] == nullptr) {
            g_instances[id] = new GGWave({
                parameters.payloadLength,
                parameters.sampleRateInp,
                parameters.sampleRateOut,
                parameters.sampleRate,
                parameters.samplesPerFrame,
                parameters.soundMarkerThreshold,
                parameters.sampleFormatInp,
                parameters.sampleFormatOut,
                parameters.operatingMode});

            return id;
        }
    }

    ggprintf("Failed to create GGWave instance - reached maximum number of instances (%d)\n", GGWAVE_MAX_INSTANCES);

    return -1;
}

extern "C"
void ggwave_free(ggwave_Instance id) {
    if (id >= 0 && id < GGWAVE_MAX_INSTANCES && g_instances[id]) {
        delete (GGWave *) g_instances[id];
        g_instances[id] = nullptr;

        return;
    }

    ggprintf("Failed to free GGWave instance - invalid GGWave instance id %d\n", id);
}

extern "C"
int ggwave_encode(
        ggwave_Instance id,
        const void * payloadBuffer,
        int payloadSize,
        ggwave_ProtocolId protocolId,
        int volume,
        void * waveformBuffer,
        int query) {
    GGWave * ggWave = (GGWave *) g_instances[id];

    if (ggWave == nullptr) {
        ggprintf("Invalid GGWave instance %d\n", id);
        return -1;
    }

    if (ggWave->init(payloadSize, (const char *) payloadBuffer, protocolId, volume) == false) {
        ggprintf("Failed to initialize Tx transmission for GGWave instance %d\n", id);
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
        ggprintf("Failed to encode data - GGWave instance %d\n", id);
        return -1;
    }

    {
        auto pSrc = (const char *) ggWave->txWaveform();
        auto pDst = (      char *) waveformBuffer;
        memcpy(pDst, pSrc, nBytes);
    }

    return nBytes;
}

extern "C"
int ggwave_decode(
        ggwave_Instance id,
        const void * waveformBuffer,
        int waveformSize,
        void * payloadBuffer) {
    GGWave * ggWave = (GGWave *) g_instances[id];

    if (ggWave->decode(waveformBuffer, waveformSize) == false) {
        ggprintf("Failed to decode data - GGWave instance %d\n", id);
        return -1;
    }

    static thread_local GGWave::TxRxData data;

    const auto dataLength = ggWave->rxTakeData(data);
    if (dataLength == -1) {
        // failed to decode message
        return -1;
    } else if (dataLength > 0) {
        memcpy(payloadBuffer, data.data(), dataLength);
    }

    return dataLength;
}

extern "C"
int ggwave_ndecode(
        ggwave_Instance id,
        const void * waveformBuffer,
        int waveformSize,
        void * payloadBuffer,
        int payloadSize) {
    GGWave * ggWave = (GGWave *) g_instances[id];

    if (ggWave->decode(waveformBuffer, waveformSize) == false) {
        ggprintf("Failed to decode data - GGWave instance %d\n", id);
        return -1;
    }

    static thread_local GGWave::TxRxData data;

    const auto dataLength = ggWave->rxTakeData(data);
    if (dataLength == -1) {
        // failed to decode message
        return -1;
    } else if (dataLength > payloadSize) {
        // the payloadBuffer is not big enough to store the data
        return -2;
    } else if (dataLength > 0) {
        memcpy(payloadBuffer, data.data(), dataLength);
    }

    return dataLength;
}

extern "C"
void ggwave_rxToggleProtocol(
        ggwave_ProtocolId protocolId,
        int state) {
    GGWave::Protocols::rx().toggle(protocolId, state != 0);
}

extern "C"
void ggwave_txToggleProtocol(
        ggwave_ProtocolId protocolId,
        int state) {
    GGWave::Protocols::tx().toggle(protocolId, state != 0);
}

extern "C"
void ggwave_rxProtocolSetFreqStart(
        ggwave_ProtocolId protocolId,
        int freqStart) {
    GGWave::Protocols::rx()[protocolId].freqStart = freqStart;
}

extern "C"
void ggwave_txProtocolSetFreqStart(
        ggwave_ProtocolId protocolId,
        int freqStart) {
    GGWave::Protocols::tx()[protocolId].freqStart = freqStart;
}

//
// C++ implementation
//

namespace {

// magic numbers used to XOR the Rx / Tx data
// this achieves more homogeneous distribution of the sound energy across the spectrum
constexpr int kDSSMagicSize = 64;
const uint8_t kDSSMagic[kDSSMagicSize] PROGMEM = {
    0x96, 0x9f, 0xb4, 0xaf, 0x1b, 0x91, 0xde, 0xc5, 0x45, 0x75, 0xe8, 0x2e, 0x0f, 0x32, 0x4a, 0x5f,
    0xb4, 0x56, 0x95, 0xcb, 0x7f, 0x6a, 0x54, 0x6a, 0x48, 0xf2, 0x0b, 0x7b, 0xcd, 0xfb, 0x93, 0x6d,
    0x3c, 0x77, 0x5e, 0xc3, 0x33, 0x47, 0xc0, 0xf1, 0x71, 0x32, 0x33, 0x27, 0x35, 0x68, 0x47, 0x1f,
    0x4e, 0xac, 0x23, 0x42, 0x5f, 0x00, 0x37, 0xa4, 0x50, 0x6d, 0x48, 0x24, 0x91, 0x7c, 0xa1, 0x4e,
};

uint8_t getDSSMagic(int i) {
#ifdef ARDUINO
    return pgm_read_byte(&kDSSMagic[i % kDSSMagicSize]);
#else
    return kDSSMagic[i % kDSSMagicSize];
#endif
}

void FFT(float * f, int N, int * wi, float * wf) {
    rdft(N, 1, f, wi, wf);
}

void FFT(const float * src, float * dst, int N, int * wi, float * wf) {
    memcpy(dst, src, N * sizeof(float));

    FFT(dst, N, wi, wf);
}

inline void addAmplitudeSmooth(
        const GGWave::Amplitude & src,
        GGWave::Amplitude & dst,
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
    return len < 4 ? 2 : GG_MAX(4, 2*(len/5));
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

//
// ggvector
//

template<typename T>
void ggvector<T>::assign(const ggvector & other) {
    m_data = other.m_data;
    m_size = other.m_size;
}

template<typename T>
void ggvector<T>::copy(const ggvector & other) {
    if (this == &other) {
        assert(false);
    }

    memcpy(m_data, other.m_data, GG_MIN(m_size, other.m_size)*sizeof(T));
}

template<typename T>
void ggvector<T>::zero() {
    memset(m_data, 0, m_size*sizeof(T));
}

template<typename T>
void ggvector<T>::zero(int n) {
    memset(m_data, 0, n*sizeof(T));
}

template struct ggvector<int16_t>;

//
// ggmatrix
//

template <typename T>
void ggmatrix<T>::zero() {
    if (m_size0 > 0 && m_size1 > 0) {
        memset(m_data, 0, m_size0*m_size1*sizeof(T));
    }
}

//
// Protocols
//

void GGWave::Protocols::enableAll() {
    for (int i = 0; i < GGWAVE_PROTOCOL_COUNT; i++) {
        auto & p = this->data[i];
        if (p.name) {
            p.enabled = true;
        }
    }
}

void GGWave::Protocols::disableAll() {
    for (int i = 0; i < GGWAVE_PROTOCOL_COUNT; i++) {
        auto & p = this->data[i];
        p.enabled = false;
    }
}

void GGWave::Protocols::toggle(ProtocolId id, bool state) {
    if (state) {
        // enable protocol
        data[id].enabled = true;
    } else {
        // disable protocol
        data[id].enabled = false;
    }
}

void GGWave::Protocols::only(ProtocolId id) {
    disableAll();
    data[id].enabled = true;
}

GGWave::TxProtocols & GGWave::Protocols::tx() {
    static TxProtocols protocols = kDefault();

    return protocols;
}

GGWave::RxProtocols & GGWave::Protocols::rx() {
    static RxProtocols protocols = kDefault();

    return protocols;
}

// this probably does not matter, but adding it anyway
#ifdef ARDUINO
const int kAlignment = 4;
#else
const int kAlignment = 8;
#endif

//template <typename T>
//void ggalloc(std::vector<T> & v, int n, void * buf, int & bufSize) {
//    if (buf == nullptr) {
//        bufSize += n*sizeof(T);
//        bufSize = ((bufSize + kAlignment - 1)/kAlignment)*kAlignment;
//        return;
//    }
//
//    v.resize(n);
//    bufSize += n*sizeof(T);
//    bufSize = ((bufSize + kAlignment - 1)/kAlignment)*kAlignment;
//}
//
//template <typename T>
//void ggalloc(std::vector<std::vector<T>> & v, int n, int m, void * buf, int & bufSize) {
//    if (buf == nullptr) {
//        bufSize += n*m*sizeof(T);
//        bufSize = ((bufSize + kAlignment - 1)/kAlignment)*kAlignment;
//        return;
//    }
//
//    v.resize(n);
//    for (int i = 0; i < n; i++) {
//        v[i].resize(m);
//    }
//    bufSize += n*m*sizeof(T);
//    bufSize = ((bufSize + kAlignment - 1)/kAlignment)*kAlignment;
//}

template <typename T>
void ggalloc(ggvector<T> & v, int n, void * buf, int & bufSize) {
    if (buf == nullptr) {
        bufSize += n*sizeof(T);
        bufSize = ((bufSize + kAlignment - 1)/kAlignment)*kAlignment;
        return;
    }

    v.assign(ggvector<T>((T *)((char *) buf + bufSize), n));
    bufSize += n*sizeof(T);
    bufSize = ((bufSize + kAlignment - 1)/kAlignment)*kAlignment;
}

template <typename T>
void ggalloc(ggmatrix<T> & v, int n, int m, void * buf, int & bufSize) {
    if (buf == nullptr) {
        bufSize += n*m*sizeof(T);
        bufSize = ((bufSize + kAlignment - 1) / kAlignment)*kAlignment;
        return;
    }

    v = ggmatrix<T>((T *)((char *) buf + bufSize), n, m);
    bufSize += n*m*sizeof(T);
    bufSize = ((bufSize + kAlignment - 1)/kAlignment)*kAlignment;
}

//
// GGWave
//

GGWave::GGWave(const Parameters & parameters) {
    prepare(parameters);
}

GGWave::~GGWave() {
    if (m_heap) {
        free(m_heap);
    }
}

bool GGWave::prepare(const Parameters & parameters, bool allocate) {
    if (m_heap) {
        free(m_heap);
        m_heap = nullptr;
        m_heapSize = 0;
    }

    // parameter initialization:

    m_sampleRateInp        = parameters.sampleRateInp;
    m_sampleRateOut        = parameters.sampleRateOut;
    m_sampleRate           = parameters.sampleRate;
    m_samplesPerFrame      = parameters.samplesPerFrame;
    m_isamplesPerFrame     = 1.0f/m_samplesPerFrame;
    m_sampleSizeInp        = bytesForSampleFormat(parameters.sampleFormatInp);
    m_sampleSizeOut        = bytesForSampleFormat(parameters.sampleFormatOut);
    m_sampleFormatInp      = parameters.sampleFormatInp;
    m_sampleFormatOut      = parameters.sampleFormatOut;
    m_hzPerSample          = m_sampleRate/m_samplesPerFrame;
    m_ihzPerSample         = 1.0f/m_hzPerSample;
    m_freqDelta_bin        = 1;
    m_freqDelta_hz         = 2*m_hzPerSample;
    m_nBitsInMarker        = 16;
    m_nMarkerFrames        = parameters.payloadLength > 0 ? 0 : kDefaultMarkerFrames;
    m_encodedDataOffset    = parameters.payloadLength > 0 ? 0 : kDefaultEncodedDataOffset;
    m_soundMarkerThreshold = parameters.soundMarkerThreshold;
    m_isFixedPayloadLength = parameters.payloadLength > 0;
    m_payloadLength        = parameters.payloadLength;
    m_isRxEnabled          = parameters.operatingMode & GGWAVE_OPERATING_MODE_RX;
    m_isTxEnabled          = parameters.operatingMode & GGWAVE_OPERATING_MODE_TX;
    m_needResampling       = m_sampleRateInp != m_sampleRate || m_sampleRateOut != m_sampleRate;
    m_txOnlyTones          = parameters.operatingMode & GGWAVE_OPERATING_MODE_TX_ONLY_TONES;
    m_isDSSEnabled         = parameters.operatingMode & GGWAVE_OPERATING_MODE_USE_DSS;

    if (m_sampleSizeInp == 0) {
        ggprintf("Invalid or unsupported capture sample format: %d\n", (int) parameters.sampleFormatInp);
        return false;
    }

    if (m_sampleSizeOut == 0) {
        ggprintf("Invalid or unsupported playback sample format: %d\n", (int) parameters.sampleFormatOut);
        return false;
    }

    if (parameters.samplesPerFrame > kMaxSamplesPerFrame) {
        ggprintf("Invalid samples per frame: %d, max: %d\n", parameters.samplesPerFrame, kMaxSamplesPerFrame);
        return false;
    }

    if (m_sampleRateInp < kSampleRateMin) {
        ggprintf("Error: capture sample rate (%g Hz) must be >= %g Hz\n", m_sampleRateInp, kSampleRateMin);
        return false;
    }

    if (m_sampleRateInp > kSampleRateMax) {
        ggprintf("Error: capture sample rate (%g Hz) must be <= %g Hz\n", m_sampleRateInp, kSampleRateMax);
        return false;
    }

    // memory allocation:

    m_heap = nullptr;
    m_heapSize = 0;

    if (this->alloc(m_heap, m_heapSize) == false) {
        ggprintf("Error: failed to compute the size of the required memory\n");
        return false;
    }

    if (allocate == false) {
        return true;
    }

    const auto heapSize0 = m_heapSize;

    m_heap = calloc(m_heapSize, 1);

    m_heapSize = 0;
    if (this->alloc(m_heap, m_heapSize) == false) {
        ggprintf("Error: failed to allocate the required memory: %d\n", m_heapSize);
        return false;
    }

    if (heapSize0 != m_heapSize) {
        ggprintf("Error: failed to allocate memory - heapSize0: %d, heapSize: %d\n", heapSize0, m_heapSize);
        return false;
    }

    if (m_isRxEnabled) {
        m_rx.samplesNeeded = m_samplesPerFrame;

        m_rx.fftWorkI[0] = 0;

        m_rx.protocol   = {};
        m_rx.protocolId = GGWAVE_PROTOCOL_COUNT;
        m_rx.protocols  = Protocols::rx();

        m_rx.minFreqStart = minFreqStart(m_rx.protocols);
    }

    if (m_isTxEnabled) {
        m_tx.protocols = Protocols::tx();
    }

    return init("", {}, 0);
}

bool GGWave::alloc(void * p, int & n) {
    const int maxLength   = m_isFixedPayloadLength ? m_payloadLength : kMaxLengthVariable;
    const int totalLength = maxLength + getECCBytesForLength(maxLength);
    const int totalTxs    = (totalLength + minBytesPerTx(Protocols::rx()) - 1)/minBytesPerTx(Protocols::tx());

    if (totalLength > kMaxDataSize) {
        ggprintf("Error: total length %d (payload %d + ECC %d bytes) is too large ( > %d)\n",
                 totalLength, maxLength, getECCBytesForLength(maxLength), kMaxDataSize);
        return false;
    }

    // common
    ::ggalloc(m_dataEncoded, totalLength + m_encodedDataOffset, p, n);

    if (m_isRxEnabled) {
        ::ggalloc(m_rx.fftOut,   2*m_samplesPerFrame, p, n);
        ::ggalloc(m_rx.fftWorkI, 3 + sqrt(m_samplesPerFrame/2), p, n);
        ::ggalloc(m_rx.fftWorkF, m_samplesPerFrame/2, p, n);

        ::ggalloc(m_rx.spectrum,           m_samplesPerFrame, p, n);
        // small extra space because sometimes resampling needs a few more samples:
        ::ggalloc(m_rx.amplitude,          m_needResampling ? m_samplesPerFrame + 128 : m_samplesPerFrame, p, n);
        // min input sampling rate is 0.125*m_sampleRate:
        ::ggalloc(m_rx.amplitudeResampled, m_needResampling ? 8*m_samplesPerFrame : m_samplesPerFrame, p, n);
        ::ggalloc(m_rx.amplitudeTmp,       m_needResampling ? 8*m_samplesPerFrame*m_sampleSizeInp : m_samplesPerFrame*m_sampleSizeInp, p, n);

        ::ggalloc(m_rx.data, maxLength + 1, p, n); // extra byte for null-termination

        if (m_isFixedPayloadLength) {
            if (m_payloadLength > kMaxLengthFixed) {
                ggprintf("Invalid payload length: %d, max: %d\n", m_payloadLength, kMaxLengthFixed);
                return false;
            }

            ::ggalloc(m_rx.spectrumHistoryFixed, totalTxs*maxFramesPerTx(Protocols::rx(), false), m_samplesPerFrame, p, n);
            ::ggalloc(m_rx.detectedBins,         2*totalLength, p, n);
            ::ggalloc(m_rx.detectedTones,        2*16*maxBytesPerTx(Protocols::rx()), p, n);
        } else {
            // variable payload length
            ::ggalloc(m_rx.amplitudeRecorded, kMaxRecordedFrames*m_samplesPerFrame, p, n);
            ::ggalloc(m_rx.amplitudeAverage,  m_samplesPerFrame, p, n);
            ::ggalloc(m_rx.amplitudeHistory,  kMaxSpectrumHistory, m_samplesPerFrame, p, n);
        }
    }

    if (m_isTxEnabled) {
        const int maxDataBits = 2*16*maxBytesPerTx(Protocols::tx());

        if (m_txOnlyTones == false) {
            ::ggalloc(m_tx.phaseOffsets,    maxDataBits, p, n);
            ::ggalloc(m_tx.bit0Amplitude,   maxDataBits, m_samplesPerFrame, p, n);
            ::ggalloc(m_tx.bit1Amplitude,   maxDataBits, m_samplesPerFrame, p, n);
            ::ggalloc(m_tx.output,          m_samplesPerFrame, p, n);
            ::ggalloc(m_tx.outputResampled, 2*m_samplesPerFrame, p, n);
            ::ggalloc(m_tx.outputTmp,       kMaxRecordedFrames*m_samplesPerFrame*m_sampleSizeOut, p, n);
            ::ggalloc(m_tx.outputI16,       kMaxRecordedFrames*m_samplesPerFrame, p, n);
        }

        const int maxTones    = m_isFixedPayloadLength ? maxTonesPerTx(Protocols::tx()) : m_nBitsInMarker;

        ::ggalloc(m_tx.data,     maxLength + 1, p, n); // first byte stores the length
        ::ggalloc(m_tx.dataBits, maxDataBits, p, n);
        ::ggalloc(m_tx.tones,    maxTones*totalTxs + (maxTones > 1 ? totalTxs : 0), p, n);
    }

    // pre-allocate Reed-Solomon memory buffers
    {
        const auto maxLength = m_isFixedPayloadLength ? m_payloadLength : kMaxLengthVariable;

        if (m_isFixedPayloadLength == false) {
            ::ggalloc(m_workRSLength, RS::ReedSolomon::getWorkSize_bytes(1, m_encodedDataOffset - 1), p, n);
        }
        ::ggalloc(m_workRSData, RS::ReedSolomon::getWorkSize_bytes(maxLength, getECCBytesForLength(maxLength)), p, n);
    }

    if (m_needResampling) {
        m_resampler.alloc(p, n);
    }

    return true;
}

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
        GGWAVE_OPERATING_MODE_RX | GGWAVE_OPERATING_MODE_TX,
    };

    return result;
}

bool GGWave::init(const char * text, TxProtocolId protocolId, const int volume) {
    return init(strlen(text), text, protocolId, volume);
}

bool GGWave::init(int dataSize, const char * dataBuffer, TxProtocolId protocolId, const int volume) {
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

        m_tx.hasData = false;
        m_tx.data.zero();
        m_dataEncoded.zero();

        if (dataSize > 0) {
            if (protocolId < 0 || protocolId >= m_tx.protocols.size()) {
                ggprintf("Invalid protocol ID: %d\n", protocolId);
                return false;
            }

            const auto & protocol = m_tx.protocols[protocolId];

            if (protocol.enabled == false) {
                ggprintf("Protocol %d is not enabled - make sure to enable it before creating the instance\n", protocolId);
                return false;
            }

            if (protocol.extra == 2 && m_isFixedPayloadLength == false) {
                ggprintf("Mono-tone protocols with variable length are not supported\n");
                return false;
            }

            m_tx.protocol   = protocol;
            m_tx.dataLength = m_isFixedPayloadLength ? m_payloadLength : dataSize;
            m_tx.sendVolume = ((double)(volume))/100.0f;

            m_tx.data[0] = m_tx.dataLength;
            for (int i = 0; i < m_tx.dataLength; ++i) {
                m_tx.data[i + 1] = i < dataSize ? dataBuffer[i] : 0;
                if (m_isDSSEnabled) {
                    m_tx.data[i + 1] ^= getDSSMagic(i);
                }
            }

            m_tx.hasData = true;
        }
    } else {
        if (dataSize > 0) {
            ggprintf("Tx is disabled - cannot transmit data with this GGWave instance\n");
        }
    }

    // Rx
    if (m_isRxEnabled) {
        m_rx.receiving = false;
        m_rx.analyzing = false;

        m_rx.framesToAnalyze = 0;
        m_rx.framesLeftToAnalyze = 0;
        m_rx.framesToRecord = 0;
        m_rx.framesLeftToRecord = 0;

        m_rx.spectrum.zero();
        m_rx.amplitude.zero();
        m_rx.amplitudeHistory.zero();

        m_rx.data.zero();

        m_rx.spectrumHistoryFixed.zero();
    }

    return true;
}

uint32_t GGWave::encodeSize_bytes() const {
    return encodeSize_samples()*m_sampleSizeOut;
}

uint32_t GGWave::encodeSize_samples() const {
    if (m_tx.hasData == false) {
        return 0;
    }

    float factor = 1.0f;
    int samplesPerFrameOut = m_samplesPerFrame;
    if (m_needResampling) {
        factor = m_sampleRate/m_sampleRateOut;
        // note : +1 extra sample in order to overestimate the buffer size
        samplesPerFrameOut = m_resampler.resample(factor, m_samplesPerFrame, m_tx.output.data(), nullptr) + 1;
    }
    const int nECCBytesPerTx = getECCBytesForLength(m_tx.dataLength);
    const int sendDataLength = m_tx.dataLength + m_encodedDataOffset;
    const int totalBytes = sendDataLength + nECCBytesPerTx;
    const int totalDataFrames = m_tx.protocol.extra*((totalBytes + m_tx.protocol.bytesPerTx - 1)/m_tx.protocol.bytesPerTx)*m_tx.protocol.framesPerTx;

    return (
            m_nMarkerFrames + totalDataFrames + m_nMarkerFrames
           )*samplesPerFrameOut;
}

uint32_t GGWave::encode() {
    if (m_isTxEnabled == false) {
        ggprintf("Tx is disabled - cannot transmit data with this GGWave instance\n");
        return 0;
    }

    if (m_needResampling) {
        m_resampler.reset();
    }

    const int nECCBytesPerTx = getECCBytesForLength(m_tx.dataLength);
    const int sendDataLength = m_tx.dataLength + m_encodedDataOffset;
    const int totalBytes = sendDataLength + nECCBytesPerTx;
    const int totalDataFrames = m_tx.protocol.extra*((totalBytes + m_tx.protocol.bytesPerTx - 1)/m_tx.protocol.bytesPerTx)*m_tx.protocol.framesPerTx;

    if (m_isFixedPayloadLength == false) {
        RS::ReedSolomon rsLength(1, m_encodedDataOffset - 1, m_workRSLength.data());
        rsLength.Encode(m_tx.data.data(), m_dataEncoded.data());
    }

    // first byte of m_tx.data contains the length of the payload, so we skip it:
    RS::ReedSolomon rsData = RS::ReedSolomon(m_tx.dataLength, nECCBytesPerTx, m_workRSData.data());
    rsData.Encode(m_tx.data.data() + 1, m_dataEncoded.data() + m_encodedDataOffset);

    // generate tones
    {
        int frameId = 0;
        bool hasData = m_tx.hasData;

        m_tx.nTones = 0;
        while (hasData) {
            if (frameId < m_nMarkerFrames) {
                for (int i = 0; i < m_nBitsInMarker; ++i) {
                    m_tx.tones[m_tx.nTones++] = 2*i + i%2;
                }
            } else if (frameId < m_nMarkerFrames + totalDataFrames) {
                int dataOffset = frameId - m_nMarkerFrames;
                dataOffset /= m_tx.protocol.framesPerTx;
                dataOffset *= m_tx.protocol.bytesPerTx;

                m_tx.dataBits.zero();

                for (int j = 0; j < m_tx.protocol.bytesPerTx; ++j) {
                    if (m_tx.protocol.extra == 1) {
                        {
                            uint8_t d = m_dataEncoded[dataOffset + j] & 15;
                            m_tx.dataBits[(2*j + 0)*16 + d] = 1;
                        }
                        {
                            uint8_t d = m_dataEncoded[dataOffset + j] & 240;
                            m_tx.dataBits[(2*j + 1)*16 + (d >> 4)] = 1;
                        }
                    } else {
                        if (dataOffset % m_tx.protocol.extra == 0) {
                            uint8_t d = m_dataEncoded[dataOffset/m_tx.protocol.extra + j] & 15;
                            m_tx.dataBits[(2*j + 0)*16 + d] = 1;
                        } else {
                            uint8_t d = m_dataEncoded[dataOffset/m_tx.protocol.extra + j] & 240;
                            m_tx.dataBits[(2*j + 0)*16 + (d >> 4)] = 1;
                        }
                    }
                }

                for (int k = 0; k < 2*m_tx.protocol.bytesPerTx*16; ++k) {
                    if (m_tx.dataBits[k] == 0) continue;

                    m_tx.tones[m_tx.nTones++] = k;
                }
            } else if (frameId < m_nMarkerFrames + totalDataFrames + m_nMarkerFrames) {
                for (int i = 0; i < m_nBitsInMarker; ++i) {
                    m_tx.tones[m_tx.nTones++] = 2*i + (1 - i%2);
                }
            } else {
                hasData = false;
                break;
            }

            if (m_tx.protocol.nTones() > 1) {
                m_tx.tones[m_tx.nTones++] = -1;
            }

            frameId += m_tx.protocol.framesPerTx;
        }

        if (m_txOnlyTones) {
            m_tx.hasData = false;
            return true;
        }
    }

    // compute Tx data
    {
        for (int k = 0; k < (int) m_tx.phaseOffsets.size(); ++k) {
            m_tx.phaseOffsets[k] = (M_PI*k)/(m_tx.protocol.nDataBitsPerTx());
        }

        // note : what is the purpose of this shuffle ? I forgot .. :(
        //std::random_device rd;
        //std::mt19937 g(rd());

        //std::shuffle(phaseOffsets.begin(), phaseOffsets.end(), g);

        for (int k = 0; k < (int) m_tx.dataBits.size(); ++k) {
            const double freq = bitFreq(m_tx.protocol, k);

            const double phaseOffset = m_tx.phaseOffsets[k];
            const double curHzPerSample = m_hzPerSample;
            const double curIHzPerSample = 1.0/curHzPerSample;

            for (int i = 0; i < m_samplesPerFrame; i++) {
                const double curi = i;
                m_tx.bit1Amplitude[k][i] = sin((2.0*M_PI)*(curi*m_isamplesPerFrame)*(freq*curIHzPerSample) + phaseOffset);
            }

            for (int i = 0; i < m_samplesPerFrame; i++) {
                const double curi = i;
                m_tx.bit0Amplitude[k][i] = sin((2.0*M_PI)*(curi*m_isamplesPerFrame)*((freq + m_hzPerSample*m_freqDelta_bin)*curIHzPerSample) + phaseOffset);
            }
        }
    }

    int frameId = 0;
    uint32_t offset = 0;
    const float factor = m_sampleRate/m_sampleRateOut;

    while (m_tx.hasData) {
        m_tx.output.zero();

        uint16_t nFreq = 0;
        if (frameId < m_nMarkerFrames) {
            nFreq = m_nBitsInMarker;

            for (int i = 0; i < m_nBitsInMarker; ++i) {
                if (i%2 == 0) {
                    ::addAmplitudeSmooth(m_tx.bit1Amplitude[i], m_tx.output, m_tx.sendVolume, 0, m_samplesPerFrame, frameId, m_nMarkerFrames);
                } else {
                    ::addAmplitudeSmooth(m_tx.bit0Amplitude[i], m_tx.output, m_tx.sendVolume, 0, m_samplesPerFrame, frameId, m_nMarkerFrames);
                }
            }
        } else if (frameId < m_nMarkerFrames + totalDataFrames) {
            int dataOffset = frameId - m_nMarkerFrames;
            int cycleModMain = dataOffset%m_tx.protocol.framesPerTx;
            dataOffset /= m_tx.protocol.framesPerTx;
            dataOffset *= m_tx.protocol.bytesPerTx;

            m_tx.dataBits.zero();

            for (int j = 0; j < m_tx.protocol.bytesPerTx; ++j) {
                if (m_tx.protocol.extra == 1) {
                    {
                        uint8_t d = m_dataEncoded[dataOffset + j] & 15;
                        m_tx.dataBits[(2*j + 0)*16 + d] = 1;
                    }
                    {
                        uint8_t d = m_dataEncoded[dataOffset + j] & 240;
                        m_tx.dataBits[(2*j + 1)*16 + (d >> 4)] = 1;
                    }
                } else {
                    if (dataOffset % m_tx.protocol.extra == 0) {
                        uint8_t d = m_dataEncoded[dataOffset/m_tx.protocol.extra + j] & 15;
                        m_tx.dataBits[(2*j + 0)*16 + d] = 1;
                    } else {
                        uint8_t d = m_dataEncoded[dataOffset/m_tx.protocol.extra + j] & 240;
                        m_tx.dataBits[(2*j + 0)*16 + (d >> 4)] = 1;
                    }
                }
            }

            for (int k = 0; k < 2*m_tx.protocol.bytesPerTx*16; ++k) {
                if (m_tx.dataBits[k] == 0) continue;

                ++nFreq;
                if (k%2) {
                    ::addAmplitudeSmooth(m_tx.bit0Amplitude[k/2], m_tx.output, m_tx.sendVolume, 0, m_samplesPerFrame, cycleModMain, m_tx.protocol.framesPerTx);
                } else {
                    ::addAmplitudeSmooth(m_tx.bit1Amplitude[k/2], m_tx.output, m_tx.sendVolume, 0, m_samplesPerFrame, cycleModMain, m_tx.protocol.framesPerTx);
                }
            }
        } else if (frameId < m_nMarkerFrames + totalDataFrames + m_nMarkerFrames) {
            nFreq = m_nBitsInMarker;

            const int fId = frameId - (m_nMarkerFrames + totalDataFrames);
            for (int i = 0; i < m_nBitsInMarker; ++i) {
                if (i%2 == 0) {
                    addAmplitudeSmooth(m_tx.bit0Amplitude[i], m_tx.output, m_tx.sendVolume, 0, m_samplesPerFrame, fId, m_nMarkerFrames);
                } else {
                    addAmplitudeSmooth(m_tx.bit1Amplitude[i], m_tx.output, m_tx.sendVolume, 0, m_samplesPerFrame, fId, m_nMarkerFrames);
                }
            }
        } else {
            m_tx.hasData = false;
            break;
        }

        if (nFreq == 0) nFreq = 1;
        const float scale = 1.0f/nFreq;
        for (int i = 0; i < m_samplesPerFrame; ++i) {
            m_tx.output[i] *= scale;
        }

        int samplesPerFrameOut = m_samplesPerFrame;
        if (m_needResampling) {
            samplesPerFrameOut = m_resampler.resample(factor, m_samplesPerFrame, m_tx.output.data(), m_tx.outputResampled.data());
        } else {
            m_tx.outputResampled.copy(m_tx.output);
        }

        // default output is in 16-bit signed int so we always compute it
        for (int i = 0; i < samplesPerFrameOut; ++i) {
            m_tx.outputI16[offset + i] = 32768*m_tx.outputResampled[i];
        }

        // convert from 32-bit float
        switch (m_sampleFormatOut) {
            case GGWAVE_SAMPLE_FORMAT_UNDEFINED: break;
            case GGWAVE_SAMPLE_FORMAT_U8:
                {
                    auto p = reinterpret_cast<uint8_t *>(m_tx.outputTmp.data());
                    for (int i = 0; i < samplesPerFrameOut; ++i) {
                        p[offset + i] = 128*(m_tx.outputResampled[i] + 1.0f);
                    }
                } break;
            case GGWAVE_SAMPLE_FORMAT_I8:
                {
                    auto p = reinterpret_cast<uint8_t *>(m_tx.outputTmp.data());
                    for (int i = 0; i < samplesPerFrameOut; ++i) {
                        p[offset + i] = 128*m_tx.outputResampled[i];
                    }
                } break;
            case GGWAVE_SAMPLE_FORMAT_U16:
                {
                    auto p = reinterpret_cast<uint16_t *>(m_tx.outputTmp.data());
                    for (int i = 0; i < samplesPerFrameOut; ++i) {
                        p[offset + i] = 32768*(m_tx.outputResampled[i] + 1.0f);
                    }
                } break;
            case GGWAVE_SAMPLE_FORMAT_I16:
                {
                    // skip because we already have the data in m_tx.outputI16
                    //auto p = reinterpret_cast<uint16_t *>(m_tx.outputTmp.data());
                    //for (int i = 0; i < samplesPerFrameOut; ++i) {
                    //    p[offset + i] = 32768*m_tx.outputResampled[i];
                    //}
                } break;
            case GGWAVE_SAMPLE_FORMAT_F32:
                {
                    auto p = reinterpret_cast<float *>(m_tx.outputTmp.data());
                    for (int i = 0; i < samplesPerFrameOut; ++i) {
                        p[offset + i] = m_tx.outputResampled[i];
                    }
                } break;
        }

        ++frameId;
        offset += samplesPerFrameOut;
    }

    m_tx.lastAmplitudeSize = offset;

    // the encoded waveform can be accessed via the txWaveform() method
    // we return the size of the waveform in bytes:
    return offset*m_sampleSizeOut;
}

bool GGWave::decode(const void * data, uint32_t nBytes) {
    if (m_isRxEnabled == false) {
        ggprintf("Rx is disabled - cannot receive data with this GGWave instance\n");
        return false;
    }

    if (m_tx.hasData) {
        ggprintf("Cannot decode while transmitting\n");
        return false;
    }

    auto dataBuffer = (uint8_t *) data;
    const float factor = m_sampleRateInp/m_sampleRate;

    while (true) {
        // read capture data
        uint32_t nBytesNeeded = m_rx.samplesNeeded*m_sampleSizeInp;

        if (m_needResampling) {
            // note : predict 4 extra samples just to make sure we have enough data
            nBytesNeeded = (m_resampler.resample(1.0f/factor, m_rx.samplesNeeded, m_rx.amplitudeResampled.data(), nullptr) + 4)*m_sampleSizeInp;
        }

        const uint32_t nBytesRecorded = GG_MIN(nBytes, nBytesNeeded);

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
                    memcpy(m_rx.amplitudeTmp.data(), dataBuffer, nBytesRecorded);
                } break;
            case GGWAVE_SAMPLE_FORMAT_F32:
                {
                    memcpy(m_rx.amplitudeResampled.data(), dataBuffer, nBytesRecorded);
                } break;
        }

        dataBuffer += nBytesRecorded;
        nBytes -= nBytesRecorded;

        if (nBytesRecorded % m_sampleSizeInp != 0) {
            ggprintf("Failure during capture - provided bytes (%d) are not multiple of sample size (%d)\n",
                    nBytesRecorded, m_sampleSizeInp);
            m_rx.samplesNeeded = m_samplesPerFrame;
            break;
        }

        // convert to 32-bit float
        int nSamplesRecorded = nBytesRecorded/m_sampleSizeInp;
        switch (m_sampleFormatInp) {
            case GGWAVE_SAMPLE_FORMAT_UNDEFINED: break;
            case GGWAVE_SAMPLE_FORMAT_U8:
                {
                    constexpr float scale = 1.0f/128;
                    auto p = reinterpret_cast<uint8_t *>(m_rx.amplitudeTmp.data());
                    for (int i = 0; i < nSamplesRecorded; ++i) {
                        m_rx.amplitudeResampled[i] = float(int16_t(*(p + i)) - 128)*scale;
                    }
                } break;
            case GGWAVE_SAMPLE_FORMAT_I8:
                {
                    constexpr float scale = 1.0f/128;
                    auto p = reinterpret_cast<int8_t *>(m_rx.amplitudeTmp.data());
                    for (int i = 0; i < nSamplesRecorded; ++i) {
                        m_rx.amplitudeResampled[i] = float(*(p + i))*scale;
                    }
                } break;
            case GGWAVE_SAMPLE_FORMAT_U16:
                {
                    constexpr float scale = 1.0f/32768;
                    auto p = reinterpret_cast<uint16_t *>(m_rx.amplitudeTmp.data());
                    for (int i = 0; i < nSamplesRecorded; ++i) {
                        m_rx.amplitudeResampled[i] = float(int32_t(*(p + i)) - 32768)*scale;
                    }
                } break;
            case GGWAVE_SAMPLE_FORMAT_I16:
                {
                    constexpr float scale = 1.0f/32768;
                    auto p = reinterpret_cast<int16_t *>(m_rx.amplitudeTmp.data());
                    for (int i = 0; i < nSamplesRecorded; ++i) {
                        m_rx.amplitudeResampled[i] = float(*(p + i))*scale;
                    }
                } break;
            case GGWAVE_SAMPLE_FORMAT_F32: break;
        }

        uint32_t offset = m_samplesPerFrame - m_rx.samplesNeeded;

        if (m_needResampling) {
            if (nSamplesRecorded <= 2*Resampler::kWidth) {
                m_rx.samplesNeeded = m_samplesPerFrame;
                break;
            }

            // reset resampler state every minute
            if (!m_rx.receiving && m_resampler.nSamplesTotal() > 60.0f*factor*m_sampleRate) {
                m_resampler.reset();
            }

            int nSamplesResampled = offset + m_resampler.resample(factor, nSamplesRecorded, m_rx.amplitudeResampled.data(), m_rx.amplitude.data() + offset);
            nSamplesRecorded = nSamplesResampled;
        } else {
            for (int i = 0; i < nSamplesRecorded; ++i) {
                m_rx.amplitude[offset + i] = m_rx.amplitudeResampled[i];
            }
        }

        // we have enough bytes to do analysis
        if (nSamplesRecorded >= m_samplesPerFrame) {
            m_rx.hasNewAmplitude = true;

            if (m_isFixedPayloadLength) {
                decode_fixed();
            } else {
                decode_variable();
            }

            int nExtraSamples = nSamplesRecorded - m_samplesPerFrame;
            for (int i = 0; i < nExtraSamples; ++i) {
                m_rx.amplitude[i] = m_rx.amplitude[m_samplesPerFrame + i];
            }

            m_rx.samplesNeeded = m_samplesPerFrame - nExtraSamples;
        } else {
            m_rx.samplesNeeded = m_samplesPerFrame - nSamplesRecorded;
            break;
        }
    }

    return true;
}

//
// instance state
//

bool GGWave::isDSSEnabled() const { return m_isDSSEnabled; }

int GGWave::samplesPerFrame() const { return m_samplesPerFrame; }
int GGWave::sampleSizeInp()   const { return m_sampleSizeInp; }
int GGWave::sampleSizeOut()   const { return m_sampleSizeOut; }

float GGWave::hzPerSample()   const { return m_hzPerSample; }
float GGWave::sampleRateInp() const { return m_sampleRateInp; }
float GGWave::sampleRateOut() const { return m_sampleRateOut; }
GGWave::SampleFormat GGWave::sampleFormatInp() const { return m_sampleFormatInp; }
GGWave::SampleFormat GGWave::sampleFormatOut() const { return m_sampleFormatOut; }

int GGWave::heapSize() const { return m_heapSize; }

//
// Tx
//

const void * GGWave::txWaveform() const {
    switch (m_sampleFormatOut) {
        case GGWAVE_SAMPLE_FORMAT_UNDEFINED: break;
        case GGWAVE_SAMPLE_FORMAT_I16:
            {
                return m_tx.outputI16.data();
            } break;
        case GGWAVE_SAMPLE_FORMAT_U8:
        case GGWAVE_SAMPLE_FORMAT_I8:
        case GGWAVE_SAMPLE_FORMAT_U16:
        case GGWAVE_SAMPLE_FORMAT_F32:
            {
                return m_tx.outputTmp.data();
            } break;
    }

    return nullptr;
}

const GGWave::Tones GGWave::txTones() const { return { m_tx.tones.data(), m_tx.nTones }; }

bool GGWave::txHasData() const { return m_tx.hasData; }

bool GGWave::txTakeAmplitudeI16(AmplitudeI16 & dst) {
    if (m_tx.lastAmplitudeSize == 0) return false;

    dst.assign({ m_tx.outputI16.data(), m_tx.lastAmplitudeSize });
    m_tx.lastAmplitudeSize = 0;

    return true;
}

const GGWave::RxProtocols & GGWave::txProtocols() const { return m_tx.protocols; }

//
// Rx
//

bool GGWave::rxReceiving() const { return m_rx.receiving; }
bool GGWave::rxAnalyzing() const { return m_rx.analyzing; }

int GGWave::rxSamplesNeeded()       const { return m_rx.samplesNeeded; }
int GGWave::rxFramesToRecord()      const { return m_rx.framesToRecord; }
int GGWave::rxFramesLeftToRecord()  const { return m_rx.framesLeftToRecord; }
int GGWave::rxFramesToAnalyze()     const { return m_rx.framesToAnalyze; }
int GGWave::rxFramesLeftToAnalyze() const { return m_rx.framesLeftToAnalyze; }

bool GGWave::rxStopReceiving() {
    if (m_rx.receiving == false) {
        return false;
    }

    m_rx.receiving = false;

    return true;
}

GGWave::RxProtocols & GGWave::rxProtocols() { return m_rx.protocols; }

int GGWave::rxDataLength() const { return m_rx.dataLength; }

const GGWave::TxRxData &      GGWave::rxData()       const { return m_rx.data; }
const GGWave::RxProtocol &    GGWave::rxProtocol()   const { return m_rx.protocol; }
const GGWave::RxProtocolId &  GGWave::rxProtocolId() const { return m_rx.protocolId; }
const GGWave::Spectrum &      GGWave::rxSpectrum()   const { return m_rx.spectrum; }
const GGWave::Amplitude &     GGWave::rxAmplitude()  const { return m_rx.amplitude; }

int GGWave::rxTakeData(TxRxData & dst) {
    if (m_rx.dataLength == 0) return 0;

    auto res = m_rx.dataLength;
    m_rx.dataLength = 0;

    if (res != -1) {
        dst.assign({ m_rx.data.data(), res });
    }

    return res;
}

bool GGWave::rxTakeSpectrum(Spectrum & dst) {
    if (m_rx.hasNewSpectrum == false) return false;

    m_rx.hasNewSpectrum = false;
    dst.assign(m_rx.spectrum);

    return true;
}

bool GGWave::rxTakeAmplitude(Amplitude & dst) {
    if (m_rx.hasNewAmplitude == false) return false;

    m_rx.hasNewAmplitude = false;
    dst.assign(m_rx.amplitude);

    return true;
}

bool GGWave::computeFFTR(const float * src, float * dst, int N) {
    if (N != m_samplesPerFrame) {
        ggprintf("computeFFTR: N (%d) must be equal to 'samplesPerFrame' %d\n", N, m_samplesPerFrame);
        return false;
    }

    FFT(src, dst, N, m_rx.fftWorkI.data(), m_rx.fftWorkF.data());

    return true;
}

int GGWave::computeFFTR(const float * src, float * dst, int N, int * wi, float * wf) {
    if (wi == nullptr) return 2*N;
    if (wf == nullptr) return 3 + sqrt(N/2);

    FFT(src, dst, N, wi, wf);

    return 1;
}

int GGWave::filter(ggwave_Filter filter, float * waveform, int N, float p0, float p1, float * w) {
    if (w == nullptr) {
        switch (filter) {
            case GGWAVE_FILTER_HANN:                  return N;
            case GGWAVE_FILTER_HAMMING:               return N;
            case GGWAVE_FILTER_FIRST_ORDER_HIGH_PASS: return 11;
        };
    }

    if (w[0] == 0.0f && w[1] == 0.0f) {
        switch (filter) {
            case GGWAVE_FILTER_HANN:
                {
                    const float f = 2.0f*M_PI/(float)N;
                    for (int i = 0; i < N; i++) {
                        w[i] = 0.5f - 0.5f*cosf(f*(float)i);
                    }
                } break;
            case GGWAVE_FILTER_HAMMING:
                {
                    const float f = 2.0f*M_PI/(float)N;
                    for (int i = 0; i < N; i++) {
                        w[i] = 0.54f - 0.46f*cosf(f*(float)i);
                    }
                } break;
            case GGWAVE_FILTER_FIRST_ORDER_HIGH_PASS:
                {
                    const float th = 2.0f*M_PI*p0/p1;
                    const float g = cos(th)/(1.0f + sin(th));
                    w[0] = (1.0f + g)/2.0f;
                    w[1] = -((1.0f + g)/2.0f);
                    w[2] = 0.0f;
                    w[3] = -g;
                    w[4] = 0.0f;

                    w[5] = 0.0f;
                    w[6] = 0.0f;
                    w[7] = 0.0f;
                    w[8] = 0.0f;
                } break;
        };
    }

    switch (filter) {
        case GGWAVE_FILTER_HANN:
        case GGWAVE_FILTER_HAMMING:
            {
                for (int i = 0; i < N; i++) {
                    waveform[i] *= w[i];
                }
            } break;
        case GGWAVE_FILTER_FIRST_ORDER_HIGH_PASS:
            {
                for (int i = 0; i < N; i++) {
                    float xn = waveform[i];
                    float yn = w[0]*xn + w[1]*w[5] + w[2]*w[6] + w[3]*w[7] + w[4]*w[8];
                    w[6] = w[5];
                    w[5] = xn;
                    w[8] = w[7];
                    w[7] = yn;

                    waveform[i] = yn;
                }
            } break;
    };

    return 1;
}

//
// GGWave::Resampler
//

GGWave::Resampler::Resampler() {}

bool GGWave::Resampler::alloc(void * p, int & n) {
    ggalloc(m_sincTable,   kWidth*kSamplesPerZeroCrossing, p, n);
    ggalloc(m_delayBuffer, 3*kWidth, p, n);
    ggalloc(m_edgeSamples, kWidth, p, n);
    ggalloc(m_samplesInp,  4096, p, n);

    if (p) {
        makeSinc();
        reset();
    }

    return true;
}

void GGWave::Resampler::reset() {
    m_state = {};
    m_edgeSamples.zero();
    m_delayBuffer.zero();
    m_samplesInp.zero();
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
        assert((int) m_samplesInp.size() >= nSamples + kWidth);
        //if ((int) m_samplesInp.size() < nSamples + kWidth) {
        //    m_samplesInp.resize(nSamples + kWidth);
        //}
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
    m_rx.amplitudeHistory[m_rx.historyId].copy(m_rx.amplitude);

    if (++m_rx.historyId >= kMaxSpectrumHistory) {
        m_rx.historyId = 0;
    }

    if (m_rx.historyId == 0 || m_rx.receiving) {
        m_rx.hasNewSpectrum = true;

        m_rx.amplitudeAverage.zero();
        for (int j = 0; j < (int) m_rx.amplitudeHistory.size(); ++j) {
            auto s = m_rx.amplitudeHistory[j];
            for (int i = 0; i < m_samplesPerFrame; ++i) {
                m_rx.amplitudeAverage[i] += s[i];
            }
        }

        float norm = 1.0f/kMaxSpectrumHistory;
        for (int i = 0; i < m_samplesPerFrame; ++i) {
            m_rx.amplitudeAverage[i] *= norm;
        }

        // calculate spectrum
        FFT(m_rx.amplitudeAverage.data(), m_rx.fftOut.data(), m_samplesPerFrame, m_rx.fftWorkI.data(), m_rx.fftWorkF.data());

        for (int i = 0; i < m_samplesPerFrame; ++i) {
            m_rx.spectrum[i] = (m_rx.fftOut[2*i + 0]*m_rx.fftOut[2*i + 0] + m_rx.fftOut[2*i + 1]*m_rx.fftOut[2*i + 1]);
        }
        for (int i = 1; i < m_samplesPerFrame/2; ++i) {
            m_rx.spectrum[i] += m_rx.spectrum[m_samplesPerFrame - i];
        }
    }

    if (m_rx.framesLeftToRecord > 0) {
        memcpy(m_rx.amplitudeRecorded.data() + (m_rx.framesToRecord - m_rx.framesLeftToRecord)*m_samplesPerFrame,
               m_rx.amplitude.data(),
               m_samplesPerFrame*sizeof(float));

        if (--m_rx.framesLeftToRecord <= 0) {
            m_rx.analyzing = true;
        }
    }

    if (m_rx.analyzing) {
        ggprintf("Analyzing captured data ..\n");

        const int stepsPerFrame = 16;
        const int step = m_samplesPerFrame/stepsPerFrame;

        bool isValid = false;
        for (int protocolId = 0; protocolId < (int) m_rx.protocols.size(); ++protocolId) {
            const auto & protocol = m_rx.protocols[protocolId];
            if (protocol.enabled == false) {
                continue;
            }

            // skip Rx protocol if it is mono-tone
            if (protocol.extra == 2) {
                continue;
            }

            // skip Rx protocol if start frequency is different from detected one
            if (protocol.freqStart != m_rx.markerFreqStart) {
                continue;
            }

            m_rx.spectrum.zero();

            m_rx.framesToAnalyze = m_nMarkerFrames*stepsPerFrame;
            m_rx.framesLeftToAnalyze = m_rx.framesToAnalyze;

            // note : not sure if looping backwards here is more meaningful than looping forwards
            for (int ii = m_nMarkerFrames*stepsPerFrame - 1; ii >= 0; --ii) {
                bool knownLength = false;

                int decodedLength = 0;
                const int offsetStart = ii;
                for (int itx = 0; itx < 1024; ++itx) {
                    int offsetTx = offsetStart + itx*protocol.framesPerTx*stepsPerFrame;
                    if (offsetTx >= m_rx.recvDuration_frames*stepsPerFrame || (itx + 1)*protocol.bytesPerTx >= (int) m_dataEncoded.size()) {
                        break;
                    }

                    memcpy(m_rx.fftOut.data(),
                           m_rx.amplitudeRecorded.data() + offsetTx*step,
                           m_samplesPerFrame*sizeof(float));

                    // note : should we skip the first and last frame here as they are amplitude-smoothed?
                    for (int k = 1; k < protocol.framesPerTx; ++k) {
                        for (int i = 0; i < m_samplesPerFrame; ++i) {
                            m_rx.fftOut[i] += m_rx.amplitudeRecorded[(offsetTx + k*stepsPerFrame)*step + i];
                        }
                    }

                    FFT(m_rx.fftOut.data(), m_samplesPerFrame, m_rx.fftWorkI.data(), m_rx.fftWorkF.data());

                    for (int i = 0; i < m_samplesPerFrame; ++i) {
                        m_rx.spectrum[i] = (m_rx.fftOut[2*i + 0]*m_rx.fftOut[2*i + 0] + m_rx.fftOut[2*i + 1]*m_rx.fftOut[2*i + 1]);
                    }
                    for (int i = 1; i < m_samplesPerFrame/2; ++i) {
                        m_rx.spectrum[i] += m_rx.spectrum[m_samplesPerFrame - i];
                    }

                    uint8_t curByte = 0;
                    for (int i = 0; i < 2*protocol.bytesPerTx; ++i) {
                        double freq = m_hzPerSample*protocol.freqStart;
                        int bin = round(freq*m_ihzPerSample) + 16*i;

                        int kmax = 0;
                        double amax = 0.0;
                        for (int k = 0; k < 16; ++k) {
                            if (m_rx.spectrum[bin + k] > amax) {
                                kmax = k;
                                amax = m_rx.spectrum[bin + k];
                            }
                        }

                        if (i%2) {
                            curByte += (kmax << 4);
                            m_dataEncoded[itx*protocol.bytesPerTx + i/2] = curByte;
                            curByte = 0;
                        } else {
                            curByte = kmax;
                        }
                    }

                    if (itx*protocol.bytesPerTx > m_encodedDataOffset && knownLength == false) {
                        RS::ReedSolomon rsLength(1, m_encodedDataOffset - 1, m_workRSLength.data());
                        if ((rsLength.Decode(m_dataEncoded.data(), m_rx.data.data()) == 0) && (m_rx.data[0] > 0 && m_rx.data[0] <= 140)) {
                            knownLength = true;
                            decodedLength = m_rx.data[0];
                            //printf("decoded length = %d, recvDuration_frames = %d\n", decodedLength, m_rx.recvDuration_frames);

                            const int nTotalBytesExpected = m_encodedDataOffset + decodedLength + ::getECCBytesForLength(decodedLength);
                            const int nTotalFramesExpected = 2*m_nMarkerFrames + ((nTotalBytesExpected + protocol.bytesPerTx - 1)/protocol.bytesPerTx)*protocol.framesPerTx;
                            if (m_rx.recvDuration_frames > nTotalFramesExpected ||
                                m_rx.recvDuration_frames < nTotalFramesExpected - 2*m_nMarkerFrames) {
                                //printf("  - invalid number of frames: %d (expected %d)\n", m_rx.recvDuration_frames, nTotalFramesExpected);
                                knownLength = false;
                                break;
                            }
                        } else {
                            break;
                        }
                    }

                    {
                        const int nTotalBytesExpected = m_encodedDataOffset + decodedLength + ::getECCBytesForLength(decodedLength);
                        if (knownLength && itx*protocol.bytesPerTx > nTotalBytesExpected + 1) {
                            break;
                        }
                    }
                }

                if (knownLength) {
                    RS::ReedSolomon rsData(decodedLength, ::getECCBytesForLength(decodedLength), m_workRSData.data());

                    if (rsData.Decode(m_dataEncoded.data() + m_encodedDataOffset, m_rx.data.data()) == 0) {
                        if (decodedLength > 0) {
                            if (m_isDSSEnabled) {
                                for (int i = 0; i < decodedLength; ++i) {
                                    m_rx.data[i] = m_rx.data[i] ^ getDSSMagic(i);
                                }
                            }

                            ggprintf("Decoded length = %d, protocol = '%s' (%d)\n", decodedLength, protocol.name, protocolId);
                            ggprintf("Received sound data successfully: '%s'\n", m_rx.data.data());

                            isValid = true;
                            m_rx.hasNewRxData = true;
                            m_rx.dataLength = decodedLength;
                            m_rx.protocol = protocol;
                            m_rx.protocolId = RxProtocolId(protocolId);
                        }
                    }
                }

                if (isValid) {
                    break;
                }
                --m_rx.framesLeftToAnalyze;
            }

            if (isValid) break;
        }

        m_rx.framesToRecord = 0;

        if (isValid == false) {
            ggprintf("Failed to capture sound data. Please try again (length = %d)\n", m_rx.data[0]);
            m_rx.dataLength = -1;
            m_rx.framesToRecord = -1;
        }

        m_rx.receiving = false;
        m_rx.analyzing = false;

        m_rx.spectrum.zero();

        m_rx.framesToAnalyze = 0;
        m_rx.framesLeftToAnalyze = 0;
    }

    // check if receiving data
    if (m_rx.receiving == false) {
        bool isReceiving = false;

        for (int i = 0; i < m_rx.protocols.size(); ++i) {
            const auto & protocol = m_rx.protocols[i];
            if (protocol.enabled == false) {
                continue;
            }

            int nDetectedMarkerBits = m_nBitsInMarker;

            for (int i = 0; i < m_nBitsInMarker; ++i) {
                double freq = bitFreq(protocol, i);
                int bin = round(freq*m_ihzPerSample);

                if (i%2 == 0) {
                    if (m_rx.spectrum[bin] <= m_soundMarkerThreshold*m_rx.spectrum[bin + m_freqDelta_bin]) --nDetectedMarkerBits;
                } else {
                    if (m_rx.spectrum[bin] >= m_soundMarkerThreshold*m_rx.spectrum[bin + m_freqDelta_bin]) --nDetectedMarkerBits;
                }
            }

            if (nDetectedMarkerBits == m_nBitsInMarker) {
                m_rx.markerFreqStart = protocol.freqStart;
                isReceiving = true;
                break;
            }
        }

        if (isReceiving) {
            if (++m_rx.nMarkersSuccess >= 1) {
            } else {
                isReceiving = false;
            }
        } else {
            m_rx.nMarkersSuccess = 0;
        }

        if (isReceiving) {
            ggprintf("Receiving sound data ...\n");

            m_rx.receiving = true;
            m_rx.data.zero();

            // max recieve duration
            m_rx.recvDuration_frames =
                2*m_nMarkerFrames +
                maxFramesPerTx(m_rx.protocols, true)*(
                        (kMaxLengthVariable + ::getECCBytesForLength(kMaxLengthVariable))/minBytesPerTx(m_rx.protocols) + 1
                        );

            m_rx.nMarkersSuccess = 0;
            m_rx.framesToRecord = m_rx.recvDuration_frames;
            m_rx.framesLeftToRecord = m_rx.recvDuration_frames;
        }
    } else {
        bool isEnded = false;

        for (int i = 0; i < m_rx.protocols.size(); ++i) {
            const auto & protocol = m_rx.protocols[i];
            if (protocol.enabled == false) {
                continue;
            }

            int nDetectedMarkerBits = m_nBitsInMarker;

            for (int i = 0; i < m_nBitsInMarker; ++i) {
                double freq = bitFreq(protocol, i);
                int bin = round(freq*m_ihzPerSample);

                if (i%2 == 0) {
                    if (m_rx.spectrum[bin] >= m_soundMarkerThreshold*m_rx.spectrum[bin + m_freqDelta_bin]) nDetectedMarkerBits--;
                } else {
                    if (m_rx.spectrum[bin] <= m_soundMarkerThreshold*m_rx.spectrum[bin + m_freqDelta_bin]) nDetectedMarkerBits--;
                }
            }

            if (nDetectedMarkerBits == m_nBitsInMarker) {
                isEnded = true;
                break;
            }
        }

        if (isEnded) {
            if (++m_rx.nMarkersSuccess >= 1) {
            } else {
                isEnded = false;
            }
        } else {
            m_rx.nMarkersSuccess = 0;
        }

        if (isEnded && m_rx.framesToRecord > 1) {
            m_rx.recvDuration_frames -= m_rx.framesLeftToRecord - 1;
            ggprintf("Received end marker. Frames left = %d, recorded = %d\n", m_rx.framesLeftToRecord, m_rx.recvDuration_frames);
            m_rx.nMarkersSuccess = 0;
            m_rx.framesLeftToRecord = 1;
        }
    }
}

//
// Fixed payload length

void GGWave::decode_fixed() {
    m_rx.hasNewSpectrum = true;

    // calculate spectrum
    FFT(m_rx.amplitude.data(), m_rx.fftOut.data(), m_samplesPerFrame, m_rx.fftWorkI.data(), m_rx.fftWorkF.data());

    float amax = 0.0f;
    for (int i = 0; i < m_samplesPerFrame; ++i) {
        m_rx.spectrum[i] = (m_rx.fftOut[2*i + 0]*m_rx.fftOut[2*i + 0] + m_rx.fftOut[2*i + 1]*m_rx.fftOut[2*i + 1]);
    }
    for (int i = 1; i < m_samplesPerFrame/2; ++i) {
        m_rx.spectrum[i] += m_rx.spectrum[m_samplesPerFrame - i];
        if (i >= m_rx.minFreqStart) {
            amax = GG_MAX(amax, m_rx.spectrum[i]);
        }
    }

    // original, floating-point version
    //m_rx.spectrumHistoryFixed[m_rx.historyIdFixed].copy(m_rx.spectrum);

    // float -> uint8_t
    amax = 255.0f/(amax == 0.0f ? 1.0f : amax);
    for (int i = 0; i < m_samplesPerFrame; ++i) {
        m_rx.spectrumHistoryFixed[m_rx.historyIdFixed][i] = GG_MIN(255.0f, GG_MAX(0.0f, (float) round(m_rx.spectrum[i]*amax)));
    }

    // float -> uint16_t
    //amax = 65535.0f/(amax == 0.0f ? 1.0f : amax);
    //for (int i = 0; i < m_samplesPerFrame; ++i) {
    //    m_rx.spectrumHistoryFixed[m_rx.historyIdFixed][i] = GG_MIN(65535.0f, GG_MAX(0.0f, (float) round(m_rx.spectrum[i]*amax)));
    //}

    if (++m_rx.historyIdFixed >= (int) m_rx.spectrumHistoryFixed.size()) {
        m_rx.historyIdFixed = 0;
    }

    bool isValid = false;
    for (int protocolId = 0; protocolId < (int) m_rx.protocols.size(); ++protocolId) {
        const auto & protocol = m_rx.protocols[protocolId];
        if (protocol.enabled == false) {
            continue;
        }

        const int binStart = protocol.freqStart;
        const int binDelta = 16;
        const int binOffset = protocol.extra == 1 ? binDelta : 0;

        if (binStart > m_samplesPerFrame) {
            continue;
        }

        const int totalLength = m_payloadLength + getECCBytesForLength(m_payloadLength);
        const int totalTxs = protocol.extra*((totalLength + protocol.bytesPerTx - 1)/protocol.bytesPerTx);

        int historyStartId = m_rx.historyIdFixed - totalTxs*protocol.framesPerTx;
        if (historyStartId < 0) {
            historyStartId += m_rx.spectrumHistoryFixed.size();
        }

        const int nTones = 2*protocol.bytesPerTx;
        m_rx.detectedBins.zero();

        int txNeededTotal   = 0;
        int txDetectedTotal = 0;
        bool detectedSignal = true;

        for (int k = 0; k < totalTxs; ++k) {
            if (k % protocol.extra == 0) {
                m_rx.detectedTones.zero(16*nTones);
            }

            for (int i = 0; i < protocol.framesPerTx; ++i) {
                int historyId = historyStartId + k*protocol.framesPerTx + i;
                if (historyId >= (int) m_rx.spectrumHistoryFixed.size()) {
                    historyId -= m_rx.spectrumHistoryFixed.size();
                }

                for (int j = 0; j < protocol.bytesPerTx; ++j) {
                    int f0bin = 0;
                    auto f0max = m_rx.spectrumHistoryFixed[historyId][binStart + 2*j*binDelta];

                    for (int b = 1; b < 16; ++b) {
                        {
                            const auto & v = m_rx.spectrumHistoryFixed[historyId][binStart + 2*j*binDelta + b];

                            if (f0max <= v) {
                                f0max = v;
                                f0bin = b;
                            }
                        }
                    }

                    int f1bin = 0;
                    if (protocol.extra == 1) {
                        auto f1max = m_rx.spectrumHistoryFixed[historyId][binStart + 2*j*binDelta + binOffset];
                        for (int b = 1; b < 16; ++b) {
                            const auto & v = m_rx.spectrumHistoryFixed[historyId][binStart + 2*j*binDelta + binOffset + b];

                            if (f1max <= v) {
                                f1max = v;
                                f1bin = b;
                            }
                        }
                    } else {
                        f1bin = f0bin;
                    }

                    if ((k + 0)%protocol.extra == 0) m_rx.detectedTones[(2*j + 0)*16 + f0bin]++;
                    if ((k + 1)%protocol.extra == 0) m_rx.detectedTones[(2*j + 1)*16 + f1bin]++;
                }
            }

            if (protocol.extra > 1 && (k % protocol.extra == 0)) continue;

            int txNeeded = 0;
            int txDetected = 0;
            for (int j = 0; j < protocol.bytesPerTx; ++j) {
                if ((k/protocol.extra)*protocol.bytesPerTx + j >= totalLength) break;
                txNeeded += 2;
                for (int b = 0; b < 16; ++b) {
                    if (m_rx.detectedTones[(2*j + 0)*16 + b] > protocol.framesPerTx/2) {
                        m_rx.detectedBins[2*((k/protocol.extra)*protocol.bytesPerTx + j) + 0] = b;
                        txDetected++;
                    }
                    if (m_rx.detectedTones[(2*j + 1)*16 + b] > protocol.framesPerTx/2) {
                        m_rx.detectedBins[2*((k/protocol.extra)*protocol.bytesPerTx + j) + 1] = b;
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
                m_dataEncoded[j] = (m_rx.detectedBins[2*j + 1] << 4) + m_rx.detectedBins[2*j + 0];
            }

            if (rsData.Decode(m_dataEncoded.data(), m_rx.data.data()) == 0) {
                if (m_isDSSEnabled) {
                    for (int i = 0; i < m_payloadLength; ++i) {
                        m_rx.data[i] = m_rx.data[i] ^ getDSSMagic(i);
                    }
                }

                ggprintf("Decoded length = %d, protocol = '%s' (%d)\n", m_payloadLength, protocol.name, protocolId);
                ggprintf("Received sound data successfully: '%s'\n", m_rx.data.data());

                isValid = true;
                m_rx.hasNewRxData = true;
                m_rx.dataLength = m_payloadLength;
                m_rx.protocol = protocol;
                m_rx.protocolId = RxProtocolId(protocolId);
            }
        }

        if (isValid) {
            break;
        }
    }
}

int GGWave::maxFramesPerTx(const Protocols & protocols, bool excludeMT) const {
    int res = 0;
    for (int i = 0; i < protocols.size(); ++i) {
        const auto & protocol = protocols[i];
        if (protocol.enabled == false) {
            continue;
        }
        if (excludeMT && protocol.extra > 1) {
            continue;
        }
        res = GG_MAX(res, protocol.framesPerTx*protocol.extra);
    }
    return res;
}

int GGWave::minBytesPerTx(const Protocols & protocols) const {
    int res = 1;
    for (int i = 0; i < protocols.size(); ++i) {
        const auto & protocol = protocols[i];
        if (protocol.enabled == false) {
            continue;
        }
        res = GG_MIN(res, (int) protocol.bytesPerTx);
    }
    return res;
}

int GGWave::maxBytesPerTx(const Protocols & protocols) const {
    int res = 1;
    for (int i = 0; i < protocols.size(); ++i) {
        const auto & protocol = protocols[i];
        if (protocol.enabled == false) {
            continue;
        }
        res = GG_MAX(res, (int) protocol.bytesPerTx);
    }
    return res;
}

int GGWave::maxTonesPerTx(const Protocols & protocols) const {
    int res = 1;
    for (int i = 0; i < protocols.size(); ++i) {
        const auto & protocol = protocols[i];
        if (protocol.enabled == false) {
            continue;
        }
        res = GG_MAX(res, protocol.nTones());
    }
    return res;
}

int GGWave::minFreqStart(const Protocols & protocols) const {
    int res = m_samplesPerFrame;
    for (int i = 0; i < protocols.size(); ++i) {
        const auto & protocol = protocols[i];
        if (protocol.enabled == false) {
            continue;
        }
        res = GG_MIN(res, protocol.freqStart);
    }
    return res;
}

double GGWave::bitFreq(const Protocol & p, int bit) const {
    return m_hzPerSample*p.freqStart + m_freqDelta_hz*bit;
}
