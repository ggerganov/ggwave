cdef extern from "ggwave.h" nogil:

     int ggwave_encode(const char * dataBuffer, int dataSize, char * outputBuffer);
