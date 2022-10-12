/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/os_interface/linux/device_command_stream_fixture.h"

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
    gemResetStats = 0;
    regRead = 0;
    getParam = 0;
    contextGetParam = 0;
    contextSetParam = 0;
    contextCreate = 0;
    contextDestroy = 0;
}

void DrmMockCustom::testIoctls() {
    if (this->ioctl_expected.total == -1)
        return;

#define NEO_IOCTL_EXPECT_EQ(PARAM)                                    \
    if (this->ioctl_expected.PARAM >= 0) {                            \
        EXPECT_EQ(this->ioctl_expected.PARAM, this->ioctl_cnt.PARAM); \
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
    NEO_IOCTL_EXPECT_EQ(regRead);
    NEO_IOCTL_EXPECT_EQ(getParam);
    NEO_IOCTL_EXPECT_EQ(contextGetParam);
    NEO_IOCTL_EXPECT_EQ(contextCreate);
    NEO_IOCTL_EXPECT_EQ(contextDestroy);
#undef NEO_IOCTL_EXPECT_EQ
}

int DrmMockCustom::ioctl(DrmIoctl request, void *arg) {
    auto ext = ioctl_res_ext.load();

    //store flags
    switch (request) {
    case DrmIoctl::GemExecbuffer2: {
        auto execbuf = static_cast<NEO::MockExecBuffer *>(arg);
        this->execBuffer = *execbuf;
        this->execBufferBufferObjects =
            *reinterpret_cast<NEO::MockExecObject *>(this->execBuffer.getBuffersPtr());
        ioctl_cnt.execbuffer2++;
        execBufferExtensions(execbuf);
    } break;

    case DrmIoctl::GemUserptr: {
        auto *userPtrParams = static_cast<NEO::GemUserPtr *>(arg);
        userPtrParams->handle = returnHandle;
        returnHandle++;
        ioctl_cnt.gemUserptr++;
    } break;

    case DrmIoctl::GemCreate: {
        auto *createParams = static_cast<NEO::GemCreate *>(arg);
        this->createParamsSize = createParams->size;
        this->createParamsHandle = createParams->handle = 1u;
        ioctl_cnt.gemCreate++;
    } break;
    case DrmIoctl::GemSetTiling: {
        auto *setTilingParams = static_cast<NEO::GemSetTiling *>(arg);
        setTilingMode = setTilingParams->tilingMode;
        setTilingHandle = setTilingParams->handle;
        setTilingStride = setTilingParams->stride;
        ioctl_cnt.gemSetTiling++;
    } break;
    case DrmIoctl::GemGetTiling: {
        auto *getTilingParams = static_cast<NEO::GemGetTiling *>(arg);
        getTilingParams->tilingMode = getTilingModeOut;
        getTilingHandleIn = getTilingParams->handle;
        ioctl_cnt.gemGetTiling++;
    } break;
    case DrmIoctl::PrimeFdToHandle: {
        auto *primeToHandleParams = static_cast<NEO::PrimeHandle *>(arg);
        //return BO
        primeToHandleParams->handle = outputHandle;
        inputFd = primeToHandleParams->fileDescriptor;
        ioctl_cnt.primeFdToHandle++;
        if (failOnPrimeFdToHandle == true) {
            return -1;
        }
    } break;
    case DrmIoctl::PrimeHandleToFd: {
        auto *handleToPrimeParams = static_cast<NEO::PrimeHandle *>(arg);
        //return FD
        inputHandle = handleToPrimeParams->handle;
        inputFlags = handleToPrimeParams->flags;
        handleToPrimeParams->fileDescriptor = outputFd;
        if (incrementOutputFdAfterCall) {
            outputFd++;
        }
        ioctl_cnt.handleToPrimeFd++;
    } break;
    case DrmIoctl::GemSetDomain: {
        auto setDomainParams = static_cast<NEO::GemSetDomain *>(arg);
        setDomainHandle = setDomainParams->handle;
        setDomainReadDomains = setDomainParams->readDomains;
        setDomainWriteDomain = setDomainParams->writeDomain;
        ioctl_cnt.gemSetDomain++;
    } break;

    case DrmIoctl::GemWait: {
        auto gemWaitParams = static_cast<NEO::GemWait *>(arg);
        gemWaitTimeout = gemWaitParams->timeoutNs;
        ioctl_cnt.gemWait++;
    } break;

    case DrmIoctl::GemClose:
        ioctl_cnt.gemClose++;
        break;

    case DrmIoctl::RegRead:
        ioctl_cnt.regRead++;
        break;

    case DrmIoctl::Getparam: {
        ioctl_cnt.contextGetParam++;
        auto getParam = static_cast<NEO::GetParam *>(arg);
        recordedGetParam = *getParam;
        *getParam->value = getParamRetValue;
    } break;

    case DrmIoctl::GemContextSetparam: {
    } break;

    case DrmIoctl::GemContextGetparam: {
        ioctl_cnt.contextGetParam++;
        auto getContextParam = static_cast<NEO::GemContextParam *>(arg);
        recordedGetContextParam = *getContextParam;
        getContextParam->value = getContextParamRetValue;
    } break;

    case DrmIoctl::GemContextCreateExt: {
        auto contextCreateParam = static_cast<NEO::GemContextCreateExt *>(arg);
        contextCreateParam->contextId = ++ioctl_cnt.contextCreate;
    } break;
    case DrmIoctl::GemContextDestroy: {
        ioctl_cnt.contextDestroy++;
    } break;
    case DrmIoctl::GemMmapOffset: {
        auto mmapOffsetParams = reinterpret_cast<NEO::GemMmapOffset *>(arg);
        mmapOffsetHandle = mmapOffsetParams->handle;
        mmapOffsetParams->offset = mmapOffsetExpected;
        mmapOffsetFlags = mmapOffsetParams->flags;
        ioctl_cnt.gemMmapOffset++;
        if (failOnMmapOffset == true) {
            return -1;
        }
    } break;
    case DrmIoctl::GemCreateExt: {
        auto createExtParams = reinterpret_cast<NEO::I915::drm_i915_gem_create_ext *>(arg);
        createExtSize = createExtParams->size;
        createExtHandle = createExtParams->handle;
        createExtExtensions = createExtParams->extensions;
        ioctl_cnt.gemCreateExt++;
    } break;
    case DrmIoctl::GemVmBind: {
    } break;
    case DrmIoctl::GemVmUnbind: {
    } break;
    default:
        int res = ioctlExtra(request, arg);
        if (returnIoctlExtraErrorValue) {
            return res;
        }
    }

    if (!ext->no.empty() && std::find(ext->no.begin(), ext->no.end(), ioctl_cnt.total.load()) != ext->no.end()) {
        ioctl_cnt.total.fetch_add(1);
        return ext->res;
    }
    ioctl_cnt.total.fetch_add(1);
    return ioctl_res.load();
}

DrmMockCustom::DrmMockCustom(RootDeviceEnvironment &rootDeviceEnvironment)
    : Drm(std::make_unique<HwDeviceIdDrm>(mockFd, mockPciPath), rootDeviceEnvironment) {
    reset();
    ioctl_expected.contextCreate = static_cast<int>(NEO::HwHelper::get(NEO::defaultHwInfo->platform.eRenderCoreFamily).getGpgpuEngineInstances(*NEO::defaultHwInfo).size());
    ioctl_expected.contextDestroy = ioctl_expected.contextCreate.load();
    setupIoctlHelper(rootDeviceEnvironment.getHardwareInfo()->platform.eProductFamily);
    createVirtualMemoryAddressSpace(NEO::HwHelper::getSubDevicesCount(rootDeviceEnvironment.getHardwareInfo()));
    isVmBindAvailable(); // NOLINT(clang-analyzer-optin.cplusplus.VirtualCall)
    reset();
}

int DrmMockCustom::waitUserFence(uint32_t ctxId, uint64_t address, uint64_t value, ValueWidth dataWidth, int64_t timeout, uint16_t flags) {
    waitUserFenceCall.called++;
    waitUserFenceCall.ctxId = ctxId;
    waitUserFenceCall.address = address;
    waitUserFenceCall.dataWidth = dataWidth;
    waitUserFenceCall.value = value;
    waitUserFenceCall.timeout = timeout;
    waitUserFenceCall.flags = flags;
    return Drm::waitUserFence(ctxId, address, value, dataWidth, timeout, flags);
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
