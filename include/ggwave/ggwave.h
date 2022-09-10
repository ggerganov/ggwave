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

#if defined(ARDUINO_UNO)
#define GGWAVE_CONFIG_FEW_PROTOCOLS
#endif

#ifdef __cplusplus
extern "C" {
#endif

    //
    // C interface
    //

#define GGWAVE_MAX_INSTANCES 4

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
#ifndef GGWAVE_CONFIG_FEW_PROTOCOLS
        GGWAVE_PROTOCOL_AUDIBLE_NORMAL,
        GGWAVE_PROTOCOL_AUDIBLE_FAST,
        GGWAVE_PROTOCOL_AUDIBLE_FASTEST,
        GGWAVE_PROTOCOL_ULTRASOUND_NORMAL,
        GGWAVE_PROTOCOL_ULTRASOUND_FAST,
        GGWAVE_PROTOCOL_ULTRASOUND_FASTEST,
#endif
        GGWAVE_PROTOCOL_DT_NORMAL,
        GGWAVE_PROTOCOL_DT_FAST,
        GGWAVE_PROTOCOL_DT_FASTEST,
        GGWAVE_PROTOCOL_MT_NORMAL,
        GGWAVE_PROTOCOL_MT_FAST,
        GGWAVE_PROTOCOL_MT_FASTEST,

#ifndef GGWAVE_CONFIG_FEW_PROTOCOLS
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

#endif
        GGWAVE_PROTOCOL_COUNT,
    } ggwave_ProtocolId;

    typedef enum {
        GGWAVE_FILTER_HANN,
        GGWAVE_FILTER_HAMMING,
        GGWAVE_FILTER_FIRST_ORDER_HIGH_PASS,
    } ggwave_Filter;

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
    enum {
        GGWAVE_OPERATING_MODE_RX            = 1 << 1,
        GGWAVE_OPERATING_MODE_TX            = 1 << 2,
        GGWAVE_OPERATING_MODE_RX_AND_TX     = (GGWAVE_OPERATING_MODE_RX |
                                               GGWAVE_OPERATING_MODE_TX),
        GGWAVE_OPERATING_MODE_TX_ONLY_TONES = 1 << 3,
        GGWAVE_OPERATING_MODE_USE_DSS       = 1 << 4,
    };

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
        int                 payloadLength;        // payload length
        float               sampleRateInp;        // capture sample rate
        float               sampleRateOut;        // playback sample rate
        float               sampleRate;           // the operating sample rate
        int                 samplesPerFrame;      // number of samples per audio frame
        float               soundMarkerThreshold; // sound marker detection threshold
        ggwave_SampleFormat sampleFormatInp;      // format of the captured audio samples
        ggwave_SampleFormat sampleFormatOut;      // format of the playback audio samples
        int                 operatingMode;        // operating mode
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
    GGWAVE_API ggwave_Instance ggwave_init(ggwave_Parameters parameters);

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
    //             payload[ret] = 0; // null terminate the string
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

    // Set freqStart for an Rx protocol
    GGWAVE_API void ggwave_rxProtocolSetFreqStart(
            ggwave_ProtocolId protocolId,
            int freqStart);

    // Set freqStart for a Tx protocol
    GGWAVE_API void ggwave_txProtocolSetFreqStart(
            ggwave_ProtocolId protocolId,
            int freqStart);

#ifdef __cplusplus
}

//
// C++ interface
//

template <typename T>
struct ggvector {
private:
    T * m_data;
    int m_size;

public:
    using value_type = T;

    ggvector() : m_data(nullptr), m_size(0) {}
    ggvector(T * data, int size) : m_data(data), m_size(size) {}

    ggvector(const ggvector<T> & other) = default;

    // delete operator=
    ggvector & operator=(const ggvector &) = delete;
    ggvector & operator=(ggvector &&) = delete;

    T & operator[](int i) { return m_data[i]; }
    const T & operator[](int i) const { return m_data[i]; }

    int size() const { return m_size; }
    T * data() const { return m_data; }

    T * begin() const { return m_data; }
    T * end() const { return m_data + m_size; }

    void assign(const ggvector & other);
    void copy(const ggvector & other);

    void zero();
    void zero(int n);
};

template <typename T>
struct ggmatrix {
private:
    T * m_data;
    int m_size0;
    int m_size1;

public:
    using value_type = T;

    ggmatrix() : m_data(nullptr), m_size0(0), m_size1(0) {}
    ggmatrix(T * data, int size0, int size1) : m_data(data), m_size0(size0), m_size1(size1) {}

    ggvector<T> operator[](int i) {
        return ggvector<T>(m_data + i*m_size1, m_size1);
    }

    int size() const { return m_size0; }

    void zero();
};

#include <stdint.h>
#include <stdio.h>

#ifdef ARDUINO
#if defined(ARDUINO_ARCH_AVR) || defined(ARDUINO_ARCH_SAMD) || defined(ARDUINO_ARDUINO_NANO33BLE) || defined(ARDUINO_ARCH_MBED_RP2040) || defined(ARDUINO_ARCH_RP2040)
#include <avr/pgmspace.h>
#else
#include <pgmspace.h>
#endif
#endif

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
    static constexpr auto kMaxLengthFixed              = 64;
    static constexpr auto kMaxSpectrumHistory          = 4;
    static constexpr auto kMaxRecordedFrames           = 2048;

    using Parameters    = ggwave_Parameters;
    using SampleFormat  = ggwave_SampleFormat;
    using ProtocolId    = ggwave_ProtocolId;
    using TxProtocolId  = ggwave_ProtocolId;
    using RxProtocolId  = ggwave_ProtocolId;
    using OperatingMode = int; // ggwave_OperatingMode;

    struct Protocol {
        const char * name;  // string identifier of the protocol

        int16_t freqStart;   // FFT bin index of the lowest frequency
        int8_t  framesPerTx; // number of frames to transmit a single chunk of data
        int8_t  bytesPerTx;  // number of bytes in a chunk of data
        int8_t  extra;       // 2 if this is a mono-tone protocol, 1 otherwise

        bool enabled;

        int nTones() const { return (2*bytesPerTx)/extra; }
        int nDataBitsPerTx() const { return 8*bytesPerTx; }
        int txDuration_ms(int samplesPerFrame, float sampleRate) const {
            return framesPerTx*((1000.0f*samplesPerFrame)/sampleRate);
        }
    };

    using TxProtocol = Protocol;
    using RxProtocol = Protocol;

    struct Protocols;

    using TxProtocols = Protocols;
    using RxProtocols = Protocols;

    struct Protocols {
        Protocol data[GGWAVE_PROTOCOL_COUNT];

        int size() const {
            return GGWAVE_PROTOCOL_COUNT;
        }

        bool empty() const {
            return size() == 0;
        }

        Protocol & operator[](ProtocolId id) {
            return data[id];
        }

        Protocol & operator[](int id) {
            return data[id];
        }

        const Protocol & operator[](ProtocolId id) const {
            return data[id];
        }

        const Protocol & operator[](int id) const {
            return data[id];
        }

        void enableAll();
        void disableAll();

        void toggle(ProtocolId id, bool state);
        void only(ProtocolId id);

        static Protocols & kDefault() {
            static Protocols protocols;

            static bool initialized = false;
            if (initialized == false) {
                for (int i = 0; i < GGWAVE_PROTOCOL_COUNT; ++i) {
                    protocols.data[i].name = nullptr;
                    protocols.data[i].enabled = false;
                }

#if defined(ARDUINO_AVR_UNO)
// For Arduino Uno, we put the strings in PROGMEM to save as much RAM as possible:
#define GGWAVE_PSTR PSTR
#else
#define GGWAVE_PSTR(str) (str)
#endif

#ifndef GGWAVE_CONFIG_FEW_PROTOCOLS
                protocols.data[GGWAVE_PROTOCOL_AUDIBLE_NORMAL]     = { GGWAVE_PSTR("Normal"),       40,  9, 3, 1, true, };
                protocols.data[GGWAVE_PROTOCOL_AUDIBLE_FAST]       = { GGWAVE_PSTR("Fast"),         40,  6, 3, 1, true, };
                protocols.data[GGWAVE_PROTOCOL_AUDIBLE_FASTEST]    = { GGWAVE_PSTR("Fastest"),      40,  3, 3, 1, true, };
                protocols.data[GGWAVE_PROTOCOL_ULTRASOUND_NORMAL]  = { GGWAVE_PSTR("[U] Normal"),   320, 9, 3, 1, true, };
                protocols.data[GGWAVE_PROTOCOL_ULTRASOUND_FAST]    = { GGWAVE_PSTR("[U] Fast"),     320, 6, 3, 1, true, };
                protocols.data[GGWAVE_PROTOCOL_ULTRASOUND_FASTEST] = { GGWAVE_PSTR("[U] Fastest"),  320, 3, 3, 1, true, };
#endif
                protocols.data[GGWAVE_PROTOCOL_DT_NORMAL]          = { GGWAVE_PSTR("[DT] Normal"),  24,  9, 1, 1, true, };
                protocols.data[GGWAVE_PROTOCOL_DT_FAST]            = { GGWAVE_PSTR("[DT] Fast"),    24,  6, 1, 1, true, };
                protocols.data[GGWAVE_PROTOCOL_DT_FASTEST]         = { GGWAVE_PSTR("[DT] Fastest"), 24,  3, 1, 1, true, };
                protocols.data[GGWAVE_PROTOCOL_MT_NORMAL]          = { GGWAVE_PSTR("[MT] Normal"),  24,  9, 1, 2, true, };
                protocols.data[GGWAVE_PROTOCOL_MT_FAST]            = { GGWAVE_PSTR("[MT] Fast"),    24,  6, 1, 2, true, };
                protocols.data[GGWAVE_PROTOCOL_MT_FASTEST]         = { GGWAVE_PSTR("[MT] Fastest"), 24,  3, 1, 2, true, };

#undef GGWAVE_PSTR
                initialized = true;
            }

            return protocols;
        }

        static TxProtocols & tx();
        static RxProtocols & rx();
    };

    using Tone = int8_t;

    // Tone data structure
    //
    //   Each Tone element is the bin index of the tone frequency.
    //   For protocol p:
    //     - freq_hz = (p.freqStart + Tone) * hzPerSample
    //     - duration_ms = p.txDuration_ms(samplesPerFrame, sampleRate)
    //
    //   If the protocol is mono-tone, each element of the vector corresponds to a single tone.
    //   Otherwise, the tones within a single Tx are separated by value of -1
    //
    using Tones = ggvector<Tone>;

    using Amplitude    = ggvector<float>;
    using AmplitudeArr = ggmatrix<float>;
    using AmplitudeI16 = ggvector<int16_t>;
    using Spectrum     = ggvector<float>;
    using RecordedData = ggvector<float>;
    using TxRxData     = ggvector<uint8_t>;

    // Default constructor
    //
    //   The GGWave object is not ready to use until you call prepare()
    //   No memory is allocated with this constructor.
    //
    GGWave() = default;

    // Constructor with parameters
    //
    //  Construct and prepare the GGWave object using the given parameters.
    //  This constructor calls prepare() for you.
    //
    GGWave(const Parameters & parameters);

    ~GGWave();

    // Prepare the GGWave object
    //
    //   All memory buffers used by the GGWave instance are allocated with this function.
    //   No memory allocations occur after that.
    //
    //   Call this method if you used the default constructor.
    //   Do not call this method if you used the constructor with parameters.
    //
    //   The encode() and decode() methods will not work until this method is called.
    //
    //   The sizes of the buffers are determined by the parameters and the contents of:
    //
    //     - GGWave::Protocols::rx()
    //     - GGWave::Protocols::tx()
    //
    //   For optimal performance and minimum memory usage, make sure to enable only the
    //   Rx and Tx protocols that you need.
    //
    //   For example, using a single protocol for Tx is achieved like this:
    //
    //     Parameters parameters;
    //     parameters.operatingMode = GGWAVE_OPERATING_MODE_TX;
    //     GGWave::Protocols::tx().only(GGWave::ProtocolId::GGWAVE_PROTOCOL_AUDIBLE_NORMAL);
    //     GGWave instance(parameters);
    //     instance.init(...);
    //     instance.encode();
    //
    //   The created instance will only be able to transmit data using the "Normal"
    //   protocol. Rx will be disabled.
    //
    //   To create a corresponding Rx-only instance, use the following:
    //
    //     Parameters parameters;
    //     parameters.operatingMode = GGWAVE_OPERATING_MODE_RX;
    //     GGWave::Protocols::rx().only(GGWave::ProtocolId::GGWAVE_PROTOCOL_AUDIBLE_NORMAL);
    //     GGWave instance(parameters);
    //     instance.decode(...);
    //
    //   If "allocate" is false, the memory buffers are not allocated and only the required size
    //   is computed. This is useful if you want to just see how much memory is needed for the
    //   specific set of parameters and protocols. Do not use this function after you have already
    //   prepared the instance. Instead, use the heapSize() method to see how much memory is used.
    //
    bool prepare(const Parameters & parameters, bool allocate = true);

    // Set file stream for the internal ggwave logging
    //
    //   By default, ggwave prints internal log messages to stderr.
    //   To disable logging all together, call this method with nullptr.
    //
    //   Note: not thread-safe. Do not call while any GGWave instances are running
    //
    static void setLogFile(FILE * fptr);

    static const Parameters & getDefaultParameters();

    // Set Tx data to encode into sound
    //
    //   This prepares the GGWave instance for transmission.
    //   To perform the actual encoding, call the encode() method.
    //
    //   Returns false upon invalid parameters or failure to initialize the transmission
    //
    bool init(const char * text, TxProtocolId protocolId, const int volume = kDefaultVolume);
    bool init(int dataSize, const char * dataBuffer, TxProtocolId protocolId, const int volume = kDefaultVolume);

    // Expected waveform size of the encoded Tx data in bytes
    //
    //   When the output sampling rate is not equal to operating sample rate the result of this method is overestimation
    //   of the actual number of bytes that would be produced
    //
    uint32_t encodeSize_bytes() const;

    // Expected waveform size of the encoded Tx data in samples
    //
    //   When the output sampling rate is not equal to operating sample rate the result of this method is overestimation
    //   of the actual number of samples that would be produced
    //
    uint32_t encodeSize_samples() const;

    // Encode Tx data into an audio waveform
    //
    //   After calling this method, use the Tx methods to get the encoded audio data.
    //
    //   The generated waveform is available through the txWaveform() method
    //   The tone frequencies are available through the txTones() method
    //
    //   Returns the number of bytes in the generated waveform
    //
    uint32_t encode();

    // Decode an audio waveform
    //
    //   data   - pointer to the waveform data
    //   nBytes - number of bytes in the waveform
    //
    //   The samples pointed to by "data" should be in the format given by sampleFormatInp().
    //   After calling this method, use the Rx methods to check if any data was decoded successfully.
    //
    //   Returns false if the provided waveform is somehow invalid
    //
    bool decode(const void * data, uint32_t nBytes);

    //
    // Instance state
    //

    bool isDSSEnabled() const;

    int samplesPerFrame() const;
    int sampleSizeInp()   const;
    int sampleSizeOut()   const;

    float hzPerSample()   const;
    float sampleRateInp() const;
    float sampleRateOut() const;
    SampleFormat sampleFormatInp() const;
    SampleFormat sampleFormatOut() const;

    int heapSize() const;

    //
    // Tx
    //

    // Get the generated Wavform samples for the last encode() call
    //
    //   Call this method after calling encode() to get the generated waveform. The format of the samples pointed to by
    //   the returned pointer is determined by the sampleFormatOut() method.
    //
    const void * txWaveform() const;

    // Get a list of the tones generated for the last encode() call
    //
    //   Call this method after calling encode() to get a list of the tones participating in the generated waveform
    //
    const Tones txTones() const;

    // true if there is data pending to be transmitted
    bool txHasData() const;

    // Consume the amplitude data from the last generated waveform
    bool txTakeAmplitudeI16(AmplitudeI16 & dst);

    // The instance will allow Tx only with these protocols. They are determined upon construction or when calling the
    // prepare() method, base on the contents of the global GGWave::Protocols::tx()
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

    // The instance will attempt to decode only these protocols.
    // They are determined upon construction or when calling the prepare() method, base on the contents of the global
    // GGWave::Protocols::rx()
    //
    // Note: do not enable protocols that were not enabled upon preparation of the GGWave instance, or the decoding
    // will likely crash
    //
    RxProtocols & rxProtocols();

    // Information about last received data
    int                  rxDataLength() const;
    const TxRxData &     rxData()       const;
    const RxProtocol &   rxProtocol()   const;
    const RxProtocolId & rxProtocolId() const;
    const Spectrum &     rxSpectrum()   const;
    const Amplitude &    rxAmplitude()  const;

    // Consume the received data
    //
    //   Returns the data length in bytes
    //
    int rxTakeData(TxRxData & dst);

    // Consume the received spectrum / amplitude data
    //
    //   Returns true if there was new data available
    //
    bool rxTakeSpectrum(Spectrum & dst);
    bool rxTakeAmplitude(Amplitude & dst);

    //
    // Utils
    //

    // Compute FFT of real values
    //
    //   src - input real-valued data, size is N
    //   dst - output complex-valued data, size is 2*N
    //
    //   N must be == samplesPerFrame()
    //
    bool computeFFTR(const float * src, float * dst, int N);

    // Compute FFT of real values (static)
    //
    //   src - input real-valued data, size is N
    //   dst - output complex-valued data, size is 2*N
    //   wi  - work buffer, with size 2*N
    //   wf  - work buffer, with size 3 + sqrt(N/2)
    //
    //   First time calling this function, make sure that wi[0] == 0
    //   This will initialize some internal coefficients and store them in wi and wf for
    //   future usage.
    //
    //   If wi == nullptr                   - returns the needed size for wi
    //   If wi != nullptr and wf == nullptr - returns the needed size for wf
    //   If wi != nullptr and wf != nullptr - returns 1 on success, 0 on failure
    //
    static int computeFFTR(const float * src, float * dst, int N, int * wi, float * wf);

    // Filter the waveform
    //
    //   filter   - filter to use
    //   waveform - input waveform, size is N
    //   N        - number of samples in the waveform
    //   p0       - parameter
    //   p1       - parameter
    //   w        - work buffer
    //
    //   Filter is applied in-place.
    //   First time calling this function, make sure that w[0] == 0 and w[1] == 0
    //   This will initialize some internal coefficients and store them in w for
    //   future usage.
    //
    //   For GGWAVE_FILTER_FIRST_ORDER_HIGH_PASS:
    //     - p0 = cutoff frequency in Hz
    //     - p1 = sample rate in Hz
    //
    //   If w == nullptr - returns the needed size for w for the specified filter
    //   If w != nullptr - returns 1 on success, 0 on failure
    //
    static int filter(ggwave_Filter filter, float * waveform, int N, float p0, float p1, float * w);

    // Resample audio waveforms from one sample rate to another using sinc interpolation
    class Resampler {
    public:
        // this controls the number of neighboring samples
        // which are used to interpolate the new samples. The
        // processing time is linearly related to this width
        static const int kWidth = 64;

        Resampler();

        bool alloc(void * p, int & n);

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

        ggvector<float> m_sincTable;
        ggvector<float> m_delayBuffer;
        ggvector<float> m_edgeSamples;
        ggvector<float> m_samplesInp;

        struct State {
            int nSamplesTotal = 0;
            int timeInt       = 0;
            int timeLast      = 0;
            double timeNow    = 0.0;
        };

        State m_state;
    };

private:
    bool alloc(void * p, int & n);

    void decode_fixed();
    void decode_variable();

    int maxFramesPerTx(const Protocols & protocols, bool excludeMT) const;
    int minBytesPerTx(const Protocols & protocols) const;
    int maxBytesPerTx(const Protocols & protocols) const;
    int maxTonesPerTx(const Protocols & protocols) const;
    int minFreqStart(const Protocols & protocols) const;

    double bitFreq(const Protocol & p, int bit) const;

    // Initialized via prepare()
    float        m_sampleRateInp        = -1.0f;
    float        m_sampleRateOut        = -1.0f;
    float        m_sampleRate           = -1.0f;
    int          m_samplesPerFrame      = -1;
    float        m_isamplesPerFrame     = -1.0f;
    int          m_sampleSizeInp        = -1;
    int          m_sampleSizeOut        = -1;
    SampleFormat m_sampleFormatInp      = GGWAVE_SAMPLE_FORMAT_UNDEFINED;
    SampleFormat m_sampleFormatOut      = GGWAVE_SAMPLE_FORMAT_UNDEFINED;

    float        m_hzPerSample          = -1.0f;
    float        m_ihzPerSample         = -1.0f;

    int          m_freqDelta_bin        = -1;
    float        m_freqDelta_hz         = -1.0f;

    int          m_nBitsInMarker        = -1;
    int          m_nMarkerFrames        = -1;
    int          m_encodedDataOffset    = -1;

    float        m_soundMarkerThreshold = -1.0f;

    bool         m_isFixedPayloadLength = false;
    int          m_payloadLength        = -1;

    bool         m_isRxEnabled          = false;
    bool         m_isTxEnabled          = false;
    bool         m_needResampling       = false;
    bool         m_txOnlyTones          = false;
    bool         m_isDSSEnabled         = false;

    // Common
    TxRxData m_dataEncoded;
    TxRxData m_workRSLength; // Reed-Solomon work buffers
    TxRxData m_workRSData;

    // Impl

    struct Rx {
        bool receiving = false;
        bool analyzing = false;

        int nMarkersSuccess     = 0;
        int markerFreqStart     = 0;
        int recvDuration_frames = 0;
        int minFreqStart        = 0;

        int framesLeftToAnalyze = 0;
        int framesLeftToRecord  = 0;
        int framesToAnalyze     = 0;
        int framesToRecord      = 0;
        int samplesNeeded       = 0;

        ggvector<float> fftOut; // complex
        ggvector<int>   fftWorkI;
        ggvector<float> fftWorkF;

        bool hasNewRxData    = false;
        bool hasNewSpectrum  = false;
        bool hasNewAmplitude = false;

        Spectrum  spectrum;
        Amplitude amplitude;
        Amplitude amplitudeResampled;
        TxRxData  amplitudeTmp;

        int dataLength = 0;

        TxRxData     data;
        RxProtocol   protocol;
        RxProtocolId protocolId;
        RxProtocols  protocols;

        // variable-length decoding
        int historyId = 0;

        Amplitude    amplitudeAverage;
        AmplitudeArr amplitudeHistory;
        RecordedData amplitudeRecorded;

        // fixed-length decoding
        int historyIdFixed = 0;

        ggmatrix<uint8_t> spectrumHistoryFixed;
        ggvector<uint8_t> detectedBins;
        ggvector<uint8_t> detectedTones;
    } m_rx;

    struct Tx {
        bool hasData = false;

        float sendVolume = 0.1f;

        int dataLength = 0;
        int lastAmplitudeSize = 0;

        ggvector<bool> dataBits;
        ggvector<double> phaseOffsets;

        AmplitudeArr bit1Amplitude;
        AmplitudeArr bit0Amplitude;

        TxRxData    data;
        TxProtocol  protocol;
        TxProtocols protocols;

        Amplitude    output;
        Amplitude    outputResampled;
        TxRxData     outputTmp;
        AmplitudeI16 outputI16;

        int nTones = 0;
        Tones tones;
    } m_tx;

    mutable Resampler m_resampler;

    void * m_heap  = nullptr;
    int m_heapSize = 0;
};

#endif

#endif
