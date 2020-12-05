#pragma once

#include <array>
#include <complex>
#include <cstdint>
#include <functional>
#include <vector>

namespace RS {
class ReedSolomon;
}

class GGWave {
public:
    enum TxMode {
        FixedLength = 0,
        VariableLength,
    };

    static constexpr auto kMaxSamplesPerFrame = 1024;
    static constexpr auto kMaxDataBits = 256;
    static constexpr auto kMaxDataSize = 256;
    static constexpr auto kMaxLength = 140;
    static constexpr auto kMaxSpectrumHistory = 4;
    static constexpr auto kMaxRecordedFrames = 1024;
    static constexpr auto kDefaultFixedLength = 82;

    struct TxProtocol {
        const char * name;

        int paramFreqStart;
        int paramFramesPerTx;
        int paramBytesPerTx;
        int paramECCBytesPerTx;
        int paramVolume;
    };

    using AmplitudeData   = std::array<float, kMaxSamplesPerFrame>;
    using AmplitudeData16 = std::array<int16_t, kMaxRecordedFrames*kMaxSamplesPerFrame>;
    using SpectrumData    = std::array<float, kMaxSamplesPerFrame>;
    using RecordedData    = std::array<float, kMaxRecordedFrames*kMaxSamplesPerFrame>;
    using TxRxData        = std::array<std::uint8_t, kMaxDataSize>;
    using TxProtocols     = std::vector<TxProtocol>;

    using CBQueueAudio = std::function<void(const void * data, uint32_t nBytes)>;
    using CBDequeueAudio = std::function<uint32_t(void * data, uint32_t nMaxBytes)>;

    GGWave(
            int aSampleRateIn,
            int aSampleRateOut,
            int aSamplesPerFrame,
            int aSampleSizeBytesIn,
            int aSampleSizeBytesOut);
    ~GGWave();

    void setTxMode(TxMode aTxMode) {
        txMode = aTxMode;
        init(0, "", getDefultTxProtocol());
    }

    bool init(int textLength, const char * stext, const TxProtocol & aProtocol);
    void send(const CBQueueAudio & cbQueueAudio);
    void receive(const CBDequeueAudio & CBDequeueAudio);

    const bool & getHasData() const { return hasData; }

    const int & getFramesToRecord()         const { return framesToRecord; }
    const int & getFramesLeftToRecord()     const { return framesLeftToRecord; }
    const int & getFramesToAnalyze()        const { return framesToAnalyze; }
    const int & getFramesLeftToAnalyze()    const { return framesLeftToAnalyze; }
    const int & getSamplesPerFrame()        const { return samplesPerFrame; }
    const int & getSampleSizeBytesIn()      const { return sampleSizeBytesIn; }
    const int & getSampleSizeBytesOut()     const { return sampleSizeBytesOut; }
    const int & getTotalBytesCaptured()     const { return totalBytesCaptured; }

    const float & getSampleRateIn()     const { return sampleRateIn; }
    const float & getAverageRxTime_ms() const { return averageRxTime_ms; }

    const TxRxData & getRxData() const { return rxData; }
    const TxProtocol & getDefultTxProtocol() const { return txProtocols[1]; }
    const TxProtocols & getTxProtocols() const { return txProtocols; }

    int takeRxData(TxRxData & dst) {
        if (lastRxDataLength == 0) return 0;

        auto res = lastRxDataLength;
        lastRxDataLength = 0;
        dst = rxData;

        return res;
    }

private:
    const TxProtocols txProtocols {
        { "Normal",     40,  9, 3, 32, 50 },
        { "Fast",       40,  6, 3, 32, 50 },
        { "Fastest",    40,  3, 3, 32, 50 },
        { "Ultrasonic", 320, 9, 3, 32, 50 },
    };

    int maxFramesPerTx() const {
        int res = 0;
        for (const auto & protocol : txProtocols) {
            res = std::max(res, protocol.paramFramesPerTx);
        }
        return res;
    }

    int minBytesPerTx() const {
        int res = txProtocols.front().paramFramesPerTx;
        for (const auto & protocol : txProtocols) {
            res = std::min(res, protocol.paramBytesPerTx);
        }
        return res;
    }

    int maxECCBytesPerTx() const {
        int res = 0;
        for (const auto & protocol : txProtocols) {
            res = std::max(res, protocol.paramECCBytesPerTx);
        }
        return res;
    }

    int nIterations;

    // Rx
    bool receivingData;
    bool analyzingData;
    bool hasNewRxData = false;

    int nCalls = 0;
    int recvDuration_frames;
    int totalBytesCaptured;
    int lastRxDataLength = 0;

    float tSum_ms = 0.0f;
    float averageRxTime_ms = 0.0;

    std::array<float, kMaxSamplesPerFrame> fftIn;
    std::array<std::complex<float>, kMaxSamplesPerFrame> fftOut;

    AmplitudeData sampleAmplitude;
    SpectrumData sampleSpectrum;

    TxRxData rxData;
    TxRxData txData;
    TxRxData txDataEncoded;

    int historyId = 0;
    AmplitudeData sampleAmplitudeAverage;
    std::array<AmplitudeData, kMaxSpectrumHistory> sampleAmplitudeHistory;

    RecordedData recordedAmplitude;

    // Tx
    bool hasData;

    float freqDelta_hz;
    float freqStart_hz;
    float hzPerSample;
    float ihzPerSample;
    float isamplesPerFrame;
    float sampleRateIn;
    float sampleRateOut;
    float sendVolume;

    int frameId;
    int framesLeftToAnalyze;
    int framesLeftToRecord;
    int framesToAnalyze;
    int framesToRecord;
    int freqDelta_bin = 1;
    int nBitsInMarker;
    int nDataBitsPerTx;
    int nECCBytesPerTx;
    int nMarkerFrames;
    int nPostMarkerFrames;
    int sampleSizeBytesIn;
    int sampleSizeBytesOut;
    int samplesPerFrame;
    int sendDataLength;

    std::string textToSend;

    TxMode txMode = TxMode::FixedLength;
    TxProtocol txProtocol;

    AmplitudeData outputBlock;
    AmplitudeData16 outputBlock16;

    std::array<bool, kMaxDataBits> dataBits;
    std::array<double, kMaxDataBits> phaseOffsets;
    std::array<double, kMaxDataBits> dataFreqs_hz;

    std::array<AmplitudeData, kMaxDataBits> bit1Amplitude;
    std::array<AmplitudeData, kMaxDataBits> bit0Amplitude;

    RS::ReedSolomon * rsData = nullptr;
    RS::ReedSolomon * rsLength = nullptr;
};
