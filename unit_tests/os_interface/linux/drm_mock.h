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

#include "runtime/helpers/options.h"
#include "runtime/helpers/aligned_memory.h"
#include "runtime/os_interface/linux/drm_neo.h"
#include "drm/i915_drm.h"

#include "gtest/gtest.h"

#include <cstdio>
#include <fstream>

using namespace OCLRT;

static const int mockFd = 33;
// Mock DRM class that responds to DRM_IOCTL_I915_GETPARAMs
class DrmMock : public Drm {
  public:
    DrmMock() : Drm(mockFd) {
        sysFsDefaultGpuPathToRestore = nullptr;
    }

    ~DrmMock() {
        if (sysFsDefaultGpuPathToRestore != nullptr) {
            sysFsDefaultGpuPath = sysFsDefaultGpuPathToRestore;
        }
    }
    virtual inline int ioctl(unsigned long request, void *arg) {
        if ((request == DRM_IOCTL_I915_GETPARAM) && (arg != nullptr)) {
            drm_i915_getparam_t *gp = (drm_i915_getparam_t *)arg;
            if (false
#if defined(I915_PARAM_EU_TOTAL)
                || (gp->param == I915_PARAM_EU_TOTAL)
#endif
#if defined(I915_PARAM_EU_COUNT)
                || (gp->param == I915_PARAM_EU_COUNT)
#endif
            ) {
                if (0 == this->StoredRetValForEUVal) {
                    *((int *)(gp->value)) = this->StoredEUVal;
                }
                return this->StoredRetValForEUVal;
            }
            if (false
#if defined(I915_PARAM_SUBSLICE_TOTAL)
                || (gp->param == I915_PARAM_SUBSLICE_TOTAL)
#endif
            ) {
                if (0 == this->StoredRetValForSSVal) {
                    *((int *)(gp->value)) = this->StoredSSVal;
                }
                return this->StoredRetValForSSVal;
            }
            if (false
#if defined(I915_PARAM_CHIPSET_ID)
                || (gp->param == I915_PARAM_CHIPSET_ID)
#endif
            ) {
                if (0 == this->StoredRetValForDeviceID) {
                    *((int *)(gp->value)) = this->StoredDeviceID;
                }
                return this->StoredRetValForDeviceID;
            }
            if (false
#if defined(I915_PARAM_REVISION)
                || (gp->param == I915_PARAM_REVISION)
#endif
            ) {
                if (0 == this->StoredRetValForDeviceRevID) {
                    *((int *)(gp->value)) = this->StoredDeviceRevID;
                }
                return this->StoredRetValForDeviceRevID;
            }
            if (false
#if defined(I915_PARAM_HAS_POOLED_EU)
                || (gp->param == I915_PARAM_HAS_POOLED_EU)
#endif
            ) {
                if (0 == this->StoredRetValForPooledEU) {
                    *((int *)(gp->value)) = this->StoredHasPooledEU;
                }
                return this->StoredRetValForPooledEU;
            }
            if (false
#if defined(I915_PARAM_MIN_EU_IN_POOL)
                || (gp->param == I915_PARAM_MIN_EU_IN_POOL)
#endif
            ) {
                if (0 == this->StoredRetValForMinEUinPool) {
                    *((int *)(gp->value)) = this->StoredMinEUinPool;
                }
                return this->StoredRetValForMinEUinPool;
            }
#if defined(I915_PARAM_HAS_SCHEDULER)
            if (gp->param == I915_PARAM_HAS_SCHEDULER) {
                *((int *)(gp->value)) = this->StoredPreemptionSupport;
                return this->StoredRetVal;
            }
#endif
            if (gp->param == I915_PARAM_HAS_ALIASING_PPGTT) {
                *((int *)(gp->value)) = this->StoredPPGTT;
                return this->StoredRetVal;
            }
            if (gp->param == I915_PARAM_HAS_EXEC_SOFTPIN) {
                *((int *)(gp->value)) = this->StoredExecSoftPin;
                return this->StoredRetVal;
            }
        }
#if defined(I915_PARAM_HAS_SCHEDULER)
        if ((request == DRM_IOCTL_I915_GEM_CONTEXT_CREATE) && (arg != nullptr)) {
            drm_i915_gem_context_create *create = (drm_i915_gem_context_create *)arg;
            create->ctx_id = this->StoredCtxId;
            return this->StoredRetVal;
        }

        if ((request == DRM_IOCTL_I915_GEM_CONTEXT_SETPARAM) && (arg != nullptr)) {
            drm_i915_gem_context_param *gp = (drm_i915_gem_context_param *)arg;
            if ((gp->param == I915_CONTEXT_PARAM_PRIORITY) && (gp->ctx_id == this->StoredCtxId)) {
                EXPECT_EQ(0u, gp->size);
                return this->StoredRetVal;
            }
            if ((gp->param == I915_CONTEXT_PRIVATE_PARAM_BOOST) && (gp->value == 1)) {
                return this->StoredRetVal;
            }
        }
#endif
        if (request == DRM_IOCTL_I915_GEM_EXECBUFFER2) {
            drm_i915_gem_execbuffer2 *execbuf = (drm_i915_gem_execbuffer2 *)arg;
            this->execBuffer = *execbuf;
        }
        if (request == DRM_IOCTL_I915_GEM_USERPTR) {
            auto *userPtrParams = (drm_i915_gem_userptr *)arg;
            userPtrParams->handle = returnHandle;
            returnHandle++;
        }
        if (request == DRM_IOCTL_I915_GEM_CREATE) {
            auto *createParams = (drm_i915_gem_create *)arg;
            this->createParamsSize = createParams->size;
            this->createParamsHandle = createParams->handle = 1u;
        }
        if (request == DRM_IOCTL_I915_GEM_SET_TILING) {
            auto *setTilingParams = (drm_i915_gem_set_tiling *)arg;
            setTilingMode = setTilingParams->tiling_mode;
            setTilingHandle = setTilingParams->handle;
            setTilingStride = setTilingParams->stride;
        }
        if (request == DRM_IOCTL_PRIME_FD_TO_HANDLE) {
            auto *primeToHandleParams = (drm_prime_handle *)arg;
            //return BO
            primeToHandleParams->handle = outputHandle;
            inputFd = primeToHandleParams->fd;
        }
        if (request == DRM_IOCTL_I915_GEM_GET_APERTURE) {
            auto aperture = (drm_i915_gem_get_aperture *)arg;
            aperture->aper_available_size = gpuMemSize;
            aperture->aper_size = gpuMemSize;
        }
        return 0;
    }

    void setSysFsDefaultGpuPath(const char *path) {
        sysFsDefaultGpuPathToRestore = sysFsDefaultGpuPath;
        sysFsDefaultGpuPath = path;
    }

    void writeConfigFile(const char *name, int deviceID) {
        std::ofstream tempfile(name, std::ios::binary);
        if (tempfile.is_open()) {
            PCIConfig config;
            config.DeviceID = deviceID;
            tempfile.write(reinterpret_cast<char *>(&config), sizeof(config));
            tempfile.close();
        }
    }

    void deleteConfigFile(const char *name) {
        std::ofstream tempfile(name);
        if (tempfile.is_open()) {
            tempfile.close();
            remove(name);
        }
    }
    void setFileDescriptor(int fd) {
        this->fd = fd;
    }

    void setDeviceID(int deviceId) { this->deviceId = deviceId; }
    void setDeviceRevID(int revisionId) { this->revisionId = revisionId; }

    bool hasPreemption() {
#if defined(I915_SCHEDULER_CAP_PREEMPTION)
        drm_i915_getparam_t gp;
        int value = 0;
        gp.value = &value;
        gp.param = I915_PARAM_HAS_SCHEDULER;
        int ret = ioctl(DRM_IOCTL_I915_GETPARAM, &gp);

        if (ret == 0 && (value & I915_SCHEDULER_CAP_PREEMPTION)) {
            return contextCreate() && setLowPriority();
        }
        return false;
#else
        return this->StoredMockPreemptionSupport == 1 ? true : false;
#endif
    }

    int StoredEUVal = -1;
    int StoredSSVal = -1;
    int StoredDeviceID = 1;
    int StoredDeviceRevID = 1;
    int StoredHasPooledEU = 1;
    int StoredMinEUinPool = 1;
    int StoredRetVal = 0;
    int StoredRetValForDeviceID = 0;
    int StoredRetValForEUVal = 0;
    int StoredRetValForSSVal = 0;
    int StoredRetValForDeviceRevID = 0;
    int StoredRetValForPooledEU = 0;
    int StoredRetValForMinEUinPool = 0;
    int StoredPPGTT = 3;
    int StoredPreemptionSupport = 7;
    int StoredMockPreemptionSupport = 0;
    int StoredExecSoftPin = 0;
    uint32_t StoredCtxId = 1;

    //DRM_IOCTL_I915_GEM_EXECBUFFER2
    drm_i915_gem_execbuffer2 execBuffer = {0};

    //DRM_IOCTL_I915_GEM_CREATE
    __u64 createParamsSize = 0;
    __u32 createParamsHandle = 0;
    //DRM_IOCTL_I915_GEM_SET_TILING
    __u32 setTilingMode = 0;
    __u32 setTilingHandle = 0;
    __u32 setTilingStride = 0;
    //DRM_IOCTL_PRIME_FD_TO_HANDLE
    __u32 outputHandle = 0;
    __s32 inputFd = 0;
    //DRM_IOCTL_I915_GEM_USERPTR
    __u32 returnHandle = 0;
    __u64 gpuMemSize = 3u * MemoryConstants::gigaByte;

  private:
    const char *sysFsDefaultGpuPathToRestore;
};
