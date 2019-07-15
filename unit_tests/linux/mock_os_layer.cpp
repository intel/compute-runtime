/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "mock_os_layer.h"

#include <cassert>
#include <iostream>

int (*c_open)(const char *pathname, int flags, ...) = nullptr;
int (*c_ioctl)(int fd, unsigned long int request, ...) = nullptr;

int fakeFd = 1023;
int haveDri = 0;                                       // index of dri to serve, -1 - none
int deviceId = NEO::deviceDescriptorTable[0].deviceId; // default supported DeviceID
int haveSoftPin = 1;
int havePreemption = I915_SCHEDULER_CAP_ENABLED |
                     I915_SCHEDULER_CAP_PRIORITY |
                     I915_SCHEDULER_CAP_PREEMPTION;
int failOnDeviceId = 0;
int failOnRevisionId = 0;
int failOnSoftPin = 0;
int failOnParamBoost = 0;
int failOnContextCreate = 0;
int failOnSetPriority = 0;
int failOnPreemption = 0;
int failOnDrmVersion = 0;
char providedDrmVersion[5] = {'i', '9', '1', '5', '\0'};
uint64_t gpuTimestamp = 0;
int ioctlSeq[8] = {0, 0, 0, 0, 0, 0, 0, 0};
size_t ioctlCnt = 0;

int open(const char *pathname, int flags, ...) {
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

int drmGetParam(drm_i915_getparam_t *param) {
    assert(param);
    int ret = 0;

    switch (param->param) {
    case I915_PARAM_CHIPSET_ID:
        *param->value = deviceId;
        ret = failOnDeviceId;
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
        std::cerr << "drm.getParam: " << std::dec << param->param << std::endl;
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
#if defined(I915_PARAM_HAS_SCHEDULER)
    case I915_CONTEXT_PARAM_PRIORITY:
        ret = failOnSetPriority;
        break;
#endif
    default:
        ret = -1;
        std::cerr << "drm.setContextParam: " << std::dec << param->param << std::endl;
        break;
    }
    return ret;
}

int drmContextCreate(drm_i915_gem_context_create *create) {
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

int drmVersion(drm_version_t *version) {
    strcpy(version->name, providedDrmVersion);

    return failOnDrmVersion;
}

int ioctl(int fd, unsigned long int request, ...) throw() {
    if (c_ioctl == nullptr)
        c_ioctl = (int (*)(int, unsigned long int, ...))dlsym(RTLD_NEXT, "ioctl");
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
            case DRM_IOCTL_I915_GEM_CONTEXT_CREATE:
                res = drmContextCreate(va_arg(vl, drm_i915_gem_context_create *));
                break;
            case DRM_IOCTL_I915_GEM_CONTEXT_DESTROY:
                res = drmContextDestroy(va_arg(vl, drm_i915_gem_context_destroy *));
                break;
            case DRM_IOCTL_VERSION:
                res = drmVersion(va_arg(vl, drm_version_t *));
                break;
            }
        }
        va_end(vl);
        return res;
    }

    res = c_ioctl(fd, request, vl);
    va_end(vl);
    return res;
}
