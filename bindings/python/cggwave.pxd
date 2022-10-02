cdef extern from "ggwave.h" nogil:

    ctypedef enum ggwave_SampleFormat:
        GGWAVE_SAMPLE_FORMAT_UNDEFINED,
        GGWAVE_SAMPLE_FORMAT_U8,
        GGWAVE_SAMPLE_FORMAT_I8,
        GGWAVE_SAMPLE_FORMAT_U16,
        GGWAVE_SAMPLE_FORMAT_I16,
        GGWAVE_SAMPLE_FORMAT_F32

    ctypedef enum ggwave_ProtocolId:
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
        GGWAVE_PROTOCOL_CUSTOM_9

    enum:
        GGWAVE_OPERATING_MODE_RX,
        GGWAVE_OPERATING_MODE_TX,
        GGWAVE_OPERATING_MODE_RX_AND_TX,
        GGWAVE_OPERATING_MODE_TX_ONLY_TONES,
        GGWAVE_OPERATING_MODE_USE_DSS

    ctypedef struct ggwave_Parameters:
        int payloadLength
        float sampleRateInp
        float sampleRateOut
        float sampleRate
        int samplesPerFrame
        float soundMarkerThreshold
        ggwave_SampleFormat sampleFormatInp
        ggwave_SampleFormat sampleFormatOut
        int operatingMode

    ctypedef int ggwave_Instance

    ggwave_Parameters ggwave_getDefaultParameters();

    ggwave_Instance ggwave_init(const ggwave_Parameters parameters);

    void ggwave_free(ggwave_Instance instance);

    int ggwave_encode(
            ggwave_Instance instance,
            const void * payloadBuffer,
            int payloadSize,
            ggwave_ProtocolId protocolId,
            int volume,
            void * waveformBuffer,
            int query);

    int ggwave_decode(
            ggwave_Instance instance,
            const void * waveformBuffer,
            int waveformSize,
            void * payloadBuffer);

    void ggwave_setLogFile(void * fptr);

    void ggwave_rxToggleProtocol(
            ggwave_ProtocolId protocolId,
            int state);

    void ggwave_txToggleProtocol(
            ggwave_ProtocolId protocolId,
            int state);
