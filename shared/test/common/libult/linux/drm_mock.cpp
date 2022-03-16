/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/libult/linux/drm_mock.h"

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/hw_info.h"

#include "drm/i915_drm.h"
#include "gtest/gtest.h"

#include <cstring>

const int DrmMock::mockFd;
const uint32_t DrmMockResources::registerResourceReturnHandle = 3;

int DrmMock::ioctl(unsigned long request, void *arg) {
    ioctlCallsCount++;
    ioctlCount.total++;

    if ((request == DRM_IOCTL_I915_GETPARAM) && (arg != nullptr)) {
        ioctlCount.contextGetParam++;
        auto gp = static_cast<drm_i915_getparam_t *>(arg);
        if (gp->param == I915_PARAM_EU_TOTAL) {
            if (0 == this->storedRetValForEUVal) {
                *gp->value = this->storedEUVal;
            }
            return this->storedRetValForEUVal;
        }
        if (gp->param == I915_PARAM_SUBSLICE_TOTAL) {
            if (0 == this->storedRetValForSSVal) {
                *gp->value = this->storedSSVal;
            }
            return this->storedRetValForSSVal;
        }
        if (gp->param == I915_PARAM_CHIPSET_ID) {
            if (0 == this->storedRetValForDeviceID) {
                *gp->value = this->storedDeviceID;
            }
            return this->storedRetValForDeviceID;
        }
        if (gp->param == I915_PARAM_REVISION) {
            if (0 == this->storedRetValForDeviceRevID) {
                *gp->value = this->storedDeviceRevID;
            }
            return this->storedRetValForDeviceRevID;
        }
        if (gp->param == I915_PARAM_HAS_POOLED_EU) {
            if (0 == this->storedRetValForPooledEU) {
                *gp->value = this->storedHasPooledEU;
            }
            return this->storedRetValForPooledEU;
        }
        if (gp->param == I915_PARAM_MIN_EU_IN_POOL) {
            if (0 == this->storedRetValForMinEUinPool) {
                *gp->value = this->storedMinEUinPool;
            }
            return this->storedRetValForMinEUinPool;
        }
        if (gp->param == I915_PARAM_HAS_SCHEDULER) {
            *gp->value = this->storedPreemptionSupport;
            return this->storedRetVal;
        }
        if (gp->param == I915_PARAM_HAS_EXEC_SOFTPIN) {
            *gp->value = this->storedExecSoftPin;
            return this->storedRetVal;
        }
        if (gp->param == I915_PARAM_CS_TIMESTAMP_FREQUENCY) {
            *gp->value = this->storedCsTimestampFrequency;
            return this->storedRetVal;
        }
    }

    if ((request == DRM_IOCTL_I915_GEM_CONTEXT_CREATE_EXT) && (arg != nullptr)) {
        ioctlCount.contextCreate++;
        auto create = static_cast<drm_i915_gem_context_create_ext *>(arg);
        auto contextCreate = static_cast<drm_i915_gem_context_create *>(arg);
        contextCreate->ctx_id = this->storedDrmContextId;
        this->receivedContextCreateFlags = create->flags;
        if (create->extensions == 0) {
            return this->storedRetVal;
        }
        receivedContextCreateSetParam = *reinterpret_cast<drm_i915_gem_context_create_ext_setparam *>(create->extensions);
        if (receivedContextCreateSetParam.base.name == I915_CONTEXT_CREATE_EXT_SETPARAM) {
            receivedContextParamRequestCount++;
            receivedContextParamRequest = receivedContextCreateSetParam.param;
            if (receivedContextCreateSetParam.param.param == I915_CONTEXT_PARAM_VM) {
                return this->storedRetVal;
            }
        }
    }

    if ((request == DRM_IOCTL_I915_GEM_CONTEXT_DESTROY) && (arg != nullptr)) {
        ioctlCount.contextDestroy++;
        auto destroy = static_cast<drm_i915_gem_context_destroy *>(arg);
        this->receivedDestroyContextId = destroy->ctx_id;
        return this->storedRetVal;
    }

    if ((request == DRM_IOCTL_I915_GEM_CONTEXT_SETPARAM) && (arg != nullptr)) {
        ioctlCount.contextSetParam++;
        receivedContextParamRequestCount++;
        receivedContextParamRequest = *static_cast<drm_i915_gem_context_param *>(arg);
        if (receivedContextParamRequest.param == I915_CONTEXT_PARAM_PRIORITY) {
            return this->storedRetVal;
        }
        if ((receivedContextParamRequest.param == I915_CONTEXT_PRIVATE_PARAM_BOOST) && (receivedContextParamRequest.value == 1)) {
            return this->storedRetVal;
        }
        if (receivedContextParamRequest.param == I915_CONTEXT_PARAM_SSEU) {
            if (storedRetValForSetSSEU == 0) {
                storedParamSseu = (*static_cast<drm_i915_gem_context_param_sseu *>(reinterpret_cast<void *>(receivedContextParamRequest.value))).slice_mask;
            }
            return this->storedRetValForSetSSEU;
        }
        if (receivedContextParamRequest.param == I915_CONTEXT_PARAM_PERSISTENCE) {
            return this->storedRetValForPersistant;
        }
        if (receivedContextParamRequest.param == I915_CONTEXT_PARAM_VM) {
            return this->storedRetVal;
        }
        if (receivedContextParamRequest.param == I915_CONTEXT_PARAM_RECOVERABLE) {
            receivedRecoverableContextValue = receivedContextParamRequest.value;
            return this->storedRetVal;
        }
    }

    if ((request == DRM_IOCTL_I915_GEM_CONTEXT_GETPARAM) && (arg != nullptr)) {
        ioctlCount.contextGetParam++;
        receivedContextParamRequestCount++;
        receivedContextParamRequest = *static_cast<drm_i915_gem_context_param *>(arg);
        if (receivedContextParamRequest.param == I915_CONTEXT_PARAM_GTT_SIZE) {
            static_cast<drm_i915_gem_context_param *>(arg)->value = this->storedGTTSize;
            return this->storedRetValForGetGttSize;
        }
        if (receivedContextParamRequest.param == I915_CONTEXT_PARAM_SSEU) {
            if (storedRetValForGetSSEU == 0) {
                (*static_cast<drm_i915_gem_context_param_sseu *>(reinterpret_cast<void *>(receivedContextParamRequest.value))).slice_mask = storedParamSseu;
            }
            return this->storedRetValForGetSSEU;
        }
        if (receivedContextParamRequest.param == I915_CONTEXT_PARAM_PERSISTENCE) {
            static_cast<drm_i915_gem_context_param *>(arg)->value = this->storedPersistentContextsSupport;
            return this->storedRetValForPersistant;
        }

        if (receivedContextParamRequest.param == I915_CONTEXT_PARAM_VM) {
            static_cast<drm_i915_gem_context_param *>(arg)->value = this->storedRetValForVmId;
            return 0u;
        }
    }

    if (request == DRM_IOCTL_I915_GEM_EXECBUFFER2) {
        ioctlCount.execbuffer2++;
        auto execbuf = static_cast<drm_i915_gem_execbuffer2 *>(arg);
        auto execObjects = reinterpret_cast<const drm_i915_gem_exec_object2 *>(execbuf->buffers_ptr);
        this->execBuffers.push_back(*execbuf);
        for (uint32_t i = 0; i < execbuf->buffer_count; i++) {
            this->receivedBos.push_back(execObjects[i]);
        }
        return 0;
    }
    if (request == DRM_IOCTL_I915_GEM_USERPTR) {
        ioctlCount.gemUserptr++;
        auto userPtrParams = static_cast<drm_i915_gem_userptr *>(arg);
        userPtrParams->handle = returnHandle;
        returnHandle++;
        return 0;
    }
    if (request == DRM_IOCTL_I915_GEM_CREATE) {
        ioctlCount.gemCreate++;
        auto createParams = static_cast<drm_i915_gem_create *>(arg);
        this->createParamsSize = createParams->size;
        this->createParamsHandle = createParams->handle = 1u;
        if (0 == this->createParamsSize) {
            return EINVAL;
        }
        return 0;
    }
    if (request == DRM_IOCTL_I915_GEM_SET_TILING) {
        ioctlCount.gemSetTiling++;
        auto setTilingParams = static_cast<drm_i915_gem_set_tiling *>(arg);
        setTilingMode = setTilingParams->tiling_mode;
        setTilingHandle = setTilingParams->handle;
        setTilingStride = setTilingParams->stride;
        return 0;
    }
    if (request == DRM_IOCTL_PRIME_FD_TO_HANDLE) {
        ioctlCount.primeFdToHandle++;
        auto primeToHandleParams = static_cast<drm_prime_handle *>(arg);
        //return BO
        primeToHandleParams->handle = outputHandle;
        inputFd = primeToHandleParams->fd;
        return fdToHandleRetVal;
    }
    if (request == DRM_IOCTL_PRIME_HANDLE_TO_FD) {
        ioctlCount.handleToPrimeFd++;
        auto primeToFdParams = static_cast<drm_prime_handle *>(arg);
        primeToFdParams->fd = outputFd;
        return 0;
    }
    if (request == DRM_IOCTL_I915_GEM_GET_APERTURE) {
        ioctlCount.gemGetAperture++;
        auto aperture = static_cast<drm_i915_gem_get_aperture *>(arg);
        aperture->aper_available_size = gpuMemSize;
        aperture->aper_size = gpuMemSize;
        return 0;
    }
    if (request == DRM_IOCTL_I915_GEM_MMAP) {
        ioctlCount.gemMmap++;
        auto mmap_arg = static_cast<drm_i915_gem_mmap *>(arg);
        mmap_arg->addr_ptr = reinterpret_cast<__u64>(lockedPtr);
        return 0;
    }
    if (request == DRM_IOCTL_I915_GEM_WAIT) {
        ioctlCount.gemWait++;
        receivedGemWait = *static_cast<drm_i915_gem_wait *>(arg);
        return 0;
    }
    if (request == DRM_IOCTL_GEM_CLOSE) {
        ioctlCount.gemClose++;
        return storedRetValForGemClose;
    }
    if (request == DRM_IOCTL_I915_GET_RESET_STATS && arg != nullptr) {
        ioctlCount.gemResetStats++;
        auto outResetStats = static_cast<drm_i915_reset_stats *>(arg);
        for (const auto &resetStats : resetStatsToReturn) {
            if (resetStats.ctx_id == outResetStats->ctx_id) {
                *outResetStats = resetStats;
                return 0;
            }
        }

        return -1;
    }

    if (request == DRM_IOCTL_I915_QUERY && arg != nullptr) {
        ioctlCount.query++;
        auto queryArg = static_cast<drm_i915_query *>(arg);
        auto queryItemArg = reinterpret_cast<drm_i915_query_item *>(queryArg->items_ptr);
        storedQueryItem = *queryItemArg;

        auto realEuCount = rootDeviceEnvironment.getHardwareInfo()->gtSystemInfo.EUCount;
        auto dataSize = static_cast<size_t>(std::ceil(realEuCount / 8.0));

        if (queryItemArg->length == 0) {
            if (queryItemArg->query_id == DRM_I915_QUERY_TOPOLOGY_INFO) {
                queryItemArg->length = static_cast<int32_t>(sizeof(drm_i915_query_topology_info) + dataSize);
                return 0;
            }
        } else {
            if (queryItemArg->query_id == DRM_I915_QUERY_TOPOLOGY_INFO) {
                auto topologyArg = reinterpret_cast<drm_i915_query_topology_info *>(queryItemArg->data_ptr);
                if (this->failRetTopology) {
                    return -1;
                }
                topologyArg->max_slices = this->storedSVal;
                topologyArg->max_subslices = this->storedSVal ? (this->storedSSVal / this->storedSVal) : 0;
                topologyArg->max_eus_per_subslice = this->storedSSVal ? (this->storedEUVal / this->storedSSVal) : 0;

                if (this->disableSomeTopology) {
                    memset(topologyArg->data, 0xCA, dataSize);
                } else {
                    memset(topologyArg->data, 0xFF, dataSize);
                }

                return 0;
            }
        }
    }

    return handleRemainingRequests(request, arg);
}
int DrmMock::waitUserFence(uint32_t ctxIdx, uint64_t address, uint64_t value, ValueWidth dataWidth, int64_t timeout, uint16_t flags) {
    waitUserFenceParams.push_back({ctxIdx, address, value, dataWidth, timeout, flags});
    return Drm::waitUserFence(ctxIdx, address, value, dataWidth, timeout, flags);
}
int DrmMockEngine::handleRemainingRequests(unsigned long request, void *arg) {
    if ((request == DRM_IOCTL_I915_QUERY) && (arg != nullptr)) {
        if (i915QuerySuccessCount == 0) {
            return EINVAL;
        }
        i915QuerySuccessCount--;
        auto query = static_cast<drm_i915_query *>(arg);
        if (query->items_ptr == 0) {
            return EINVAL;
        }
        for (auto i = 0u; i < query->num_items; i++) {
            handleQueryItem(reinterpret_cast<drm_i915_query_item *>(query->items_ptr) + i);
        }
        return 0;
    }
    return -1;
}

std::map<unsigned long, const char *> ioctlCodeStringMap = {
    {DRM_IOCTL_I915_INIT, "DRM_IOCTL_I915_INIT"},
    {DRM_IOCTL_I915_FLUSH, "DRM_IOCTL_I915_FLUSH"},
    {DRM_IOCTL_I915_FLIP, "DRM_IOCTL_I915_FLIP"},
    {DRM_IOCTL_GEM_CLOSE, "DRM_IOCTL_GEM_CLOSE"},
    {DRM_IOCTL_I915_BATCHBUFFER, "DRM_IOCTL_I915_BATCHBUFFER"},
    {DRM_IOCTL_I915_IRQ_EMIT, "DRM_IOCTL_I915_IRQ_EMIT"},
    {DRM_IOCTL_I915_IRQ_WAIT, "DRM_IOCTL_I915_IRQ_WAIT"},
    {DRM_IOCTL_I915_GETPARAM, "DRM_IOCTL_I915_GETPARAM"},
    {DRM_IOCTL_I915_SETPARAM, "DRM_IOCTL_I915_SETPARAM"},
    {DRM_IOCTL_I915_ALLOC, "DRM_IOCTL_I915_ALLOC"},
    {DRM_IOCTL_I915_FREE, "DRM_IOCTL_I915_FREE"},
    {DRM_IOCTL_I915_INIT_HEAP, "DRM_IOCTL_I915_INIT_HEAP"},
    {DRM_IOCTL_I915_CMDBUFFER, "DRM_IOCTL_I915_CMDBUFFER"},
    {DRM_IOCTL_I915_DESTROY_HEAP, "DRM_IOCTL_I915_DESTROY_HEAP"},
    {DRM_IOCTL_I915_SET_VBLANK_PIPE, "DRM_IOCTL_I915_SET_VBLANK_PIPE"},
    {DRM_IOCTL_I915_GET_VBLANK_PIPE, "DRM_IOCTL_I915_GET_VBLANK_PIPE"},
    {DRM_IOCTL_I915_VBLANK_SWAP, "DRM_IOCTL_I915_VBLANK_SWAP"},
    {DRM_IOCTL_I915_HWS_ADDR, "DRM_IOCTL_I915_HWS_ADDR"},
    {DRM_IOCTL_I915_GEM_INIT, "DRM_IOCTL_I915_GEM_INIT"},
    {DRM_IOCTL_I915_GEM_EXECBUFFER, "DRM_IOCTL_I915_GEM_EXECBUFFER"},
    {DRM_IOCTL_I915_GEM_EXECBUFFER2, "DRM_IOCTL_I915_GEM_EXECBUFFER2"},
    {DRM_IOCTL_I915_GEM_EXECBUFFER2_WR, "DRM_IOCTL_I915_GEM_EXECBUFFER2_WR"},
    {DRM_IOCTL_I915_GEM_PIN, "DRM_IOCTL_I915_GEM_PIN"},
    {DRM_IOCTL_I915_GEM_UNPIN, "DRM_IOCTL_I915_GEM_UNPIN"},
    {DRM_IOCTL_I915_GEM_BUSY, "DRM_IOCTL_I915_GEM_BUSY"},
    {DRM_IOCTL_I915_GEM_SET_CACHING, "DRM_IOCTL_I915_GEM_SET_CACHING"},
    {DRM_IOCTL_I915_GEM_GET_CACHING, "DRM_IOCTL_I915_GEM_GET_CACHING"},
    {DRM_IOCTL_I915_GEM_THROTTLE, "DRM_IOCTL_I915_GEM_THROTTLE"},
    {DRM_IOCTL_I915_GEM_ENTERVT, "DRM_IOCTL_I915_GEM_ENTERVT"},
    {DRM_IOCTL_I915_GEM_LEAVEVT, "DRM_IOCTL_I915_GEM_LEAVEVT"},
    {DRM_IOCTL_I915_GEM_CREATE, "DRM_IOCTL_I915_GEM_CREATE"},
    {DRM_IOCTL_I915_GEM_PREAD, "DRM_IOCTL_I915_GEM_PREAD"},
    {DRM_IOCTL_I915_GEM_PWRITE, "DRM_IOCTL_I915_GEM_PWRITE"},
    {DRM_IOCTL_I915_GEM_SET_DOMAIN, "DRM_IOCTL_I915_GEM_SET_DOMAIN"},
    {DRM_IOCTL_I915_GEM_SW_FINISH, "DRM_IOCTL_I915_GEM_SW_FINISH"},
    {DRM_IOCTL_I915_GEM_SET_TILING, "DRM_IOCTL_I915_GEM_SET_TILING"},
    {DRM_IOCTL_I915_GEM_GET_TILING, "DRM_IOCTL_I915_GEM_GET_TILING"},
    {DRM_IOCTL_I915_GEM_GET_APERTURE, "DRM_IOCTL_I915_GEM_GET_APERTURE"},
    {DRM_IOCTL_I915_GET_PIPE_FROM_CRTC_ID, "DRM_IOCTL_I915_GET_PIPE_FROM_CRTC_ID"},
    {DRM_IOCTL_I915_GEM_MADVISE, "DRM_IOCTL_I915_GEM_MADVISE"},
    {DRM_IOCTL_I915_OVERLAY_PUT_IMAGE, "DRM_IOCTL_I915_OVERLAY_PUT_IMAGE"},
    {DRM_IOCTL_I915_OVERLAY_ATTRS, "DRM_IOCTL_I915_OVERLAY_ATTRS"},
    {DRM_IOCTL_I915_SET_SPRITE_COLORKEY, "DRM_IOCTL_I915_SET_SPRITE_COLORKEY"},
    {DRM_IOCTL_I915_GET_SPRITE_COLORKEY, "DRM_IOCTL_I915_GET_SPRITE_COLORKEY"},
    {DRM_IOCTL_I915_GEM_WAIT, "DRM_IOCTL_I915_GEM_WAIT"},
    {DRM_IOCTL_I915_GEM_CONTEXT_CREATE, "DRM_IOCTL_I915_GEM_CONTEXT_CREATE"},
    {DRM_IOCTL_I915_GEM_CONTEXT_CREATE_EXT, "DRM_IOCTL_I915_GEM_CONTEXT_CREATE_EXT"},
    {DRM_IOCTL_I915_GEM_CONTEXT_DESTROY, "DRM_IOCTL_I915_GEM_CONTEXT_DESTROY"},
    {DRM_IOCTL_I915_REG_READ, "DRM_IOCTL_I915_REG_READ"},
    {DRM_IOCTL_I915_GET_RESET_STATS, "DRM_IOCTL_I915_GET_RESET_STATS"},
    {DRM_IOCTL_I915_GEM_USERPTR, "DRM_IOCTL_I915_GEM_USERPTR"},
    {DRM_IOCTL_I915_GEM_CONTEXT_GETPARAM, "DRM_IOCTL_I915_GEM_CONTEXT_GETPARAM"},
    {DRM_IOCTL_I915_GEM_CONTEXT_SETPARAM, "DRM_IOCTL_I915_GEM_CONTEXT_SETPARAM"},
    {DRM_IOCTL_I915_PERF_OPEN, "DRM_IOCTL_I915_PERF_OPEN"},
    {DRM_IOCTL_I915_PERF_ADD_CONFIG, "DRM_IOCTL_I915_PERF_ADD_CONFIG"},
    {DRM_IOCTL_I915_PERF_REMOVE_CONFIG, "DRM_IOCTL_I915_PERF_REMOVE_CONFIG"},
    {DRM_IOCTL_I915_QUERY, "DRM_IOCTL_I915_QUERY"},
    {DRM_IOCTL_I915_GEM_MMAP, "DRM_IOCTL_I915_GEM_MMAP"},
    {DRM_IOCTL_PRIME_FD_TO_HANDLE, "DRM_IOCTL_PRIME_FD_TO_HANDLE"},
    {DRM_IOCTL_PRIME_HANDLE_TO_FD, "DRM_IOCTL_PRIME_HANDLE_TO_FD"},
    {static_cast<unsigned long>(101010101), "101010101"}};

std::map<int, const char *> ioctlParamCodeStringMap = {
    {I915_PARAM_CHIPSET_ID, "I915_PARAM_CHIPSET_ID"},
    {I915_PARAM_REVISION, "I915_PARAM_REVISION"},
    {I915_PARAM_HAS_EXEC_SOFTPIN, "I915_PARAM_HAS_EXEC_SOFTPIN"},
    {I915_PARAM_HAS_POOLED_EU, "I915_PARAM_HAS_POOLED_EU"},
    {I915_PARAM_HAS_SCHEDULER, "I915_PARAM_HAS_SCHEDULER"},
    {I915_PARAM_EU_TOTAL, "I915_PARAM_EU_TOTAL"},
    {I915_PARAM_SUBSLICE_TOTAL, "I915_PARAM_SUBSLICE_TOTAL"},
    {I915_PARAM_MIN_EU_IN_POOL, "I915_PARAM_MIN_EU_IN_POOL"},
    {I915_PARAM_CS_TIMESTAMP_FREQUENCY, "I915_PARAM_CS_TIMESTAMP_FREQUENCY"},
    {static_cast<int>(101010101), "101010101"}};
