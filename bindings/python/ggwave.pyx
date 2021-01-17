cimport cython
from cpython.mem cimport PyMem_Malloc, PyMem_Free
import re

cimport cggwave

def ggwaveTest():
    cggwave.testC()
    return 0
