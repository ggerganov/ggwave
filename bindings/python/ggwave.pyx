cimport cython
from cpython.mem cimport PyMem_Malloc, PyMem_Free

import re
import struct

cimport cggwave

def parameters():
    return cggwave.ggwave_defaultParameters()

def init(parameters):
    return <object>cggwave.ggwave_init(parameters)

def free(instance):
    return cggwave.ggwave_free(<void *>instance)

def encode(data, txProtocol = 1, volume = 10):
    cdef bytes data_bytes = data.encode()
    cdef char* cdata = data_bytes

    cdef bytes output_bytes = bytes(1024*1024)
    cdef char* coutput = output_bytes

    instance = init(parameters())
    n = cggwave.ggwave_encode(<void *>instance, cdata, len(data_bytes), txProtocol, volume, coutput)
    free(instance)

    # add short silence at the end
    n += 16*1024

    return struct.unpack("h"*n, output_bytes[0:2*n])
