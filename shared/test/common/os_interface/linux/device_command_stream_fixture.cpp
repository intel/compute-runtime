/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/os_interface/linux/device_command_stream_fixture.h"

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/os_interface/linux/i915.h"

#include "gtest/gtest.h"

const int mockFd = 33;
const char *mockPciPath = "";

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
    gemMmapOffset = 0;
    gemSetDomain = 0;
    gemWait = 0;
    gemClose = 0;
    getResetStats = 0;
    regRead = 0;
    getParam = 0;
    contextGetParam = 0;
    contextSetParam = 0;
    contextCreate = 0;
    contextDestroy = 0;
}

void DrmMockCustom::testIoctls() {
    if (this->ioctlExpected.total == -1)
        return;

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
    NEO_IOCTL_EXPECT_EQ(getResetStats);
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
        ioctlCnt.syncObjFdToHandle++;
        if (failOnSyncObjFdToHandle == true) {
            return -1;
        }
    } break;
    case DrmIoctl::syncObjWait: {
        ioctlCnt.syncObjWait++;
        if (failOnSyncObjWait == true) {
            return -1;
        }
    } break;
    case DrmIoctl::syncObjSignal: {
        ioctlCnt.syncObjSignal++;
        if (failOnSyncObjSignal == true) {
            return -1;
        }
    } break;
    case DrmIoctl::syncObjTimelineWait: {
        ioctlCnt.syncObjTimelineWait++;
        if (failOnSyncObjTimelineWait == true) {
            return -1;
        }
    } break;
    case DrmIoctl::syncObjTimelineSignal: {
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
    case DrmIoctl::getResetStats: {
        ioctlCnt.getResetStats++;
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
    DebugManagerStateRestore restore;
    debugManager.flags.IgnoreProductSpecificIoctlHelper.set(true);
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
