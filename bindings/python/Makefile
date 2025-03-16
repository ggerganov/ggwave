default: build

PKG_NAME := ggwave
DEST := dist/
VER := $(shell cat setup.py | grep version |cut -d\"  -f2)
VERSION := $(strip $(VER))
ORIG_TGZ := $(PKG_NAME)_$(VERSION).orig.tar.gz
ARCH := $(shell dpkg --print-architecture)
DEB_BUILD_OPTIONS := nocheck
DEB_BUILD_DIR := $(DEST)$(PKG_NAME)-$(VERSION)
DEB_VER := 0
DEB := python3-$(PKG_NAME)_$(VERSION)-$(DEB_VER)_$(ARCH).deb
SOURCE_DATE_EPOCH := $(shell git log -1 --pretty=%ct)
SOURCE_PKG_CMD := python -m build --sdist
DEB_BUILD_CMD := dpkg-buildpackage -rfakeroot -uc -us
COGAPP ?= $(shell which cog || which cogapp)

.PHONY:
ggwave:
	# create a clean (maybe updated) copy of ggwave src
	rm -rf ggwave && mkdir ggwave && cp -r ../../include ggwave/ && cp -r ../../src ggwave/

deps:
	echo python -m pip install cogapp cython twine

pyggwave.bycython.cpp: ggwave.pyx cggwave.pxd
	cython --cleanup=3 --cplus ggwave.pyx -o ggwave.bycython.cpp

# To build package, README.rst is needed, because it goes into long description of package,
# which is what is visible on PyPI.
# However, to generate README.rst from README-tmpl.rst, built package is needed (for `import ggwave` in cog)!
# Therefore, we first build package without README.rst, use it to generate README.rst,
# and then finally build package again but with README.rst.

BUILD_SOURCE_FILES=ggwave pyggwave.bycython.cpp setup.py

$(DEST)$(DEB): $(DEST)$(ORIG_TGZ)
	-rm -r $(DEB_BUILD_DIR)
	tar -C $(DEST) -xf $(DEST)$(ORIG_TGZ)
	cp -arp debian $(DEB_BUILD_DIR)
	cd $(DEB_BUILD_DIR) && \
	DEB_BUILD_OPTIONS=$(DEB_BUILD_OPTIONS) $(DEB_BUILD_CMD)

deb_sdist: $(BUILD_SOURCE_FILES) README.rst
	SOURCE_DATE_EPOCH=$(SOURCE_DATE_EPOCH) $(SOURCE_PKG_CMD)
	mv $(DEST)$(PKG_NAME)-$(VERSION).tar.gz $(DEST)$(ORIG_TGZ)

_deb:
	@$(eval COGAPP=cogapp)

deb: _deb deb_sdist $(DEST)$(DEB)
	dpkg --contents $(DEST)$(DEB)
	ls -alh $(DEST)*$(PKG_NAME)*
	sha256sum $(DEST)$(DEB)

buildWithoutREADME.rst: ${BUILD_SOURCE_FILES}
	GGWAVE_OMIT_README_RST=1 python setup.py build_ext -i

README.rst: buildWithoutREADME.rst README-tmpl.rst
	$(COGAPP) -d -o README.rst README-tmpl.rst

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
