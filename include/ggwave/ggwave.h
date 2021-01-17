#ifndef GGWAVE_H
#define GGWAVE_H

// Define GGWAVE_API macro to properly export symbols
#ifdef GGWAVE_SHARED
#    ifdef _WIN32
#        ifdef GGWAVE_BUILD
#            define GGWAVE_API __declspec(dllexport)
#        else
#            define GGWAVE_API __declspec(dllimport)
#        endif
#    else
#        define GGWAVE_API __attribute__ ((visibility ("default")))
#    endif
#else
#    define GGWAVE_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

	typedef enum {
		GGWAVE_SAMPLE_FORMAT_I16,
		GGWAVE_SAMPLE_FORMAT_F32,
	} ggwave_SampleFormat;

	typedef enum {
		GGWAVE_TX_PROTOCOL_AUDIBLE_NORMAL,
		GGWAVE_TX_PROTOCOL_AUDIBLE_FAST,
		GGWAVE_TX_PROTOCOL_AUDIBLE_FASTEST,
		GGWAVE_TX_PROTOCOL_ULTRASOUND_NORMAL,
		GGWAVE_TX_PROTOCOL_ULTRASOUND_FAST,
		GGWAVE_TX_PROTOCOL_ULTRASOUND_FASTEST,
	} ggwave_TxProtocol;

	typedef struct {
		int sampleRateIn;
		int sampleRateOut;
		int samplesPerFrame;
		ggwave_SampleFormat formatIn;
		ggwave_SampleFormat formatOut;
	} ggwave_Parameters;

    typedef int ggwave_Instance;

    GGWAVE_API ggwave_Parameters ggwave_defaultParameters(void);

    GGWAVE_API ggwave_Instance ggwave_init(const ggwave_Parameters parameters);

    GGWAVE_API void ggwave_free(ggwave_Instance instance);

    GGWAVE_API int ggwave_encode(
            ggwave_Instance instance,
            const char * dataBuffer,
            int dataSize,
            ggwave_TxProtocol txProtocol,
            int volume,
            char * outputBuffer);

    GGWAVE_API int ggwave_decode(
            ggwave_Instance instance,
            const char * dataBuffer,
            int dataSize,
            char * outputBuffer);

#ifdef __cplusplus
}

#include <cstdint>
#include <functional>
#include <vector>

class GGWave {
public:
    static constexpr auto kBaseSampleRate = 48000;
    static constexpr auto kDefaultSamplesPerFrame = 1024;
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

    static const TxProtocols & getTxProtocols() {
        static TxProtocols kTxProtocols {
            { "Normal",      40,  9, 3, },
            { "Fast",        40,  6, 3, },
            { "Fastest",     40,  3, 3, },
            { "[U] Normal",  320, 9, 3, },
            { "[U] Fast",    320, 6, 3, },
            { "[U] Fastest", 320, 3, 3, },
        };

        return kTxProtocols;
    }

    using AmplitudeData   = std::vector<float>;
    using AmplitudeData16 = std::vector<int16_t>;
    using SpectrumData    = std::vector<float>;
    using RecordedData    = std::vector<float>;
    using TxRxData        = std::vector<std::uint8_t>;

    // todo : rename to CBEnqueueAudio
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

    // todo : rename to "encode" / "decode"
    bool send(const CBQueueAudio & cbQueueAudio);
    void receive(const CBDequeueAudio & CBDequeueAudio);

    const bool & hasTxData() const { return m_hasNewTxData; }
    const bool & isReceiving() const { return m_receivingData; }
    const bool & isAnalyzing() const { return m_analyzingData; }

    const int & getFramesToRecord()         const { return m_framesToRecord; }
    const int & getFramesLeftToRecord()     const { return m_framesLeftToRecord; }
    const int & getFramesToAnalyze()        const { return m_framesToAnalyze; }
    const int & getFramesLeftToAnalyze()    const { return m_framesLeftToAnalyze; }
    const int & getSamplesPerFrame()        const { return m_samplesPerFrame; }
    const int & getSampleSizeBytesIn()      const { return m_sampleSizeBytesIn; }
    const int & getSampleSizeBytesOut()     const { return m_sampleSizeBytesOut; }

    const float & getSampleRateIn() const { return m_sampleRateIn; }
    const float & getSampleRateOut() const { return m_sampleRateOut; }

    static int getDefultTxProtocolId() { return 1; }
    static const TxProtocol & getDefultTxProtocol() { return getTxProtocols()[getDefultTxProtocolId()]; }

    const TxRxData & getRxData() const { return m_rxData; }
    const TxProtocol & getRxProtocol() const { return m_rxProtocol; }
    const int & getRxProtocolId() const { return m_rxProtocolId; }

    int takeRxData(TxRxData & dst);
    int takeTxAmplitudeData16(AmplitudeData16 & dst);
    bool takeSpectrum(SpectrumData & dst);

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
    bool m_receivingData;
    bool m_analyzingData;

    int m_markerFreqStart;
    int m_recvDuration_frames;

    int m_framesLeftToAnalyze;
    int m_framesLeftToRecord;
    int m_framesToAnalyze;
    int m_framesToRecord;

    std::vector<float> m_fftIn;  // real
    std::vector<float> m_fftOut; // complex

    bool m_hasNewSpectrum;
    SpectrumData m_sampleSpectrum;
    AmplitudeData m_sampleAmplitude;

    bool m_hasNewRxData;
    int m_lastRxDataLength;
    TxRxData m_rxData;
    TxProtocol m_rxProtocol;
    int m_rxProtocolId;

    int m_historyId = 0;
    AmplitudeData m_sampleAmplitudeAverage;
    std::vector<AmplitudeData> m_sampleAmplitudeHistory;

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

    AmplitudeData m_outputBlock;
    AmplitudeData16 m_outputBlock16;
    AmplitudeData16 m_txAmplitudeData16;
};

#endif

#endif
