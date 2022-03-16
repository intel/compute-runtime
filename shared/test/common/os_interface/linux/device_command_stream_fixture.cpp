/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/os_interface/linux/device_command_stream_fixture.h"

const int mockFd = 33;
const char *mockPciPath = "";

void Ioctls::reset() {
    total = 0;
    query = 0;
    execbuffer2 = 0;
    gemUserptr = 0;
    gemCreate = 0;
    gemSetTiling = 0;
    gemGetTiling = 0;
    gemGetAperture = 0;
    primeFdToHandle = 0;
    handleToPrimeFd = 0;
    gemMmap = 0;
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
    NEO_IOCTL_EXPECT_EQ(gemSetTiling);
    NEO_IOCTL_EXPECT_EQ(gemGetTiling);
    NEO_IOCTL_EXPECT_EQ(primeFdToHandle);
    NEO_IOCTL_EXPECT_EQ(handleToPrimeFd);
    NEO_IOCTL_EXPECT_EQ(gemMmap);
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

int DrmMockCustom::ioctl(unsigned long request, void *arg) {
    auto ext = ioctl_res_ext.load();

    //store flags
    switch (request) {
    case DRM_IOCTL_I915_GEM_EXECBUFFER2: {
        drm_i915_gem_execbuffer2 *execbuf = (drm_i915_gem_execbuffer2 *)arg;
        this->execBuffer = *execbuf;
        this->execBufferBufferObjects =
            *reinterpret_cast<drm_i915_gem_exec_object2 *>(this->execBuffer.buffers_ptr);
        ioctl_cnt.execbuffer2++;
        execBufferExtensions(execbuf);
    } break;

    case DRM_IOCTL_I915_GEM_USERPTR: {
        auto *userPtrParams = (drm_i915_gem_userptr *)arg;
        userPtrParams->handle = returnHandle;
        returnHandle++;
        ioctl_cnt.gemUserptr++;
    } break;

    case DRM_IOCTL_I915_GEM_CREATE: {
        auto *createParams = (drm_i915_gem_create *)arg;
        this->createParamsSize = createParams->size;
        this->createParamsHandle = createParams->handle = 1u;
        ioctl_cnt.gemCreate++;
    } break;
    case DRM_IOCTL_I915_GEM_SET_TILING: {
        auto *setTilingParams = (drm_i915_gem_set_tiling *)arg;
        setTilingMode = setTilingParams->tiling_mode;
        setTilingHandle = setTilingParams->handle;
        setTilingStride = setTilingParams->stride;
        ioctl_cnt.gemSetTiling++;
    } break;
    case DRM_IOCTL_I915_GEM_GET_TILING: {
        auto *getTilingParams = (drm_i915_gem_get_tiling *)arg;
        getTilingParams->tiling_mode = getTilingModeOut;
        getTilingHandleIn = getTilingParams->handle;
        ioctl_cnt.gemGetTiling++;
    } break;
    case DRM_IOCTL_PRIME_FD_TO_HANDLE: {
        auto *primeToHandleParams = (drm_prime_handle *)arg;
        //return BO
        primeToHandleParams->handle = outputHandle;
        inputFd = primeToHandleParams->fd;
        ioctl_cnt.primeFdToHandle++;
    } break;
    case DRM_IOCTL_PRIME_HANDLE_TO_FD: {
        auto *handleToPrimeParams = (drm_prime_handle *)arg;
        //return FD
        inputHandle = handleToPrimeParams->handle;
        inputFlags = handleToPrimeParams->flags;
        handleToPrimeParams->fd = outputFd;
        ioctl_cnt.handleToPrimeFd++;
    } break;
    case DRM_IOCTL_I915_GEM_MMAP: {
        auto mmapParams = (drm_i915_gem_mmap *)arg;
        mmapHandle = mmapParams->handle;
        mmapPad = mmapParams->pad;
        mmapOffset = mmapParams->offset;
        mmapSize = mmapParams->size;
        mmapFlags = mmapParams->flags;
        mmapParams->addr_ptr = mmapAddrPtr;
        ioctl_cnt.gemMmap++;
    } break;
    case DRM_IOCTL_I915_GEM_SET_DOMAIN: {
        auto setDomainParams = (drm_i915_gem_set_domain *)arg;
        setDomainHandle = setDomainParams->handle;
        setDomainReadDomains = setDomainParams->read_domains;
        setDomainWriteDomain = setDomainParams->write_domain;
        ioctl_cnt.gemSetDomain++;
    } break;

    case DRM_IOCTL_I915_GEM_WAIT: {
        auto gemWaitParams = (drm_i915_gem_wait *)arg;
        gemWaitTimeout = gemWaitParams->timeout_ns;
        ioctl_cnt.gemWait++;
    } break;

    case DRM_IOCTL_GEM_CLOSE:
        ioctl_cnt.gemClose++;
        break;

    case DRM_IOCTL_I915_REG_READ:
        ioctl_cnt.regRead++;
        break;

    case DRM_IOCTL_I915_GETPARAM: {
        ioctl_cnt.contextGetParam++;
        auto getParam = (drm_i915_getparam_t *)arg;
        recordedGetParam = *getParam;
        *getParam->value = getParamRetValue;
    } break;

    case DRM_IOCTL_I915_GEM_CONTEXT_SETPARAM: {
    } break;

    case DRM_IOCTL_I915_GEM_CONTEXT_GETPARAM: {
        ioctl_cnt.contextGetParam++;
        auto getContextParam = (drm_i915_gem_context_param *)arg;
        recordedGetContextParam = *getContextParam;
        getContextParam->value = getContextParamRetValue;
    } break;

    case DRM_IOCTL_I915_GEM_CONTEXT_CREATE_EXT: {
        auto contextCreateParam = reinterpret_cast<drm_i915_gem_context_create_ext *>(arg);
        contextCreateParam->ctx_id = ++ioctl_cnt.contextCreate;
    } break;
    case DRM_IOCTL_I915_GEM_CONTEXT_DESTROY: {
        ioctl_cnt.contextDestroy++;
    } break;
    case DRM_IOCTL_I915_GEM_MMAP_OFFSET: {
        auto mmapOffsetParams = reinterpret_cast<drm_i915_gem_mmap_offset *>(arg);
        mmapOffsetParams->handle = mmapOffsetHandle;
        mmapOffsetParams->offset = mmapOffsetExpected;
        mmapOffsetFlags = mmapOffsetParams->flags;
        ioctl_cnt.gemMmapOffset++;
        if (failOnMmapOffset == true) {
            return -1;
        }
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
    isVmBindAvailable();
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
