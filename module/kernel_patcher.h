/*
 * Copyright (c) 2009-2012 Evan Lojewski. All rights reserved.
 * Copyright (c) 2013 xZenue LLC. All rights reserved.
 *
 *
 * This work is licensed under the
 *  Creative Commons Attribution-NonCommercial 3.0 Unported License.
 *  To view a copy of this license, visit http://creativecommons.org/licenses/by-nc/3.0/.
 */

#ifndef __KERNEL_PATCHER_H
#define __KERNEL_PATCHER_H

#include <libkern/OSTypes.h>
#include <mach-o/loader.h>
#include <mach-o/nlist.h>
#include <mach-o/reloc.h>

#include "modules.h"

void patch_kernel(void* kernelData, void* arg2, void* arg3, void *arg4);

#endif /* !__KERNEL_PATCHER_H */
