MODULE_NAME = FileNVRAM
MODULE_AUTHOR = xZenue LLC.
MODULE_DESCRIPTION = FileNVRAM module for preloading NVRAM values
MODULE_VERSION = "1.1.4"
MODULE_COMPAT_VERSION = "1.0.0"
MODULE_START = $(MODULE_NAME)_start
MODULE_DEPENDENCIES = Chameleon

DIR = FileNVRAM
MKEXT = ../obj/FileNVRAM.mkext

MODULE_OBJS   = FileNVRAM.x86.mach.o kernel_patcher.x86.mach.o

${OBJROOT}/FileNVRAM.x86.mach.o: ${MKEXT}.h

HAS_EMBEDED:=$(shell test -f ${MKEXT} && (echo "true"; touch ${MKEXT}; (cd ../obj && xxd -i ${notdir ${MKEXT}} > ${abspath ${MKEXT}.h})))

override SYMROOT=../sym
override OBJROOT=obj
override BOOT_ELF=

include chameleon/i386/modules/MakeInc.dir

INC += -I../obj/

ifeq (${HAS_EMBEDED},true)
DEFINES +=-DHAS_MKEXT ${DEFINES}
else
DEFINES +=-DNO_MKEXT ${DEFINES}
endif




${MKEXT}.h: ${MKEXT}
	xxd -i ${MKEXT} > $@

dst:

