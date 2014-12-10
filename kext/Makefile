#
# Makefile for FileNVRAM
#

all: build/Release/FileNVRAM.kext

dst: ${DSTROOT}/FileNVRAM.mkext


build/Release/FileNVRAM.kext:
	@xcodebuild

${DSTROOT}/FileNVRAM.mkext: build/Release/FileNVRAM.kext
	@echo "[KEXTCACHE] $@"
	@kextcache -mkext1 $@ $<

distclean:
	@echo "[RM] ${DSTROOT}/FileNVRAM.mkext"
	rm -rf ${DSTROOT}/FileNVRAM.mkext

clean:
	@xcodebuild clean
