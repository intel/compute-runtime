/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "mock_os_layer.h"

#include "shared/source/helpers/string.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/drm_wrappers.h"
#include "shared/source/os_interface/linux/i915.h"

#include <cassert>
#include <dirent.h>
#include <iostream>
#include <sys/stat.h>
#include <sys/sysmacros.h>

int (*c_open)(const char *pathname, int flags, ...) = nullptr;
int (*openFull)(const char *pathname, int flags, ...) = nullptr;

int fakeFd = 1023;
int haveDri = 0;                                       // index of dri to serve, -1 - none
int deviceId = NEO::deviceDescriptorTable[0].deviceId; // default supported DeviceID
int revisionId = 17;
int haveSoftPin = 1;
int havePreemption = I915_SCHEDULER_CAP_ENABLED |
                     I915_SCHEDULER_CAP_PRIORITY |
                     I915_SCHEDULER_CAP_PREEMPTION;
int vmId = 0;
int failOnDeviceId = 0;
int failOnEuTotal = 0;
int failOnSubsliceTotal = 0;
int failOnRevisionId = 0;
int failOnSoftPin = 0;
int failOnParamBoost = 0;
int failOnSetParamSseu = 0;
int failOnGetParamSseu = 0;
int failOnContextCreate = 0;
int failOnVirtualMemoryCreate = 0;
int failOnSetPriority = 0;
int failOnPreemption = 0;
int failOnDrmVersion = 0;
int accessCalledTimes = 0;
int readLinkCalledTimes = 0;
int fstatCalledTimes = 0;
bool forceExtraIoctlDuration = 0;
char providedDrmVersion[5] = {'i', '9', '1', '5', '\0'};
uint64_t gpuTimestamp = 0;
int ioctlSeq[8] = {0, 0, 0, 0, 0, 0, 0, 0};
size_t ioctlCnt = 0;

int fstat(int fd, struct stat *buf) {
    ++fstatCalledTimes;
    buf->st_rdev = 0x0;
    return 0;
}

int access(const char *pathname, int mode) {
    ++accessCalledTimes;
    return 0;
}

ssize_t readlink(const char *path, char *buf, size_t bufsiz) {
    ++readLinkCalledTimes;

    if (readLinkCalledTimes % 2 == 1) {
        return -1;
    }

    constexpr size_t sizeofPath = sizeof("../../devices/pci0000:4a/0000:4a:02.0/0000:4b:00.0/0000:4c:01.0/0000:00:03.0/drm/renderD128");

    strcpy_s(buf, sizeofPath, "../../devices/pci0000:4a/0000:4a:02.0/0000:4b:00.0/0000:4c:01.0/0000:00:03.0/drm/renderD128");

    return sizeofPath;
}

int open(const char *pathname, int flags, ...) {
    if (openFull != nullptr) {
        return openFull(pathname, flags);
    }
    if (c_open == nullptr) {
        c_open = (int (*)(const char *, int, ...))dlsym(RTLD_NEXT, "open");
    }

    if (strncmp("/dev/dri/", pathname, 9) == 0) {
        if (haveDri >= 0) {
            return fakeFd;
        } else {
            return -1;
        }
    }

    return c_open(pathname, flags);
}
bool failOnOpenDir = false;
DIR *validDir = reinterpret_cast<DIR *>(0xc001);
DIR *opendir(const char *name) {
    if (failOnOpenDir) {
        return nullptr;
    }
    return validDir;
}
int closedir(DIR *dirp) {
    return 0u;
}

struct dirent entries[] = {
    {0, 0, 0, 0, "."},
    {0, 0, 0, 0, "pci-0000:00:03.1-render"},
    {0, 0, 0, 0, "platform-4010000000.pcie-pci-0000:00:02.0-render"},
    {0, 0, 0, 0, "pci-0000:test1-render"},
    {0, 0, 0, 0, "pci-0000:test2-render"},
    {0, 0, 0, 0, "pci-0000:1234-render"},
    {0, 0, 0, 0, "pci-0000:3:0.0-render"},
    {0, 0, 0, 0, "pci-0a00:00:03.1-render"},
    {0, 0, 0, 0, "pci-0000:b3:03.1-render"},
    {0, 0, 0, 0, "pci-0000:00:b3.1-render"},
    {0, 0, 0, 0, "pci-0000:00:03.a-render"},
    {0, 0, 0, 0, "pci-0000:00:03.a-render-12"},
    {0, 0, 0, 0, "pcii0000:00:03.a-render"},
    {0, 0, 0, 0, "pcii-render"},
};

uint32_t entryIndex = 0u;
const uint32_t numEntries = sizeof(entries) / sizeof(entries[0]);

struct dirent *readdir(DIR *dir) {
    if (entryIndex >= numEntries) {
        entryIndex = 0;
        return nullptr;
    }
    return &entries[entryIndex++];
}

int drmGetParam(NEO::GetParam *param) {
    assert(param);
    int ret = 0;

    switch (param->param) {
    case I915_PARAM_CHIPSET_ID:
        *param->value = deviceId;
        ret = failOnDeviceId;
        break;
    case I915_PARAM_EU_TOTAL:
        *param->value = 3;
        ret = failOnEuTotal;
        break;
    case I915_PARAM_SUBSLICE_TOTAL:
        *param->value = 1;
        ret = failOnSubsliceTotal;
        break;
    case I915_PARAM_REVISION:
        *param->value = revisionId;
        ret = failOnRevisionId;
        break;
    case I915_PARAM_HAS_EXEC_SOFTPIN:
        *param->value = haveSoftPin;
        ret = failOnSoftPin;
        break;
#if defined(I915_PARAM_HAS_SCHEDULER)
    case I915_PARAM_HAS_SCHEDULER:
        *param->value = havePreemption;
        ret = failOnPreemption;
        break;
#endif
    default:
        ret = -1;
        break;
    }
    return ret;
}

int drmSetContextParam(NEO::GemContextParam *param) {
    assert(param);
    int ret = 0;

    switch (param->param) {
    case NEO::contextPrivateParamBoost:
        ret = failOnParamBoost;
        break;
    case I915_CONTEXT_PARAM_VM:
        break;
#if defined(I915_PARAM_HAS_SCHEDULER)
    case I915_CONTEXT_PARAM_PRIORITY:
        ret = failOnSetPriority;
        break;
#endif
    case I915_CONTEXT_PARAM_SSEU:
        if (param->size == sizeof(NEO::GemContextParamSseu) && param->value != 0 && param->contextId == 0) {
            ret = failOnSetParamSseu;
        } else {
            ret = -1;
        }
        break;
    default:
        ret = -1;
        break;
    }
    return ret;
}
int drmGetContextParam(NEO::GemContextParam *param) {
    int ret = 0;

    switch (param->param) {
    case I915_CONTEXT_PARAM_SSEU:
        if (param->size == sizeof(NEO::GemContextParamSseu) && param->value != 0 && param->contextId == 0) {
            ret = failOnGetParamSseu;
        } else {
            ret = -1;
        }
        break;
    default:
        ret = -1;
        break;
    }
    return ret;
}

int drmContextCreate(NEO::GemContextCreateExt *create) {
    assert(create);

    create->contextId = 1;
    return failOnContextCreate;
}

int drmContextDestroy(NEO::GemContextDestroy *destroy) {
    assert(destroy);

    if (destroy->contextId == 1)
        return 0;
    else
        return -1;
}

int drmVirtualMemoryCreate(NEO::GemVmControl *control) {
    assert(control);
    if (!failOnVirtualMemoryCreate) {
        control->vmId = ++vmId;
    }
    return failOnVirtualMemoryCreate;
}

int drmVirtualMemoryDestroy(NEO::GemVmControl *control) {
    assert(control);

    vmId--;
    return (control->vmId > 0) ? 0 : -1;
}

int drmVersion(NEO::DrmVersion *version) {
    memcpy_s(version->name, version->nameLen, providedDrmVersion, strlen(providedDrmVersion) + 1);

    return failOnDrmVersion;
}

int drmQueryItem(NEO::Query *query) {
    auto queryItemArg = reinterpret_cast<NEO::QueryItem *>(query->itemsPtr);
    if (queryItemArg->length == 0) {
        if (queryItemArg->queryId == DRM_I915_QUERY_TOPOLOGY_INFO) {
            queryItemArg->length = sizeof(NEO::QueryTopologyInfo) + 1;
            return 0;
        }
    } else {
        if (queryItemArg->queryId == DRM_I915_QUERY_TOPOLOGY_INFO) {
            auto topologyArg = reinterpret_cast<NEO::QueryTopologyInfo *>(queryItemArg->dataPtr);
            topologyArg->maxSlices = 1;
            topologyArg->maxSubslices = 1;
            topologyArg->maxEusPerSubslice = 3;
            topologyArg->data[0] = 0xFF;
            return failOnEuTotal || failOnSubsliceTotal;
        }
    }
    return drmOtherRequests(DRM_IOCTL_I915_QUERY, query);
}

int ioctl(int fd, unsigned long int request, ...) throw() {
    using namespace std::chrono_literals;

    if (forceExtraIoctlDuration) {
        auto start = std::chrono::steady_clock::now();
        std::chrono::steady_clock::time_point end;

        do {
            end = std::chrono::steady_clock::now();
        } while ((end - start) == 0ns);
    }

    int res;
    va_list vl;
    va_start(vl, request);
    if (fd == fakeFd) {
        res = ioctlSeq[ioctlCnt % (sizeof(ioctlSeq) / sizeof(int))];
        ioctlCnt++;

        if (res == 0) {
            switch (request) {
            case DRM_IOCTL_I915_GETPARAM:
                res = drmGetParam(va_arg(vl, NEO::GetParam *));
                break;
            case DRM_IOCTL_I915_GEM_CONTEXT_SETPARAM:
                res = drmSetContextParam(va_arg(vl, NEO::GemContextParam *));
                break;
            case DRM_IOCTL_I915_GEM_CONTEXT_GETPARAM:
                res = drmGetContextParam(va_arg(vl, NEO::GemContextParam *));
                break;
            case DRM_IOCTL_I915_GEM_CONTEXT_CREATE_EXT:
                res = drmContextCreate(va_arg(vl, NEO::GemContextCreateExt *));
                break;
            case DRM_IOCTL_I915_GEM_CONTEXT_DESTROY:
                res = drmContextDestroy(va_arg(vl, NEO::GemContextDestroy *));
                break;
            case DRM_IOCTL_I915_GEM_VM_CREATE:
                res = drmVirtualMemoryCreate(va_arg(vl, NEO::GemVmControl *));
                break;
            case DRM_IOCTL_I915_GEM_VM_DESTROY:
                res = drmVirtualMemoryDestroy(va_arg(vl, NEO::GemVmControl *));
                break;
            case DRM_IOCTL_VERSION:
                res = drmVersion(va_arg(vl, NEO::DrmVersion *));
                break;
            case DRM_IOCTL_I915_QUERY:
                res = drmQueryItem(va_arg(vl, NEO::Query *));
                break;
            default:
                res = drmOtherRequests(request, vl);
                break;
            }
        }
        va_end(vl);
        return res;
    }

    va_end(vl);
    return -1;
}
