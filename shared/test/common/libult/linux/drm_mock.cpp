/*
 * Copyright (C) 2019-2026 Intel Corporation
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
#include "shared/test/common/helpers/default_hw_info.h"

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

    if (request == DrmIoctl::gemMmapOffset) {
        auto mmapArg = static_cast<GemMmapOffset *>(arg);
        mmapArg->offset = 0;
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
    if (request == DrmIoctl::queryContextHealth && arg != nullptr) {
        ioctlCount.queryContextHealth++;
        auto outResetStats = static_cast<drm_i915_reset_stats *>(arg);
        for (const auto &resetStats : resetStatsToReturn) {
            if (resetStats.contextId == outResetStats->ctx_id) {
                outResetStats->batch_active = resetStats.batchActive;
                outResetStats->batch_pending = resetStats.batchPending;
                return 0;
            }
        }

        if (allowMissingResetStats) {
            auto contextId = outResetStats->ctx_id;
            *outResetStats = {};
            outResetStats->ctx_id = contextId;
            return 0;
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

const int mockFd = 33;
const char *mockPciPath = "";

DrmMockSuccess::DrmMockSuccess(int fd, RootDeviceEnvironment &rootDeviceEnvironment) : Drm(std::make_unique<HwDeviceIdDrm>(fd, mockPciPath), rootDeviceEnvironment) {
    setupIoctlHelper(NEO::defaultHwInfo->platform.eProductFamily);
}

DrmMockFail::DrmMockFail(RootDeviceEnvironment &rootDeviceEnvironment) : Drm(std::make_unique<HwDeviceIdDrm>(mockFd, mockPciPath), rootDeviceEnvironment) {
    setupIoctlHelper(NEO::defaultHwInfo->platform.eProductFamily);
}

void Ioctls::reset() {
    total = 0;
    query = 0;
    execbuffer2 = 0;
    gemUserptr = 0;
    gemCreate = 0;
    gemCreateExt = 0;
    gemSetTiling = 0;
    gemGetTiling = 0;
    gemVmCreate = 0;
    gemVmDestroy = 0;
    primeFdToHandle = 0;
    handleToPrimeFd = 0;
    syncObjFdToHandle = 0;
    syncObjDestroy = 0;
    syncObjWait = 0;
    syncObjSignal = 0;
    syncObjTimelineWait = 0;
    syncObjTimelineSignal = 0;
    gemMmapOffset = 0;
    gemSetDomain = 0;
    gemWait = 0;
    gemClose = 0;
    queryContextHealth = 0;
    regRead = 0;
    getParam = 0;
    contextGetParam = 0;
    contextSetParam = 0;
    contextCreate = 0;
    contextDestroy = 0;
    perfOpen = 0;
    perfEnable = 0;
    perfDisable = 0;
}

void DrmMockCustom::testIoctls() {
    if (this->ioctlExpected.total == -1) {
        return;
    }

#define NEO_IOCTL_EXPECT_EQ(PARAM)                                  \
    if (this->ioctlExpected.PARAM >= 0) {                           \
        EXPECT_EQ(this->ioctlExpected.PARAM, this->ioctlCnt.PARAM); \
    }
    NEO_IOCTL_EXPECT_EQ(execbuffer2);
    NEO_IOCTL_EXPECT_EQ(gemUserptr);
    NEO_IOCTL_EXPECT_EQ(gemCreate);
    NEO_IOCTL_EXPECT_EQ(gemCreateExt);
    NEO_IOCTL_EXPECT_EQ(gemSetTiling);
    NEO_IOCTL_EXPECT_EQ(gemGetTiling);
    NEO_IOCTL_EXPECT_EQ(primeFdToHandle);
    NEO_IOCTL_EXPECT_EQ(handleToPrimeFd);
    NEO_IOCTL_EXPECT_EQ(gemMmapOffset);
    NEO_IOCTL_EXPECT_EQ(gemSetDomain);
    NEO_IOCTL_EXPECT_EQ(gemWait);
    NEO_IOCTL_EXPECT_EQ(gemClose);
    NEO_IOCTL_EXPECT_EQ(queryContextHealth);
    NEO_IOCTL_EXPECT_EQ(regRead);
    NEO_IOCTL_EXPECT_EQ(getParam);
    NEO_IOCTL_EXPECT_EQ(contextGetParam);
    NEO_IOCTL_EXPECT_EQ(contextCreate);
    NEO_IOCTL_EXPECT_EQ(contextDestroy);
#undef NEO_IOCTL_EXPECT_EQ
}

int DrmMockCustom::ioctl(DrmIoctl request, void *arg) {
    auto ext = ioctlResExt.load();

    // store flags
    switch (request) {
    case DrmIoctl::gemExecbuffer2: {
        auto execbuf = static_cast<NEO::MockExecBuffer *>(arg);
        this->execBuffer = *execbuf;
        this->execBufferBufferObjects =
            *reinterpret_cast<NEO::MockExecObject *>(this->execBuffer.getBuffersPtr());
        ioctlCnt.execbuffer2++;
        execBufferExtensions(execbuf);
    } break;

    case DrmIoctl::gemUserptr: {
        auto *userPtrParams = static_cast<NEO::GemUserPtr *>(arg);
        userPtrParams->handle = returnHandle;
        returnHandle++;
        ioctlCnt.gemUserptr++;
    } break;

    case DrmIoctl::gemCreate: {
        auto *createParams = static_cast<NEO::GemCreate *>(arg);
        this->createParamsSize = createParams->size;
        this->createParamsHandle = createParams->handle = 1u;
        ioctlCnt.gemCreate++;
    } break;
    case DrmIoctl::gemSetTiling: {
        auto *setTilingParams = static_cast<NEO::GemSetTiling *>(arg);
        setTilingMode = setTilingParams->tilingMode;
        setTilingHandle = setTilingParams->handle;
        setTilingStride = setTilingParams->stride;
        ioctlCnt.gemSetTiling++;
    } break;
    case DrmIoctl::gemGetTiling: {
        auto *getTilingParams = static_cast<NEO::GemGetTiling *>(arg);
        getTilingParams->tilingMode = getTilingModeOut;
        getTilingHandleIn = getTilingParams->handle;
        ioctlCnt.gemGetTiling++;
    } break;
    case DrmIoctl::primeFdToHandle: {
        auto *primeToHandleParams = static_cast<NEO::PrimeHandle *>(arg);
        // return BO
        primeToHandleParams->handle = outputHandle;
        inputFd = primeToHandleParams->fileDescriptor;
        ioctlCnt.primeFdToHandle++;
        if (failOnSecondPrimeFdToHandle == true) {
            failOnSecondPrimeFdToHandle = false;
            failOnPrimeFdToHandle = true;
            break;
        }
        if (failOnPrimeFdToHandle == true) {
            return -1;
        }
    } break;
    case DrmIoctl::primeHandleToFd: {
        auto *handleToPrimeParams = static_cast<NEO::PrimeHandle *>(arg);
        // return FD
        inputHandle = handleToPrimeParams->handle;
        inputFlags = handleToPrimeParams->flags;
        handleToPrimeParams->fileDescriptor = outputFd;
        if (incrementOutputFdAfterCall) {
            outputFd++;
        }
        ioctlCnt.handleToPrimeFd++;
        if (failOnPrimeHandleToFd == true) {
            return -1;
        }
    } break;
    case DrmIoctl::syncObjFdToHandle: {
        auto syncObjHandle = static_cast<NEO::SyncObjHandle *>(arg);
        syncObjHandle->handle = 1u; // Return a valid non-zero handle
        ioctlCnt.syncObjFdToHandle++;
        if (failOnSyncObjFdToHandle == true) {
            return -1;
        }
    } break;
    case DrmIoctl::syncObjDestroy: {
        ioctlCnt.syncObjDestroy++;
        if (failOnSyncObjDestroy == true) {
            return -1;
        }
    } break;
    case DrmIoctl::syncObjWait: {
        auto waitArgs = static_cast<NEO::SyncObjWait *>(arg);
        syncObjWaitTimeoutNs = waitArgs->timeoutNs;
        syncObjWaitFlags = waitArgs->flags;
        syncObjWaitCountHandles = waitArgs->countHandles;
        ioctlCnt.syncObjWait++;
        if (failOnSyncObjWait == true) {
            return -1;
        }
    } break;
    case DrmIoctl::syncObjSignal: {
        syncObjSignalCountHandles = static_cast<NEO::SyncObjArray *>(arg)->countHandles;
        ioctlCnt.syncObjSignal++;
        if (failOnSyncObjSignal == true) {
            return -1;
        }
    } break;
    case DrmIoctl::syncObjTimelineWait: {
        auto waitArgs = static_cast<SyncObjTimelineWait *>(arg);
        syncObjTimelineWaitTimeoutNs = waitArgs->timeoutNs;
        syncObjTimelineWaitFlags = waitArgs->flags;
        auto waitPoints = reinterpret_cast<const uint64_t *>(waitArgs->points);
        syncObjTimelineWaitPoints.assign(waitPoints, waitPoints + waitArgs->countHandles);
        ioctlCnt.syncObjTimelineWait++;
        if (failOnSyncObjTimelineWait == true) {
            return -1;
        }
    } break;
    case DrmIoctl::syncObjTimelineSignal: {
        auto signalArgs = static_cast<NEO::SyncObjTimelineArray *>(arg);
        auto signalPoints = reinterpret_cast<const uint64_t *>(signalArgs->points);
        syncObjTimelineSignalPoints.assign(signalPoints, signalPoints + signalArgs->countHandles);
        ioctlCnt.syncObjTimelineSignal++;
        if (failOnSyncObjTimelineSignal == true) {
            return -1;
        }
    } break;
    case DrmIoctl::gemSetDomain: {
        auto setDomainParams = static_cast<NEO::GemSetDomain *>(arg);
        setDomainHandle = setDomainParams->handle;
        setDomainReadDomains = setDomainParams->readDomains;
        setDomainWriteDomain = setDomainParams->writeDomain;
        ioctlCnt.gemSetDomain++;
    } break;

    case DrmIoctl::gemWait: {
        auto gemWaitParams = static_cast<NEO::GemWait *>(arg);
        gemWaitTimeout = gemWaitParams->timeoutNs;
        ioctlCnt.gemWait++;
    } break;

    case DrmIoctl::gemClose:
        ioctlCnt.gemClose++;
        break;

    case DrmIoctl::regRead:
        ioctlCnt.regRead++;
        break;

    case DrmIoctl::getparam: {
        ioctlCnt.contextGetParam++;
        auto getParam = static_cast<NEO::GetParam *>(arg);
        recordedGetParam = *getParam;
        *getParam->value = getParamRetValue;
    } break;

    case DrmIoctl::gemContextSetparam: {
    } break;

    case DrmIoctl::gemContextGetparam: {
        ioctlCnt.contextGetParam++;
        auto getContextParam = static_cast<NEO::GemContextParam *>(arg);
        recordedGetContextParam = *getContextParam;
        getContextParam->value = getContextParamRetValue;
    } break;

    case DrmIoctl::gemContextCreateExt: {
        auto contextCreateParam = static_cast<NEO::GemContextCreateExt *>(arg);
        contextCreateParam->contextId = ++ioctlCnt.contextCreate;
    } break;
    case DrmIoctl::gemContextDestroy: {
        ioctlCnt.contextDestroy++;
    } break;
    case DrmIoctl::gemMmapOffset: {
        auto mmapOffsetParams = reinterpret_cast<NEO::GemMmapOffset *>(arg);
        mmapOffsetHandle = mmapOffsetParams->handle;
        mmapOffsetParams->offset = mmapOffsetExpected;
        mmapOffsetFlags = mmapOffsetParams->flags;
        ioctlCnt.gemMmapOffset++;
        if (failOnMmapOffset == true) {
            return -1;
        }
    } break;
    case DrmIoctl::gemCreateExt: {
        auto createExtParams = reinterpret_cast<NEO::I915::drm_i915_gem_create_ext *>(arg);
        createExtSize = createExtParams->size;
        createExtHandle = createExtParams->handle;
        createExtExtensions = createExtParams->extensions;
        ioctlCnt.gemCreateExt++;
        if (failOnCreateExt == true) {
            return -1;
        }
    } break;
    case DrmIoctl::gemVmBind: {
    } break;
    case DrmIoctl::gemVmUnbind: {
    } break;
    case DrmIoctl::gemVmCreate: {
        auto vmCreate = reinterpret_cast<NEO::GemVmControl *>(arg);
        vmCreate->vmId = vmIdToCreate;
        break;
    }
    case DrmIoctl::queryContextHealth: {
        ioctlCnt.queryContextHealth++;
        break;
    }
    default:
        int res = ioctlExtra(request, arg);
        if (returnIoctlExtraErrorValue) {
            return res;
        }
    }

    if (!ext->no.empty() && std::find(ext->no.begin(), ext->no.end(), ioctlCnt.total.load()) != ext->no.end()) {
        ioctlCnt.total.fetch_add(1);
        return ext->res;
    }
    ioctlCnt.total.fetch_add(1);
    return ioctlRes.load();
}

std::unique_ptr<DrmMockCustom> DrmMockCustom::create(RootDeviceEnvironment &rootDeviceEnvironment) {
    return DrmMockCustom::create(std::make_unique<HwDeviceIdDrm>(mockFd, mockPciPath), rootDeviceEnvironment);
}

std::unique_ptr<DrmMockCustom> DrmMockCustom::create(std::unique_ptr<HwDeviceIdDrm> &&hwDeviceId, RootDeviceEnvironment &rootDeviceEnvironment) {
    auto drm{new DrmMockCustom{std::move(hwDeviceId), rootDeviceEnvironment}};

    drm->reset();
    auto &gfxCoreHelper = rootDeviceEnvironment.getHelper<NEO::GfxCoreHelper>();
    drm->ioctlExpected.contextCreate = static_cast<int>(gfxCoreHelper.getGpgpuEngineInstances(rootDeviceEnvironment).size());
    drm->ioctlExpected.contextDestroy = drm->ioctlExpected.contextCreate.load();
    drm->setupIoctlHelper(rootDeviceEnvironment.getHardwareInfo()->platform.eProductFamily);
    drm->createVirtualMemoryAddressSpace(NEO::GfxCoreHelper::getSubDevicesCount(rootDeviceEnvironment.getHardwareInfo()));
    drm->isVmBindAvailable();
    drm->reset();

    return std::unique_ptr<DrmMockCustom>{drm};
}

DrmMockCustom::DrmMockCustom(std::unique_ptr<HwDeviceIdDrm> &&hwDeviceId, RootDeviceEnvironment &rootDeviceEnvironment)
    : Drm(std::move(hwDeviceId), rootDeviceEnvironment) {}

int DrmMockCustom::waitUserFence(uint32_t ctxId, uint64_t address, uint64_t value, ValueWidth dataWidth, int64_t timeout, uint16_t flags, bool userInterrupt, uint32_t externalInterruptId, NEO::GraphicsAllocation *allocForInterruptWait) {
    waitUserFenceCall.called++;
    waitUserFenceCall.ctxId = ctxId;
    waitUserFenceCall.address = address;
    waitUserFenceCall.dataWidth = dataWidth;
    waitUserFenceCall.value = value;
    waitUserFenceCall.timeout = timeout;
    waitUserFenceCall.flags = flags;

    if (waitUserFenceCall.called == waitUserFenceCall.failSpecificCall) {
        return 123;
    }

    if (waitUserFenceCall.failOnWaitUserFence == true) {
        return -1;
    }

    return Drm::waitUserFence(ctxId, address, value, dataWidth, timeout, flags, userInterrupt, externalInterruptId, allocForInterruptWait);
}

bool DrmMockCustom::isVmBindAvailable() {
    isVmBindAvailableCall.called++;
    if (isVmBindAvailableCall.callParent) {
        return Drm::isVmBindAvailable();
    } else {
        return isVmBindAvailableCall.returnValue;
    }
}

bool DrmMockCustom::getSetPairAvailable() {
    getSetPairAvailableCall.called++;
    if (getSetPairAvailableCall.callParent) {
        return Drm::getSetPairAvailable();
    } else {
        return getSetPairAvailableCall.returnValue;
    }
}

bool DrmMockCustom::isChunkingAvailable() {
    isChunkingAvailableCall.called++;
    if (isChunkingAvailableCall.callParent) {
        return Drm::isChunkingAvailable();
    } else {
        return isChunkingAvailableCall.returnValue;
    }
}

bool DrmMockCustom::getChunkingAvailable() {
    getChunkingAvailableCall.called++;
    if (getChunkingAvailableCall.callParent) {
        return Drm::getChunkingAvailable();
    } else {
        return getChunkingAvailableCall.returnValue;
    }
}

uint32_t DrmMockCustom::getChunkingMode() {
    getChunkingModeCall.called++;
    if (getChunkingModeCall.callParent) {
        return Drm::getChunkingMode();
    } else {
        return getChunkingModeCall.returnValue;
    }
}
