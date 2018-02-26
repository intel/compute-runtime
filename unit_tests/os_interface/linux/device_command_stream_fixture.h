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
#include "runtime/os_interface/linux/drm_neo.h"
#include <atomic>
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <iostream>
#include "drm/i915_drm.h"
#include "runtime/helpers/aligned_memory.h"
#include "runtime/os_interface/linux/drm_memory_manager.h"

#define RENDER_DEVICE_NAME_MATCHER ::testing::StrEq("/dev/dri/renderD128")

using OCLRT::Drm;

static const int mockFd = 33;

class DrmMockImpl : public Drm {
  public:
    DrmMockImpl(int fd) : Drm(fd){};
    MOCK_METHOD2(ioctl, int(unsigned long request, void *arg));
};

class DrmMockSuccess : public Drm {
  public:
    DrmMockSuccess() : Drm(mockFd) {}

    int ioctl(unsigned long request, void *arg) override { return 0; };
};

class DrmMockFail : public Drm {
  public:
    DrmMockFail() : Drm(mockFd) {}

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
    void overideCoherencyPatchActive(bool newCoherencyPatchActiveValue) { coherencyDisablePatchActive = newCoherencyPatchActiveValue; }

    std::atomic<int> ioctl_cnt;
    std::atomic<int> ioctl_res;
    std::atomic<int> ioctl_expected;
    std::atomic<IoctlResExt *> ioctl_res_ext;

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

    int ioctl(unsigned long request, void *arg) override {
        auto ext = ioctl_res_ext.load();

        //store flags
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
        if (request == DRM_IOCTL_I915_GEM_MMAP) {
            auto mmapParams = (drm_i915_gem_mmap *)arg;
            mmapHandle = mmapParams->handle;
            mmapPad = mmapParams->pad;
            mmapOffset = mmapParams->offset;
            mmapSize = mmapParams->size;
            mmapFlags = mmapParams->flags;
            mmapParams->addr_ptr = mmapAddrPtr;
        }
        if (request == DRM_IOCTL_I915_GEM_SET_DOMAIN) {
            auto setDomainParams = (drm_i915_gem_set_domain *)arg;
            setDomainHandle = setDomainParams->handle;
            setDomainReadDomains = setDomainParams->read_domains;
            setDomainWriteDomain = setDomainParams->write_domain;
        }

        if (ext->no != -1 && ext->no == ioctl_cnt.load()) {
            ioctl_cnt.fetch_add(1);
            return ext->res;
        }
        ioctl_cnt.fetch_add(1);
        return ioctl_res.load();
    };

    IoctlResExt NONE = {-1, 0};
    void reset() {
        ioctl_cnt = ioctl_res = ioctl_expected = 0;
        ioctl_res_ext = &NONE;
    }

    DrmMockCustom() : Drm(mockFd) {
        reset();
    }
};
