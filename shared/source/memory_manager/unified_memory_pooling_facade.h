/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/device_bitfield.h"
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/memory_manager/free_policy_type.h"
#include "shared/source/memory_manager/pool_info.h"
#include "shared/source/unified_memory/unified_memory.h"
#include "shared/source/utilities/stackvec.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>

namespace NEO {
class Device;
class GraphicsAllocation;
class SVMAllocsManager;
class UsmMemAllocPool;
class UsmMemAllocPoolsManager;
struct UnifiedMemoryProperties;

// pool is a bare pointer into a UsmMemAllocPoolsManager bucket, returned after the manager
// lock is dropped. Only a live chunk keeps that pool off trimEmptyPools; the residency paths
// deliberately dereference it without one and rely on the API-level pointer being valid.
struct UsmPoolLookupResult {
    UsmMemAllocPool *pool{nullptr};
    PoolInfo poolInfo{};
    void *pooledAllocationBasePtr{nullptr};
    size_t pooledAllocationSize{0u};
    bool isAllocatedInPool() const { return nullptr != pooledAllocationBasePtr; }
};

class UsmMemAllocPoolsFacade : NEO::NonCopyableAndNonMovableClass {
  public:
    using CustomCleanupFn = std::function<void(const void *)>;
    using PeerAllocationsFn = std::function<StackVec<GraphicsAllocation *, 4>(const void *)>;
    struct InitParams {
        CustomCleanupFn customCleanup{};
        bool trackResidency{false};
        bool compressedHint{false};
        PeerAllocationsFn peerAllocations{};
        bool uncompressedPool{false};
    };
    UsmMemAllocPoolsFacade() noexcept;
    ~UsmMemAllocPoolsFacade();
    static bool poolingEnabled(InternalMemoryType memoryType, bool enabledByDefault);
    static bool isPoolManagerSupported(InternalMemoryType memoryType, const Device *device);
    bool initialize(InternalMemoryType memoryType, const RootDeviceIndicesContainer &rootDeviceIndices, const std::map<uint32_t, DeviceBitfield> &subdeviceBitfields, Device *device, SVMAllocsManager *svmMemoryManager, const InitParams &initParams);
    bool isInitialized() const;
    void cleanup();
    void *createUnifiedMemoryAllocation(size_t size, const UnifiedMemoryProperties &memoryProperties);
    bool freeSVMAlloc(const void *ptr, FreePolicyType policy);
    size_t getPooledAllocationSize(const void *ptr);
    void *getPooledAllocationBasePtr(const void *ptr);
    UsmPoolLookupResult getPoolContainingAlloc(const void *ptr);
    UsmMemAllocPool *getPool() const { return pool.get(); }
    UsmMemAllocPoolsManager *getPoolManager() const { return poolManager.get(); }

  protected:
    std::unique_ptr<UsmMemAllocPool> pool;
    std::unique_ptr<UsmMemAllocPoolsManager> poolManager;
};

} // namespace NEO
