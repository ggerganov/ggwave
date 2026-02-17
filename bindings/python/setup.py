from setuptools import setup, Extension
from codecs import open
import os
import sys
import shutil

cmdclass = {}
long_description = ""

# Build directly from cython source file(s) if user wants so (probably for some experiments).
# Otherwise, pre-generated c source file(s) are used.
# User has to set environment variable GGWAVE_USE_CYTHON.
# e.g.: GGWAVE_USE_CYTHON=1 python setup.py install
USE_CYTHON = os.getenv('GGWAVE_USE_CYTHON', False)
if not os.path.exists("ggwave.bycython.cpp"):
    USE_CYTHON = True

if USE_CYTHON:
    from Cython.Build import build_ext
    ggwave_module_src = "ggwave.pyx"
    cmdclass['build_ext'] = build_ext
else:
    ggwave_module_src = "ggwave.bycython.cpp"

# Load README.rst into long description.
# User can skip using README.rst as long description: GGWAVE_OMIT_README_RST=1 python setup.py install
OMIT_README_RST = os.getenv('GGWAVE_OMIT_README_RST', False)
if not OMIT_README_RST:
    here = os.path.abspath(os.path.dirname(__file__))
    readme_rst_path = os.path.join(here, 'README.rst')
    if os.path.exists(readme_rst_path):
        with open(readme_rst_path, encoding='utf-8') as f:
            long_description = f.read()
    else:
        print("WARNING: README.rst undefined")

# Prepare sources
if not os.path.exists("ggwave"):
    os.makedirs("ggwave")
    
if not os.path.exists("ggwave/src"):
    if os.path.exists("../../src"):
        shutil.copytree("../../src", "ggwave/src")
    else:
        print("WARNING: ../../src undefined")

if not os.path.exists("ggwave/include"):
    if os.path.exists("../../include"):
        shutil.copytree("../../include", "ggwave/include")
    else:
        print("WARNING: ../../include undefined")

# Compile args
compile_args = ["-O3", "-std=c++11"]
if sys.platform == 'win32':
    compile_args = ["/O2"]

setup(
    # Information
    name = "ggwave",
    description = "Tiny data-over-sound library.",
    long_description = long_description,
    version = "0.4.2",
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
                             extra_compile_args=compile_args)],
    cmdclass = cmdclass
)
