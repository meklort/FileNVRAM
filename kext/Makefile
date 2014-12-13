#
# Makefile for FileNVRAM (kext)
#

all: ${OBJROOT}/FileNVRAM.mkext

dst:

.PHONY: build/Release/FileNVRAM.kext
build/Release/FileNVRAM.kext:
	@xcodebuild

${OBJROOT}/FileNVRAM.mkext: build/Release/FileNVRAM.kext
	@echo "[KEXTCACHE] $@"
	@kextcache -mkext1 $@ $<

distclean:
	@echo "[RM] ${OBJROOT}/FileNVRAM.mkext"
	@rm -rf ${OBJROOT}/FileNVRAM.mkext

clean:
	@xcodebuild clean
