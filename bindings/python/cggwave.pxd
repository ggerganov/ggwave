cdef extern from "ggwave.h" nogil:

    ctypedef enum ggwave_SampleFormat:
        GGWAVE_SAMPLE_FORMAT_UNDEFINED,
        GGWAVE_SAMPLE_FORMAT_U8,
        GGWAVE_SAMPLE_FORMAT_I8,
        GGWAVE_SAMPLE_FORMAT_U16,
        GGWAVE_SAMPLE_FORMAT_I16,
        GGWAVE_SAMPLE_FORMAT_F32

    ctypedef enum ggwave_TxProtocolId:
        GGWAVE_TX_PROTOCOL_AUDIBLE_NORMAL,
        GGWAVE_TX_PROTOCOL_AUDIBLE_FAST,
        GGWAVE_TX_PROTOCOL_AUDIBLE_FASTEST,
        GGWAVE_TX_PROTOCOL_ULTRASOUND_NORMAL,
        GGWAVE_TX_PROTOCOL_ULTRASOUND_FAST,
        GGWAVE_TX_PROTOCOL_ULTRASOUND_FASTEST

    ctypedef struct ggwave_Parameters:
        int payloadLength
        float sampleRateInp
        float sampleRateOut
        int samplesPerFrame
        float soundMarkerThreshold
        ggwave_SampleFormat sampleFormatInp
        ggwave_SampleFormat sampleFormatOut

    ctypedef int ggwave_Instance

    ggwave_Parameters ggwave_getDefaultParameters();

    ggwave_Instance ggwave_init(const ggwave_Parameters parameters);

    void ggwave_free(ggwave_Instance instance);

    int ggwave_encode(
            ggwave_Instance instance,
            const char * dataBuffer,
            int dataSize,
            ggwave_TxProtocolId txProtocolId,
            int volume,
            char * outputBuffer,
            int query);

    int ggwave_decode(
            ggwave_Instance instance,
            const char * dataBuffer,
            int dataSize,
            char * outputBuffer);
