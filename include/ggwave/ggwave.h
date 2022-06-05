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

    // Protocol ids
    typedef enum {
        GGWAVE_PROTOCOL_AUDIBLE_NORMAL,
        GGWAVE_PROTOCOL_AUDIBLE_FAST,
        GGWAVE_PROTOCOL_AUDIBLE_FASTEST,
        GGWAVE_PROTOCOL_ULTRASOUND_NORMAL,
        GGWAVE_PROTOCOL_ULTRASOUND_FAST,
        GGWAVE_PROTOCOL_ULTRASOUND_FASTEST,
        GGWAVE_PROTOCOL_DT_NORMAL,
        GGWAVE_PROTOCOL_DT_FAST,
        GGWAVE_PROTOCOL_DT_FASTEST,
        GGWAVE_PROTOCOL_MT_NORMAL,
        GGWAVE_PROTOCOL_MT_FAST,
        GGWAVE_PROTOCOL_MT_FASTEST,

        GGWAVE_PROTOCOL_CUSTOM_0,
        GGWAVE_PROTOCOL_CUSTOM_1,
        GGWAVE_PROTOCOL_CUSTOM_2,
        GGWAVE_PROTOCOL_CUSTOM_3,
        GGWAVE_PROTOCOL_CUSTOM_4,
        GGWAVE_PROTOCOL_CUSTOM_5,
        GGWAVE_PROTOCOL_CUSTOM_6,
        GGWAVE_PROTOCOL_CUSTOM_7,
        GGWAVE_PROTOCOL_CUSTOM_8,
        GGWAVE_PROTOCOL_CUSTOM_9,

        GGWAVE_PROTOCOL_COUNT,
    } ggwave_ProtocolId;

    // Operating modes of ggwave
    //
    //   GGWAVE_OPERATING_MODE_RX:
    //     The instance will be able to receive audio data
    //
    //   GGWAVE_OPERATING_MODE_TX:
    //     The instance will be able generate audio waveforms for transmission
    //
    //   GGWAVE_OPERATING_MODE_TX_ONLY_TONES:
    //     The encoding process generates only a list of tones instead of full audio
    //     waveform. This is useful for low-memory devices and embedded systems.
    //
    //   GGWAVE_OPERATING_MODE_USE_DSS:
    //     Enable the built-in Direct Sequence Spread (DSS) algorithm
    //
    typedef enum {
        GGWAVE_OPERATING_MODE_RX            = 1 << 1,
        GGWAVE_OPERATING_MODE_TX            = 1 << 2,
        GGWAVE_OPERATING_MODE_RX_AND_TX     = (GGWAVE_OPERATING_MODE_RX |
                                               GGWAVE_OPERATING_MODE_TX),
        GGWAVE_OPERATING_MODE_TX_ONLY_TONES = 1 << 3,
        GGWAVE_OPERATING_MODE_USE_DSS       = 1 << 4,
    } ggwave_OperatingMode;

    // GGWave instance parameters
    //
    //   If payloadLength <= 0, then GGWave will transmit with variable payload length
    //   depending on the provided payload. Sound markers are used to identify the
    //   start and end of the transmission.
    //
    //   If payloadLength > 0, then the transmitted payload will be of the specified
    //   fixed length. In this case, no sound markers are emitted and a slightly
    //   different decoding scheme is applied. This is useful in cases where the
    //   length of the payload is known in advance.
    //
    //   The sample rates are values typically between 1000 and 96000.
    //   Default value: GGWave::kDefaultSampleRate
    //
    //   The captured audio is resampled to the specified sampleRate if sampleRatInp
    //   is different from sampleRate. Same applies to the transmitted audio.
    //
    //   The samplesPerFrame is the number of samples on which ggwave performs FFT.
    //   This affects the number of bins in the Fourier spectrum.
    //   Default value: GGWave::kDefaultSamplesPerFrame
    //
    //   The operatingMode controls which functions of the ggwave instance are enabled.
    //   Use this parameter to reduce the memory footprint of the ggwave instance. For
    //   example, if only Rx is enabled, then the memory buffers needed for the Tx will
    //   not be allocated.
    //
    typedef struct {
        int payloadLength;                      // payload length
        float sampleRateInp;                    // capture sample rate
        float sampleRateOut;                    // playback sample rate
        float sampleRate;                       // the operating sample rate
        int samplesPerFrame;                    // number of samples per audio frame
        float soundMarkerThreshold;             // sound marker detection threshold
        ggwave_SampleFormat sampleFormatInp;    // format of the captured audio samples
        ggwave_SampleFormat sampleFormatOut;    // format of the playback audio samples
        ggwave_OperatingMode operatingMode;     // operating mode
    } ggwave_Parameters;

    // GGWave instances are identified with an integer and are stored
    // in a private map container. Using void * caused some issues with
    // the python module and unfortunately had to do it this way
    typedef int ggwave_Instance;

    // Change file stream for internal ggwave logging. NULL - disable logging
    //
    //   Intentionally passing it as void * instead of FILE * to avoid including a header
    //
    //     // log to standard error
    //     ggwave_setLogFile(stderr);
    //
    //     // log to standard output
    //     ggwave_setLogFile(stdout);
    //
    //     // disable logging
    //     ggwave_setLogFile(NULL);
    //
    //  Note: not thread-safe. Do not call while any GGWave instances are running
    //
    GGWAVE_API void ggwave_setLogFile(void * fptr);

    // Helper method to get default instance parameters
    GGWAVE_API ggwave_Parameters ggwave_getDefaultParameters(void);

    // Create a new GGWave instance with the specified parameters
    //
    //   The newly created instance is added to the internal map container.
    //   This function returns an id that can be used to identify this instance.
    //   Make sure to deallocate the instance at the end by calling ggwave_free()
    //
    GGWAVE_API ggwave_Instance ggwave_init(const ggwave_Parameters parameters);

    // Free a GGWave instance
    GGWAVE_API void ggwave_free(ggwave_Instance instance);

    // Encode data into audio waveform
    //
    //   instance       - the GGWave instance to use
    //   payloadBuffer  - the data to encode
    //   payloadSize    - number of bytes in the input payloadBuffer
    //   protocolId     - the protocol to use for encoding
    //   volume         - the volume of the generated waveform [0, 100]
    //                    usually 25 is OK and you should not go over 50
    //   waveformBuffer - the generated audio waveform. must be big enough to fit the generated data
    //   query          - if == 0, encode data in to waveformBuffer, returns number of bytes
    //                    if != 0, do not perform encoding.
    //                    if == 1, return waveform size in bytes
    //                    if != 1, return waveform size in samples
    //
    //   returns the number of generated bytes or samples (see query)
    //
    //   returns -1 if there was an error
    //
    //   This function can be used to encode some binary data (payload) into an audio waveform.
    //
    //     payload -> waveform
    //
    //   When calling it, make sure that the waveformBuffer is big enough to store the
    //   generated waveform. This means that its size must be at least:
    //
    //     nSamples*sizeOfSample_bytes
    //
    //   Where nSamples is the number of audio samples in the waveform and sizeOfSample_bytes
    //   is the size of a single sample in bytes based on the sampleFormatOut parameter
    //   specified during the initialization of the GGWave instance.
    //
    //   If query != 0, then this function does not perform the actual encoding and just
    //   outputs the expected size of the waveform that would be generated if you call it
    //   with query == 0. This mechanism can be used to ask ggwave how much memory to
    //   allocate for the waveformBuffer. For example:
    //
    //     // this is the data to encode
    //     const char * payload = "test";
    //
    //     // query the number of bytes in the waveform
    //     int n = ggwave_encode(instance, payload, 4, GGWAVE_PROTOCOL_AUDIBLE_FAST, 25, NULL, 1);
    //
    //     // allocate the output buffer
    //     char waveform[n];
    //
    //     // generate the waveform
    //     ggwave_encode(instance, payload, 4, GGWAVE_PROTOCOL_AUDIBLE_FAST, 25, waveform, 0);
    //
    //   The payloadBuffer can be any binary data that you would like to transmit (i.e. the payload).
    //   Usually, this is some text, but it can be any sequence of bytes.
    //
    GGWAVE_API int ggwave_encode(
            ggwave_Instance instance,
            const void * payloadBuffer,
            int payloadSize,
            ggwave_ProtocolId protocolId,
            int volume,
            void * waveformBuffer,
            int query);

    // Decode an audio waveform into data
    //
    //   instance       - the GGWave instance to use
    //   waveformBuffer - the audio waveform
    //   waveformSize   - number of bytes in the input waveformBuffer
    //   payloadBuffer  - stores the decoded data on success
    //                    the maximum size of the output is GGWave::kMaxDataSize
    //
    //   returns the number of decoded bytes
    //
    //   Use this function to continuously provide audio samples to a GGWave instance.
    //   On each call, GGWave will analyze the provided data and if it detects a payload,
    //   it will return a non-zero result.
    //
    //     waveform -> payload
    //
    //   If the return value is -1 then there was an error during the decoding process.
    //   Usually can occur if there is a lot of background noise in the audio.
    //
    //   If the return value is greater than 0, then there are that number of bytes decoded.
    //
    //   IMPORTANT:
    //   Notice that the decoded data written to the payloadBuffer is NOT null terminated.
    //
    //   Example:
    //
    //     char payload[256];
    //
    //     while (true) {
    //         ... capture samplesPerFrame audio samples into waveform ...
    //
    //         int ret = ggwave_decode(instance, waveform, samplesPerFrame*sizeOfSample_bytes, payload);
    //         if (ret > 0) {
    //             printf("Received payload: '%s'\n", payload);
    //         }
    //     }
    //
    GGWAVE_API int ggwave_decode(
            ggwave_Instance instance,
            const void * waveformBuffer,
            int waveformSize,
            void * payloadBuffer);

    // Memory-safe overload of ggwave_decode
    //
    //   payloadSize - optionally specify the size of the output buffer
    //
    //   If the return value is -2 then the provided payloadBuffer was not big enough to
    //   store the decoded data.
    //
    //   See ggwave_decode for more information
    //
    GGWAVE_API int ggwave_ndecode(
            ggwave_Instance instance,
            const void * waveformBuffer,
            int waveformSize,
            void * payloadBuffer,
            int payloadSize);

    // Toggle Rx protocols on and off
    //
    //   protocolId - Id of the Rx protocol to modify
    //   state      - 0 - disable, 1 - enable
    //
    //   If an Rx protocol is enabled, newly constructued GGWave instances will attempt to decode
    //   received data using this protocol. By default, all protocols are enabled.
    //   Use this function to restrict the number of Rx protocols used in the decoding
    //   process. This helps to reduce the number of false positives and improves the transmission
    //   accuracy, especially when the Tx/Rx protocol is known in advance.
    //
    //   Note that this function does not affect the decoding process of instances that have
    //   already been created.
    //
    GGWAVE_API void ggwave_rxToggleProtocol(
            ggwave_ProtocolId protocolId,
            int state);

    // Toggle Tx protocols on and off
    //
    //   protocolId - Id of the Tx protocol to modify
    //   state      - 0 - disable, 1 - enable
    //
    //   If an Tx protocol is enabled, newly constructued GGWave instances will be able to transmit
    //   data using this protocol. By default, all protocols are enabled.
    //   Use this function to restrict the number of Tx protocols used for transmission.
    //   This can reduce the required memory by the GGWave instance.
    //
    //   Note that this function does not affect instances that have already been created.
    //
    GGWAVE_API void ggwave_txToggleProtocol(
            ggwave_ProtocolId protocolId,
            int state);

#ifdef __cplusplus
}

//
// C++ interface
//

#include <cstdint>
#include <iosfwd>
#include <vector>

class GGWave {
public:
    static constexpr auto kSampleRateMin               = 1000.0f;
    static constexpr auto kSampleRateMax               = 96000.0f;
    static constexpr auto kDefaultSampleRate           = 48000.0f;
    static constexpr auto kDefaultSamplesPerFrame      = 1024;
    static constexpr auto kDefaultVolume               = 10;
    static constexpr auto kDefaultSoundMarkerThreshold = 3.0f;
    static constexpr auto kDefaultMarkerFrames         = 16;
    static constexpr auto kDefaultEncodedDataOffset    = 3;
    static constexpr auto kMaxSamplesPerFrame          = 1024;
    static constexpr auto kMaxDataSize                 = 256;
    static constexpr auto kMaxLengthVariable           = 140;
    static constexpr auto kMaxLengthFixed              = 16;
    static constexpr auto kMaxSpectrumHistory          = 4;
    static constexpr auto kMaxRecordedFrames           = 2048;

    using Parameters    = ggwave_Parameters;
    using SampleFormat  = ggwave_SampleFormat;
    using ProtocolId    = ggwave_ProtocolId;
    using TxProtocolId  = ggwave_ProtocolId;
    using RxProtocolId  = ggwave_ProtocolId;
    using OperatingMode = ggwave_OperatingMode;

    struct Protocol {
        const char * name;  // string identifier of the protocol

        int freqStart;      // FFT bin index of the lowest frequency
        int framesPerTx;    // number of frames to transmit a single chunk of data
        int bytesPerTx;     // number of bytes in a chunk of data
        int extra;          // 2 if this is a mono-tone protocol, 1 otherwise

        bool enabled;

        int nDataBitsPerTx() const { return 8*bytesPerTx; }
    };

    using TxProtocol = Protocol;
    using RxProtocol = Protocol;

    struct Protocols;

    using TxProtocols = Protocols;
    using RxProtocols = Protocols;

    struct Protocols : public std::vector<Protocol> {
        using std::vector<Protocol>::vector;

        void enableAll();
        void disableAll();

        void toggle(ProtocolId id, bool state);
        void only(ProtocolId id);
        void only(std::initializer_list<ProtocolId> ids);

        static Protocols & kDefault() {
            static Protocols kProtocols(GGWAVE_PROTOCOL_COUNT);

            static bool kInitialized = false;
            if (kInitialized == false) {
                for (auto & protocol : kProtocols) {
                    protocol.name = nullptr;
                    protocol.enabled = false;
                }

                kProtocols[GGWAVE_PROTOCOL_AUDIBLE_NORMAL]     = { "Normal",       40,  9, 3, 1, true, };
                kProtocols[GGWAVE_PROTOCOL_AUDIBLE_FAST]       = { "Fast",         40,  6, 3, 1, true, };
                kProtocols[GGWAVE_PROTOCOL_AUDIBLE_FASTEST]    = { "Fastest",      40,  3, 3, 1, true, };
                kProtocols[GGWAVE_PROTOCOL_ULTRASOUND_NORMAL]  = { "[U] Normal",   320, 9, 3, 1, true, };
                kProtocols[GGWAVE_PROTOCOL_ULTRASOUND_FAST]    = { "[U] Fast",     320, 6, 3, 1, true, };
                kProtocols[GGWAVE_PROTOCOL_ULTRASOUND_FASTEST] = { "[U] Fastest",  320, 3, 3, 1, true, };
                kProtocols[GGWAVE_PROTOCOL_DT_NORMAL]          = { "[DT] Normal",  24,  9, 1, 1, true, };
                kProtocols[GGWAVE_PROTOCOL_DT_FAST]            = { "[DT] Fast",    24,  6, 1, 1, true, };
                kProtocols[GGWAVE_PROTOCOL_DT_FASTEST]         = { "[DT] Fastest", 24,  3, 1, 1, true, };
                kProtocols[GGWAVE_PROTOCOL_MT_NORMAL]          = { "[MT] Normal",  24,  9, 1, 2, true, };
                kProtocols[GGWAVE_PROTOCOL_MT_FAST]            = { "[MT] Fast",    24,  6, 1, 2, true, };
                kProtocols[GGWAVE_PROTOCOL_MT_FASTEST]         = { "[MT] Fastest", 24,  3, 1, 2, true, };

                kInitialized = true;
            }

            return kProtocols;
        }

        static TxProtocols & tx();
        static RxProtocols & rx();
    };

    struct ToneData {
        float freq_hz;
        float duration_ms;
    };

    using TonesPerFrame = std::vector<ToneData>;
    using Tones         = std::vector<TonesPerFrame>;

    using Amplitude    = std::vector<float>;
    using AmplitudeI16 = std::vector<int16_t>;
    using Spectrum     = std::vector<float>;
    using RecordedData = std::vector<float>;
    using TxRxData     = std::vector<uint8_t>;

    // constructor
    //
    //  All memory buffers used by the GGWave instance are allocated upon construction.
    //  No memory allocations occur after that.
    //
    //  The sizes of the buffers are determined by the parameters and the contents of:
    //
    //    - GGWave::Protocols::rx()
    //    - GGWave::Protocols::tx()
    //
    //  For optimal performance and minimum memory usage, make sure to enable only the
    //  Rx and Tx protocols that you need.
    //
    //  For example, using a single protocol for Tx is achieved like this:
    //
    //    Parameters parameters;
    //    parameters.operatingMode = GGWAVE_OPERATING_MODE_TX;
    //    GGWave::Protocols::tx().only(GGWave::ProtocolId::GGWAVE_PROTOCOL_AUDIBLE_NORMAL);
    //    GGWave instance(parameters);
    //    instance.init(...);
    //    instance.encode();
    //
    //  The created instance will only be able to transmit data using the "Normal"
    //  protocol. Rx will be disabled.
    //
    //  To create a corresponding Rx-only instance, use the following:
    //
    //    Parameters parameters;
    //    parameters.operatingMode = GGWAVE_OPERATING_MODE_RX;
    //    GGWave::Protocols::rx().only(GGWave::ProtocolId::GGWAVE_PROTOCOL_AUDIBLE_NORMAL);
    //    GGWave instance(parameters);
    //    instance.decode(...);
    //
    GGWave(const Parameters & parameters);
    ~GGWave();

    // set file stream for the internal ggwave logging
    //
    //  By default, ggwave prints internal log messages to stderr.
    //  To disable logging all together, call this method with nullptr.
    //
    //  Note: not thread-safe. Do not call while any GGWave instances are running
    //
    static void setLogFile(FILE * fptr);

    static const Parameters & getDefaultParameters();

    // set Tx data to encode
    //
    //  This prepares the GGWave instance for transmission.
    //  To perform the actual encoding, the encode() method must be called
    //
    //  returns false upon invalid parameters or failure to initialize
    //
    bool init(const char * text, TxProtocolId protocolId, const int volume = kDefaultVolume);
    bool init(int dataSize, const char * dataBuffer, TxProtocolId protocolId, const int volume = kDefaultVolume);

    // expected waveform size of the encoded Tx data in bytes
    //
    //   When the output sampling rate is not equal to operating sample rate the result of this method is overestimation of
    //   the actual number of bytes that would be produced
    //
    uint32_t encodeSize_bytes() const;

    // expected waveform size of the encoded Tx data in samples
    //
    //   When the output sampling rate is not equal to operating sample rate the result of this method is overestimation of
    //   the actual number of samples that would be produced
    //
    uint32_t encodeSize_samples() const;

    // encode Tx data into an audio waveform
    //
    //   After calling this method, the generated waveform is available through the txData() method
    //
    //   returns the number of bytes in the generated waveform
    //
    uint32_t encode();

    const void * txData() const;

    // decode an audio waveform
    //
    //   data   - pointer to the waveform data
    //   nBytes - number of bytes in the waveform
    //
    //   After calling this method, use the Rx methods to check if any data was decoded successfully.
    //
    //   returns false if the provided waveform is somehow invalid
    //
    bool decode(const void * data, uint32_t nBytes);

    //
    // instance state
    //

    bool isDSSEnabled() const;

    int samplesPerFrame() const;
    int sampleSizeInp()   const;
    int sampleSizeOut()   const;

    float sampleRateInp() const;
    float sampleRateOut() const;
    SampleFormat sampleFormatInp() const;
    SampleFormat sampleFormatOut() const;

    //
    // Tx
    //

    // get a list of the tones generated for the last waveform
    //
    //   Call this method after calling encode() to get a list of the tones
    //   participating in the generated waveform
    //
    const Tones & txTones() const;

    // true if there is data pending to be transmitted
    bool txHasData() const;

    // consume the amplitude data from the last generated waveform
    bool txTakeAmplitudeI16(AmplitudeI16 & dst);

    // the instance will allow Tx only with these protocols
    // they are determined upon construction, using GGWave::Protocols::tx()
    const TxProtocols & txProtocols() const;

    //
    // Rx
    //

    bool rxReceiving() const;
    bool rxAnalyzing() const;

    int rxSamplesNeeded()       const;
    int rxFramesToRecord()      const;
    int rxFramesLeftToRecord()  const;
    int rxFramesToAnalyze()     const;
    int rxFramesLeftToAnalyze() const;

    bool rxStopReceiving();

    // the instance will attempt to decode only these protocols
    // they are determined upon construction, using GGWave::Protocols::rx()
    //
    // note: do not enable protocols that were not enabled upon construction of the GGWave
    // instance, or the decoding will likely crash
    RxProtocols & rxProtocols();

    // information about last received data
    int                  rxDataLength() const;
    const TxRxData &     rxData()       const;
    const RxProtocol &   rxProtocol()   const;
    const RxProtocolId & rxProtocolId() const;
    const Spectrum &     rxSpectrum()   const;
    const Amplitude &    rxAmplitude()  const;

    // consume the received data
    //
    // returns the data length in bytes
    int rxTakeData(TxRxData & dst);

    // consume the received spectrum / amplitude data
    //
    // returns true if there was new data available
    bool rxTakeSpectrum(Spectrum & dst);
    bool rxTakeAmplitude(Amplitude & dst);

    //
    // Utils
    //

    // compute FFT of real values
    //
    //   src - input real-valued data, size is N
    //   dst - output complex-valued data, size is 2*N
    //
    //   N must be == samplesPerFrame()
    //
    bool computeFFTR(const float * src, float * dst, int N);

    // resample audio waveforms from one sample rate to another using sinc interpolation
    class Resampler {
    public:
        // this controls the number of neighboring samples
        // which are used to interpolate the new samples. The
        // processing time is linearly related to this width
        static const int kWidth = 64;

        Resampler();

        void reset();

        int nSamplesTotal() const { return m_state.nSamplesTotal; }

        int resample(
                float factor,
                int nSamples,
                const float * samplesInp,
                float * samplesOut);

    private:
        float getData(int j) const;
        void newData(float data);
        void makeSinc();
        double sinc(double x) const;

        static const int kDelaySize = 140;

        // this defines how finely the sinc function is sampled for storage in the table
        static const int kSamplesPerZeroCrossing = 32;

        std::vector<float> m_sincTable;
        std::vector<float> m_delayBuffer;
        std::vector<float> m_edgeSamples;
        std::vector<float> m_samplesInp;

        struct State {
            int nSamplesTotal = 0;
            int timeInt = 0;
            int timeLast = 0;
            double timeNow = 0.0;
        };

        State m_state;
    };

private:
    void decode_fixed();
    void decode_variable();

    int maxFramesPerTx(const Protocols & protocols, bool excludeMT) const;
    int minBytesPerTx(const Protocols & protocols) const;
    int maxBytesPerTx(const Protocols & protocols) const;

    double bitFreq(const Protocol & p, int bit) const;

    const float m_sampleRateInp;
    const float m_sampleRateOut;
    const float m_sampleRate;
    const int m_samplesPerFrame;
    const float m_isamplesPerFrame;
    const int m_sampleSizeInp;
    const int m_sampleSizeOut;
    const SampleFormat m_sampleFormatInp;
    const SampleFormat m_sampleFormatOut;

    const float m_hzPerSample;
    const float m_ihzPerSample;

    const int m_freqDelta_bin;
    const float m_freqDelta_hz;

    const int m_nBitsInMarker;
    const int m_nMarkerFrames;
    const int m_encodedDataOffset;

    const float m_soundMarkerThreshold;

    const bool m_isFixedPayloadLength;
    const int m_payloadLength;

    const bool m_isRxEnabled;
    const bool m_isTxEnabled;
    const bool m_needResampling;
    const bool m_txOnlyTones;
    const bool m_isDSSEnabled;

    // common
    TxRxData m_dataEncoded;
    TxRxData m_workRSLength; // Reed-Solomon work buffers
    TxRxData m_workRSData;
    TxRxData m_dssMagic;

    // Impl
    struct Rx;
    Rx * m_rx;

    struct Tx;
    Tx * m_tx;

    Resampler * m_resampler;
};

#endif

#endif
