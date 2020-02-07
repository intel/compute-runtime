/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/helpers/aligned_memory.h"
#include "core/helpers/hw_helper.h"
#include "core/os_interface/linux/drm_memory_manager.h"
#include "core/os_interface/linux/drm_neo.h"
#include "core/unit_tests/helpers/default_hw_info.h"
#include "runtime/platform/platform.h"
#include "unit_tests/helpers/gtest_helpers.h"

#include "drm/i915_drm.h"
#include "engine_node.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include <atomic>
#include <cstdint>
#include <iostream>

#define RENDER_DEVICE_NAME_MATCHER ::testing::StrEq("/dev/dri/renderD128")

using NEO::constructPlatform;
using NEO::Drm;
using NEO::HwDeviceId;
using NEO::RootDeviceEnvironment;

static const int mockFd = 33;

class DrmMockImpl : public Drm {
  public:
    DrmMockImpl(int fd) : Drm(std::make_unique<HwDeviceId>(fd), *constructPlatform()->peekExecutionEnvironment()->rootDeviceEnvironments[0]){};
    MOCK_METHOD2(ioctl, int(unsigned long request, void *arg));
};

class DrmMockSuccess : public Drm {
  public:
    DrmMockSuccess() : DrmMockSuccess(*constructPlatform()->peekExecutionEnvironment()->rootDeviceEnvironments[0]) {}
    DrmMockSuccess(RootDeviceEnvironment &rootDeviceEnvironment) : Drm(std::make_unique<HwDeviceId>(mockFd), rootDeviceEnvironment) {}

    int ioctl(unsigned long request, void *arg) override { return 0; };
};

class DrmMockFail : public Drm {
  public:
    DrmMockFail() : Drm(std::make_unique<HwDeviceId>(mockFd), *constructPlatform()->peekExecutionEnvironment()->rootDeviceEnvironments[0]) {}

    int ioctl(unsigned long request, void *arg) override { return -1; };
};

class DrmMockTime : public DrmMockSuccess {
  public:
    int ioctl(unsigned long request, void *arg) override {
        drm_i915_reg_read *reg = reinterpret_cast<drm_i915_reg_read *>(arg);
        reg->val = getVal() << 32;
        return 0;
    };

    uint64_t getVal() {
        static uint64_t val = 0;
        return ++val;
    }
};

class DrmMockCustom : public Drm {
  public:
    struct IoctlResExt {
        int32_t no;
        int32_t res;

        IoctlResExt(int32_t no, int32_t res) : no(no), res(res) {}
    };

    class Ioctls {
      public:
        void reset() {
            total = 0;
            execbuffer2 = 0;
            gemUserptr = 0;
            gemCreate = 0;
            gemSetTiling = 0;
            gemGetTiling = 0;
            primeFdToHandle = 0;
            handleToPrimeFd = 0;
            gemMmap = 0;
            gemSetDomain = 0;
            gemWait = 0;
            gemClose = 0;
            regRead = 0;
            getParam = 0;
            contextGetParam = 0;
            contextCreate = 0;
            contextDestroy = 0;
        }

        std::atomic<int32_t> total;
        std::atomic<int32_t> execbuffer2;
        std::atomic<int32_t> gemUserptr;
        std::atomic<int32_t> gemCreate;
        std::atomic<int32_t> gemSetTiling;
        std::atomic<int32_t> gemGetTiling;
        std::atomic<int32_t> primeFdToHandle;
        std::atomic<int32_t> handleToPrimeFd;
        std::atomic<int32_t> gemMmap;
        std::atomic<int32_t> gemSetDomain;
        std::atomic<int32_t> gemWait;
        std::atomic<int32_t> gemClose;
        std::atomic<int32_t> regRead;
        std::atomic<int32_t> getParam;
        std::atomic<int32_t> contextGetParam;
        std::atomic<int32_t> contextCreate;
        std::atomic<int32_t> contextDestroy;
    };

    std::atomic<int> ioctl_res;
    Ioctls ioctl_cnt;
    Ioctls ioctl_expected;
    std::atomic<IoctlResExt *> ioctl_res_ext;

    void testIoctls() {
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

    //DRM_IOCTL_I915_GEM_EXECBUFFER2
    drm_i915_gem_execbuffer2 execBuffer = {0};

    //DRM_IOCTL_I915_GEM_CREATE
    __u64 createParamsSize = 0;
    __u32 createParamsHandle = 0;
    //DRM_IOCTL_I915_GEM_SET_TILING
    __u32 setTilingMode = 0;
    __u32 setTilingHandle = 0;
    __u32 setTilingStride = 0;
    //DRM_IOCTL_I915_GEM_GET_TILING
    __u32 getTilingModeOut = I915_TILING_NONE;
    __u32 getTilingHandleIn = 0;
    //DRM_IOCTL_PRIME_FD_TO_HANDLE
    __u32 outputHandle = 0;
    __s32 inputFd = 0;
    //DRM_IOCTL_PRIME_HANDLE_TO_FD
    __u32 inputHandle = 0;
    __s32 outputFd = 0;
    __s32 inputFlags = 0;
    //DRM_IOCTL_I915_GEM_USERPTR
    __u32 returnHandle = 0;
    //DRM_IOCTL_I915_GEM_MMAP
    __u32 mmapHandle = 0;
    __u32 mmapPad = 0;
    __u64 mmapOffset = 0;
    __u64 mmapSize = 0;
    __u64 mmapAddrPtr = 0x7F4000001000;
    __u64 mmapFlags = 0;
    //DRM_IOCTL_I915_GEM_SET_DOMAIN
    __u32 setDomainHandle = 0;
    __u32 setDomainReadDomains = 0;
    __u32 setDomainWriteDomain = 0;
    //DRM_IOCTL_I915_GETPARAM
    drm_i915_getparam_t recordedGetParam = {0};
    int getParamRetValue = 0;
    //DRM_IOCTL_I915_GEM_CONTEXT_GETPARAM
    drm_i915_gem_context_param recordedGetContextParam = {0};
    __u64 getContextParamRetValue = 0;

    int errnoValue = 0;

    int ioctl(unsigned long request, void *arg) override {
        auto ext = ioctl_res_ext.load();

        //store flags
        switch (request) {
        case DRM_IOCTL_I915_GEM_EXECBUFFER2: {
            drm_i915_gem_execbuffer2 *execbuf = (drm_i915_gem_execbuffer2 *)arg;
            this->execBuffer = *execbuf;
            ioctl_cnt.execbuffer2++;
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

        case DRM_IOCTL_I915_GEM_WAIT:
            ioctl_cnt.gemWait++;
            break;

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

        case DRM_IOCTL_I915_GEM_CONTEXT_CREATE: {
            auto contextCreateParam = reinterpret_cast<drm_i915_gem_context_create *>(arg);
            contextCreateParam->ctx_id = ++ioctl_cnt.contextCreate;
        } break;
        case DRM_IOCTL_I915_GEM_CONTEXT_DESTROY: {
            ioctl_cnt.contextDestroy++;
        } break;

        default:
            ioctlExtra(request, arg);
        }

        if (ext->no != -1 && ext->no == ioctl_cnt.total.load()) {
            ioctl_cnt.total.fetch_add(1);
            return ext->res;
        }
        ioctl_cnt.total.fetch_add(1);
        return ioctl_res.load();
    };

    virtual int ioctlExtra(unsigned long request, void *arg) {
        switch (request) {
        default:
            std::cout << "unexpected IOCTL: " << std::hex << request << std::endl;
            UNRECOVERABLE_IF(true);
            break;
        }
        return 0;
    }

    IoctlResExt NONE = {-1, 0};
    void reset() {
        ioctl_res = 0;
        ioctl_cnt.reset();
        ioctl_expected.reset();
        ioctl_res_ext = &NONE;
    }

    DrmMockCustom() : Drm(std::make_unique<HwDeviceId>(mockFd), *constructPlatform()->peekExecutionEnvironment()->rootDeviceEnvironments[0]) {
        reset();
        ioctl_expected.contextCreate = static_cast<int>(NEO::HwHelper::get(NEO::platformDevices[0]->platform.eRenderCoreFamily).getGpgpuEngineInstances().size());
        ioctl_expected.contextDestroy = ioctl_expected.contextCreate.load();
    }
    int getErrno() override {
        return errnoValue;
    }
};
