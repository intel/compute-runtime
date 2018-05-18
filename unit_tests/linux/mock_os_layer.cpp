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

#include "mock_os_layer.h"
#include <cassert>
#include <iostream>
const char *devDri[2] = {"/dev/dri/renderD128", "/dev/dri/card0"};

int (*c_open)(const char *pathname, int flags, ...) = nullptr;
int (*c_ioctl)(int fd, unsigned long int request, ...) = nullptr;

int fakeFd = 1023;
int haveDri = 0;                                         // index of dri to serve, -1 - none
int deviceId = OCLRT::deviceDescriptorTable[0].deviceId; // default supported DeviceID
int haveSoftPin = 1;
int havePreemption = 1;
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

    for (int i = 0; i < 2; i++) {
        if (strcmp(devDri[i], pathname) == 0) {
            if (i == haveDri) {
                return fakeFd;
            } else {
                return -1;
            }
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
#if defined(I915_PARAM_HAS_PREEMPTION)
    case I915_PARAM_HAS_PREEMPTION:
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
#if defined(I915_PARAM_HAS_PREEMPTION)
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

    va_list vl;
    va_start(vl, request);

    if (fd == fakeFd) {
        auto res = ioctlSeq[ioctlCnt % (sizeof(ioctlSeq) / sizeof(int))];
        ioctlCnt++;

        if (res == 0) {
            switch (request) {
            case DRM_IOCTL_I915_GETPARAM:
                return drmGetParam(va_arg(vl, drm_i915_getparam_t *));
            case DRM_IOCTL_I915_GEM_CONTEXT_SETPARAM:
                return drmSetContextParam(va_arg(vl, drm_i915_gem_context_param *));
            case DRM_IOCTL_I915_GEM_CONTEXT_CREATE:
                return drmContextCreate(va_arg(vl, drm_i915_gem_context_create *));
            case DRM_IOCTL_I915_GEM_CONTEXT_DESTROY:
                return drmContextDestroy(va_arg(vl, drm_i915_gem_context_destroy *));
            case DRM_IOCTL_VERSION:
                return drmVersion(va_arg(vl, drm_version_t *));
            }
        }

        return res;
    }

    return c_ioctl(fd, request, vl);
}
