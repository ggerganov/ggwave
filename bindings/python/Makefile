default: build

.PHONY:
ggwave:
	# create a clean (maybe updated) copy of ggwave src
	rm -rf ggwave && mkdir ggwave && cp -r ../../include ggwave/ && cp -r ../../src ggwave/

pyggwave.bycython.cpp: ggwave.pyx cggwave.pxd
	python -m pip install cython
	cython --cplus ggwave.pyx -o ggwave.bycython.cpp

# To build package, README.rst is needed, because it goes into long description of package,
# which is what is visible on PyPI.
# However, to generate README.rst from README-tmpl.rst, built package is needed (for `import ggwave` in cog)!
# Therefore, we first build package without README.rst, use it to generate README.rst,
# and then finally build package again but with README.rst.

BUILD_SOURCE_FILES=ggwave pyggwave.bycython.cpp setup.py

buildWithoutREADME.rst: ${BUILD_SOURCE_FILES}
	GGWAVE_OMIT_README_RST=1 python setup.py build_ext -i

README.rst: buildWithoutREADME.rst README-tmpl.rst
	python -m pip install cogapp
	cog -d -o README.rst README-tmpl.rst

BUILD_FILES=${BUILD_SOURCE_FILES} README.rst

build: ${BUILD_FILES}
	python setup.py build_ext -i

sdist: ggwave pyggwave.bycython.cpp setup.py README.rst MANIFEST.in
	python setup.py sdist

publish: clean sdist
	twine upload --repository pypi dist/*

clean:
	rm -rf ggwave dist ggwave.egg-info build
	rm -f ggwave.c *.bycython.* ggwave.*.so
	rm -f README.rst
