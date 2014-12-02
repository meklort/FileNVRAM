//
//  FileIO.c
//  FileNVRAM
//
//  Created by Evan Lojewski on 1/13/13.
//  Copyright (c) 2013 xZenue LLC. All rights reserved.
//
// This work is licensed under the
//  Creative Commons Attribution-NonCommercial 3.0 Unported License.
//  To view a copy of this license, visit http://creativecommons.org/licenses/by-nc/3.0/.
//

#include "FileIO.h"

int write_buffer(const char* path, char* buffer, int length, vfs_context_t ctx)
{
    
    int error;
    struct vnode * vp;
    
    if ((error = vnode_open(path, (O_TRUNC | O_CREAT | FWRITE | O_NOFOLLOW), S_IRUSR, VNODE_LOOKUP_NOFOLLOW, &vp, ctx)))
    {
        printf("failed opening vnode at path %s, errno %d\n",path, error);
        return error;
    }
    
    if ((error = vn_rdwr(UIO_WRITE, vp, buffer, length, 0, UIO_SYSSPACE, IO_NOCACHE|IO_NODELOCKED|IO_UNIT, vfs_context_ucred(ctx), (int *) 0, vfs_context_proc(ctx))))
    {
        printf("Error writing to vnode at path %s, errno %d\n",path,error);
    }
	
    if ((error = vnode_close(vp, FWASWRITTEN, ctx)))
    {
        printf("Error closing vnode errno %d\n",error);
    }
    
    
    return 0;
}

#if 0 /* Disabled, has not been tested and probably doesn't work. */

int read_buffer(const char* path, char* buffer, int length, vfs_context_t ctx)
{
    int error;
    struct vnode * vp;
    
    if ((error = vnode_open(path, (O_RDONLY | FREAD | O_NOFOLLOW), S_IRUSR, VNODE_LOOKUP_NOFOLLOW, &vp, ctx)))
    {
        printf("failed opening vnode at path %s, errno %d\n",path, error);
        return error;
    }
    
    if ((error = vn_rdwr(UIO_READ, vp, buffer, length, 0, UIO_SYSSPACE, IO_NOCACHE|IO_NODELOCKED|IO_UNIT, vfs_context_ucred(ctx), (int *) 0, vfs_context_proc(ctx))))
    {
        printf("Error writing to vnode at path %s, errno %d\n",path,error);
    }
    
    if ((error = vnode_close(vp, FWASWRITTEN, ctx)))
    {
        printf("Error closing vnode errno %d\n",error);
    }
    
    return 0;
}

#endif