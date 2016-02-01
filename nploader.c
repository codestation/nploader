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
#include <pspiofilemgr.h>
#include <string.h>
#include <inttypes.h>
#include "nploader.h"
#include "hook.h"
#include "path.h"
#include "logger.h"

#define PSP_O_NPDRM 0x40000000

u32 sctrlHENFindFunction(char *modname, char *libname, u32 nid);

int decrypted = 0;

// sceIo functions
SceUID (*sceIoOpen_func)(const char *file, int flags, SceMode mode) = NULL;
SceUID (*sceIoOpenAsync_func)(const char *file, int flags, SceMode mode) = NULL;
SceOff (*sceIoLseek_func)(SceUID fd, SceOff offset, int whence) = NULL;

#ifdef CUSTOM_PATH
SceUID (*sceIoDopen_func)(const char *dirname) = NULL;
int (*sceIoChdir_func)(const char *path) = NULL;
int (*sceIoGetstat_func)(const char *file, SceIoStat *stat) = NULL;
#endif

// npDrm functions
int (*sceNpDrmEdataSetupKey_func)(SceUID fd) = NULL;
SceOff (*sceNpDrmEdataGetDataSize_func)(SceUID fd) = NULL;
int (*sceNpDrmRenameCheck_func)(const char *file) = NULL;

// loadModule functions
SceUID (*sceKernelLoadModuleNpDrm_func)(const char *path, int flags, SceKernelLMOption *option) = NULL;
SceUID (*sceKernelLoadModule_func)(const char *path, int flags, SceKernelLMOption *option) = NULL;

// the games calls this function (sceNpDrmEdataSetupKey) before reading the edat files
// returns 0 on success or a SCE error code otherwise
int np_setup(SceUID fd) {
    kprintf("sceNpDrmEdataSetupKey called with fd: %08X\n", fd);
    if (decrypted) {
        kprintf("DLC already decrypted for fd: %08X, returning 0 for sceNpDrmEdataSetupKey\n", fd);
        return 0;
    }
    if (!sceNpDrmEdataSetupKey_func) {
        sceNpDrmEdataSetupKey_func = (void *) sctrlHENFindFunction("scePspNpDrm_Driver", "scePspNpDrm_user", 0x08D98894);
        //kprintf("Found sceNpDrmEdataSetupKey addr: %08X\n", (u32)sceNpDrmEdataSetupKey_func);
    }
    int res = sceNpDrmEdataSetupKey_func(fd);
    kprintf("sceNpDrmEdataSetupKey returned: %08X\n", res);
    return res;
}

// check if the file is encrypted with the edat format
int check_encryption(const char *file) {
    int res;
    int k1 = pspSdkSetK1(0);
    SceUID fd = sceIoOpen(file, PSP_O_RDONLY, 0777);
    if (fd >= 0) {
        char buffer[8];
        int read = sceIoRead(fd, buffer, sizeof(buffer));
        sceIoClose(fd);
        if (read == sizeof(buffer)) {
            if (memcmp(buffer, "\0PSPEDAT", sizeof(buffer))) {
                res = 0;
            } else {
                res = 1;
            }
        } else {
            res = read;
        }
    } else {
        res = fd;
    }
    pspSdkSetK1(k1);
    return res;
}


#ifdef CUSTOM_PATH
int np_stat(const char *file, SceIoStat *stat) {
    if (!sceIoGetstat_func) {
        sceIoGetstat_func = (void *) sctrlHENFindFunction("sceIOFileManager", "IoFileMgrForUser", 0xACE946E8);
        kprintf("Found sceIoGetstat addr: %08X\n", (u32)sceIoGetstat_func);
    }
    char *redir = redirect_path(file);
    if(redir) file = redir;
    int res = sceIoGetstat_func(file, stat);
    restore_usermem();
    return res;
}
//#else
//int np_stat(const char *file, SceIoStat *stat) { return 0; }
#endif

#ifdef CUSTOM_PATH
int np_chdir(const char *path) {
    if (!sceIoChdir_func) {
        sceIoChdir_func = (void *) sctrlHENFindFunction("sceIOFileManager", "IoFileMgrForUser", 0x55F4717D);
        kprintf("Found sceIoChdir addr: %08X\n", (u32)sceIoChdir_func);
    }
    char *redir = redirect_path(path);
    if(redir) path = redir;
    int res = sceIoChdir_func(path);
    restore_usermem();
    return res;
}
//#else
//int np_chdir(const char *path) { return 0; }
#endif

#ifdef CUSTOM_PATH
SceUID np_dopen(const char *dirname) {
    if (!sceIoDopen_func) {
        sceIoDopen_func = (void *) sctrlHENFindFunction("sceIOFileManager", "IoFileMgrForUser", 0xB29DDF9C);
        kprintf("Found sceIoDopen addr: %08X\n", (u32)sceIoDopen_func);
    }
    char *redir = redirect_path(dirname);
    if(redir) dirname = redir;
    int res = sceIoDopen_func(dirname);
	kprintf("sceIoDopen of %s returned %08X\n", dirname, res);
    restore_usermem();
    return res;
}
//#else
//SceUID np_dopen(const char *dirname) { return 0; }
#endif

// generic function for both sceIoOpen/sceIoOpenAsync
SceUID _npprx_open(const char *file, int flags, SceMode mode, int async) {
    // only check for files with the npdrm flag on that doesn't came from the UMD
    if (((flags & PSP_O_NPDRM) == PSP_O_NPDRM) && strncasecmp(file, "disc0", 5)) {
#ifdef CUSTOM_PATH
        char *redir = redirect_path(file);
        if(redir) file = redir;
#endif
        int res = check_encryption(file);
        switch (res) {
        case 0: // DECRYPTED
            flags &= ~PSP_O_NPDRM;
            kprintf("> %s is decrypted, disabling npdrm functionality\n", file);
            decrypted = 1;
            break;
        case 1: // ENCRYPTED
            kprintf("> %s is encrypted, do not override npdrm\n", file);
            decrypted = 0;
            break;
        default:
            kprintf("Failed to open/read %s, code: %08X\n", file, res);
        }
    }
    int res = async ? sceIoOpenAsync_func(file, flags, mode) :
            sceIoOpen_func(file, flags, mode);
#ifdef CUSTOM_PATH
    restore_usermem();
#endif
    return res;
}

// sceIoOpen hook function
SceUID np_open(const char *file, int flags, SceMode mode) {
    if (!sceIoOpen_func) {
        sceIoOpen_func = (void *) sctrlHENFindFunction("sceIOFileManager", "IoFileMgrForUser", 0x109F50BC);
        kprintf("Found sceIoOpen addr: %08X\n", (u32)sceIoOpen_func);
    }
    int res = _npprx_open(file, flags, mode, 0);
    kprintf("sceIoOpen called: %s, flags: %08X, mode: %08X, fd: %08X\n", file, flags, mode, res);
    return res;
}

// sceIoOpenAsync hook function
SceUID np_open_async(const char *file, int flags, SceMode mode) {
    if (!sceIoOpenAsync_func) {
        sceIoOpenAsync_func = (void *) sctrlHENFindFunction("sceIOFileManager", "IoFileMgrForUser", 0x89AA9906);
        kprintf("Found sceIoOpenAsync addr: %08X\n", (u32)sceIoOpenAsync_func);
    }
    kprintf("Calling sceIoOpenAsync: %s, flags: %08X, mode: %08X\n", file, flags, mode);
    return _npprx_open(file, flags, mode, 1);
}

// returns the real size of the decrypted edat file (sceNpDrmEdataGetDataSize)
SceOff np_size(SceUID fd) {
    int k1 = pspSdkSetK1(0);
    if (!sceNpDrmEdataGetDataSize_func) {
        sceNpDrmEdataGetDataSize_func = (void *) sctrlHENFindFunction("scePspNpDrm_Driver", "scePspNpDrm_user", 0x219EF5CC);
        //kprintf("sceNpDrmEdataGetDataSize addr: %08X\n", (u32)sceNpDrmEdataGetDataSize_func);
    }
    if (decrypted) {
        if (!sceIoLseek_func) {
            sceIoLseek_func = (void *) sctrlHENFindFunction("sceIOFileManager","IoFileMgrForUser", 0x27EB27B8);
            kprintf("sceIoLseek addr: %08X\n", (u32)sceIoLseek_func);
        }
        pspSdkSetK1(k1);
        SceOff cur = sceIoLseek_func(fd, 0, PSP_SEEK_CUR);
        SceOff size = sceIoLseek_func(fd, 0, PSP_SEEK_END);
        sceIoLseek_func(fd, cur, PSP_SEEK_SET);
        int k1 = pspSdkSetK1(0);
        kprintf("DLC already decrypted for fd: %08X, returning %u for sceNpDrmEdataGetDataSize\n", fd, (u32)size);
        pspSdkSetK1(k1);
        return size;
    }
    pspSdkSetK1(k1);
    SceOff size = sceNpDrmEdataGetDataSize_func(fd);
    kprintf("sceNpDrmEdataGetDataSize called, fd: %08X, size: %u\n", fd, (u32)size);
    return size;
}

// checks if the edat/sprx file hasn't been renamed, must return 0 on success (sceNpDrmRenameCheck)
SceUID np_rename(const char *file) {
    int k1 = pspSdkSetK1(0);
    if (!sceNpDrmRenameCheck_func) {
        sceNpDrmRenameCheck_func = (void *) sctrlHENFindFunction("scePspNpDrm_Driver", "scePspNpDrm_user", 0x275987D1);
        kprintf("sceNpDrmRenameCheck addr: %08X\n", (u32)sceNpDrmRenameCheck_func);
    }
#ifdef CUSTOM_PATH
    char *redir = redirect_path(file);
    if(redir) file = redir;
#endif
    int res = check_encryption(file);
    switch (res) {
    case 0:
        kprintf("DLC already decrypted for %s, returning 0 for sceNpDrmRenameCheck\n", file);
        break;
    default:
        kprintf("Failed to open/read %s, code: %08X\n", file, res);
    case 1:
        pspSdkSetK1(k1);
        res = sceNpDrmRenameCheck_func(file);
        k1 = pspSdkSetK1(0);
        kprintf("sceNpDrmRenameCheck called, file: %s, res: %08X\n", file, res);
    }
#ifdef CUSTOM_PATH
        restore_usermem();
#endif
    pspSdkSetK1(k1);
    return res;
}

// (sceKernelLoadModule2?) special function to launch sprx files from the memory stick
SceUID np_load(const char *path, int flags, SceKernelLMOption *option) {
    int k1 = pspSdkSetK1(0);
    if (!sceKernelLoadModuleNpDrm_func) {
        sceKernelLoadModuleNpDrm_func = (void *) sctrlHENFindFunction("sceModuleManager", "ModuleMgrForUser", 0xF2D8D1B4);
        kprintf("Found sceKernelLoadModuleNpDrm addr: %08X\n", (u32)sceKernelLoadModuleNpDrm_func);
    }
    if (!sceKernelLoadModule_func) {
        sceKernelLoadModule_func = (void *) sctrlHENFindFunction("sceModuleManager", "ModuleMgrForUser", 0x977DE386);
        kprintf("Found sceKernelLoadModule addr: %08X\n", (u32)sceKernelLoadModule_func);
    }
    SceUID res = -1;

#ifdef CUSTOM_PATH
    char *redir = redirect_path(path);
    if(redir) path = redir;
#endif
    switch (check_encryption(path)) {
    case 0: // DECRYPTED
        kprintf("DLC already decrypted for %s, using sceKernelLoadModule\n", path);
        pspSdkSetK1(k1);
        res = sceKernelLoadModule_func(path, flags, option);
        pspSdkSetK1(0);
#ifdef CUSTOM_PATH
        restore_usermem();
#endif
        kprintf("sceKernelLoadModule called, path: %s, flags: %08X, option: %08X, modid: %08X\n", path, flags, (u32)option, res);
        break;
    case 1: // ENCRYPTED
        kprintf("DLC encrypted for %s, using sceKernelLoadModule2\n", path);
        pspSdkSetK1(k1);
        res = sceKernelLoadModuleNpDrm_func(path, flags, option);
        pspSdkSetK1(0);
#ifdef CUSTOM_PATH
        restore_usermem();
#endif
        kprintf("sceKernelLoadModuleNpDrm called, path: %s, flags: %08X, option: %08X, modid: %08X\n", path, flags, (u32)option, res);
        break;
    default:
        kprintf("Error in check_encryption!\n");
    }
    pspSdkSetK1(k1);
    return res;
}
