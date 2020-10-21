/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/string.h"
#include "shared/source/os_interface/linux/drm_neo.h"

#include "opencl/source/platform/platform.h"
#include "opencl/test/unit_test/linux/mock_os_layer.h"

#include "drm/i915_drm.h"
#include "gtest/gtest.h"

#include <cstdio>
#include <fstream>
#include <limits.h>

using namespace NEO;

// Mock DRM class that responds to DRM_IOCTL_I915_GETPARAMs
class DrmMock : public Drm {
  public:
    using Drm::checkQueueSliceSupport;
    using Drm::classHandles;
    using Drm::engineInfo;
    using Drm::generateUUID;
    using Drm::getQueueSliceCount;
    using Drm::memoryInfo;
    using Drm::nonPersistentContextsSupported;
    using Drm::preemptionSupported;
    using Drm::query;
    using Drm::requirePerContextVM;
    using Drm::sliceCountChangeSupported;
    using Drm::virtualMemoryIds;

    DrmMock(int fd, RootDeviceEnvironment &rootDeviceEnvironment) : Drm(std::make_unique<HwDeviceId>(fd, ""), rootDeviceEnvironment) {
        sliceCountChangeSupported = true;
        if (!rootDeviceEnvironment.executionEnvironment.isPerContextMemorySpaceRequired()) {
            createVirtualMemoryAddressSpace(HwHelper::getSubDevicesCount(rootDeviceEnvironment.getHardwareInfo()));
        }
    }
    DrmMock(RootDeviceEnvironment &rootDeviceEnvironment) : DrmMock(mockFd, rootDeviceEnvironment) {}

    int ioctl(unsigned long request, void *arg) override;
    int getErrno() override {
        if (baseErrno) {
            return Drm::getErrno();
        }
        return errnoRetVal;
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
        hwDeviceId = std::make_unique<HwDeviceId>(fd, "");
    }

    void setPciPath(const char *pciPath) {
        hwDeviceId = std::make_unique<HwDeviceId>(getFileDescriptor(), pciPath);
    }

    void setDeviceID(int deviceId) { this->deviceId = deviceId; }
    void setDeviceRevID(int revisionId) { this->revisionId = revisionId; }

    static const int mockFd = 33;

    bool failRetTopology = false;
    bool baseErrno = true;
    int errnoRetVal = 0;
    int StoredEUVal = 8;
    int StoredSSVal = 2;
    int StoredSVal = 1;
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
    int StoredRetValForVmId = 1;

    bool disableSomeTopology = false;

    uint32_t receivedCreateContextId = 0;
    uint32_t receivedDestroyContextId = 0;
    uint32_t ioctlCallsCount = 0;

    uint32_t receivedContextParamRequestCount = 0;
    drm_i915_gem_context_param receivedContextParamRequest = {};

    //DRM_IOCTL_I915_GEM_EXECBUFFER2
    drm_i915_gem_execbuffer2 execBuffer = {0};
    uint64_t bbFlags;

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
    int fdToHandleRetVal = 0;
    //DRM_IOCTL_HANDLE_TO_FD
    __s32 outputFd = 0;
    //DRM_IOCTL_I915_GEM_USERPTR
    __u32 returnHandle = 0;
    __u64 gpuMemSize = 3u * MemoryConstants::gigaByte;
    //DRM_IOCTL_I915_GEM_MMAP
    uint64_t lockedPtr[4];

    uint64_t storedGTTSize = 1ull << 47;
    uint64_t storedParamSseu = ULONG_MAX;

    virtual int handleRemainingRequests(unsigned long request, void *arg) { return -1; }
};

class DrmMockNonFailing : public DrmMock {
  public:
    using DrmMock::DrmMock;
    int handleRemainingRequests(unsigned long request, void *arg) override { return 0; }
};

class DrmMockEngine : public DrmMock {
  public:
    uint32_t i915QuerySuccessCount = std::numeric_limits<uint32_t>::max();
    uint32_t queryEngineInfoSuccessCount = std::numeric_limits<uint32_t>::max();

    DrmMockEngine(RootDeviceEnvironment &rootDeviceEnvironment) : DrmMock(rootDeviceEnvironment) {}

    virtual int handleRemainingRequests(unsigned long request, void *arg) {
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

    void handleQueryItem(drm_i915_query_item *queryItem) {
        switch (queryItem->query_id) {
        case DRM_I915_QUERY_ENGINE_INFO:
            if (queryEngineInfoSuccessCount == 0) {
                queryItem->length = -EINVAL;
            } else {
                queryEngineInfoSuccessCount--;
                auto numberOfEngines = 2u;
                int engineInfoSize = sizeof(drm_i915_query_engine_info) + numberOfEngines * sizeof(drm_i915_engine_info);
                if (queryItem->length == 0) {
                    queryItem->length = engineInfoSize;
                } else {
                    EXPECT_EQ(engineInfoSize, queryItem->length);
                    auto queryEnginenInfo = reinterpret_cast<drm_i915_query_engine_info *>(queryItem->data_ptr);
                    EXPECT_EQ(0u, queryEnginenInfo->num_engines);
                    queryEnginenInfo->num_engines = numberOfEngines;
                    queryEnginenInfo->engines[0].engine.engine_class = I915_ENGINE_CLASS_RENDER;
                    queryEnginenInfo->engines[0].engine.engine_instance = 1;
                    queryEnginenInfo->engines[1].engine.engine_class = I915_ENGINE_CLASS_COPY;
                    queryEnginenInfo->engines[1].engine.engine_instance = 1;
                }
            }
            break;
        }
    }
};

class DrmMockResources : public DrmMock {
  public:
    using DrmMock::DrmMock;

    bool registerResourceClasses() override {
        registerClassesCalled = true;
        return true;
    }

    uint32_t registerResource(ResourceClass classType, void *data, size_t size) override {
        registeredClass = classType;
        memcpy_s(registeredData, sizeof(registeredData), data, size);
        registeredDataSize = size;
        return registerResourceReturnHandle;
    }

    void unregisterResource(uint32_t handle) override {
        unregisterCalledCount++;
        unregisteredHandle = handle;
    }

    static const uint32_t registerResourceReturnHandle;

    uint32_t unregisteredHandle = 0;
    uint32_t unregisterCalledCount = 0;
    ResourceClass registeredClass = ResourceClass::MaxSize;
    bool registerClassesCalled = false;
    uint64_t registeredData[128];
    size_t registeredDataSize;
};
