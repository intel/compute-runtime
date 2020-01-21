/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "core/os_interface/linux/drm_neo.h"

#include "drm/i915_drm.h"

#include <array>
#include <cstring>
#include <dlfcn.h>
#include <fcntl.h>
#include <stdarg.h>
#include <sys/types.h>

extern "C" {
int open(const char *pathname, int flags, ...);
int ioctl(int fd, unsigned long int request, ...) throw();
}

extern int (*c_open)(const char *pathname, int flags, ...);
extern int (*openFull)(const char *pathname, int flags, ...);
extern int (*c_ioctl)(int __fd, unsigned long int __request, ...);

extern int drmOtherRequests(unsigned long int request, va_list vl);

extern int fakeFd;
extern int haveDri;  // index of dri to serve, -1 - none
extern int deviceId; // known DeviceID
extern int haveSoftPin;
extern int failOnDeviceId;
extern int failOnEuTotal;
extern int failOnSubsliceTotal;
extern int failOnRevisionId;
extern int failOnSoftPin;
extern int failOnParamBoost;
extern int failOnContextCreate;
extern int failOnSetPriority;
extern int failOnPreemption;
extern int havePreemption;
extern int failOnDrmVersion;
extern char providedDrmVersion[5];
extern int ioctlSeq[8];
extern size_t ioctlCnt;

extern std::array<NEO::Drm *, 1> drms;
