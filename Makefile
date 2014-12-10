#
# Makefile for FileNVRAM
#
PRODUCT=FileNVRAM

ROOT = $(abspath $(CURDIR))
SRCROOT = ${ROOT}/src
SDKROOT = ${ROOT}/sdk
OBJROOT = $(ROOT)/obj
SYMROOT = $(ROOT)/sym
DSTROOT = $(ROOT)/dst
DOCROOT = $(ROOT)/doc

SUBDIRS = kext 

#module
module: kext

.PHONY: ${SUBDIRS}

all clean distclean: ${SUBDIRS}

test:

.PHONY: dst
dst: ${DSTROOT} ${SUBDIRS}
	@echo "[DST] ${PRODUCT}.${MAJOR}.${MINOR}.${REVISION}.tgz"

#sdk: ${SUBDIRS} 
#	@echo "[DOXYGEN] html"
#	@${DOXYGEN} Doxyfile > /dev/null
#	@#echo "[DOXYGEN] docset"
#	@#make -C ${SDKROOT}/docs/html &> /dev/null
#	@find ${SDKROOT} -type d -empty -delete
#	@#rm -rf ${SDKROOT}/docs/*.docset
#	@#mv ${SDKROOT}/docs/html/*.docset ${SDKROOT}/docs
#	@#rm ${SDKROOT}/docs/html/*.xml ${SDKROOT}/docs/html/*.plist ${SDKROOT}/docs/html/Makefile


${SUBDIRS}: ${SYMROOT}
	@echo ================= make $@ ================
	@${MAKE} -r -R -C "$@" ${MAKECMDGOALS}	\
		DSTROOT='${DSTROOT}'		\
		SYMROOT='${SYMROOT}'		\
		DOCROOT='${DOCROOT}'
	


${DSTROOT} ${SYMROOT}:
	@echo "[MKDIR] $@"
	@mkdir -p $@
