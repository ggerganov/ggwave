from setuptools import setup, Extension
from codecs import open
import os

cmdclass = {}
long_description = ""

# Build directly from cython source file(s) if user wants so (probably for some experiments).
# Otherwise, pre-generated c source file(s) are used.
# User has to set environment variable GGWAVE_USE_CYTHON.
# e.g.: GGWAVE_USE_CYTHON=1 python setup.py install
here = os.path.abspath(os.path.dirname(__file__))
USE_CYTHON = os.getenv('GGWAVE_USE_CYTHON', False)

# Try to find the pre-generated C++ file
ggwave_module_src = os.path.join(here, "ggwave.bycython.cpp")
if not os.path.exists(ggwave_module_src):
    # Fallback to .pyx if the .cpp is missing
    ggwave_module_src = "ggwave.pyx"
    USE_CYTHON = True

if USE_CYTHON:
    try:
        from Cython.Build import build_ext
        cmdclass['build_ext'] = build_ext
    except ImportError:
        if ggwave_module_src.endswith(".pyx"):
            raise ImportError("Cython is required to build from .pyx source. Please install it: pip install cython")

# Load README.rst into long description.
# User can skip using README.rst as long description: GGWAVE_OMIT_README_RST=1 python setup.py install
OMIT_README_RST = os.getenv('GGWAVE_OMIT_README_RST', False)
if not OMIT_README_RST:
    readme_path = os.path.join(here, 'README.rst')
    if os.path.exists(readme_path):
        with open(readme_path, encoding='utf-8') as f:
            long_description = f.read()
    else:
        long_description = "Tiny data-over-sound library."

setup(
    # Information
    name = "ggwave",
    description = "Tiny data-over-sound library.",
    long_description = long_description,
    version = "0.4.3",
    url = "https://github.com/ggerganov/ggwave",
    author = "Georgi Gerganov",
    author_email = "ggerganov@gmail.com",
    license = "MIT",
    keywords = "data-over-sound fsk ecc serverless pairing qrcode ultrasound",
    # Build instructions
    ext_modules = [Extension("ggwave",
                             [ggwave_module_src, "ggwave/src/ggwave.cpp"],
                             include_dirs=["ggwave/include", "ggwave/include/ggwave"],
                             depends=["ggwave/include/ggwave/ggwave.h"],
                             language="c++",
                             extra_compile_args=["-O3", "-std=c++11"])],
    cmdclass = cmdclass
)
