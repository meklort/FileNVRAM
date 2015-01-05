/*
 * Copyright (c) 2009-2012 Evan Lojewski. All rights reserved.
 * Copyright (c) 2013 xZenue LLC. All rights reserved.
 *
 *
 * This work is licensed under the 
 *  Creative Commons Attribution-NonCommercial 3.0 Unported License. 
 *  To view a copy of this license, visit http://creativecommons.org/licenses/by-nc/3.0/.
 */

#include "libsaio.h"
#include "kernel_patcher.h"
#include "modules.h"
#include "sl.h"
#include <libsaio/bootstruct.h>


/****************************************** Defines ******************************************/

#define KERNEL_ANY	0x00
#define KERNEL_64	0x01
#define KERNEL_32	0x02
#define KERNEL_ERR	0xFF

/***************************************** Datatypes *****************************************/

typedef struct section_t
{
	const char* segment;
    const char* section;
    UInt64 address;
    UInt64 offset;
	struct section_t* next;
} section_t;

/***************************************** Functions *****************************************/

/** section_handler: used by the macho loader to notify us of a section **/
static void             section_handler(char* base, char* new_base, char* section, char* segment, void* cmd, UInt64 offset, UInt64 address);

/** Determine the type of kernel being loaded / patched **/
static int              determineKernelArchitecture(void* kernelData);
static section_t*       lookup_section(const char* segment, const char* section);
static symbolList_t*    lookup_kernel_symbol(const char* name);
static void             register_section(const char* segment, const char* section);
static void             patch_readStartupExtensions(void* kernelData);


/***************************************** Variable *****************************************/

static section_t*       kernelSections = NULL;  /** Linked list holding memory sections **/
static symbolList_t*    kernelSymbols = NULL;   /** List of kernel funciton addresses **/



/**
 ** Perform all steps needed to patch the specified kernel
 **/
void patch_kernel(void* kernelData, void* arg2, void* arg3, void *arg4)
{
    extern symbolList_t* moduleSymbols;
	int arch = determineKernelArchitecture(kernelData);
	
	if(arch == KERNEL_ERR)
	{
        /* Something went wrong, we aren't on x86? */
		return;
	}
    
    // Use the symbol handler from the module system. This requires us to backup the moduleSymbols variable and restore it once complete
    symbolList_t* origmoduleSymbols = moduleSymbols;
    moduleSymbols = NULL;   // clear out list of symbols
    
    /* Watch for the following sections while being parsed*/
    register_section("__KLD", "__text");
    register_section("__TEXT","__text");
    
	parse_mach(kernelData, NULL, NULL, &add_symbol, &section_handler);
    kernelSymbols = moduleSymbols;    // save symbols for future use, if needed
    moduleSymbols = origmoduleSymbols; // restore orig pointer;
    
    /** Perform patches **/
    patch_readStartupExtensions(kernelData);
    
}

static symbolList_t* lookup_kernel_symbol(const char* name)
{
    /* Locate the specified kernel symbol in the list */
	symbolList_t *symbol = kernelSymbols;
    
	while(symbol && strcmp(symbol->symbol, name) !=0)
	{
		symbol = symbol->next;
	}
	
    /* Return the found symbol, or NULL */
    return symbol;
}


/**
 ** Update the specified section in the section list, if it exists
 **/
static section_t* lookup_section(const char* segment, const char* section)
{    
	if(!segment || !section) return NULL;
	
    section_t* sections = kernelSections;
    
    /* Locate the first section with the given names */
	while(sections && 
		  !(strcmp(sections->segment, segment) == 0 &&
		    strcmp(sections->section, section) == 0))
	{
		sections = sections->next;
	}
	
    /* Return the section found, or NULL */
	return sections;
}

/**
 ** Record the given section in the section list
 **/
void register_section(const char* segment, const char* section)
{
    if(kernelSections == NULL)
	{
        /* First Entry */
		kernelSections = malloc(sizeof(section_t));
		kernelSections->next = NULL;
        
        kernelSections->segment = segment;
        kernelSections->section = section;
        kernelSections->address = 0;
        kernelSections->offset = 0;
	}
	else
    {
		section_t *sect = kernelSections;
		while(sect->next != NULL) sect = sect->next;
		
		sect->next = malloc(sizeof(section_t));
		sect = sect->next;
        
        sect->segment = segment;
        sect->section = section;
        sect->address = 0;
        sect->offset = 0;        
    }
}

/**
 ** Read the macho header and determine if it's a valid x86 32 or 64bit image.
 **/
static int determineKernelArchitecture(void* kernelData)
{
    switch(((struct mach_header*)kernelData)->magic)
    {
        case MH_MAGIC:
            return KERNEL_32;
            
        case MH_MAGIC_64:
            return KERNEL_64;

        default:
            return KERNEL_ERR;
    }
}

/**
 ** Handle callbacks from the macho parser, recording addresses if previously requested
 **/
static void section_handler(char* base, char* new_base, char* section, char* segment, void* cmd, UInt64 offset, UInt64 address)
{	
    // Find the section in our list of registered / requested sections to watch...
    section_t *kernelSection = lookup_section(segment, section);
	
    if(kernelSection)
    {
        // And save the address
		kernelSection->address = address;
        kernelSection->offset = offset;
    }
}

/**
 ** Locate the last instance of _OSKextLog inside of __ZN12KLDBootstrap23readPrelinkedExtensionsEP7section
 ** and replace it with __ZN12KLDBootstrap20readBooterExtensionsEv so that mkexts can be loaded with prelinked kernels.
 **/
void patch_readStartupExtensions(void* kernelData)
{    
    UInt8* bytes = (UInt8*)kernelData;
    bool is64bit = (determineKernelArchitecture(kernelData) == KERNEL_64);
        
    symbolList_t* getsegbyname              = lookup_kernel_symbol("_getsegbyname");
    symbolList_t* readBooterExtensions      = lookup_kernel_symbol("__ZN12KLDBootstrap20readBooterExtensionsEv");
    symbolList_t* readPrelinkedExtensions   = is64bit ?
                lookup_kernel_symbol("__ZN12KLDBootstrap23readPrelinkedExtensionsEP10section_64") : //64bit
                lookup_kernel_symbol("__ZN12KLDBootstrap23readPrelinkedExtensionsEP7section");      //32bit
    
    if(!readPrelinkedExtensions)
    {
		return;
    }
	
    symbolList_t* OSKextLog  = lookup_kernel_symbol("_OSKextLog");
    
    section_t* __KLD = lookup_section("__KLD","__text");
    
    
    
	UInt32 readBooterExtensionsLocation     = readBooterExtensions    ? readBooterExtensions->addr    - __KLD->address + __KLD->offset: 0;
	UInt32 readPrelinkedExtensionsLocation  = readPrelinkedExtensions ? readPrelinkedExtensions->addr - __KLD->address + __KLD->offset : 0;
	UInt32 OSKextLogLocation                = OSKextLog               ? OSKextLog->addr               - __KLD->address + __KLD->offset : 0;
	UInt32 getsegbynameLocation             = getsegbyname            ? getsegbyname->addr            - __KLD->address + __KLD->offset : 0;
    
    
    
    // Step 1: Locate the First _OSKextLog call inside of __ZN12KLDBootstrap23readPrelinkedExtensionsEP10section
    UInt32 patchLocation = readPrelinkedExtensionsLocation - (UInt32)kernelData;
    OSKextLogLocation -= (UInt32)kernelData;
    readBooterExtensionsLocation -= (UInt32)kernelData;
	getsegbynameLocation -= (UInt32)kernelData;
    //printf("Starting at 0x%X\n", readPrelinkedExtensions->addr - (UInt32)kernelData + __KLD->offset - __KLD->address);
    //printf("Starting at 0x%X\n", patchLocation + __KLD->address - __KLD->offset);
    //printf("Starting at 0x%X\n", __KLD->address - __KLD->offset);
    
    
    
	while(
		  (bytes[patchLocation -1] != 0xE8) ||
		  ( ( (UInt32)(getsegbynameLocation - patchLocation  - 4) ) != (UInt32)((bytes[patchLocation + 0] << 0  |
                                                                                 bytes[patchLocation + 1] << 8  |
                                                                                 bytes[patchLocation + 2] << 16 |
                                                                                 bytes[patchLocation + 3] << 24)))
		  )
	{
		patchLocation++;
	}
    patchLocation++;
    
    
    // Second one...
    while(
		  (bytes[patchLocation -1] != 0xE8) ||
		  ( ( (UInt32)(getsegbynameLocation - patchLocation  - 4) ) != (UInt32)((bytes[patchLocation + 0] << 0  |
                                                                                 bytes[patchLocation + 1] << 8  |
                                                                                 bytes[patchLocation + 2] << 16 |
                                                                                 bytes[patchLocation + 3] << 24)))
		  )
	{
		patchLocation++;
	}
    patchLocation++;
    
    
    //printf("patchLocation at 0x%X\n", patchLocation - __KLD->offset + __KLD->address);
    
    while(
		  (bytes[patchLocation -1] != 0xE8) ||
		  ( ( (UInt32)(OSKextLogLocation - patchLocation  - 4) ) != (UInt32)((bytes[patchLocation + 0] << 0  |
                                                                              bytes[patchLocation + 1] << 8  |
                                                                              bytes[patchLocation + 2] << 16 |
                                                                              bytes[patchLocation + 3] << 24)))
		  )
	{
		patchLocation--;
	}
    //printf("patchLocation at 0x%X\n", patchLocation - __KLD->offset + __KLD->address);
    
    
    // Step 2: remove the _OSKextLog call, this call takes the form:
    // 00886a73	movl	$0x00887508,0x08(%esp)
    // 00886a7b	movl	$0x00010084,0x04(%esp)
    // 00886a83	movl	$0x00000000,(%esp)
    // 00886a8a	calll	0x006060c0
    // 00886a8f
    // This is a total of 28 bytes
    int i = 0;
    
    if(is64bit)
    {
        // TODO: Calculate size programaticaly
        patchLocation -= 0x14; // 64bit
        for(i = 0; i < 0x10; i++) bytes[++patchLocation] = 0x90;
        
    }
    else
    {
        patchLocation -= 0x19; // 32bit
        for(i = 0; i < 0x15; i++) bytes[++patchLocation] = 0x90;
        
    }
    
    
    
    // Step 3: Add a call to __ZN12KLDBootstrap21readStartupExtensionsEv.
    // This takes the form of
    // 00886ea6	movl	%esi,(%esp)
    // 00886ea9	calll	0x00886716
    // 00886eae
	
	if(!is64bit)
	{
		// 32bit
		bytes[patchLocation] = 0x89;
		bytes[++patchLocation] = 0x34;
		bytes[++patchLocation] = 0x24;
	}
	else
	{
		// 64bit
		// movq	%rbx,%rdi
		bytes[patchLocation] = 0x48;
		bytes[++patchLocation] = 0x89;
		bytes[++patchLocation] = 0xdf;
	}
	++patchLocation;	// 0xe8 -> call
    UInt32 rel = patchLocation+5;
    bytes[++patchLocation] = (readBooterExtensionsLocation - rel) >> 0;
    bytes[++patchLocation] = (readBooterExtensionsLocation - rel) >> 8;
    bytes[++patchLocation] = (readBooterExtensionsLocation - rel) >> 16;
    bytes[++patchLocation] = (readBooterExtensionsLocation - rel) >> 24;
}


// checkOSVersion


