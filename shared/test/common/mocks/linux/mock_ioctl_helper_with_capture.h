/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/linux/engine_info.h"
#include "shared/source/os_interface/linux/ioctl_helper.h"
#include "shared/source/os_interface/linux/memory_info.h"
#include "shared/source/utilities/stackvec.h"
#include "shared/test/common/libult/linux/drm_mock.h"

#include <optional>
#include <vector>

namespace NEO {

class MockIoctlHelperWithCapture : public IoctlHelperUpstream {
  public:
    using IoctlHelperUpstream::ioctl;
    using IoctlHelperUpstream::IoctlHelperUpstream;

    struct QueryResult {
        int ret = 0;
        int32_t length = 0;
    };

    int ioctl(DrmIoctl request, void *arg) override {
        if (request != DrmIoctl::query || queryCalled >= queryResults.size()) {
            return IoctlHelperUpstream::ioctl(request, arg);
        }
        auto &result = queryResults[queryCalled++];
        auto query = static_cast<Query *>(arg);
        reinterpret_cast<QueryItem *>(query->itemsPtr)->length = result.length;
        return result.ret;
    }

    struct CreateGemExtCall {
        MemRegionsVec memClassInstances;
        size_t allocSize = 0;
        uint64_t patIndex = 0;
    };

    int createGemExt(const MemRegionsVec &memClassInstances, size_t allocSize, uint32_t &handle, uint64_t patIndex, std::optional<uint32_t> vmId, int32_t pairHandle, bool isChunked, uint32_t numOfChunks, std::optional<uint32_t> memPolicyMode, std::optional<std::vector<unsigned long>> memPolicyNodemask, std::optional<bool> isCoherent, GemCreateExtHint hint, std::optional<bool> deferBacking) override {
        createGemExtCalls.push_back({memClassInstances, allocSize, patIndex});
        handle = createGemExtHandle;
        return createGemExtResult;
    }

    std::unique_ptr<MemoryInfo> createMemoryInfo() override {
        if (!memoryRegionsToReturn) {
            return nullptr;
        }
        return std::make_unique<MemoryInfo>(*memoryRegionsToReturn, drm);
    }

    std::unique_ptr<EngineInfo> createEngineInfo(bool isSysmanEnabled) override {
        createEngineInfoCalled++;
        if (!enginesToReturn) {
            return nullptr;
        }
        StackVec<std::vector<EngineCapabilities>, 2> engineInfosPerTile{*enginesToReturn};
        return std::make_unique<EngineInfo>(&drm, engineInfosPerTile);
    }

    std::vector<QueryResult> queryResults;
    size_t queryCalled = 0u;

    std::vector<CreateGemExtCall> createGemExtCalls;
    uint32_t createGemExtHandle = 1u;
    int createGemExtResult = 0;

    std::optional<std::vector<MemoryRegion>> memoryRegionsToReturn;

    uint32_t createEngineInfoCalled = 0u;
    std::optional<std::vector<EngineCapabilities>> enginesToReturn;
};

class DrmMockWithCaptureHelper : public DrmMock {
  public:
    DrmMockWithCaptureHelper(RootDeviceEnvironment &rootDeviceEnvironment) : DrmMock(rootDeviceEnvironment) {
        ioctlHelper = std::make_unique<MockIoctlHelperWithCapture>(*this);
    }

    MockIoctlHelperWithCapture *getMockIoctlHelper() {
        return static_cast<MockIoctlHelperWithCapture *>(ioctlHelper.get());
    }
};

} // namespace NEO
