/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "runtime/helpers/aligned_memory.h"
#include "runtime/helpers/options.h"
#include "runtime/os_interface/linux/drm_neo.h"

#include "drm/i915_drm.h"
#include "gtest/gtest.h"

#include <cstdio>
#include <fstream>

using namespace OCLRT;

#if !defined(I915_PARAM_HAS_PREEMPTION)
#define I915_PARAM_HAS_PREEMPTION 0x806
#endif

static const int mockFd = 33;
// Mock DRM class that responds to DRM_IOCTL_I915_GETPARAMs
class DrmMock : public Drm {
  public:
    using Drm::memoryInfo;
    using Drm::preemptionSupported;

    DrmMock() : Drm(mockFd) {
        sysFsDefaultGpuPathToRestore = nullptr;
    }

    ~DrmMock() {
        if (sysFsDefaultGpuPathToRestore != nullptr) {
            sysFsDefaultGpuPath = sysFsDefaultGpuPathToRestore;
        }
    }
    int ioctl(unsigned long request, void *arg) override {
        ioctlCallsCount++;

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
#if defined(I915_PARAM_HAS_PREEMPTION)
            if (gp->param == I915_PARAM_HAS_PREEMPTION) {
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

        if ((request == DRM_IOCTL_I915_GEM_CONTEXT_CREATE) && (arg != nullptr)) {
            drm_i915_gem_context_create *create = (drm_i915_gem_context_create *)arg;
            create->ctx_id = this->StoredCtxId;
            return this->StoredRetVal;
        }

        if ((request == DRM_IOCTL_I915_GEM_CONTEXT_DESTROY) && (arg != nullptr)) {
            drm_i915_gem_context_destroy *destroy = (drm_i915_gem_context_destroy *)arg;
            this->receivedDestroyContextId = destroy->ctx_id;
            return this->StoredRetVal;
        }

        if ((request == DRM_IOCTL_I915_GEM_CONTEXT_SETPARAM) && (arg != nullptr)) {
            receivedContextParamRequestCount++;
            receivedContextParamRequest = *reinterpret_cast<drm_i915_gem_context_param *>(arg);
            if (receivedContextParamRequest.param == I915_CONTEXT_PARAM_PRIORITY) {
                return this->StoredRetVal;
            }
            if ((receivedContextParamRequest.param == I915_CONTEXT_PRIVATE_PARAM_BOOST) && (receivedContextParamRequest.value == 1)) {
                return this->StoredRetVal;
            }
        }

        if (request == DRM_IOCTL_I915_GEM_EXECBUFFER2) {
            drm_i915_gem_execbuffer2 *execbuf = (drm_i915_gem_execbuffer2 *)arg;
            this->execBuffer = *execbuf;
            return 0;
        }
        if (request == DRM_IOCTL_I915_GEM_USERPTR) {
            auto *userPtrParams = (drm_i915_gem_userptr *)arg;
            userPtrParams->handle = returnHandle;
            returnHandle++;
            return 0;
        }
        if (request == DRM_IOCTL_I915_GEM_CREATE) {
            auto *createParams = (drm_i915_gem_create *)arg;
            this->createParamsSize = createParams->size;
            this->createParamsHandle = createParams->handle = 1u;
            return 0;
        }
        if (request == DRM_IOCTL_I915_GEM_SET_TILING) {
            auto *setTilingParams = (drm_i915_gem_set_tiling *)arg;
            setTilingMode = setTilingParams->tiling_mode;
            setTilingHandle = setTilingParams->handle;
            setTilingStride = setTilingParams->stride;
            return 0;
        }
        if (request == DRM_IOCTL_PRIME_FD_TO_HANDLE) {
            auto *primeToHandleParams = (drm_prime_handle *)arg;
            //return BO
            primeToHandleParams->handle = outputHandle;
            inputFd = primeToHandleParams->fd;
            return 0;
        }
        if (request == DRM_IOCTL_I915_GEM_GET_APERTURE) {
            auto aperture = (drm_i915_gem_get_aperture *)arg;
            aperture->aper_available_size = gpuMemSize;
            aperture->aper_size = gpuMemSize;
            return 0;
        }
        if (request == DRM_IOCTL_I915_GEM_MMAP) {
            auto mmap_arg = static_cast<drm_i915_gem_mmap *>(arg);
            mmap_arg->addr_ptr = reinterpret_cast<__u64>(lockedPtr);
            return 0;
        }
        if (request == DRM_IOCTL_I915_GEM_WAIT) {
            return 0;
        }

        return handleRemainingRequests(request, arg);
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

    void checkPreemptionSupport() override {
        preemptionSupported = StoredPreemptionSupport;
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
    int StoredPreemptionSupport = 0;
    int StoredExecSoftPin = 0;
    uint32_t StoredCtxId = 1;
    uint32_t receivedDestroyContextId = 0;
    uint32_t ioctlCallsCount = 0;

    uint32_t receivedContextParamRequestCount = 0;
    drm_i915_gem_context_param receivedContextParamRequest = {};

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
    //DRM_IOCTL_I915_GEM_MMAP
    uint64_t lockedPtr[4];

    virtual int handleRemainingRequests(unsigned long request, void *arg);

  private:
    const char *sysFsDefaultGpuPathToRestore;
};
