#
# Makefile for FileNVRAM
#

all: ${SYMROOT}/FileNVRAM.mkext

dst:

.PHONY: build/Release/FileNVRAM.kext
build/Release/FileNVRAM.kext:
	@xcodebuild

${SYMROOT}/FileNVRAM.mkext: build/Release/FileNVRAM.kext
	@echo "[KEXTCACHE] $@"
	@kextcache -mkext1 $@ $<

distclean:
	@echo "[RM] ${SYMROOT}/FileNVRAM.mkext"
	@rm -rf ${SYMROOT}/FileNVRAM.mkext

clean:
	@xcodebuild clean
