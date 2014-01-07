//
//  FileNVRAM.c
//  FileNVRAM
//
//  Created by Chris Morton on 1/11/13.
//  Copyright (c) 2013 xZenue LLC. All rights reserved.
//
// This work is licensed under the
//  Creative Commons Attribution-NonCommercial 3.0 Unported License.
//  To view a copy of this license, visit http://creativecommons.org/licenses/by-nc/3.0/.
//


#include "FileNVRAM.h"
#include "Support.h"
#include <IOKit/IOUserClient.h>

/** The cpp file is included here to hide symbol names. **/
#include "Support.cpp"


/** Private Macros **/

/** Private variables **/
#define kIOPMPowerOff 0
#define POWER_STATE_OFF     0
#define POWER_STATE_ON      1
static IOPMPowerState sPowerStates[] = {
    {1, kIOPMPowerOff, kIOPMPowerOff, kIOPMPowerOff, 0, 0, 0, 0, 0, 0, 0, 0},
    {1, kIOPMPowerOn,  kIOPMPowerOn,  kIOPMPowerOn, 0, 0, 0, 0, 0, 0, 0, 0}
};

/** Private Functions **/



OSDefineMetaClassAndStructors(FileNVRAM, IODTNVRAM);


void FileNVRAM::setPath(OSString* path)
{
	OSSafeReleaseNULL(mFilePath);
    path->retain();
    LOG("Setting path to %s\n", path->getCStringNoCopy());
    mFilePath = path;
}

bool FileNVRAM::start(IOService *provider)
{
    LOG("start() called (%d)\n", mInitComplete);

    //start is called upon wake for some reason.
    if(mInitComplete)           return true;
    if(!super::start(provider)) return false;

    LOG("start() called (%d)\n", mInitComplete);

	mFilePath		= NULL;			// no know file
    mLoggingEnabled = false;        // start with logging disabled, can be update for debug
    mInitComplete   = false;        // Don't resync anything that's already in the file system.
	mSafeToSync     = false;        // Don't sync untill later

	// set a default file path
    //setPath(OSString::withCString("/Extra/nvram.plist"));
    
    IORegistryEntry* bootnvram = IORegistryEntry::fromPath(NVRAM_FILE_DT_LOCATION, gIODTPlane);
    IORegistryEntry* root = IORegistryEntry::fromPath("/", gIODTPlane);

    // Create the command gate.
    mCommandGate = IOCommandGate::commandGate( this, dispatchCommand );
	getWorkLoop()->addEventSource( mCommandGate );    
    
    // Replace the IOService dicionary with an empty one, clean out variables we don't want.
    OSDictionary* dict = OSDictionary::withCapacity(1);
    if (!dict) return false;
    setPropertyTable(dict);
        
    
    copyEntryProperties(NULL, bootnvram);
    if(bootnvram) bootnvram->detachFromParent(root, gIODTPlane);

    /* Do we need to generate MLB? */
    if(!getProperty(APPLE_MLB_KEY))
    {
        OSString* str = OSString::withCString(NVRAM_GEN_MLB);
        handleSetting(str, kOSBooleanTrue, this);
        str->release();
    }
    
    /* Do we need to generate ROM? */
    if(!getProperty(APPLE_ROM_KEY))
    {
        OSString* str = OSString::withCString(NVRAM_GEN_ROM);
        handleSetting(str, kOSBooleanTrue, this);
        str->release();
    }
    
    IOTimerEventSource* mTimer = IOTimerEventSource::timerEventSource(this, OSMemberFunctionCast(IOTimerEventSource::Action, this, &FileNVRAM::timeoutOccurred));

    getWorkLoop()->addEventSource( mTimer);
    //mTimer->setTimeoutMS(10); // callback isn't being setup right, causes a panic
    mInitComplete = true;
    
    PMinit();
    registerPowerDriver(this, sPowerStates, sizeof(sPowerStates)/sizeof(IOPMPowerState));
    provider->joinPMtree(this);

    // We should be root right now... cache this for later. 
    mCtx = vfs_context_current();
	mSafeToSync = true;
    
    // Create entry in device tree -> IODeviceTree:/options
    setName("AppleEFINVRAM");
    setName("options", gIODTPlane);
    attachToParent(root, gIODTPlane);
    registerService();
    
    // Register with the platform expert
    const OSSymbol* funcSym = OSSymbol::withCString("RegisterNVRAM");
    if(funcSym)
    {
        callPlatformFunction(funcSym, false, this, NULL, NULL, NULL);
        funcSym->release();
    }

    return true;
}

void FileNVRAM::stop(IOService *provider)
{
    OSSafeReleaseNULL(mFilePath);
    
    if(mTimer)
    {
        mTimer->cancelTimeout();
        getWorkLoop()->removeEventSource(mTimer);
        mTimer->release();
    }
    
    if(mCommandGate)
    {
        getWorkLoop()->removeEventSource(mCommandGate);
    }
    
    PMstop();
	LOG("Stop called, attempting to detachFromParent\n");
	
    IORegistryEntry* root = IORegistryEntry::fromPath("/", gIODTPlane);
	detachFromParent(root, gIODTPlane);
	
	LOG("Stop has passed the detach point.. move along now\n");
	
}


void FileNVRAM::copyEntryProperties(const char* prefix, IORegistryEntry* entry)
{
    IORegistryEntry* child;
    OSDictionary* properties;
    OSCollectionIterator *iter;
    
    if(entry)
    {
        // Parse all IORegistery Children
        OSIterator * iterator = entry->getChildIterator(gIODTPlane);
        
        if(iterator)
        {
            while((child = OSDynamicCast(IORegistryEntry, iterator->getNextObject())) != NULL)
            {
                const char* name = child->getName();
                
                if(prefix)
                {
                    // This is a special FileNVRAM guid for settings
                    if(strcmp(FILE_NVRAM_GUID, prefix) == 0)
                    {
                        OSString* key = OSString::withCString(child->getName());
                        handleSetting(key, kOSBooleanTrue, this);
                        key->release();
                    }
                    else
                    {
                        size_t size = strlen(prefix) + sizeof(NVRAM_SEPERATOR) + strlen(name);
                        char* newPrefix = (char*)IOMalloc(size);
                        
                        snprintf(newPrefix, size, "%s%s%s", prefix, NVRAM_SEPERATOR, name);
                        copyEntryProperties(newPrefix, child);
                        
                        IOFree(newPrefix, size);
                    }
                }
                else
                {
                    copyEntryProperties(name, child);
                }
            }
            iterator->release();
        }
        
        // Parse entry properties and add them to our self
        properties = entry->dictionaryWithProperties();
        
        bool                 result = true;
        OSObject             *object;
        const OSSymbol       *key;
        
        
        iter = OSCollectionIterator::withCollection(properties);
        if (iter == 0) return;
        
        while (result) {
            
            key = OSDynamicCast(OSSymbol, iter->getNextObject());
            if (key == 0) break;
            
            if(key->isEqualTo("name")) continue; // Special property in IORegistery, ignore
            
            object = properties->getObject(key);
            if (object == 0) continue;
            
            if(prefix)
            {
                if(strcmp(FILE_NVRAM_GUID, prefix) == 0)
                {
                    handleSetting(key, object, this);
                }
                else
                {
                    size_t size = strlen(prefix) + sizeof(NVRAM_SEPERATOR) + strlen(key->getCStringNoCopy());
                    char* newKey = (char*)IOMalloc(size);
                    
                    snprintf(newKey, size, "%s%s%s", prefix, NVRAM_SEPERATOR, key->getCStringNoCopy());
                    
                    setProperty(OSSymbol::withCString(newKey), object);
                    
                    IOFree(newKey, size);
                }
            }
            else
            {
                setProperty(key, object);
            }
        }
        
        iter->release();
    }
}



bool FileNVRAM::init(IORegistryEntry *old, const IORegistryPlane *plane)
{
    LOG("init(%p, %p) called\n", old, plane);
    return IOService::init(old,plane);
}


bool FileNVRAM::passiveMatch (OSDictionary *matching, bool changesOK)
{
    OSString *str = OSDynamicCast (OSString, matching->getObject
                                   (gIOProviderClassKey));
    
    if(str) LOG("passiveMatch(%s) called\n", str->getCStringNoCopy());
    
    if (str && str->isEqualTo ("AppleEFINVRAM")) return true;
    return super::passiveMatch (matching, changesOK);
}

IOReturn FileNVRAM::syncOFVariables(void)
{
    LOG("syncOFVariables() called\n");
    return kIOReturnSuccess;
}

void FileNVRAM::registerNVRAMController(IONVRAMController *nvram)
{
    LOG("registerNVRAMController(%p) called\n", nvram);
}

void FileNVRAM::sync(void)
{
    LOG("sync() called\n");
	mCommandGate->runCommand( ( void * ) kNVRAMSyncCommand, NULL, NULL, NULL );
}

void FileNVRAM::doSync(void)
{
    LOG("doSync() called\n");
    
    if(!mFilePath) return;
    if(!mSafeToSync) return;
    
    LOG("doSync() running\n");

	//create the output Dictionary
	OSDictionary * outputDict = OSDictionary::withCapacity(1);
	
	//going to have to spin over ourselves..
	OSDictionary * inputDict = dictionaryWithProperties();
	OSCollectionIterator *iter = OSCollectionIterator::withCollection(inputDict);
	
	if (iter == 0)
	{
		LOG("FAILURE!. No iterator on input dictionary (myself)\n");
		return;
	}
	
	OSSymbol * key = NULL;
	OSObject * value = NULL;
    
	while( (key = OSDynamicCast(OSSymbol,iter->getNextObject())))
	{
		
		//just get the value now anyway
		value = inputDict->getObject(key);
		
		//if the key conmektains :, look to see if it's in the map already, cause we'll add a child pair to it
		//otherwise we just slam the key/val pair in
		
		const char * keyChar = key->getCStringNoCopy();
		const char * guidValueStr = NULL;
		if( ( guidValueStr = strstr(keyChar , NVRAM_SEPERATOR)) != NULL)
		{
			//we have a GUID child to deal with
			//now substring out the GUID cause thats going to be a DICT itself on the new outputDict
			//guidValueStr points to the :
			size_t guidCutOff = guidValueStr - keyChar;
			
			//allocate buffer
			//we ar ereally accounting for + sizeof('\0')
			//thats always 1. so 1.
			char guidStr[guidCutOff+1];
			strlcpy(guidStr, keyChar, guidCutOff+1);
			
			//in theory we have a guid and a value
			//LOG("sync() -> Located GUIDStr as %s\n",guidStr);
        
			//check for ?OSDictionary? from the dictionary
			OSDictionary * guidDict = OSDynamicCast(OSDictionary, outputDict->getObject(guidStr));
			if(!guidDict)
			{
				guidDict = OSDictionary::withCapacity(1);
				outputDict->setObject(guidStr,guidDict);
			}
			
			//now we have a dict for the guid no matter what (mapping GUID | DICT)
			guidDict->setObject(OSString::withCString(guidValueStr+strlen(NVRAM_SEPERATOR)), value);
		}
		else
		{
			//we are boring.
			outputDict->setObject(key,value);
		}
	}//end while
		
	//serialize and write this out
	OSSerialize *s = OSSerialize::withCapacity(10000);
	s->addString(NVRAM_FILE_HEADER);
	outputDict->serialize(s);
    s->addString(NVRAM_FILE_FOOTER);

	
	int error =	write_buffer(mFilePath->getCStringNoCopy(), s->text(), s->getLength(), mCtx);
	if(error)
    {
		ERROR("Unable to write to %s, errno %d\n", mFilePath->getCStringNoCopy(), error);
	}
    
	//now free the dictionaries && iter
	iter->release();
	outputDict->release();
	s->release();
	
}

bool FileNVRAM::serializeProperties(OSSerialize *s) const
{
    bool result = IOService::serializeProperties(s);
    LOG("serializeProperties(%p) = %s\n", s, s->text());
    return result;
}

OSObject * FileNVRAM::getProperty(const OSSymbol *aKey) const
{        
    OSObject* value = IOService::getProperty(aKey);
    if(value)
    {
        OSSerialize *s = OSSerialize::withCapacity(1000);
        if(value->serialize(s))
        {
            LOG("getProperty(%s) = %s called\n", aKey->getCStringNoCopy(), s->text());
        }
        else
        {
            LOG("getProperty(%s) = %p called\n", aKey->getCStringNoCopy(), value);
        }
        s->release();
    }
    else
    {

        // Ignore BSD Name for now in logs, it pollutes
        if(!aKey->isEqualTo("BSD Name"))
		{
			LOG("getProperty(%s) = %p called\n", aKey->getCStringNoCopy(), (void*)NULL);
		}
    }
    return value;
}

OSObject * FileNVRAM::getProperty(const char *aKey) const
{
    const OSSymbol *keySymbol;
    OSObject *theObject = 0;
    
    keySymbol = OSSymbol::withCStringNoCopy(aKey);
    if (keySymbol != 0) {
        theObject = getProperty(keySymbol);
        keySymbol->release();
    }
    
    return theObject;
}

bool FileNVRAM::setProperty(const OSSymbol *aKey, OSObject *anObject)
{
    // Verify permissions.
    IOReturn     result;
    result = IOUserClient::clientHasPrivilege(current_task(), kIOClientPrivilegeAdministrator);
    if (result != kIOReturnSuccess) return false;
	
    OSSerialize *s = OSSerialize::withCapacity(1000);
    if(anObject->serialize(s))
    {
        
        LOG("setProperty(%s, (%s) %s) called\n", aKey->getCStringNoCopy(), anObject->getMetaClass()->getClassName(), s->text());
    }
    else
    {
        LOG("setProperty(%s, (%s) %p) called\n", aKey->getCStringNoCopy(), anObject->getMetaClass()->getClassName(), anObject);
    }
    s->release();
    bool stat = IOService::setProperty(aKey, cast(aKey, anObject));
    if(mInitComplete) sync();
    return stat;
}

void FileNVRAM::removeProperty(const OSSymbol *aKey)
{
    // Verify permissions.
    IOReturn     result;
    result = IOUserClient::clientHasPrivilege(current_task(), kIOClientPrivilegeAdministrator);
    if (result != kIOReturnSuccess) return;

    LOG("removeProperty() called\n");
    
    IOService::removeProperty(aKey);
    if(mInitComplete) sync();
}

IOReturn FileNVRAM::setProperties(OSObject *properties)
{
    bool                 result = true;
    OSObject             *object;
    const OSSymbol       *key;
    const OSString       *tmpStr;
    OSDictionary         *dict;
    OSCollectionIterator *iter;
    
    dict = OSDynamicCast(OSDictionary, properties);
    if (!dict) return kIOReturnBadArgument;
    
    iter = OSCollectionIterator::withCollection(dict);
    if (!iter) return kIOReturnBadArgument;
    
    while (result)
    {
        key = OSDynamicCast(OSSymbol, iter->getNextObject());
        if (!key) break;
        
        object = dict->getObject(key);
        if (!object) continue;
        
        if (key->isEqualTo(kIONVRAMDeletePropertyKey))
        {
            tmpStr = OSDynamicCast(OSString, object);
            if (tmpStr)
            {
                key = OSSymbol::withString(tmpStr);
                removeProperty(key);
                key->release();
                result = true;
            }
            else
            {
                result = false;
            }
        }
        else if(key->isEqualTo(kIONVRAMSyncNowPropertyKey))
        {
            tmpStr = OSDynamicCast(OSString, object);
            if (tmpStr)
            {
                result = true; // We are not going to gaurantee sync, this is best effort

                if(safeToSync()) sync();
            }
            else
            {
                result = false;
            }
        }
        else
        {
            result = setProperty(key, object);
        }
        
    }
    
    iter->release();
    
    if (result) return kIOReturnSuccess;
    else return kIOReturnError;
}

IOReturn FileNVRAM::readXPRAM(IOByteCount offset, UInt8 *buffer, IOByteCount length)
{
    LOG("readXPRAM(%zu, %p, %zu) called\n", (size_t)offset, buffer, (size_t)length);
    return kIOReturnUnsupported;
}


IOReturn FileNVRAM::writeXPRAM(IOByteCount offset, UInt8 *buffer,
                               IOByteCount length)
{
    LOG("writeXPRAM(%zu, %p, %zu) called\n", (size_t)offset, buffer, (size_t)length);
    return kIOReturnUnsupported;
}

IOReturn FileNVRAM::readNVRAMProperty(IORegistryEntry *entry,
                                      const OSSymbol **name,
                                      OSData **value)
{
    LOG("readNVRAMProperty(%s, %p, %p) called\n", entry->getName(), name, value);
    return kIOReturnUnsupported;
}

IOReturn FileNVRAM::writeNVRAMProperty(IORegistryEntry *entry,
                                       const OSSymbol *name,
                                       OSData *value)
{
    LOG("writeNVRAMProperty(%s, %s, %p) called\n",
        entry->getName(),
        name->getCStringNoCopy(),
        value);
    return kIOReturnUnsupported;
}

OSDictionary * FileNVRAM::getNVRAMPartitions(void)
{
    LOG("getNVRAMPartitions() called\n");
    return 0;
}

IOReturn FileNVRAM::readNVRAMPartition(const OSSymbol *partitionID,
                                       IOByteCount offset, UInt8 *buffer,
                                       IOByteCount length)
{
    LOG("readNVRAMPartition(%s, %zu, %p, %zu) called\n",
            partitionID->getCStringNoCopy(),
            (size_t)offset,
            buffer,
            (size_t)length);
    return kIOReturnNotFound;
}

IOReturn FileNVRAM::writeNVRAMPartition(const OSSymbol *partitionID,
                                        IOByteCount offset, UInt8 *buffer,
                                        IOByteCount length)
{
    LOG("writeNVRAMPartition(%s, %zu, %p, %zu) called\n",
            partitionID->getCStringNoCopy(),
            (size_t)offset,
            buffer,
            (size_t)length);
    return kIOReturnSuccess;
}

IOByteCount FileNVRAM::savePanicInfo(UInt8 *buffer, IOByteCount length)
{
    // NOTE: In the event of a panic, we *cannot* use printf's.
    // Also, we double panic when we call the next line, investigate.
    //setProperty(OSSymbol::withCString("PANIC"), OSString::withCString("test"));
    
    return length;
}

bool FileNVRAM::safeToSync(void)
{
    static int count;
    LOG("safeToSync() called\n");

    // Don't sync every time... we already do it so it shouldn't be needed, but just in case.
    count++;
    if(count % 4) return true;
    else          return false;
}

IOReturn FileNVRAM::dispatchCommand( OSObject* owner,
                                    void* arg0,
                                    void* arg1,
                                    void* arg2,
                                    void* arg3 )
{
    FileNVRAM* self = OSDynamicCast(FileNVRAM, owner);
    if(!self) return kIOReturnBadArgument;

    size_t command = (size_t) arg0;
    switch (command)
    {
        case kNVRAMSyncCommand:
            self->doSync();
            break;
            
        default:
            break;
    }
    
    return kIOReturnSuccess;
}
void FileNVRAM::timeoutOccurred(IOTimerEventSource* timer)
{
    uint64_t timeout = 20000; // 20ms
    // Check to see if BSD has been published, if so sync();
    
    OSDictionary *  dict = 0;
    IOService *     match = 0;
    boolean_t		found = false;
    
    do
    {
        dict = IOService::resourceMatching( "BSD" );
        if(dict)
        {
            if(IOService::waitForMatchingService( dict, timeout ))
            {
                found = true;
            }
        }
    } while( false );
    
    OSSafeReleaseNULL(dict);
    OSSafeReleaseNULL(match);

    if(found)
    {
        LOG("BSD found, syncing");
        mTimer->cancelTimeout();
        sync();
    }
    else
    {
        mTimer->setTimeoutMS(10);
    }
}

IOReturn FileNVRAM::setPowerState ( unsigned long whichState, IOService * whatDevice )
{
    LOG("setPowerState() state %lu\n",whichState);
    
    switch(whichState)
    {
        case POWER_STATE_OFF:
            LOG("Entering sleep\n");
            mSafeToSync = false;
            // Going to sleep. Perform state-saving tasks here.
            break;
            
        default:
        case POWER_STATE_ON:
            LOG("Wakeing\n");
            // Waking up. Perform device initialization here.
            mSafeToSync = true;
            break;
    }

    return kIOPMAckImplied;
}

OSObject* FileNVRAM::cast(const OSSymbol* key, OSObject* obj)
{
    const char* legacy[] = {
        "boot-args",
        "boot-script",
    };
    
    OSString* str = OSDynamicCast(OSString, key);
    if(str)
    {
        for(int i = 0; i < sizeof(legacy)/sizeof(char*); i++)
        {
            if(str->isEqualTo(legacy[i]))
            {
                LOG("Found legacy key %s\n", str->getCStringNoCopy());
                
                // add null char, convert to OSString
                OSData* data = OSDynamicCast(OSData, obj);
                if(data)
                {
                    data->appendByte(0x00, 1);
                    return OSString::withCString((const char*)data->getBytesNoCopy());
                }
            }
        }
    }
    return obj;
}
