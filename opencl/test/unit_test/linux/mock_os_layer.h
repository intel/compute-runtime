/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/linux/drm_wrappers.h"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <fcntl.h>
#include <stdarg.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

namespace NEO {
struct GemVmControl;
struct Query;
} // namespace NEO

extern int (*openFunc)(const char *pathname, int flags, ...);
extern int (*openFull)(const char *pathname, int flags, ...);

extern int fakeFd;
extern int haveDri;  // index of dri to serve, -1 - none
extern int deviceId; // known DeviceID
extern int revisionId;
extern int vmId;
extern int failOnDeviceId;
extern int failOnEuTotal;
extern int failOnSubsliceTotal;
extern int failOnRevisionId;
extern int failOnParamBoost;
extern int failOnContextCreate;
extern int failOnVirtualMemoryCreate;
extern int failOnSetPriority;
extern int failOnPreemption;
extern int havePreemption;
extern int failOnDrmVersion;
extern int captureVirtualMemoryCreate;
extern char providedDrmVersion[5];
extern int ioctlSeq[8];
extern size_t ioctlCnt;
extern bool failOnOpenDir;
extern uint32_t entryIndex;
extern int accessCalledTimes;
extern int readLinkCalledTimes;
extern int fstatCalledTimes;
extern bool forceExtraIoctlDuration;
extern std::vector<NEO::GemVmControl> capturedVmCreate;
extern int drmQuery(NEO::Query *param);

namespace NEO {
struct Query;
}

namespace DrmQueryConfig {
extern int failOnQueryEngineInfo;
extern int failOnQueryMemoryInfo;
extern unsigned int retrieveQueryMemoryInfoRegionCount;
} // namespace DrmQueryConfig
