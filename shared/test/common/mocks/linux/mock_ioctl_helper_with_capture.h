/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/common_types.h"
#include "shared/source/os_interface/linux/engine_info.h"
#include "shared/source/os_interface/linux/ioctl_helper.h"
#include "shared/source/os_interface/linux/memory_info.h"
#include "shared/source/utilities/stackvec.h"
#include "shared/test/common/libult/linux/drm_mock.h"

#include <optional>
#include <utility>
#include <vector>

namespace NEO {

class MockIoctlHelperWithCapture : public IoctlHelperUpstream {
  public:
    using IoctlHelperUpstream::ioctl;
    using IoctlHelperUpstream::IoctlHelperUpstream;
    using IoctlHelperUpstream::waitUserFence;

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

    struct VmPrefetchCall {
        uint64_t start = 0;
        uint64_t length = 0;
        uint32_t region = 0;
        uint32_t vmId = 0;
    };

    struct CreateGemExtCall {
        MemRegionsVec memClassInstances;
        std::optional<std::vector<unsigned long>> memPolicyNodemask;
        size_t allocSize = 0;
        uint64_t patIndex = 0;
        std::optional<uint32_t> vmId;
        std::optional<uint32_t> memPolicyMode;
        int32_t pairHandle = -1;
        uint32_t numOfChunks = 0;
        bool isChunked = false;
    };

    int createGemExt(const MemRegionsVec &memClassInstances, size_t allocSize, uint32_t &handle, uint64_t patIndex, std::optional<uint32_t> vmId, int32_t pairHandle, bool isChunked, uint32_t numOfChunks, std::optional<uint32_t> memPolicyMode, std::optional<std::vector<unsigned long>> memPolicyNodemask, std::optional<bool> isCoherent, GemCreateExtHint hint, std::optional<bool> deferBacking) override {
        createGemExtCalls.push_back({memClassInstances, memPolicyNodemask, allocSize, patIndex, vmId, memPolicyMode, pairHandle, numOfChunks, isChunked});
        handle = createGemExtHandle;
        return createGemExtResult;
    }

    bool retrieveMmapOffsetForBufferObject(BufferObject &bo, uint64_t flags, uint64_t &offset) override {
        retrieveMmapOffsetCalled++;
        offset = mmapOffsetToReturn;
        return retrieveMmapOffsetResult;
    }

    std::unique_ptr<MemoryInfo> createMemoryInfo() override {
        if (!memoryRegionsToReturn) {
            return nullptr;
        }
        return std::make_unique<MemoryInfo>(*memoryRegionsToReturn, drm);
    }

    bool setVmBoAdvise(int32_t handle, uint32_t attribute, void *region) override {
        vmBoAdviseCalled++;
        return vmBoAdviseResult;
    }

    bool setVmBoAdviseForChunking(int32_t handle, uint64_t start, uint64_t length, uint32_t attribute, void *region) override {
        vmBoAdviseForChunkingCalled++;
        return vmBoAdviseResult;
    }

    bool setVmPrefetch(uint64_t start, uint64_t length, uint32_t region, uint32_t vmId) override {
        vmPrefetchCalls.push_back({start, length, region, vmId});
        return vmPrefetchResult;
    }

    std::optional<MemoryClassInstance> getPreferredLocationRegion(PreferredLocation memoryLocation, uint32_t memoryInstance) override {
        if (!preferredLocationRegionAvailable) {
            return std::nullopt;
        }
        return MemoryClassInstance{static_cast<uint16_t>(getDrmParamValue(DrmParam::memoryClassDevice)), static_cast<uint16_t>(memoryInstance)};
    }

    std::optional<uint32_t> getVmAdviseAtomicAttribute() override {
        return vmAdviseAtomicAttribute;
    }

    CacheRegion closAlloc(CacheLevel cacheLevel) override {
        if (!closSupported || ++closIndex == 0) {
            return CacheRegion::none;
        }
        return static_cast<CacheRegion>(closIndex);
    }

    uint16_t closAllocWays(CacheRegion closIndex, uint16_t cacheLevel, uint16_t numWays) override {
        if (!closSupported || toUnderlying(closIndex) > this->closIndex || numWays > closMaxNumWays - closAllocatedNumWays) {
            return 0;
        }
        closAllocatedNumWays += numWays;
        return numWays;
    }

    CacheRegion closFree(CacheRegion closIndex) override {
        if (!closSupported || toUnderlying(closIndex) > this->closIndex) {
            return CacheRegion::none;
        }
        this->closIndex--;
        return closIndex;
    }

    bool isVmBindPatIndexExtSupported() override {
        return true;
    }

    void fillVmBindExtSetPat(VmBindExtSetPatT &vmBindExtSetPat, uint64_t patIndex, uint64_t nextExtension) override {
        filledVmBindExtPatIndex = patIndex;
    }

    int vmBind(const VmBindParams &vmBindParams) override {
        vmBindCalled++;
        receivedVmBind = vmBindParams;
        receivedVmBindPatIndex = std::exchange(filledVmBindExtPatIndex, std::nullopt);
        return vmBindResult;
    }

    int vmUnbind(const VmBindParams &vmBindParams) override {
        vmUnbindCalled++;
        receivedVmUnbind = vmBindParams;
        receivedVmUnbindPatIndex = std::exchange(filledVmBindExtPatIndex, std::nullopt);
        return vmUnbindResult;
    }

    int waitUserFence(uint32_t ctxId, uint64_t address, uint64_t value, uint32_t dataWidth, int64_t timeout, uint16_t flags,
                      bool userInterrupt, uint32_t externalInterruptId, GraphicsAllocation *allocForInterruptWait) override {
        waitUserFenceCalled++;
        return waitUserFenceResult;
    }

    bool requiresUserFenceSetup(bool bind) const override {
        return userFenceSetupRequired;
    }

    bool isVmBindAvailable() override {
        isVmBindAvailableCalled++;
        return vmBindAvailable;
    }

    bool isSetPairAvailable() override {
        isSetPairAvailableCalled++;
        return setPairAvailable;
    }

    bool isChunkingAvailable() override {
        isChunkingAvailableCalled++;
        return chunkingAvailable;
    }

    std::unique_ptr<EngineInfo> createEngineInfo(bool isSysmanEnabled) override {
        createEngineInfoCalled++;
        if (!enginesToReturn) {
            return nullptr;
        }
        StackVec<std::vector<EngineCapabilities>, 2> engineInfosPerTile{*enginesToReturn};
        return std::make_unique<EngineInfo>(&drm, engineInfosPerTile);
    }

    bool closSupported = false;
    uint16_t closIndex = 0u;
    uint16_t closMaxNumWays = 32u;
    uint16_t closAllocatedNumWays = 0u;

    std::vector<QueryResult> queryResults;
    size_t queryCalled = 0u;

    std::vector<CreateGemExtCall> createGemExtCalls;
    uint32_t createGemExtHandle = 1u;
    int createGemExtResult = 0;

    std::optional<std::vector<MemoryRegion>> memoryRegionsToReturn;

    std::vector<VmPrefetchCall> vmPrefetchCalls;
    std::optional<uint32_t> vmAdviseAtomicAttribute = 0u;
    uint32_t vmBoAdviseCalled = 0u;
    uint32_t vmBoAdviseForChunkingCalled = 0u;
    bool vmBoAdviseResult = true;
    bool vmPrefetchResult = true;
    bool preferredLocationRegionAvailable = false;

    bool retrieveMmapOffsetResult = true;
    uint32_t retrieveMmapOffsetCalled = 0u;
    uint64_t mmapOffsetToReturn = 0u;

    std::optional<uint64_t> filledVmBindExtPatIndex;
    std::optional<VmBindParams> receivedVmBind;
    std::optional<uint64_t> receivedVmBindPatIndex;
    std::optional<VmBindParams> receivedVmUnbind;
    std::optional<uint64_t> receivedVmUnbindPatIndex;
    uint32_t vmBindCalled = 0u;
    int vmBindResult = 0;
    uint32_t vmUnbindCalled = 0u;
    int vmUnbindResult = 0;
    uint32_t waitUserFenceCalled = 0u;
    int waitUserFenceResult = 0;
    bool userFenceSetupRequired = false;

    uint32_t isVmBindAvailableCalled = 0u;
    uint32_t isSetPairAvailableCalled = 0u;
    uint32_t isChunkingAvailableCalled = 0u;
    bool vmBindAvailable = false;
    bool setPairAvailable = false;
    bool chunkingAvailable = false;

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
