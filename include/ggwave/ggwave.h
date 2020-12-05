#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <vector>
#include <memory>

namespace RS {
class ReedSolomon;
}

class GGWave {
public:
    static constexpr auto kMaxSamplesPerFrame = 1024;
    static constexpr auto kMaxDataBits = 256;
    static constexpr auto kMaxDataSize = 256;
    static constexpr auto kMaxLength = 140;
    static constexpr auto kMaxSpectrumHistory = 4;
    static constexpr auto kMaxRecordedFrames = 1024;

    struct TxProtocol {
        const char * name;

        int freqStart;
        int framesPerTx;
        int bytesPerTx;

        int nDataBitsPerTx() const { return 8*bytesPerTx; }
    };

    using TxProtocols     = std::vector<TxProtocol>;

    const TxProtocols kTxProtocols {
        { "Normal",      40,  9, 3, },
        { "Fast",        40,  6, 3, },
        { "Fastest",     40,  3, 3, },
        { "[U] Normal",  320, 9, 3, },
        { "[U] Fast",    320, 6, 3, },
        { "[U] Fastest", 320, 3, 3, },
    };

    using AmplitudeData   = std::array<float, kMaxSamplesPerFrame>;
    using AmplitudeData16 = std::array<int16_t, kMaxRecordedFrames*kMaxSamplesPerFrame>;
    using SpectrumData    = std::array<float, kMaxSamplesPerFrame>;
    using RecordedData    = std::array<float, kMaxRecordedFrames*kMaxSamplesPerFrame>;
    using TxRxData        = std::array<std::uint8_t, kMaxDataSize>;

    using CBQueueAudio = std::function<void(const void * data, uint32_t nBytes)>;
    using CBDequeueAudio = std::function<uint32_t(void * data, uint32_t nMaxBytes)>;

    GGWave(
            int sampleRateIn,
            int sampleRateOut,
            int samplesPerFrame,
            int sampleSizeBytesIn,
            int sampleSizeBytesOut);
    ~GGWave();

    bool init(int textLength, const char * stext, const TxProtocol & aProtocol, const int volume);
    void send(const CBQueueAudio & cbQueueAudio);
    void receive(const CBDequeueAudio & CBDequeueAudio);

    const bool & hasTxData() const { return m_hasNewTxData; }

    const int & getFramesToRecord()         const { return m_framesToRecord; }
    const int & getFramesLeftToRecord()     const { return m_framesLeftToRecord; }
    const int & getFramesToAnalyze()        const { return m_framesToAnalyze; }
    const int & getFramesLeftToAnalyze()    const { return m_framesLeftToAnalyze; }
    const int & getSamplesPerFrame()        const { return m_samplesPerFrame; }
    const int & getSampleSizeBytesIn()      const { return m_sampleSizeBytesIn; }
    const int & getSampleSizeBytesOut()     const { return m_sampleSizeBytesOut; }

    const float & getSampleRateIn() const { return m_sampleRateIn; }

    const TxProtocol & getDefultTxProtocol() const { return kTxProtocols[1]; }
    const TxProtocols & getTxProtocols() const { return kTxProtocols; }

    const TxRxData & getRxData() const { return m_rxData; }
    int takeRxData(TxRxData & dst);

private:
    int maxFramesPerTx() const;
    int minBytesPerTx() const;

    double bitFreq(const TxProtocol & p, int bit) const {
        return m_hzPerSample*p.freqStart + m_freqDelta_hz*bit;
    }

    const float m_sampleRateIn;
    const float m_sampleRateOut;
    const int m_samplesPerFrame;
    const float m_isamplesPerFrame;
    const int m_sampleSizeBytesIn;
    const int m_sampleSizeBytesOut;

    const float m_hzPerSample;
    const float m_ihzPerSample;

    const int m_freqDelta_bin;
    const float m_freqDelta_hz;

    const int m_nBitsInMarker;
    const int m_nMarkerFrames;
    const int m_nPostMarkerFrames;
    const int m_encodedDataOffset;

    // Rx
    bool m_hasNewRxData;
    bool m_receivingData;
    bool m_analyzingData;

    int m_markerFreqStart;

    int m_recvDuration_frames;
    int m_lastRxDataLength;

    int m_framesLeftToAnalyze;
    int m_framesLeftToRecord;
    int m_framesToAnalyze;
    int m_framesToRecord;

    std::array<float, kMaxSamplesPerFrame> m_fftIn;    // real
    std::array<float, 2*kMaxSamplesPerFrame> m_fftOut; // complex

    AmplitudeData m_sampleAmplitude;
    SpectrumData m_sampleSpectrum;

    TxRxData m_rxData;

    int m_historyId = 0;
    AmplitudeData m_sampleAmplitudeAverage;
    std::array<AmplitudeData, kMaxSpectrumHistory> m_sampleAmplitudeHistory;

    RecordedData m_recordedAmplitude;

    // Tx
    bool m_hasNewTxData;
    int m_nECCBytesPerTx;
    int m_sendDataLength;
    float m_sendVolume;

    int m_txDataLength;
    TxRxData m_txData;
    TxRxData m_txDataEncoded;

    TxProtocol m_txProtocol;

    std::unique_ptr<RS::ReedSolomon> m_rsLength;
};
