#ifndef GGWAVE_H
#define GGWAVE_H

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

    //
    // C interface
    //

    // Data format of the audio samples
    typedef enum {
        GGWAVE_SAMPLE_FORMAT_UNDEFINED,
        GGWAVE_SAMPLE_FORMAT_U8,
        GGWAVE_SAMPLE_FORMAT_I8,
        GGWAVE_SAMPLE_FORMAT_U16,
        GGWAVE_SAMPLE_FORMAT_I16,
        GGWAVE_SAMPLE_FORMAT_F32,
    } ggwave_SampleFormat;

    // TxProtocol ids
    typedef enum {
        GGWAVE_TX_PROTOCOL_AUDIBLE_NORMAL,
        GGWAVE_TX_PROTOCOL_AUDIBLE_FAST,
        GGWAVE_TX_PROTOCOL_AUDIBLE_FASTEST,
        GGWAVE_TX_PROTOCOL_ULTRASOUND_NORMAL,
        GGWAVE_TX_PROTOCOL_ULTRASOUND_FAST,
        GGWAVE_TX_PROTOCOL_ULTRASOUND_FASTEST,
    } ggwave_TxProtocolId;

    // GGWave instance parameters
    typedef struct {
        int sampleRateIn;                       // capture sample rate
        int sampleRateOut;                      // playback sample rate
        int samplesPerFrame;                    // number of samples per audio frame
        ggwave_SampleFormat sampleFormatIn;     // format of the captured audio samples
        ggwave_SampleFormat sampleFormatOut;    // format of the playback audio samples
    } ggwave_Parameters;

    // GGWave instances are identified with an integer and are stored
    // in a private map container. Using void * caused some issues with
    // the python module and unfortunately had to do it this way
    typedef int ggwave_Instance;

    // Helper method to get default instance parameters
    GGWAVE_API ggwave_Parameters ggwave_getDefaultParameters(void);

    // Create a new GGWave instance with the specified parameters
    GGWAVE_API ggwave_Instance ggwave_init(const ggwave_Parameters parameters);

    // Free a GGWave instance
    GGWAVE_API void ggwave_free(ggwave_Instance instance);

    // Encode data into audio waveform
    //   instance       - the GGWave instance to use
    //   dataBuffer     - the data to encode
    //   dataSize       - number of bytes in the input dataBuffer
    //   txProtocolId   - the protocol to use for encoding
    //   volume         - the volume of the generated waveform [0, 100]
    //   outputBuffer   - the generated audio waveform. must be big enough to fit the generated data
    //
    //   returns the number of generated samples
    //
    //   returns -1 if there was an error
    //
    //   todo : implement api to query the size of the generated waveform before generating it
    //          so that the user can allocate enough memory for the outputBuffer
    //
    GGWAVE_API int ggwave_encode(
            ggwave_Instance instance,
            const char * dataBuffer,
            int dataSize,
            ggwave_TxProtocolId txProtocolId,
            int volume,
            char * outputBuffer);

    // Decode an audio waveform into data
    //   instance       - the GGWave instance to use
    //   dataBuffer     - the audio waveform
    //   dataSize       - number of bytes in the input dataBuffer
    //   outputBuffer   - stores the decoded data on success
    //
    //   returns the number of decoded bytes
    //
    //   Use this function to continuously provide audio samples to a GGWave instance.
    //   On each call, GGWave will analyze the provided data and if it detects a payload,
    //   it will return a non-zero result.
    //
    //   If the return value is -1 then there was an error during the decoding process.
    //   Usually can occur if there is a lot of background noise in the audio.
    //
    //   If the return value is greater than 0, then there will be that number of bytes
    //   decoded in the outputBuffer
    //
    GGWAVE_API int ggwave_decode(
            ggwave_Instance instance,
            const char * dataBuffer,
            int dataSize,
            char * outputBuffer);

#ifdef __cplusplus
}

//
// C++ interface
//

#include <cstdint>
#include <functional>
#include <vector>
#include <map>
#include <string>

class GGWave {
public:
    static constexpr auto kBaseSampleRate = 48000;
    static constexpr auto kDefaultSamplesPerFrame = 1024;
    static constexpr auto kDefaultVolume = 10;
    static constexpr auto kMaxSamplesPerFrame = 1024;
    static constexpr auto kMaxDataBits = 256;
    static constexpr auto kMaxDataSize = 256;
    static constexpr auto kMaxLength = 140;
    static constexpr auto kMaxSpectrumHistory = 4;
    static constexpr auto kMaxRecordedFrames = 1024;

    using Parameters    = ggwave_Parameters;
    using SampleFormat  = ggwave_SampleFormat;
    using TxProtocolId  = ggwave_TxProtocolId;

    struct TxProtocol {
        const char * name;  // string identifier of the protocol

        int freqStart;      // FFT bin index of the lowest frequency
        int framesPerTx;    // number of frames to transmit a single chunk of data
        int bytesPerTx;     // number of bytes in a chunk of data

        int nDataBitsPerTx() const { return 8*bytesPerTx; }
    };

    using TxProtocols = std::map<TxProtocolId, TxProtocol>;

    static const TxProtocols & getTxProtocols() {
        static const TxProtocols kTxProtocols {
            { GGWAVE_TX_PROTOCOL_AUDIBLE_NORMAL,        { "Normal",      40,  9, 3, } },
            { GGWAVE_TX_PROTOCOL_AUDIBLE_FAST,          { "Fast",        40,  6, 3, } },
            { GGWAVE_TX_PROTOCOL_AUDIBLE_FASTEST,       { "Fastest",     40,  3, 3, } },
            { GGWAVE_TX_PROTOCOL_ULTRASOUND_NORMAL,     { "[U] Normal",  320, 9, 3, } },
            { GGWAVE_TX_PROTOCOL_ULTRASOUND_FAST,       { "[U] Fast",    320, 6, 3, } },
            { GGWAVE_TX_PROTOCOL_ULTRASOUND_FASTEST,    { "[U] Fastest", 320, 3, 3, } },
        };

        return kTxProtocols;
    }

    using AmplitudeData    = std::vector<float>;
    using AmplitudeDataI16 = std::vector<int16_t>;
    using SpectrumData     = std::vector<float>;
    using RecordedData     = std::vector<float>;
    using TxRxData         = std::vector<std::uint8_t>;

    using CBEnqueueAudio = std::function<void(const void * data, uint32_t nBytes)>;
    using CBDequeueAudio = std::function<uint32_t(void * data, uint32_t nMaxBytes)>;

    GGWave(const Parameters & parameters);
    ~GGWave();

    static const Parameters & getDefaultParameters();

    bool init(const std::string & text, const int volume = kDefaultVolume);
    bool init(const std::string & text, const TxProtocol & txProtocol, const int volume = kDefaultVolume);
    bool init(int dataSize, const char * dataBuffer, const int volume = kDefaultVolume);
    bool init(int dataSize, const char * dataBuffer, const TxProtocol & txProtocol, const int volume = kDefaultVolume);

    bool encode(const CBEnqueueAudio & cbEnqueueAudio);
    void decode(const CBDequeueAudio & cbDequeueAudio);

    const bool & hasTxData()    const { return m_hasNewTxData; }
    const bool & isReceiving()  const { return m_receivingData; }
    const bool & isAnalyzing()  const { return m_analyzingData; }

    const int & getFramesToRecord()         const { return m_framesToRecord; }
    const int & getFramesLeftToRecord()     const { return m_framesLeftToRecord; }
    const int & getFramesToAnalyze()        const { return m_framesToAnalyze; }
    const int & getFramesLeftToAnalyze()    const { return m_framesLeftToAnalyze; }
    const int & getSamplesPerFrame()        const { return m_samplesPerFrame; }
    const int & getSampleSizeBytesIn()      const { return m_sampleSizeBytesIn; }
    const int & getSampleSizeBytesOut()     const { return m_sampleSizeBytesOut; }

    const float & getSampleRateIn()     const { return m_sampleRateIn; }
    const float & getSampleRateOut()    const { return m_sampleRateOut; }

    static TxProtocolId getDefaultTxProtocolId()     { return GGWAVE_TX_PROTOCOL_AUDIBLE_FAST; }
    static const TxProtocol & getDefaultTxProtocol() { return getTxProtocols().at(getDefaultTxProtocolId()); }
    static const TxProtocol & getTxProtocol(int id)  { return getTxProtocols().at(TxProtocolId(id)); }
    static const TxProtocol & getTxProtocol(TxProtocolId id) { return getTxProtocols().at(id); }

    const TxRxData & getRxData()            const { return m_rxData; }
    const TxProtocol & getRxProtocol()      const { return m_rxProtocol; }
    const TxProtocolId & getRxProtocolId()  const { return m_rxProtocolId; }

    int takeRxData(TxRxData & dst);
    int takeTxAmplitudeDataI16(AmplitudeDataI16 & dst);
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
    const SampleFormat m_sampleFormatIn;
    const SampleFormat m_sampleFormatOut;

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
    int m_samplesNeeded;

    std::vector<float> m_fftIn;  // real
    std::vector<float> m_fftOut; // complex

    bool m_hasNewSpectrum;
    SpectrumData m_sampleSpectrum;
    AmplitudeData m_sampleAmplitude;
    TxRxData m_sampleAmplitudeTmp;

    bool m_hasNewRxData;
    int m_lastRxDataLength;
    TxRxData m_rxData;
    TxProtocol m_rxProtocol;
    TxProtocolId m_rxProtocolId;

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
    TxRxData m_outputBlockTmp;
    AmplitudeDataI16 m_outputBlockI16;
    AmplitudeDataI16 m_txAmplitudeDataI16;
};

#endif

#endif
