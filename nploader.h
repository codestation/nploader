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

#ifndef NPLOADER_H_
#define NPLOADER_H_

#include <pspsdk.h>
#include <pspiofilemgr_stat.h>

extern SceUID (*sceIoOpen_func)(const char *file, int flags, SceMode mode);
extern SceUID (*sceIoOpenAsync_func)(const char *file, int flags, SceMode mode);
extern SceUID (*sceIoDopen_func)(const char *dirname);

int (*sceIoChdir_func)(const char *path);
int (*sceIoGetstat_func)(const char *file, SceIoStat *stat);

extern int (*sceNpDrmEdataSetupKey_func)(SceUID fd);
extern SceOff (*sceNpDrmEdataGetDataSize_func)(SceUID fd);
extern int (*sceNpDrmRenameCheck_func)(const char *file);

extern SceUID (*sceKernelLoadModuleNpDrm_func)(const char *path, int flags, SceKernelLMOption *option);

SceUID np_open(const char *file, int flags, SceMode mode);
SceUID np_open_async(const char *file, int flags, SceMode mode);
SceUID np_dopen(const char *dirname);

int np_chdir(const char *path);
int np_stat(const char *file, SceIoStat *stat);

int np_setup(SceUID fd);
SceOff np_size(SceUID fd);
int np_rename(const char *file);

SceUID np_load(const char *path, int flags, SceKernelLMOption *option);

#endif /* NPLOADER_H_ */
