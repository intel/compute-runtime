/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#pragma once

#include "runtime/os_interface/linux/drm_neo.h"
#include "drm/i915_drm.h"

#include <stdarg.h>
#include <sys/types.h>
#include <fcntl.h>
#include <dlfcn.h>

#include <cstring>
#include <array>

extern "C" {
int open(const char *pathname, int flags, ...);
int ioctl(int fd, unsigned long int request, ...) throw();
}

extern int (*c_open)(const char *pathname, int flags, ...);
extern int (*c_ioctl)(int __fd, unsigned long int __request, ...);

extern int fakeFd;
extern int haveDri;  // index of dri to serve, -1 - none
extern int deviceId; // known DeviceID
extern int haveSoftPin;
extern int failOnDeviceId;
extern int failOnRevisionId;
extern int failOnSoftPin;
extern int failOnParamBoost;
extern int failOnContextCreate;
extern int failOnSetPriority;
extern int failOnDrmVersion;
extern char providedDrmVersion[5];
extern int ioctlSeq[8];
extern size_t ioctlCnt;

extern std::array<OCLRT::Drm *, 1> drms;

inline void resetOSMockGlobalState() {
    fakeFd = 1023;
    haveDri = 0;
    deviceId = OCLRT::deviceDescriptorTable[0].deviceId;
    haveSoftPin = 1;
    failOnDeviceId = 0;
    failOnRevisionId = 0;
    failOnSoftPin = 0;
    failOnParamBoost = 0;
    std::memset(ioctlSeq, 0, sizeof(ioctlSeq));
    ioctlCnt = 0;
}
