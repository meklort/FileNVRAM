#
# Makefile for FileNVRAM
#

all: build/Release/FileNVRAM.kext

dst: ${DSTROOT}/FileNVRAM.mkext


build/Release/FileNVRAM.kext:
	@xcodebuild

${DSTROOT}/FileNVRAM.mkext: build/Release/FileNVRAM.kext
	echo "[KEXTCACHE] $@"
	@kextcache -mkext1 $@ $<

distclean:

clean:
	@xcodebuild clean
