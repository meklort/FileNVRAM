//
//  Support.cpp
//  FileNVRAM
//
//  Created by Evan Lojewski on 1/29/13.
//  Copyright (c) 2013 xZenue LLC. All rights reserved.
//
// This work is licensed under the
//  Creative Commons Attribution-NonCommercial 3.0 Unported License.
//  To view a copy of this license, visit http://creativecommons.org/licenses/by-nc/3.0/.
//

#include "Support.h"

static inline const char * strstr(const char *s, const char *find)
{
    char c, sc;
    size_t len;
	
    if ((c = *find++) != 0) {
        len = strlen(find);
        do {
            do {
                if ((sc = *s++) == 0)
                    return (NULL);
            } while (sc != c);
        } while (strncmp(s, find, len) != 0);
        s--;
    }
    return s;
}

static inline void gen_random(char *s, const int len)
{
    // NOTE: Not all characters have the same prob, but it's good enough
    const char alphanum[] =
    "0123456789"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    
    for (int i = 0; i < len; ++i) {
        s[i] = alphanum[random() % (sizeof(alphanum) - 1)];
    }
    
    s[len] = 0;
}

static inline void handleSetting(const OSObject* object, const OSObject* value, FileNVRAM* entry)
{
    bool mLoggingEnabled = entry->mLoggingEnabled;

    OSString* key = OSDynamicCast( OSString, object);
    
    if(!key)
    {
        LOG("Unknown key\n");
        return;
    }
    else
    {
        LOG("Handling key %s\n", key->getCStringNoCopy());
    }
    
    if(key->isEqualTo(NVRAM_SET_FILE_PATH))
    {
        OSString* str = OSDynamicCast(OSString, value);
        if(str)
        {
            entry->setPath(str);
        }
        else
        {
            OSData* dat = OSDynamicCast(OSData, value);
            if(dat)
            {
                OSString* str = OSString::withCString((const char*)dat->getBytesNoCopy());
                entry->setPath(str);
                str->release();
            }
        }
        // Where to get path from?
    }
    else if(key->isEqualTo(NVRAM_GEN_MLB))
    {
        OSBoolean* shouldgen = OSDynamicCast(OSBoolean, value);
        if(shouldgen && shouldgen->isTrue())
        {
            IORegistryEntry* root = IORegistryEntry::fromPath("/", gIODTPlane);
            if(!root) return; // Cant gen, / not found.
            
            OSString* serial = OSDynamicCast(OSString, root->getProperty(kIOPlatformSerialNumberKey));
            if(!serial) return; // Can't gen, no serial found
            
            size_t serialLen = strlen(serial->getCStringNoCopy()); // should always be 12
            
            char buffer[serialLen + 5 + 1]; // usualy 12 + 5 random char + null
            
            strncpy(buffer, serial->getCStringNoCopy(), serialLen + 1);
            gen_random(&buffer[serialLen], 5);
            
            
            const OSSymbol* sym = OSSymbol::withCString(APPLE_MLB_KEY);
            OSData* value = OSData::withBytes(buffer, (int)sizeof(buffer));
            entry->setProperty(sym, value);
            sym->release();
            value->release();
            shouldgen->release();
        }
    }
    else if(key->isEqualTo(NVRAM_GEN_ROM))
    {
        OSBoolean* shouldgen = OSDynamicCast(OSBoolean, value);
        if(shouldgen && shouldgen->isTrue())
        {
            // Ideally we'd read this from the primary NIC's MAC address
            LOG("Generating ROM\n");
            
            char buffer[6]; // mac addr
            bzero(buffer, sizeof(buffer));
            
            for(int i = 0; i < sizeof(buffer); i++)
            {
                buffer[i] = random() % 255;
            }
            
            
            const OSSymbol* sym = OSSymbol::withCString(APPLE_ROM_KEY);
            OSData* value = OSData::withBytes(buffer, sizeof(buffer));
            entry->setProperty(sym, value);
            sym->release();
            value->release();
        }
    }
    else if(key->isEqualTo(NVRAM_ENABLE_LOG))
    {
        OSBoolean* shouldlog = OSDynamicCast(OSBoolean, value);
        if(shouldlog)
        {
            mLoggingEnabled = entry->mLoggingEnabled = shouldlog->getValue();
            LOG("Setting logging to %s.\n", mLoggingEnabled ? "enabled" : "disabled");
        }

    }
}
