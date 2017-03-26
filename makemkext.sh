#!/bin/bash

mydir="$(cd "$(dirname "$BASH_SOURCE")"; pwd)"
EXT_PATH="$(dirname $(dirname $(dirname "${mydir}")))/sym/i386/FileNVRAM"

if [[ ! -d "${EXT_PATH}/FileNVRAM.kext" ]]; then
    exit 1
fi

chmod -R 755 "${EXT_PATH}"/FileNVRAM.kext
chown -R root:wheel "${EXT_PATH}"/FileNVRAM.kext

kextcache -mkext1  "${EXT_PATH}"/FileNVRAM.mkext "${EXT_PATH}"

# allow r/w permissions recursively (to clean the project)
chmod -R 777 "${EXT_PATH}"

cd "${EXT_PATH}"
xxd -i FileNVRAM.mkext > FileNVRAM.mkext.h

