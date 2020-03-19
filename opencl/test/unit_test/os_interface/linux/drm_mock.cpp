/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/os_interface/linux/drm_mock.h"

const int DrmMock::mockFd;

int DrmMock::ioctl(unsigned long request, void *arg) {
    ioctlCallsCount++;

    if ((request == DRM_IOCTL_I915_GETPARAM) && (arg != nullptr)) {
        auto gp = static_cast<drm_i915_getparam_t *>(arg);
        if (gp->param == I915_PARAM_EU_TOTAL) {
            if (0 == this->StoredRetValForEUVal) {
                *gp->value = this->StoredEUVal;
            }
            return this->StoredRetValForEUVal;
        }
        if (gp->param == I915_PARAM_SUBSLICE_TOTAL) {
            if (0 == this->StoredRetValForSSVal) {
                *gp->value = this->StoredSSVal;
            }
            return this->StoredRetValForSSVal;
        }
        if (gp->param == I915_PARAM_CHIPSET_ID) {
            if (0 == this->StoredRetValForDeviceID) {
                *gp->value = this->StoredDeviceID;
            }
            return this->StoredRetValForDeviceID;
        }
        if (gp->param == I915_PARAM_REVISION) {
            if (0 == this->StoredRetValForDeviceRevID) {
                *gp->value = this->StoredDeviceRevID;
            }
            return this->StoredRetValForDeviceRevID;
        }
        if (gp->param == I915_PARAM_HAS_POOLED_EU) {
            if (0 == this->StoredRetValForPooledEU) {
                *gp->value = this->StoredHasPooledEU;
            }
            return this->StoredRetValForPooledEU;
        }
        if (gp->param == I915_PARAM_MIN_EU_IN_POOL) {
            if (0 == this->StoredRetValForMinEUinPool) {
                *gp->value = this->StoredMinEUinPool;
            }
            return this->StoredRetValForMinEUinPool;
        }
        if (gp->param == I915_PARAM_HAS_SCHEDULER) {
            *gp->value = this->StoredPreemptionSupport;
            return this->StoredRetVal;
        }
        if (gp->param == I915_PARAM_HAS_EXEC_SOFTPIN) {
            *gp->value = this->StoredExecSoftPin;
            return this->StoredRetVal;
        }
    }

    if ((request == DRM_IOCTL_I915_GEM_CONTEXT_CREATE) && (arg != nullptr)) {
        auto create = static_cast<drm_i915_gem_context_create *>(arg);
        create->ctx_id = this->StoredCtxId;
        return this->StoredRetVal;
    }

    if ((request == DRM_IOCTL_I915_GEM_CONTEXT_DESTROY) && (arg != nullptr)) {
        auto destroy = static_cast<drm_i915_gem_context_destroy *>(arg);
        this->receivedDestroyContextId = destroy->ctx_id;
        return this->StoredRetVal;
    }

    if ((request == DRM_IOCTL_I915_GEM_CONTEXT_SETPARAM) && (arg != nullptr)) {
        receivedContextParamRequestCount++;
        receivedContextParamRequest = *static_cast<drm_i915_gem_context_param *>(arg);
        if (receivedContextParamRequest.param == I915_CONTEXT_PARAM_PRIORITY) {
            return this->StoredRetVal;
        }
        if ((receivedContextParamRequest.param == I915_CONTEXT_PRIVATE_PARAM_BOOST) && (receivedContextParamRequest.value == 1)) {
            return this->StoredRetVal;
        }
        if (receivedContextParamRequest.param == I915_CONTEXT_PARAM_SSEU) {
            if (StoredRetValForSetSSEU == 0) {
                storedParamSseu = (*static_cast<drm_i915_gem_context_param_sseu *>(reinterpret_cast<void *>(receivedContextParamRequest.value))).slice_mask;
            }
            return this->StoredRetValForSetSSEU;
        }
        if (receivedContextParamRequest.param == I915_CONTEXT_PARAM_PERSISTENCE) {
            return this->StoredRetValForPersistant;
        }
    }

    if ((request == DRM_IOCTL_I915_GEM_CONTEXT_GETPARAM) && (arg != nullptr)) {
        receivedContextParamRequestCount++;
        receivedContextParamRequest = *static_cast<drm_i915_gem_context_param *>(arg);
        if (receivedContextParamRequest.param == I915_CONTEXT_PARAM_GTT_SIZE) {
            static_cast<drm_i915_gem_context_param *>(arg)->value = this->storedGTTSize;
            return this->StoredRetValForGetGttSize;
        }
        if (receivedContextParamRequest.param == I915_CONTEXT_PARAM_SSEU) {
            if (StoredRetValForGetSSEU == 0) {
                (*static_cast<drm_i915_gem_context_param_sseu *>(reinterpret_cast<void *>(receivedContextParamRequest.value))).slice_mask = storedParamSseu;
            }
            return this->StoredRetValForGetSSEU;
        }
        if (receivedContextParamRequest.param == I915_CONTEXT_PARAM_PERSISTENCE) {
            static_cast<drm_i915_gem_context_param *>(arg)->value = this->StoredPersistentContextsSupport;
            return this->StoredRetValForPersistant;
        }
    }

    if (request == DRM_IOCTL_I915_GEM_EXECBUFFER2) {
        auto execbuf = static_cast<drm_i915_gem_execbuffer2 *>(arg);
        this->execBuffer = *execbuf;
        return 0;
    }
    if (request == DRM_IOCTL_I915_GEM_USERPTR) {
        auto userPtrParams = static_cast<drm_i915_gem_userptr *>(arg);
        userPtrParams->handle = returnHandle;
        returnHandle++;
        return 0;
    }
    if (request == DRM_IOCTL_I915_GEM_CREATE) {
        auto createParams = static_cast<drm_i915_gem_create *>(arg);
        this->createParamsSize = createParams->size;
        this->createParamsHandle = createParams->handle = 1u;
        if (0 == this->createParamsSize) {
            return EINVAL;
        }
        return 0;
    }
    if (request == DRM_IOCTL_I915_GEM_SET_TILING) {
        auto setTilingParams = static_cast<drm_i915_gem_set_tiling *>(arg);
        setTilingMode = setTilingParams->tiling_mode;
        setTilingHandle = setTilingParams->handle;
        setTilingStride = setTilingParams->stride;
        return 0;
    }
    if (request == DRM_IOCTL_PRIME_FD_TO_HANDLE) {
        auto primeToHandleParams = static_cast<drm_prime_handle *>(arg);
        //return BO
        primeToHandleParams->handle = outputHandle;
        inputFd = primeToHandleParams->fd;
        return 0;
    }
    if (request == DRM_IOCTL_I915_GEM_GET_APERTURE) {
        auto aperture = static_cast<drm_i915_gem_get_aperture *>(arg);
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
