cimport cython
from cpython.mem cimport PyMem_Malloc, PyMem_Free

import re
import struct

cimport cggwave

def encode(data):
    cdef bytes data_bytes = data.encode();
    cdef char* cdata = data_bytes;

    cdef bytes output_bytes = bytes(1024*1024)
    cdef char* coutput = output_bytes;

    n = cggwave.ggwave_encode(cdata, len(data_bytes), coutput)

    # add short silence at the end
    n += 16*1024

    return struct.unpack("h"*n, output_bytes[0:2*n]);
