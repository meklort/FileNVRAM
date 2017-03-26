MODULE_NAME = FileNVRAM
MODULE_AUTHOR = xZenue LLC.
MODULE_DESCRIPTION = FileNVRAM module for preloading NVRAM values
MODULE_VERSION = "1.1.5"
MODULE_COMPAT_VERSION = "1.0.0"
MODULE_START = $(MODULE_NAME)_start
MODULE_DEPENDENCIES = Chameleon

# 1 to build the old mkext
USE_MKEXT = 0

# 0 for /Extra/nvram.uuid.plist, 1 to use /.nvram.plist
USE_ROOT_DIR = 1

DIR = FileNVRAM
EXT = ${SYMROOT}/FileNVRAM
MKEXT = ${EXT}/FileNVRAM.mkext

MODULE_OBJS = FileNVRAM.o kernel_patcher.o

${OBJROOT}/FileNVRAM.o: ${MKEXT}.h

include ../MakeInc.dir

ifeq ($(MAKECMDGOALS),all)
    $(shell ${CURDIR}/buildkext ${EXT} ${USE_MKEXT} ${USE_ROOT_DIR} >&2)

ifeq ($(USE_MKEXT),1)
    DEFINES +=-DHAS_MKEXT ${DEFINES}
else
    DEFINES +=-DHAS_EMBEDDED_KEXT ${DEFINES}
endif
endif
