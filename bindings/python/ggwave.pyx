cimport cython
from cpython.mem cimport PyMem_Malloc, PyMem_Free

import re
import struct

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

    cdef bytes output_bytes = bytes(1024*1024)
    cdef char* coutput = output_bytes

    own = False
    if (instance is None):
        own = True
        instance = init(getDefaultParameters())

    n = cggwave.ggwave_encode(instance, cdata, len(data_bytes), txProtocolId, volume, coutput)

    if (own):
        free(instance)

    # add short silence at the end
    n += 16*1024

    return struct.unpack("h"*n, output_bytes[0:2*n])

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
