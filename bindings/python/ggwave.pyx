cimport cython

from cpython.mem cimport PyMem_Malloc, PyMem_Free

import re

cimport cggwave

def getDefaultParameters():
    return cggwave.ggwave_getDefaultParameters()

def init(parameters = None):
    if (parameters is None):
        parameters = getDefaultParameters()

    return cggwave.ggwave_init(parameters)

def free(instance):
    return cggwave.ggwave_free(instance)

def encode(payload, txProtocolId = 1, volume = 10, instance = None):
    """ Encode payload into an audio waveform.
        @param {string} payload, the data to be encoded
        @return Generated audio waveform bytes representing 16-bit signed integer samples.
    """

    cdef bytes data_bytes = payload.encode()
    cdef char* cdata = data_bytes

    own = False
    if (instance is None):
        own = True
        instance = init(getDefaultParameters())

    n = cggwave.ggwave_encode(instance, cdata, len(data_bytes), txProtocolId, volume, NULL, 1)

    cdef bytes output_bytes = bytes(n)
    cdef char* coutput = output_bytes

    n = cggwave.ggwave_encode(instance, cdata, len(data_bytes), txProtocolId, volume, coutput, 0)

    if (own):
        free(instance)

    return output_bytes

def decode(instance, waveform):
    """ Analyze and decode audio waveform to obtain original payload
        @param {bytes} waveform, the audio waveform to decode
        @return The decoded payload if successful.
    """

    cdef bytes data_bytes = waveform
    cdef char* cdata = data_bytes

    cdef bytes output_bytes = bytes(256)
    cdef char* coutput = output_bytes

    rxDataLength = cggwave.ggwave_decode(instance, cdata, len(data_bytes), coutput)

    if (rxDataLength > 0):
        return coutput[0:rxDataLength]

    return None
