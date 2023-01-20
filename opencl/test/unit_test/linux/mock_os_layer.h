/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <fcntl.h>
#include <stdarg.h>
#include <sys/types.h>
#include <unistd.h>

extern int (*c_open)(const char *pathname, int flags, ...);
extern int (*openFull)(const char *pathname, int flags, ...);

extern int drmOtherRequests(unsigned long int request, ...);

extern int fakeFd;
extern int haveDri;  // index of dri to serve, -1 - none
extern int deviceId; // known DeviceID
extern int revisionId;
extern int haveSoftPin;
extern int vmId;
extern int failOnDeviceId;
extern int failOnEuTotal;
extern int failOnSubsliceTotal;
extern int failOnRevisionId;
extern int failOnSoftPin;
extern int failOnParamBoost;
extern int failOnContextCreate;
extern int failOnVirtualMemoryCreate;
extern int failOnSetPriority;
extern int failOnPreemption;
extern int havePreemption;
extern int failOnDrmVersion;
extern char providedDrmVersion[5];
extern int ioctlSeq[8];
extern size_t ioctlCnt;
extern bool failOnOpenDir;
extern uint32_t entryIndex;
extern int accessCalledTimes;
extern int readLinkCalledTimes;
extern int fstatCalledTimes;
extern bool forceExtraIoctlDuration;
