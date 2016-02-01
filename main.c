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

#include <pspkernel.h>
#include <string.h>
#include "hook.h"
#include "nploader.h"
#include "path.h"
#include "logger.h"

PSP_MODULE_INFO("nploader", PSP_MODULE_KERNEL, 0, 8);

STMOD_HANDLER previous = NULL;
int module_found = 0;
int loader_found = 0;

#define MAX_MODULE_NUMBER 256

void patch_io(SceModule2 *module) {
    const char * base = "IoFileMgrForUser";
    kprintf(">> Patching %s imports\n", base);
    // sceIoOpen
    //sceIoOpen_func = NULL;
    if (hook_import_bynid(module, base, 0x109F50BC, np_open, 1) < 0) {
        kprintf(">>> hook to sceIoOpen failed\n");
        // something weird happened so is best to abort now
        return;
    }
    // sceIoOpenAsync
    //sceIoOpenAsync_func = NULL;
    if (hook_import_bynid(module, base, 0x89AA9906, np_open_async, 1) < 0)
        kprintf(">> hook to sceIoOpenAsync failed (not critical)\n");

#ifdef CUSTOM_PATH
    if(use_custom_path) {
        // sceIoDopen
        //sceIoDopen_func = NULL;
        // those function get hooked only is a custom path was declared
        if(hook_import_bynid(module, base, 0xB29DDF9C, np_dopen, 1) < 0)
            kprintf(">>> hook to sceIoDopen failed (used for dlc redirection)\n");
        // sceIoChdir
        //sceIoChdir_func = NULL;
        if(hook_import_bynid(module, base, 0x55F4717D, np_chdir, 1) < 0)
            kprintf(">>> hook to sceIoChdir failed (used for dlc redirection)\n");
        // sceIoGetstat
        //sceIoGetstat_func = NULL;
        if(hook_import_bynid(module, base, 0xACE946E8, np_stat, 1) < 0)
            kprintf(">>> hook to sceIoGetstat failed (used for dlc redirection)\n");
    }
#endif
}

void patch_load(SceModule2 *module) {
    const char *base = "ModuleMgrForUser";
    kprintf(">> Patching %s imports\n", base);
    // sceKernelLoadModule2
    //sceKernelLoadModuleNpDrm_func = NULL;
    if (hook_import_bynid(module, base, 0xF2D8D1B4, np_load, 1) < 0)
        kprintf(">>> hook to sceKernelLoadModuleNpDrm failed (not critical)\n");
}

int patch_drm(SceModule2 *module) {
    const char *base = "scePspNpDrm_user";
    kprintf(">> Patching %s imports\n", base);
    // sceNpDrmEdataSetupKey
    sceNpDrmEdataSetupKey_func = NULL;
    if (hook_import_bynid(module, base, 0x08D98894, np_setup, 1) < 0) {
        kprintf(">>> hook to sceNpDrmEdataSetupKey failed\n");
        return 0;
    }
    // sceNpDrmEdataGetDataSize
    sceNpDrmEdataGetDataSize_func = NULL;
    if (hook_import_bynid(module, base, 0x219EF5CC, np_size, 1) < 0)
        kprintf(">>> hook to sceNpDrmEdataGetDataSize failed (not critical)\n");

    // sceNpDrmRenameCheck
    sceNpDrmRenameCheck_func = NULL;
    if (hook_import_bynid(module, base, 0x275987D1, np_rename, 1) < 0)
        kprintf(">>> hook to sceNpDrmRenameCheck failed (not critical)\n");
    return 1;
}

int hook_user_modules() {
    int count;
    SceUID memid = sceKernelAllocPartitionMemory(PSP_MEMORY_PARTITION_KERNEL,
            "np_modid", PSP_SMEM_High, MAX_MODULE_NUMBER * sizeof(SceUID), NULL);
    if(memid < 0) {
        kprintf("%s alloc failed: %08X\n", __func__, memid);
        return 0;
    }
    SceUID *modid_lst = sceKernelGetBlockHeadAddr(memid);
    memset(modid_lst, 0, MAX_MODULE_NUMBER * sizeof(SceUID));
    int ret = sceKernelGetModuleIdList(modid_lst, MAX_MODULE_NUMBER * sizeof(SceUID), &count);
    if(ret < 0) {
        kprintf("%s module list failed: %08X\n", __func__, ret);
        return 0;
    }
    for(int i = 0; i < count; i++) {
        SceModule2 *pmod = (SceModule2*)sceKernelFindModuleByUID(modid_lst[i]);
        if (pmod && !(pmod->attribute & 0x1000)) {
            kprintf("> Re-patching module: %s\n", pmod->modname);
            patch_drm(pmod);
        }
    }
    sceKernelFreePartitionMemory(memid);
    return 1;
}

int module_start_handler(SceModule2 * module) {
	kprintf("> Loaded, text_addr: %08X, entry_addr: %08X, name: %s\n", module->text_addr, module->entry_addr, module->modname);
    if (!module_found &&
           (module->text_addr == 0x08804000  ||  // base address for game eboots
            module->text_addr == 0x08900000) &&  // new games seems to load at this address
            module->entry_addr != 0xFFFFFFFF &&  // skip some user mode prx that loads @ 0x08804000
            strcmp(module->modname, "opnssmp")){ // this loads @ 0x08804000 too
        //blacklist the Prometheus iso loader
        if (!loader_found && (!strcmp(module->modname, "PLoaderGUI"))) {
            kprintf("Prometheus loader found\n");
            loader_found = 1;
        } else {
            kprintf("Game found: %s\n", module->modname);
            if (patch_drm(module)) {
#ifdef CUSTOM_PATH
                read_custom_path();
#endif
                // only patch the sceio and loadcore funcs if
                // the npdrm hook was successful
                patch_io(module);
                patch_load(module);
            }
            module_found = 1;
        }
    } else {
        // patch other user modules that uses the npdrm api (fixes Buzz! video load)
        if(module_found && !(module->attribute & 0x1000)) {
            kprintf("> Patching module: %s\n", module->modname);
            if(patch_drm(module)) {
                patch_io(module);
                patch_load(module);
            }
        }
    }
    if (module_found && !strcmp(module->modname, "scePspNpDrm_Driver")) {
        // some games reload scePspNpDrm and it could change address so we need
        // to invalidate the previous func addresses and re-hook
        hook_user_modules();
    }
    return previous ? previous(module) : 0;
}

int module_start(SceSize argc, void *argp) {
    kprintf("------------------\nNPloader starting\n");
#ifdef CUSTOM_PATH
    get_plugin_path(argp);
#endif
    previous = sctrlHENSetStartModuleHandler(module_start_handler);
    return 0;
}

int module_stop(SceSize args, void *argp) {
    return 0;
}
