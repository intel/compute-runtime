/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/libult/linux/drm_mock.h"

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/linux/i915.h"
#include "shared/source/os_interface/linux/sys_calls.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"

#include "gtest/gtest.h"

#include <cstring>

const int DrmMock::mockFd;
const uint32_t DrmMockResources::registerResourceReturnHandle = 3;

DrmMock::DrmMock(int fd, RootDeviceEnvironment &rootDeviceEnvironment) : Drm(std::make_unique<HwDeviceIdDrm>(fd, ""), rootDeviceEnvironment) {
    sliceCountChangeSupported = true;

    if (rootDeviceEnvironment.executionEnvironment.isDebuggingEnabled()) {
        if (rootDeviceEnvironment.executionEnvironment.getDebuggingMode() == DebuggingMode::offline) {
            setPerContextVMRequired(false);
        } else {
            setPerContextVMRequired(true);
        }
    }

    DebugManagerStateRestore restore;
    debugManager.flags.IgnoreProductSpecificIoctlHelper.set(true);
    setupIoctlHelper(rootDeviceEnvironment.getHardwareInfo()->platform.eProductFamily);
    if (!isPerContextVMRequired()) {
        createVirtualMemoryAddressSpace(GfxCoreHelper::getSubDevicesCount(rootDeviceEnvironment.getHardwareInfo()));
    }
    storedPreemptionSupport =
        I915_SCHEDULER_CAP_ENABLED |
        I915_SCHEDULER_CAP_PRIORITY |
        I915_SCHEDULER_CAP_PREEMPTION;
}

int DrmMock::handleRemainingRequests(DrmIoctl request, void *arg) {
    if (request == DrmIoctl::gemWaitUserFence && arg != nullptr) {
        return 0;
    }
    if (request == DrmIoctl::gemVmBind) {
        auto vmBindInput = static_cast<GemVmBind *>(arg);
        vmBindInputs.push_back(*vmBindInput);
        return 0;
    }

    ioctlCallsCount--;
    return -1;
};

int DrmMock::ioctl(DrmIoctl request, void *arg) {
    ioctlCallsCount++;
    ioctlCount.total++;

    if ((request == DrmIoctl::getparam) && (arg != nullptr)) {
        ioctlCount.contextGetParam++;
        auto gp = static_cast<GetParam *>(arg);
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
        if (gp->param == I915_PARAM_CS_TIMESTAMP_FREQUENCY) {
            *gp->value = this->storedCsTimestampFrequency;
            return this->storedRetVal;
        }
        if (gp->param == I915_PARAM_OA_TIMESTAMP_FREQUENCY) {
            *gp->value = this->storedOaTimestampFrequency;
            return this->storedRetVal;
        }
        if (gp->param == I915_PARAM_HAS_CONTEXT_FREQ_HINT) {
            *gp->value = this->storedRetValForHasContextFreqHint;
            return this->storedRetVal;
        }
    }

    if ((request == DrmIoctl::gemContextCreateExt) && (arg != nullptr)) {
        ioctlCount.contextCreate++;
        auto create = static_cast<GemContextCreateExt *>(arg);
        create->contextId = this->storedDrmContextId;
        this->receivedContextCreateFlags = create->flags;
        if (create->extensions == 0) {
            return this->storedRetVal;
        }
        receivedContextCreateSetParam = *reinterpret_cast<GemContextCreateExtSetParam *>(create->extensions);
        if (receivedContextCreateSetParam.base.name == I915_CONTEXT_CREATE_EXT_SETPARAM) {
            receivedContextParamRequestCount++;
            if (receivedContextCreateSetParam.base.nextExtension != 0) {
                auto offset = offsetof(GemContextCreateExtSetParam, param);
                auto ext = reinterpret_cast<GemContextParam *>(receivedContextCreateSetParam.base.nextExtension + offset);
                if (ext->param == I915_CONTEXT_PARAM_LOW_LATENCY) {
                    this->lowLatencyHintRequested = true;
                }
            }
            receivedContextParamRequest = *reinterpret_cast<GemContextParam *>(&receivedContextCreateSetParam.param);
            if (receivedContextCreateSetParam.param.param == I915_CONTEXT_PARAM_VM) {
                this->requestSetVmId = receivedContextParamRequest.value;

                return this->storedRetVal;
            }
        }
    }

    if ((request == DrmIoctl::perfOpen) && (arg != nullptr)) {
        ioctlCount.perfOpen++;
        if (failPerfOpen) {
            return -1;
        } else {
            return NEO::SysCalls::ioctl(mockFd, DRM_IOCTL_I915_PERF_OPEN, arg);
        }
    }

    if ((request == DrmIoctl::gemVmCreate) && (arg != nullptr)) {
        ioctlCount.gemVmCreate++;
        auto gemVmControl = static_cast<GemVmControl *>(arg);
        receivedGemVmControl = *gemVmControl;
        gemVmControl->vmId = ++latestCreatedVmId;
        return storedRetValForVmCreate;
    }

    if ((request == DrmIoctl::gemVmDestroy) && (arg != nullptr)) {
        ioctlCount.gemVmDestroy++;

        return 0;
    }

    if ((request == DrmIoctl::gemContextDestroy) && (arg != nullptr)) {
        ioctlCount.contextDestroy++;
        auto destroy = static_cast<GemContextDestroy *>(arg);
        this->receivedDestroyContextId = destroy->contextId;
        return this->storedRetVal;
    }

    if ((request == DrmIoctl::gemContextSetparam) && (arg != nullptr)) {
        ioctlCount.contextSetParam++;
        receivedContextParamRequestCount++;
        receivedContextParamRequest = *static_cast<GemContextParam *>(arg);
        if (receivedContextParamRequest.param == I915_CONTEXT_PARAM_PRIORITY) {
            return this->storedRetVal;
        }
        if ((receivedContextParamRequest.param == contextPrivateParamBoost) && (receivedContextParamRequest.value == 1)) {
            return this->storedRetVal;
        }
        if (receivedContextParamRequest.param == I915_CONTEXT_PARAM_SSEU) {
            if (storedRetValForSetSSEU == 0) {
                storedParamSseu = (*static_cast<GemContextParamSseu *>(reinterpret_cast<void *>(receivedContextParamRequest.value))).sliceMask;
            }
            return this->storedRetValForSetSSEU;
        }
        if (receivedContextParamRequest.param == I915_CONTEXT_PARAM_PERSISTENCE) {
            return this->storedRetValForPersistant;
        }
        if (receivedContextParamRequest.param == I915_CONTEXT_PARAM_VM) {
            this->requestSetVmId = receivedContextParamRequest.value;
            return this->storedRetVal;
        }
        if (receivedContextParamRequest.param == I915_CONTEXT_PARAM_RECOVERABLE) {
            receivedRecoverableContextValue = receivedContextParamRequest.value;
            unrecoverableContextSet = true;
            return this->storedRetVal;
        }
    }

    if ((request == DrmIoctl::gemContextGetparam) && (arg != nullptr)) {
        ioctlCount.contextGetParam++;
        receivedContextParamRequestCount++;
        receivedContextParamRequest = *static_cast<GemContextParam *>(arg);
        if (receivedContextParamRequest.param == I915_CONTEXT_PARAM_GTT_SIZE) {
            static_cast<GemContextParam *>(arg)->value = this->storedGTTSize;
            return this->storedRetValForGetGttSize;
        }
        if (receivedContextParamRequest.param == I915_CONTEXT_PARAM_SSEU) {
            if (storedRetValForGetSSEU == 0) {
                (*static_cast<GemContextParamSseu *>(reinterpret_cast<void *>(receivedContextParamRequest.value))).sliceMask = storedParamSseu;
            }
            return this->storedRetValForGetSSEU;
        }
        if (receivedContextParamRequest.param == I915_CONTEXT_PARAM_PERSISTENCE) {
            static_cast<GemContextParam *>(arg)->value = this->storedPersistentContextsSupport;
            return this->storedRetValForPersistant;
        }

        if (receivedContextParamRequest.param == I915_CONTEXT_PARAM_VM) {
            static_cast<GemContextParam *>(arg)->value = this->storedRetValForVmId;
            if (incrementVmId) {
                this->storedRetValForVmId++;
            }
            return 0u;
        }
    }

    if (request == DrmIoctl::gemExecbuffer2) {
        ioctlCount.execbuffer2++;
        auto execbuf = static_cast<NEO::MockExecBuffer *>(arg);
        auto execObjects = reinterpret_cast<const MockExecObject *>(execbuf->getBuffersPtr());
        this->execBuffers.push_back(*execbuf);
        for (uint32_t i = 0; i < execbuf->getBufferCount(); i++) {
            this->receivedBos.push_back(execObjects[i]);
        }
        return execBufferResult;
    }
    if (request == DrmIoctl::gemUserptr) {
        ioctlCount.gemUserptr++;
        auto userPtrParams = static_cast<NEO::GemUserPtr *>(arg);
        userPtrParams->handle = returnHandle;
        returnHandle++;
        return 0;
    }
    if (request == DrmIoctl::gemCreate) {
        ioctlCount.gemCreate++;
        auto createParams = static_cast<NEO::GemCreate *>(arg);
        this->createParamsSize = createParams->size;
        this->createParamsHandle = createParams->handle = 1u;
        if (0 == this->createParamsSize) {
            return EINVAL;
        }
        return 0;
    }
    if (request == DrmIoctl::gemSetTiling) {
        ioctlCount.gemSetTiling++;
        auto setTilingParams = static_cast<NEO::GemSetTiling *>(arg);
        setTilingMode = setTilingParams->tilingMode;
        setTilingHandle = setTilingParams->handle;
        setTilingStride = setTilingParams->stride;
        return 0;
    }
    if (request == DrmIoctl::primeFdToHandle) {
        ioctlCount.primeFdToHandle++;
        auto primeToHandleParams = static_cast<PrimeHandle *>(arg);
        // return BO
        primeToHandleParams->handle = outputHandle;
        inputFd = primeToHandleParams->fileDescriptor;
        return fdToHandleRetVal;
    }
    if (request == DrmIoctl::primeHandleToFd) {
        ioctlCount.handleToPrimeFd++;
        auto primeToFdParams = static_cast<PrimeHandle *>(arg);
        primeToFdParams->fileDescriptor = outputFd;
        return 0;
    }
    if (request == DrmIoctl::gemWait) {
        ioctlCount.gemWait++;
        receivedGemWait = *static_cast<GemWait *>(arg);
        return 0;
    }
    if (request == DrmIoctl::gemClose) {
        ioctlCount.gemClose++;
        return storedRetValForGemClose;
    }
    if (request == DrmIoctl::getResetStats && arg != nullptr) {
        ioctlCount.getResetStats++;
        auto outResetStats = static_cast<ResetStats *>(arg);
        for (const auto &resetStats : resetStatsToReturn) {
            if (resetStats.contextId == outResetStats->contextId) {
                *outResetStats = resetStats;
                return 0;
            }
        }

        return -1;
    }

    if (request == DrmIoctl::query && arg != nullptr) {
        ioctlCount.query++;
        auto queryArg = static_cast<Query *>(arg);
        auto queryItemArg = reinterpret_cast<QueryItem *>(queryArg->itemsPtr);
        storedQueryItem = *queryItemArg;

        UNRECOVERABLE_IF((this->storedSVal != 0) && (this->storedSSVal % this->storedSVal != 0));
        UNRECOVERABLE_IF((this->storedSSVal != 0) && (this->storedEUVal % this->storedSSVal != 0));

        const uint16_t subslicesPerSlice = this->storedSVal ? (this->storedSSVal / this->storedSVal) : 0u;
        const uint16_t eusPerSubslice = this->storedSSVal ? (this->storedEUVal / this->storedSSVal) : 0u;
        const uint16_t subsliceOffset = static_cast<uint16_t>(Math::divideAndRoundUp(this->storedSVal, 8u));
        const uint16_t subsliceStride = static_cast<uint16_t>(Math::divideAndRoundUp(subslicesPerSlice, 8u));
        const uint16_t euOffset = subsliceOffset + this->storedSVal * subsliceStride;
        const uint16_t euStride = static_cast<uint16_t>(Math::divideAndRoundUp(eusPerSubslice, 8u));

        const uint16_t dataSize = euOffset + this->storedSSVal * euStride;

        if (queryItemArg->length == 0) {
            if (queryItemArg->queryId == DRM_I915_QUERY_TOPOLOGY_INFO) {
                queryItemArg->length = static_cast<int32_t>(sizeof(QueryTopologyInfo) + dataSize);
                return 0;
            }
        } else {
            if (queryItemArg->queryId == DRM_I915_QUERY_TOPOLOGY_INFO) {
                auto topologyArg = reinterpret_cast<QueryTopologyInfo *>(queryItemArg->dataPtr);
                if (this->failRetTopology) {
                    return -1;
                }
                topologyArg->maxSlices = this->storedSVal;
                topologyArg->maxSubslices = subslicesPerSlice;
                topologyArg->maxEusPerSubslice = eusPerSubslice;
                topologyArg->subsliceOffset = subsliceOffset;
                topologyArg->subsliceStride = subsliceStride;
                topologyArg->euOffset = euOffset;
                topologyArg->euStride = euStride;

                if (this->disableSomeTopology) {
                    memset(topologyArg->data, 0b11000110, dataSize);
                } else {
                    memset(topologyArg->data, 0b11111111, dataSize);
                }

                return 0;
            }
        }
    }

    return handleRemainingRequests(request, arg);
}
int DrmMock::waitUserFence(uint32_t ctxIdx, uint64_t address, uint64_t value, ValueWidth dataWidth, int64_t timeout, uint16_t flags, bool userInterrupt, uint32_t externalInterruptId, GraphicsAllocation *allocForInterruptWait) {
    waitUserFenceParams.push_back({ctxIdx, address, value, dataWidth, timeout, flags});
    return Drm::waitUserFence(ctxIdx, address, value, dataWidth, timeout, flags, userInterrupt, externalInterruptId, allocForInterruptWait);
}
int DrmMockEngine::handleRemainingRequests(DrmIoctl request, void *arg) {
    if ((request == DrmIoctl::query) && (arg != nullptr)) {
        if (i915QuerySuccessCount == 0) {
            return EINVAL;
        }
        i915QuerySuccessCount--;
        auto query = static_cast<Query *>(arg);
        if (query->itemsPtr == 0) {
            return EINVAL;
        }
        for (auto i = 0u; i < query->numItems; i++) {
            handleQueryItem(reinterpret_cast<QueryItem *>(query->itemsPtr) + i);
        }
        return 0;
    }
    return -1;
}
DrmMock::~DrmMock() {
    if (expectIoctlCallsOnDestruction) {
        EXPECT_EQ(expectedIoctlCallsOnDestruction, ioctlCallsCount);
    }
}
