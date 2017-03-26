/*
 *  FileNVRAM.c
 *  FileNVRAM
 *
 *  Created by Evan Lojewski on 1/11/13.
 *  Modified by Evan Lojewski and Chris Morton on 1/11/13
 *  Copyright (c) 2013-2014 xZenue LLC. All rights reserved.
 *
 *
 * This work is licensed under the
 *  Creative Commons Attribution-NonCommercial 3.0 Unported License.
 *  To view a copy of this license, visit http://creativecommons.org/licenses/by-nc/3.0/.
 */

#include "FileNVRAM_internal.h"
#include "FileNVRAM.h"

#include "libsa.h"
#include "libsaio.h"
#include "bootstruct.h"
#include "modules.h"
#include "xml.h"
#include "string.h"
#include "smbios_getters.h"
#include "convert.h"
#include "boot.h" /* to get gBIOSDev */

#include "kernel_patcher.h"

#if HAS_MKEXT
// File to be embedded
#include <FileNVRAM.mkext.h>
#endif /* HAS_MKEXT */

#if HAS_EMBEDDED_KEXT
// kext executable/Info.plist to be embedded
#include "../../../sym/i386/FileNVRAM/FileNVRAMInfo.plist.h"
#include "../../../sym/i386/FileNVRAM/FileNVRAM.binary.h"
#if HAS_MKEXT
#undef HAS_MKEXT /* not both ..in the event HAS_MKEXT is defined for some reason */
#endif

struct DriverInfo
{
    char *plistAddr;
    long plistLength;
    void *executableAddr;
    long executableLength;
    void *bundlePathAddr;
    long bundlePathLength;
};
typedef struct DriverInfo DriverInfo, *DriverInfoPtr;

static void loadEmbeddedExtension(void* arg1,
                                  void* arg2,
                                  void* arg3,
                                  void* arg4);
#endif /* HAS_EMBEDDED_KEXT */

extern void addBootArg(const char * argStr);

#if HAS_MKEXT
static bool addMKext(void* binary, unsigned long len);
#endif

static void FileNVRAM_hook();

static void processDict(TagPtr tag, Node* node);
static EFI_CHAR8* getSmbiosUUID();
static void InternalreadSMBIOSInfo(SMBEntryPoint *eps);
static BVRef scanforNVRAM(int hdNum);
static void readplist();
static void getcommandline(char* args, char* args_end);
static void clearBootArgsHook();

static char*  gCommandline;
static bool   gOldPath = false;
static BVRef  gbvr;
static TagPtr gPListData;
static TagPtr gNVRAMData;

/********************************************************************/
/**                     Public API Functions                       **/
/********************************************************************/

/**
 ** Lookup the requested NVRAM variable, returning the XML tag if found.
 **/
TagPtr getNVRAMVariable(char* key)
{
    if(gNVRAMData) return XMLGetProperty(gNVRAMData,key);
    return NULL;
}

/**
 ** Add the requested NVRAM tag, replacing any old values.
 **/

void addNVRAMVariable(char* key, TagPtr entry)
{
    if(gNVRAMData)
    {
        removeNVRAMVariable(key);
        XMLAddTagToDictionary(gNVRAMData, key, entry);
    }    
}

/**
 ** Remove the requested NVRAM variable.
 **/
void removeNVRAMVariable(char* key)
{
    if(gNVRAMData)
    {
        // look through dict and find entry, then remove it
        TagPtr tagList, parentTag, tag;
            
        tag = 0;
        parentTag = 0;
        tagList = gNVRAMData->tag;
        while (tagList)
        {
            tag = tagList;
            tagList = tag->tagNext;
            
            if ((tag->type != kTagTypeKey) || (tag->string == 0)) continue;
            
            if (!strcmp(tag->string, key))
            {
                // Entry found

                if(parentTag)
                {
                    // Remove element
                    parentTag->tagNext = tag->tagNext;
                    
                    // free tag
                    tag->tagNext = 0;
                    XMLFreeTag(tag);
                }
                else
                {
                    // First entry, have dict point to second.
                    gNVRAMData->tag = tag->tagNext;
                    tag->tagNext = 0;
                    XMLFreeTag(tag);
                }
                return;
            }
            parentTag = tag;
        }
    }
}



/********************************************************************/
/**                     Initialization code                        **/
/********************************************************************/

void FileNVRAM_start()
{
    register_hook_callback("ModulesLoaded",&readplist);         // delayed Initialization code, runs after all initial modules have loaded.
}



/********************************************************************/
/**                Private internal functions                      **/
/********************************************************************/
static void InternalreadSMBIOSInfo(SMBEntryPoint *eps)
{
    // Copied from readSMBIOSInfo, we need it to run early, so we duplicate + modify here
    
    // Locate teh SMB header in main memory
	uint8_t *structPtr = (uint8_t *)eps->dmi.tableAddress;
	SMBStructHeader *structHeader = (SMBStructHeader *)structPtr;
    
	for (;((eps->dmi.tableAddress + eps->dmi.tableLength) > ((uint32_t)(uint8_t *)structHeader + sizeof(SMBStructHeader)));)
	{
		switch (structHeader->type)
		{
			case kSMBTypeSystemInformation:
                // Read out the platfomr UUID and save it.
				Platform.UUID = ((SMBSystemInformation *)structHeader)->uuid;
                return;
				break;
		}
        
		structPtr = (uint8_t *)((uint32_t)structHeader + structHeader->length);
		for (; ((uint16_t *)structPtr)[0] != 0; structPtr++);
        
		if (((uint16_t *)structPtr)[0] == 0)
			structPtr += 2;
        
		structHeader = (SMBStructHeader *)structPtr;
	}
}

static BVRef scanforNVRAM(int hdNum)
{
    verbose("FileNVRAM.dylib, scanning for nvram file:\n");
    // Locate the nvram.plist file that was modified last.
    
    const char* uuid = getStringFromUUID((const uint8_t *)getSmbiosUUID());
    
    // get hd count
    int deviceCount = 0;
    BVRef chain = newFilteredBVChain(0, hdNum, 0, 0, &deviceCount);
    
    // Locate file w/ newest tiemstamp
    BVRef bvr;
    char  dirSpec[512], fileSpec[512];
    int   ret;
    long  flags;
    u_int32_t time, newestTime;
    BVRef result;
    
    result = NULL;
    char label[128];
    for (bvr = chain; bvr; bvr = bvr->next)
    {
        time = -1; ret = -1;
        verbose("\tscanning hd(%d,%d)/\n", BIOS_DEV_UNIT(bvr), bvr->part_no);
        sprintf(dirSpec, "hd(%d,%d)/", BIOS_DEV_UNIT(bvr), bvr->part_no);
        strcpy(fileSpec, ".nvram.plist");
        ret = GetFileInfo(dirSpec, fileSpec, &flags, &time);
        if (ret == 0)
        {
            if(time > newestTime)
            {
                if(bvr->description)
                {
                    bvr->description(bvr, label, sizeof(label)-1);
                    verbose("\tfound nvram.plist at /Volumes/%s [hd(%d,%d)]\n",
                            label,
                            BIOS_DEV_UNIT(bvr),
                            bvr->part_no);
                }
                
                newestTime = (u_int32_t)time;
                result = bvr;
            }
        }
    }
    // no file? look at the old path (/Extra/)
    if(!result)
    {
        verbose("\tNo nvram file was found, looking inside the Extra folder(s)..\n");
        for (bvr = chain; bvr; bvr = bvr->next)
        {
            time = -1; ret = -1;
            verbose("\tscanning hd(%d,%d)/Extra\n", BIOS_DEV_UNIT(bvr), bvr->part_no);
            sprintf(dirSpec, "hd(%d,%d)/Extra/", BIOS_DEV_UNIT(bvr), bvr->part_no);
            if(!uuid) strcpy(fileSpec, "nvram.plist");
            else sprintf(fileSpec, "nvram.%s.plist", uuid);
            ret = GetFileInfo(dirSpec, fileSpec, &flags, &time);
            if (ret == 0)
            {
                if(time > newestTime)
                {
                    if(bvr->description)
                    {
                        bvr->description(bvr, label, sizeof(label)-1);
                        verbose("\tfound %s at /Volumes/%s/Extra [hd(%d,%d)]\n",
                                fileSpec,
                                label,
                                BIOS_DEV_UNIT(bvr),
                                bvr->part_no);
                    }
                    
                    newestTime = (u_int32_t)time;
                    result = bvr;
                }
            }
        }
        if(result)
        {
            gOldPath = true;
        }
    }
    
    return result;
}

void processDict(TagPtr dictionary, Node * node)
{
    // Handle all NVRAM variables in the nvrma plist.
    int count = 0;
    count = XMLTagCount(dictionary);
    
    while(count)
    {
        const char* key = XMLCastString(XMLGetKey(dictionary,count));
		
        TagPtr entry = XMLGetProperty(dictionary,key);
        
        if(XMLIsData(entry))
        {
            int length = 0;
            int len = 0;
            char *base64 = XMLCastData(entry, &length);
            uint8_t *data = (uint8_t *)BASE64Decode(base64, strlen(base64), &len);
            DT__AddProperty(node, key, len, data);
        }
        else if(XMLIsString(entry))
        {
            char* value = XMLCastString(entry);
            DT__AddProperty(node, key, strlen(value), value);
        }
        else if(XMLIsInteger(entry))
        {
            int value = XMLCastInteger(entry);
            DT__AddProperty(node, key, sizeof(value), &value);
        }
        else if(XMLIsBoolean(entry))
        {
            int value = XMLCastBoolean(entry);
            DT__AddProperty(node, key, sizeof(value), &value);
        }
        else if (XMLIsDict(entry))
        {
            Node * subNode = DT__AddChild(node, key);
            processDict(entry, subNode);
        }
        else
        {
            // Let the user know that the XML function for the given data type
            //  failed, or does not exist in this version of chameleon
            printf("Unable to handle key %s\n", key);
        }
        
        count--;
        
    }
}

/*
 * Get the SystemID from the bios dmi info
 */
static EFI_CHAR8* getSmbiosUUID()
{
	static EFI_CHAR8		 uuid[UUID_LEN];
	int						 i, isZero, isOnes;
	SMBByte					*p;
	
	p = (SMBByte*)Platform.UUID;
	
	for (i=0, isZero=1, isOnes=1; i<UUID_LEN; i++)
	{
		if (p[i] != 0x00) isZero = 0;
		if (p[i] != 0xff) isOnes = 0;
	}
	
	if (isZero || isOnes) // empty or setable means: no uuid present
	{
		verbose("No UUID present in SMBIOS System Information Table\n");
		return 0;
	}
	
	memcpy(uuid, p, UUID_LEN);
	return uuid;
}

static void clearBootArgsHook()
{
    // Chameleon boot args have been reset (boot failed or first time), populate with nvram var.
    if(gNVRAMData)
    {
        int length;
        char* value = 0;
        TagPtr entry = getNVRAMVariable("boot-args");
        if(XMLIsString(entry))
        {
            value = XMLCastString(entry);
        }
        else if(XMLIsData(entry))
        {
            value = XMLCastData(entry, &length);
        }
        if(entry) addBootArg(value);

    }

}

static void getcommandline(char* args, char* args_end)
{
    // Remove previously save arguments
    if(gCommandline)
    {
        free(gCommandline);
        gCommandline = 0;
    }
    
    // Grab the current boot args from the user and save it
    if(strlen(args))
    {
        gCommandline = malloc(strlen(args)+1);
        strcpy(gCommandline, args);
    }
}

// helper function for xml.c
static char * strdelchar(char *buffer, char c)
{
    // please use a null terminated buffer
    if(buffer == NULL)
    {
        return NULL;
    }
    if(!strlen(buffer))
    {
        return buffer;
    }
    char *cleaned = buffer, *f = buffer;
    
    for(; *buffer != '\0'; ++buffer)
    {
        if(*buffer != c)
        {
            *f++ = *buffer;
        }
        
    }
    *f = '\0';
    
    return cleaned;
}

/**
 ** Main initialization code.
 ** Find and read out the nvram variables.
 **/
static void readplist()
{
#if HAS_MKEXT
    /* We need to patch the kernel to load up an mkext in the event that the kernel is prelinked. */
    if(!is_module_loaded("KernelPatcher.dylib")) register_hook_callback("DecodeKernel", &patch_kernel);
#endif

#if HAS_EMBEDDED_KEXT
    /* let chameleon to load this kext */
    register_hook_callback("DriversLoaded", &loadEmbeddedExtension);
#endif
    
    // We need the platform UUID *early*
    InternalreadSMBIOSInfo(getSmbios(SMBIOS_ORIGINAL));
    const char* uuid = getStringFromUUID((const uint8_t *)getSmbiosUUID());
    
    // By the time we are here, the file system has already been probed, lets fine the nvram plist.
    int bvCount = 0;
    scanDisks(gBIOSDev, &bvCount);
    gbvr = scanforNVRAM(bvCount);
    
    /** Load Dictionary if possible **/
    if(gbvr)
    {
        char* nvramPath = NULL;
        if (gOldPath)
        {
            nvramPath = malloc(strlen("hd(%d,%d)/Extra/nvram.plist") + (uuid ? (strlen(uuid) +2) : 0) +1);
            if(!uuid) sprintf(nvramPath, "hd(%d,%d)/Extra/nvram.plist", BIOS_DEV_UNIT(gbvr), gbvr->part_no);
            else sprintf(nvramPath, "hd(%d,%d)/Extra/nvram.%s.plist", BIOS_DEV_UNIT(gbvr), gbvr->part_no, uuid);
        }
        else
        {
            nvramPath = malloc(strlen("hd(%d,%d)//.nvram.plist") + 1);
            sprintf(nvramPath, "hd(%d,%d)/.nvram.plist", BIOS_DEV_UNIT(gbvr), gbvr->part_no);
        }
        
        verbose("\tloading %s: ", nvramPath);
        
        int fh = open(nvramPath, 0);
        if(fh >= 0)
        {
            unsigned int plistSize = file_size(fh);
            if (plistSize > 0)
            {
                char* plistBase = (char*) malloc(plistSize);
                
                if (plistSize && read(fh, plistBase, plistSize) == plistSize)
                {
                    // cleaning nvram.plist from \n and \t.
                    // data tag have those bytes after the serialization:
                    //
                    //  <key>SystemAudioVolume</key>
                    //  <data>
                    //      fw==
                    //  </data>
                    //
                    // but I want it to be: <data>fw==</data>
                    //
                    // so some garbage ends up in /chosen/nvram. just remove them!
                    XMLParseFile(strdelchar(strdelchar(plistBase, '\n'), '\t'), &gPListData);
                    if(gPListData)
                    {
                        gNVRAMData = XMLCastDict(XMLGetProperty(gPListData,"NVRAM"));
                        verbose("success.\n");
                    }
                    else
                    {
                        verbose("error parsing file.\n");
                    }
                }
                else
                {
                    verbose("reading failed.\n");
                }
            }
            else
            {
                verbose("empty file.\n");
            }
        }
        else
        {
            verbose("failed to open file.\n");
        }
        free(nvramPath);
        
    }
    // hook anyway. We supposed that if no .nvram.plist was found is because that does not exist,
    // but let treat the command line arguments
    register_hook_callback("DriversLoaded",&FileNVRAM_hook);    // Main code, runs when kernel has begun booting.
    register_hook_callback("BootOptions", (void (*)(void *, void *, void *, void *)) &getcommandline);     // Code executed every time the boot options / command line is used.
    register_hook_callback("ClearArgs", &clearBootArgsHook);    // Code executed every time the boot arguments are cleared out.
}


void FileNVRAM_hook()
{
    bool disable = false;
    getBoolForKey(BOOT_KEY_NVRAM_DISABLED, &disable, &bootInfo->chameleonConfig);
    if(disable) return;
    
    const char* uuid = getStringFromUUID((const uint8_t *)getSmbiosUUID());
    
    Node * nvramNode = DT__FindNode("/chosen/nvram", true);
    Node * settingsNode = DT__AddChild(nvramNode, FILE_NVRAM_GULD);
    
    if(gCommandline)
    {
        DT__AddProperty(nvramNode, "boot-args", strlen(gCommandline)+1, (void*)gCommandline);
    }
    else
    {
        // Ensure boot-args is zero'd out.
        char* null = "";
        DT__AddProperty(nvramNode, "boot-args", strlen(null)+1, (void*)null);
        removeNVRAMVariable("boot-args");
    }
    
    if(gNVRAMData)
    {
        processDict(gNVRAMData, nvramNode);
    }
    
    char* path = NULL;
    
    char label[128];
    if(gbvr && gbvr->description)
    {
        gbvr->description(gbvr, label, sizeof(label)-1);
        
        if (gOldPath)
        {
            path = malloc(strlen("/Volumes/%s/Extra/nvram.plist") +
                          strlen(label) +
                          (uuid ? (strlen(uuid) +2) : 0)
                          +1);
            
            if(uuid) sprintf(path, "/Volumes/%s/Extra/nvram.%s.plist", label, uuid);
            else sprintf(path, "/Volumes/%s/Extra/nvram.plist", label);
        }
        else
        {
            path = malloc(strlen("/Volumes//.nvram.plist") + strlen(label) +1);
            sprintf(path, "/Volumes/%s/.nvram.plist", label);
        }
        
        DT__AddProperty(settingsNode, NVRAM_SET_FILE_PATH, strlen(path), path);
    }
    else
    {
        // FileNVRAM.kext will anyway set a default path..
    }
    
#if HAS_MKEXT /* executed only if HAS_EMBEDDED_KEXT is not defined */
    addMKext(FileNVRAM_mkext, FileNVRAM_mkext_len);
#endif
    
#if HAS_EMBEDDED_KEXT
    loadEmbeddedExtension(NULL,
                          NULL,
                          NULL,
                          NULL);
#endif
}

#if HAS_MKEXT
static bool addMKext(void* binary, unsigned long len)
{
    char             segName[32];
    unsigned long    driversAddr, driversLength;


    // Thin binary, if needed
    ThinFatFile(&binary, &len);
    
    DriversPackage * package = binary;
    
    // Verify the MKext.
    if (( GetPackageElement(signature1) != kDriverPackageSignature1) ||
        ( GetPackageElement(signature2) != kDriverPackageSignature2) ||
        ( GetPackageElement(length)      > kLoadSize )               ||
        ( GetPackageElement(adler32)    !=
         Adler32((unsigned char *)&package->version, GetPackageElement(length) - 0x10) ) )
    {
        return false;
    }
    
    // Make space for the MKext.
    driversLength = GetPackageElement(length);
    driversAddr   = AllocateKernelMemory(driversLength);
    
    // Copy the MKext.
    memcpy((void *)driversAddr, (void *)package, driversLength);
    
    // Add the MKext to the memory map.
    sprintf(segName, "DriversPackage-%lx", driversAddr);
    AllocateMemoryRange(segName, driversAddr, driversLength,
                        kBootDriverTypeMKEXT);
    
    return true;
}
#endif

#if HAS_EMBEDDED_KEXT
/*
 Note: FileNVRAM has the OSBundleRequired set to "Root", to be tested it in Safe Boot..
 */
void loadEmbeddedExtension(void* arg1,
                           void* arg2,
                           void* arg3,
                           void* arg4)
{
    TagPtr plistPtr, prop;
    DriverInfoPtr driver;
    char segName[32];
    long driverAddr, driverLength, fakeBundlePathLength;
    char *bundlePath = NULL, *executableName;
    
    void *executableAddr            = (void *)FileNVRAM_binary;
    void *plistAddr                 = (void *)FileNVRAMInfo_plist;
    unsigned long  executableLength = (unsigned long)FileNVRAM_binary_len;
    long plistLength                = (long)FileNVRAMInfo_plist_len;
    
    ThinFatFile(&executableAddr, &executableLength);
    
    XMLParseFile(plistAddr, &plistPtr);
    
    prop = XMLGetProperty(plistPtr, kPropCFBundleExecutable);
    
    if(prop != 0)
    {
        executableName = prop->string;
        fakeBundlePathLength = strlen("/System/Library/Extensions/.kext") + strlen(executableName) +1;
        bundlePath = malloc(fakeBundlePathLength);
        snprintf(bundlePath, fakeBundlePathLength, "%s/%s.kext", "/System/Library/Extensions", executableName);
        
        driverLength = sizeof(DriverInfo) + plistLength + executableLength + fakeBundlePathLength;
        driverAddr = AllocateKernelMemory(driverLength);
        
        // Set up the DriverInfo.
        driver = (DriverInfoPtr)driverAddr;
        driver->executableLength = executableLength;
        driver->plistAddr = (char *)(driverAddr + sizeof(DriverInfo));
        driver->plistLength = plistLength;
        
        driver->executableAddr = (void *)(driverAddr + sizeof(DriverInfo) + plistLength);
        driver->executableLength = executableLength;
        
        driver->bundlePathAddr = (void *)(driverAddr + sizeof(DriverInfo) +
                                          plistLength + driver->executableLength);
        
        driver->bundlePathLength = fakeBundlePathLength;
        
        // Save the plist and bundle.
        strlcpy(driver->plistAddr, plistAddr, driver->plistLength);
        
        memcpy(driver->executableAddr, executableAddr, executableLength);
        
        strlcpy(driver->bundlePathAddr, bundlePath, fakeBundlePathLength);
        
        // Add an entry to the memory map.
        snprintf(segName, sizeof(segName), "Driver-%lx", (unsigned long)driver);
        AllocateMemoryRange(segName, driverAddr, driverLength, kBootDriverTypeKEXT);
    }
    
    if(bundlePath) free(bundlePath);
}
#endif
