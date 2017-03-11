//
//  FileNVRAM.h
//  FileNVRAM
//
//  Created by Chris Morton on 1/11/13.
//  Copyright (c) 2013-2014 xZenue LLC. All rights reserved.
//
// This work is licensed under the
//  Creative Commons Attribution-NonCommercial 3.0 Unported License.
//  To view a copy of this license, visit http://creativecommons.org/licenses/by-nc/3.0/.
//
#ifndef FileNVRAM_FileNVRAM_h
#define FileNVRAM_FileNVRAM_h

#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/vnode.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/fcntl.h>
#include <libkern/libkern.h>

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

/* This is needed for Info.plist to work properly */
#define FileNVRAM AppleNVRAM

#define HEADER      __FILE__ "[" TOSTRING(__LINE__) "]: "

#define DISABLED    0
#define ERROR       1
#define INFO        2
#define NOTICE      3

#define LOG(__level__, x...)           \
do {                        \
    if(mLoggingLevel >= __level__)     \
    {                       \
        char pname[256];        \
        proc_name(proc_pid(vfs_context_proc(vfs_context_current())), pname, sizeof(pname)); \
        printf("Process: %s, ", pname);                                                     \
        printf( HEADER x);  \
    }                       \
} while(0)

#ifndef kIONVRAMDeletePropertyKey
#define kIONVRAMDeletePropertyKey	"IONVRAM-DELETE-PROPERTY"
#endif
#ifndef kIONVRAMSyncNowPropertyKey
#define kIONVRAMSyncNowPropertyKey	"IONVRAM-SYNCNOW-PROPERTY"
#endif
#ifndef kIONVRAMForceSyncNowPropertyKey
#define kIONVRAMForceSyncNowPropertyKey		"IONVRAM-FORCESYNCNOW-PROPERTY"
#endif
#ifndef kIODTNVRAMPanicInfoKey
#define kIODTNVRAMPanicInfoKey	 "aapl,panic-info"
#endif

#ifndef kIONVRAMPrivilege
#define kIONVRAMPrivilege	kIOClientPrivilegeAdministrator
#endif

#define private public
#include <IOKit/IONVRAM.h>
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/IOService.h>
#include <IOKit/IOCommandGate.h>
#include <IOKit/IOTimerEventSource.h>


#define APPLE_MLB_KEY           "4D1EDE05-38C7-4A6A-9CC6-4BCCA8B38C14:MLB"
#define APPLE_ROM_KEY           "4D1EDE05-38C7-4A6A-9CC6-4BCCA8B38C14:ROM"

#define FILE_NVRAM_GUID         "D8F0CCF5-580E-4334-87B6-9FBBB831271D"

#define NVRAM_ENABLE_LOG        "-EnableLogging"
#define BOOT_KEY_NVRAM_DISABLED "-NoFileNVRAM"
#define BOOT_KEY_NVRAM_RDONLY   "-FileNVRAMro"
#define NVRAM_SET_FILE_PATH     "NVRAMFile"
#define FILE_NVRAM_PATH			"/Extra/nvram.plist"

#define NVRAM_SEPERATOR         ":"
#define NVRAM_FILE_DT_LOCATION	"/chosen/nvram"
#define NVRAM_FILE_HEADER		"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n" \
                                "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"\
                                "\t<plist version=\"1.0\">\n<dict>\n<key>NVRAM</key>\n"
#define NVRAM_FILE_FOOTER       "</dict></plist>\n"

#define NVRAM_MISS_KEY			"NVRAM_MISS"
#define NVRAM_MISS_HEADER       "\n<key>NVRAM_MISS</key>\n"


#define kNVRAMSyncCommand   1
#define kNVRAMSetProperty   2
#define kNVRAMGetProperty   4

#define super IODTNVRAM


#if __cplusplus < 201103L
/* Define override for backwards compat pre c++11 compilers. */
#define override
#endif

class FileNVRAM : public IODTNVRAM
{
    OSDeclareDefaultStructors(FileNVRAM);
    
public:
    virtual bool	start(IOService *provider) override;
    virtual void	stop(IOService *provider) override;
    
    virtual bool    passiveMatch (OSDictionary *matching, bool changesOK);
    
    virtual void    copyEntryProperties(const char* prefix, IORegistryEntry* entry);
    virtual void    copyUnserialzedData(const char* prefix, OSDictionary* dict);
    
    virtual IOReturn syncOFVariables(void) override;
    virtual bool init(IORegistryEntry *old, const IORegistryPlane *plane) override;
    
    virtual void registerNVRAMController(IONVRAMController *nvram) override;
    
    virtual void sync(void) override;
    virtual void doSync(void);
    
    virtual bool serializeProperties(OSSerialize *s) const override;
    
    virtual OSObject *getProperty(const OSSymbol *aKey) const override;
    virtual OSObject *copyProperty(const OSSymbol *aKey) const override;
    virtual OSObject *getProperty(const char *aKey) const override;
    virtual OSObject *copyProperty(const char *aKey) const override;
    
    virtual bool setProperty(const OSSymbol *aKey, OSObject *anObject) override;
    virtual void removeProperty(const OSSymbol *aKey) override;
    virtual IOReturn setProperties(OSObject *properties) override;
    
    virtual IOReturn readXPRAM(IOByteCount offset, UInt8 *buffer,
                               IOByteCount length) override;
    virtual IOReturn writeXPRAM(IOByteCount offset, UInt8 *buffer,
                                IOByteCount length) override;
    
    virtual IOReturn readNVRAMProperty(IORegistryEntry *entry,
                                       const OSSymbol **name,
                                       OSData **value) override;
    virtual IOReturn writeNVRAMProperty(IORegistryEntry *entry,
                                        const OSSymbol *name,
                                        OSData *value) override;
    
    virtual OSDictionary *getNVRAMPartitions(void) override;
    
    virtual IOReturn readNVRAMPartition(const OSSymbol *partitionID,
                                        IOByteCount offset, UInt8 *buffer,
                                        IOByteCount length) override;
    
    virtual IOReturn writeNVRAMPartition(const OSSymbol *partitionID,
                                         IOByteCount offset, UInt8 *buffer,
                                         IOByteCount length) override;
    
    virtual IOByteCount savePanicInfo(UInt8 *buffer, IOByteCount length) override;
    virtual bool safeToSync(void) override;
    
    IOReturn setPowerState ( unsigned long whichState, IOService * whatDevice ) override;
    
    
private:
    static void timeoutOccurred(OSObject *target, IOTimerEventSource* timer);
    
    virtual void registerNVRAM();
    
    virtual void setPath(OSString* path);
    
    virtual OSObject* cast(const OSSymbol* key, OSObject* obj);
    
    static IOReturn dispatchCommand( OSObject* owner, void* arg0, void* arg1, void* arg2, void* arg3 );
    
    virtual IOReturn write_buffer(char* buffer);
    virtual IOReturn read_buffer(char** buffer, uint64_t* length);
    
    bool mReadOnly;
    bool mInitComplete;
    bool mSafeToSync;
    UInt8 mLoggingLevel;
    
    vfs_context_t mCtx;
    
    OSDictionary * mNvramMissDict;
    IOCommandGate* mCommandGate;
    OSString*      mFilePath;
    IOTimerEventSource* mTimer;
};

#if __cplusplus < 201103L
/* Undefine override so it can be used as usual. */
#undef override
#endif



#endif /* FileNVRAM_FileNVRAM_h */
