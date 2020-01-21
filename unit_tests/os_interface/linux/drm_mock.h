/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "core/memory_manager/memory_constants.h"
#include "core/os_interface/linux/drm_neo.h"

#include "drm/i915_drm.h"

#include <cstdio>
#include <fstream>
#include <limits.h>

using namespace NEO;

// Mock DRM class that responds to DRM_IOCTL_I915_GETPARAMs
class DrmMock : public Drm {
  public:
    using Drm::checkQueueSliceSupport;
    using Drm::engineInfo;
    using Drm::getQueueSliceCount;
    using Drm::memoryInfo;
    using Drm::nonPersistentContextsSupported;
    using Drm::preemptionSupported;
    using Drm::query;
    using Drm::sliceCountChangeSupported;

    DrmMock() : Drm(mockFd) {
        sliceCountChangeSupported = true;
    }

    ~DrmMock() {
        if (sysFsDefaultGpuPathToRestore != nullptr) {
            sysFsDefaultGpuPath = sysFsDefaultGpuPathToRestore;
        }
    }

    int ioctl(unsigned long request, void *arg) override;

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

    inline uint32_t createMemoryRegionId(uint16_t type, uint16_t instance) const {
        return (1u << (type + 16)) | (1u << instance);
    }

    static inline uint16_t getMemoryTypeFromRegion(uint32_t region) { return Math::log2(region >> 16); };
    static inline uint16_t getInstanceFromRegion(uint32_t region) { return Math::log2(region & 0xFFFF); };

    static const int mockFd = 33;

    int StoredEUVal = -1;
    int StoredSSVal = -1;
    int StoredDeviceID = 1;
    int StoredDeviceRevID = 1;
    int StoredHasPooledEU = 1;
    int StoredMinEUinPool = 1;
    int StoredPersistentContextsSupport = 1;
    int StoredRetVal = 0;
    int StoredRetValForGetGttSize = 0;
    int StoredRetValForGetSSEU = 0;
    int StoredRetValForSetSSEU = 0;
    int StoredRetValForDeviceID = 0;
    int StoredRetValForEUVal = 0;
    int StoredRetValForSSVal = 0;
    int StoredRetValForDeviceRevID = 0;
    int StoredRetValForPooledEU = 0;
    int StoredRetValForMinEUinPool = 0;
    int StoredRetValForPersistant = 0;
    int StoredPreemptionSupport =
        I915_SCHEDULER_CAP_ENABLED |
        I915_SCHEDULER_CAP_PRIORITY |
        I915_SCHEDULER_CAP_PREEMPTION;
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

    uint64_t storedGTTSize = 1ull << 47;
    uint64_t storedParamSseu = ULONG_MAX;

    virtual int handleRemainingRequests(unsigned long request, void *arg) { return -1; }

  private:
    const char *sysFsDefaultGpuPathToRestore = nullptr;
};
