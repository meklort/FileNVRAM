/*
 *  FileNVRAM_internal.h
 *  FileNVRAM
 *
 *  Created by Evan Lojewski on 1/18/13.
 *  Copyright (c) 2013 xZenue LLC. All rights reserved.
 *
 *
 * This work is licensed under the
 *  Creative Commons Attribution-NonCommercial 3.0 Unported License.
 *  To view a copy of this license, visit http://creativecommons.org/licenses/by-nc/3.0/.
 */


#ifndef FILENVRAM_INTERNAL_H
#define FILENVRAM_INTERNAL_H

#define BOOT_KEY_NVRAM_DISABLED		"NoFileNVRAM"

#define FILE_NVRAM_GULD         "D8F0CCF5-580E-4334-87B6-9FBBB831271D"
#define NVRAM_GEN_MLB           "GenerateMLB"
#define NVRAM_GEN_ROM           "GenerateROM"
#define NVRAM_ENABLE_LOG        "EnableLogging"
#define NVRAM_SET_FILE_PATH     "NVRAMFile"

#define GetPackageElement(e)     OSSwapBigToHostInt32(package->e)
#define kDriverPackageSignature1 'MKXT'
#define kDriverPackageSignature2 'MOSX'

struct DriversPackage {
    unsigned long signature1;
    unsigned long signature2;
    unsigned long length;
    unsigned long adler32;
    unsigned long version;
    unsigned long numDrivers;
    unsigned long reserved1;
    unsigned long reserved2;
};
typedef struct DriversPackage DriversPackage;


// We could use the boot2 version.. but meh
static inline unsigned long
Adler32( unsigned char * buffer, long length )
{
    long          cnt;
    unsigned long result, lowHalf, highHalf;
    
    lowHalf  = 1;
    highHalf = 0;
    
	for ( cnt = 0; cnt < length; cnt++ )
    {
        if ((cnt % 5000) == 0)
        {
            lowHalf  %= 65521L;
            highHalf %= 65521L;
        }
        
        lowHalf  += buffer[cnt];
        highHalf += lowHalf;
    }
    
	lowHalf  %= 65521L;
	highHalf %= 65521L;
    
	result = (highHalf << 16) | lowHalf;
    
	return result;
}

#endif /* FILENVRAM_INTERNAL_H */
