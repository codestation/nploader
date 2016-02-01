/*
 *  nploader module
 *
 *  Copyright (C) 2011  Codestation
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <pspsdk.h>
#include <pspsysmem.h>
#include <pspmodulemgr.h>
#include <ctype.h>
#include "logger.h"

#ifdef CUSTOM_PATH

#define USERMEM_LIMIT ((char *)0xA000000)
#define USERMEM_PTR (USERMEM_LIMIT-256)

char userbuf_backup[256];
char buffer[64];
int use_custom_path = 0;

const char *dlc_path = "/PSP/GAME";

char *redirect_path(const char *path) {
    if(use_custom_path && strncasecmp(path, "disc", 4)) {
        int k1 = pspSdkSetK1(0);
        // get the file path without the device
        char *str = strchr(path, ':');
        // check if is a bugged path (Buzz! game)
        int offset = str[1] == '/' ? strlen(dlc_path) : strlen(dlc_path) - 1;
        // get the compare string
        const char *cmp = (offset == strlen(dlc_path)) ? dlc_path : dlc_path + 1;
        if(str && strncasecmp(str + 1, cmp, offset) == 0) {
            memcpy(userbuf_backup, USERMEM_PTR, sizeof(userbuf_backup));
            strcpy(USERMEM_PTR, buffer);
            strcat(USERMEM_PTR, str + offset + 1);
            kprintf("%s redirected to %s\n", path, USERMEM_PTR);
            pspSdkSetK1(k1);
            return USERMEM_PTR;
        }
        pspSdkSetK1(k1);
    }
    return NULL;
}

void restore_usermem() {
    if(use_custom_path) {
        memcpy(USERMEM_LIMIT - 256, userbuf_backup, sizeof(userbuf_backup));
    }
}

void read_custom_path() {
    char *str;
    strcat(buffer, "nploader.ini");
    SceUID fd = sceIoOpen(buffer, PSP_O_RDONLY, 0777);
    if(fd >= 0) {
        sceIoRead(fd, buffer, sizeof(buffer));
        sceIoClose(fd);
        str = strchr(buffer, '\r');
        if(str) str[0] = 0; // Windows newline
        str = strchr(buffer, '\n');
        if(str) str[0] = 0;
        SceUID dfd = sceIoDopen(buffer);
        if(dfd >= 0) {
            sceIoDclose(dfd);
            kprintf("Found custom DLC path: %s\n", buffer);
            use_custom_path = 1;
        } else {
            kprintf("The directory %s doesn't exists\n", buffer);
        }
    }
}

void get_plugin_path(const char *argp) {
    strcpy(buffer, argp);
    strrchr(buffer, '/')[1] = 0;
}
#endif

int strncasecmp(const char *s1, const char *s2, size_t n) {
    while (n > 0 && toupper((unsigned char) *s1)
            == toupper((unsigned char) *s2)) {
        if (*s1 == '\0')
            return 0;
        s1++;
        s2++;
        n--;
    }
    if (n == 0)
        return 0;
    return toupper((unsigned char) *s1) - toupper((unsigned char) *s2);
}

