//
//  Support.cpp
//  FileNVRAM
//
//  Created by Evan Lojewski on 1/29/13.
//  Copyright (c) 2013-2017 xZenue LLC. All rights reserved.
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
