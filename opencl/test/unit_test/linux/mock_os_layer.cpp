/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "mock_os_layer.h"

#include "shared/source/helpers/string.h"

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

int drmGetParam(drm_i915_getparam_t *param) {
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
        *param->value = 0x0;
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

int drmSetContextParam(drm_i915_gem_context_param *param) {
    assert(param);
    int ret = 0;

    switch (param->param) {
    case I915_CONTEXT_PRIVATE_PARAM_BOOST:
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
        if (param->size == sizeof(struct drm_i915_gem_context_param_sseu) && param->value != 0 && param->ctx_id == 0) {
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
int drmGetContextParam(drm_i915_gem_context_param *param) {
    int ret = 0;

    switch (param->param) {
    case I915_CONTEXT_PARAM_SSEU:
        if (param->size == sizeof(struct drm_i915_gem_context_param_sseu) && param->value != 0 && param->ctx_id == 0) {
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

int drmContextCreate(drm_i915_gem_context_create_ext *create) {
    assert(create);

    create->ctx_id = 1;
    return failOnContextCreate;
}

int drmContextDestroy(drm_i915_gem_context_destroy *destroy) {
    assert(destroy);

    if (destroy->ctx_id == 1)
        return 0;
    else
        return -1;
}

int drmVirtualMemoryCreate(drm_i915_gem_vm_control *control) {
    assert(control);
    control->vm_id = ++vmId;
    return failOnVirtualMemoryCreate;
}

int drmVirtualMemoryDestroy(drm_i915_gem_vm_control *control) {
    assert(control);

    vmId--;
    return (control->vm_id > 0) ? 0 : -1;
}

int drmVersion(drm_version_t *version) {
    strcpy(version->name, providedDrmVersion);

    return failOnDrmVersion;
}

int drmQueryItem(drm_i915_query *query) {
    auto queryItemArg = reinterpret_cast<drm_i915_query_item *>(query->items_ptr);
    if (queryItemArg->length == 0) {
        if (queryItemArg->query_id == DRM_I915_QUERY_TOPOLOGY_INFO) {
            queryItemArg->length = sizeof(drm_i915_query_topology_info) + 1;
            return 0;
        }
    } else {
        if (queryItemArg->query_id == DRM_I915_QUERY_TOPOLOGY_INFO) {
            auto topologyArg = reinterpret_cast<drm_i915_query_topology_info *>(queryItemArg->data_ptr);
            topologyArg->max_slices = 1;
            topologyArg->max_subslices = 1;
            topologyArg->max_eus_per_subslice = 3;
            topologyArg->data[0] = 0xFF;
            return failOnEuTotal || failOnSubsliceTotal;
        }
    }
    return drmOtherRequests(DRM_IOCTL_I915_QUERY, query);
}

int ioctl(int fd, unsigned long int request, ...) throw() {
    int res;
    va_list vl;
    va_start(vl, request);
    if (fd == fakeFd) {
        res = ioctlSeq[ioctlCnt % (sizeof(ioctlSeq) / sizeof(int))];
        ioctlCnt++;

        if (res == 0) {
            switch (request) {
            case DRM_IOCTL_I915_GETPARAM:
                res = drmGetParam(va_arg(vl, drm_i915_getparam_t *));
                break;
            case DRM_IOCTL_I915_GEM_CONTEXT_SETPARAM:
                res = drmSetContextParam(va_arg(vl, drm_i915_gem_context_param *));
                break;
            case DRM_IOCTL_I915_GEM_CONTEXT_GETPARAM:
                res = drmGetContextParam(va_arg(vl, drm_i915_gem_context_param *));
                break;
            case DRM_IOCTL_I915_GEM_CONTEXT_CREATE_EXT:
                res = drmContextCreate(va_arg(vl, drm_i915_gem_context_create_ext *));
                break;
            case DRM_IOCTL_I915_GEM_CONTEXT_DESTROY:
                res = drmContextDestroy(va_arg(vl, drm_i915_gem_context_destroy *));
                break;
            case DRM_IOCTL_I915_GEM_VM_CREATE:
                res = drmVirtualMemoryCreate(va_arg(vl, drm_i915_gem_vm_control *));
                break;
            case DRM_IOCTL_I915_GEM_VM_DESTROY:
                res = drmVirtualMemoryDestroy(va_arg(vl, drm_i915_gem_vm_control *));
                break;
            case DRM_IOCTL_VERSION:
                res = drmVersion(va_arg(vl, drm_version_t *));
                break;
            case DRM_IOCTL_I915_QUERY:
                res = drmQueryItem(va_arg(vl, drm_i915_query *));
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
