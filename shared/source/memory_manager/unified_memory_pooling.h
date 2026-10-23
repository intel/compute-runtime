/*
 * Copyright (C) 2023-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/memory_manager/engine_completion_snapshot.h"
#include "shared/source/memory_manager/free_policy_type.h"
#include "shared/source/memory_manager/mem_free_callback.h"
#include "shared/source/memory_manager/memory_operations_status.h"
#include "shared/source/memory_manager/pool_info.h"
#include "shared/source/memory_manager/unified_memory_pooling_facade.h"
#include "shared/source/memory_manager/unified_memory_properties.h"
#include "shared/source/unified_memory/unified_memory.h"
#include "shared/source/utilities/heap_allocator.h"
#include "shared/source/utilities/mockable_virtual_defaults.h"
#include "shared/source/utilities/sorted_vector.h"

#include <functional>
#include <map>
#include <optional>
#include <vector>
namespace NEO {
class GraphicsAllocation;
class MemoryManager;
class MemoryOperationsHandler;
class SVMAllocsManager;
class UsmMemAllocPool;
struct SvmAllocationData;

struct UsmPoolFreeResult {
    bool freeSucceeded{false};
    bool poolNowEmpty{false};
};

class UsmMemAllocPool : NEO::NonCopyableAndNonMovableClass {
  public:
    struct AllocationInfo {
        uint64_t address;
        size_t size;
        size_t requestedSize;
        uint32_t allocId;
        std::unordered_map<Device *, bool> isResident;
        std::vector<MemFreeCallback> memFreeCallbacks;
        bool isExported{false};
    };

    // A chunk freed with FreePolicyType::defer. The chunk is withheld from
    // chunkAllocator until the pool allocation retires the work snapshotted here.
    struct DeferredFreeInfo {
        AllocationInfo allocationInfo;
        EngineCompletionSnapshot snapshot;
    };

    using AllocationsInfoStorage = BaseSortedPointerWithValueVector<AllocationInfo>;
    using CustomCleanupFn = UsmMemAllocPoolsFacade::CustomCleanupFn;
    using PeerAllocationsFn = UsmMemAllocPoolsFacade::PeerAllocationsFn;

    UsmMemAllocPool() = default;
    MOCKABLE_VIRTUAL ~UsmMemAllocPool() = default;
    MOCKABLE_VIRTUAL bool initialize(SVMAllocsManager *svmMemoryManager, const UnifiedMemoryProperties &memoryProperties, size_t poolSize, size_t minServicedSize, size_t maxServicedSize);
    bool initialize(SVMAllocsManager *svmMemoryManager, void *ptr, SvmAllocationData *svmData, size_t minServicedSize, size_t maxServicedSize);
    bool isInitialized() const;
    size_t getPoolSize() const;
    MOCKABLE_VIRTUAL void cleanup();
    static bool alignmentIsAllowed(size_t alignment);
    static bool flagsAreAllowed(const UnifiedMemoryProperties &memoryProperties);
    static bool compressionHintsAreAllowed(const Device *device, const UnifiedMemoryProperties &memoryProperties, std::optional<bool> poolCompressed);
    static UnifiedMemoryProperties createPoolMemoryProperties(InternalMemoryType memoryType,
                                                              const RootDeviceIndicesContainer &rootDeviceIndices,
                                                              const std::map<uint32_t, DeviceBitfield> &subdeviceBitfields,
                                                              Device *device,
                                                              bool compressedHint,
                                                              bool uncompressedHint);
    static bool freeIfOwned(UsmMemAllocPool *pool, const void *ptr, FreePolicyType policy);
    static double getPercentOfFreeMemoryForRecycling(InternalMemoryType memoryType);
    bool sizeIsAllowed(size_t size);
    bool canBePooled(size_t size, const UnifiedMemoryProperties &memoryProperties);
    MOCKABLE_VIRTUAL void *createUnifiedMemoryAllocation(size_t size, const UnifiedMemoryProperties &memoryProperties);
    bool isInPoolRange(const void *ptr) const;
    bool isEmpty() const;
    std::optional<bool> isCompressed() const;
    MOCKABLE_VIRTUAL UsmPoolFreeResult freeSVMAlloc(const void *ptr, FreePolicyType policy);
    void reclaimDeferredFreeChunks();
    void markChunkExported(const void *ptr);
    UsmPoolLookupResult lookupAlloc(const void *ptr);
    uint32_t getChunkAllocId(const void *ptr);
    bool addMemFreeCallback(const void *ptr, MemFreeCallback callback);
    std::vector<MemFreeCallback> takeMemFreeCallbacks(const void *ptr);
    size_t getOffsetInPool(const void *ptr) const;
    uint64_t getPoolAddress() const;
    std::mutex &getMutex() noexcept { return mtx; }
    void enableResidencyTracking() { this->trackResidency = true; }

    MemoryOperationsStatus makeChunkResident(const void *ptr, Device *targetDevice, GraphicsAllocation *targetAllocation);
    MemoryOperationsStatus evictChunk(const void *ptr, Device *targetDevice);

    void setCustomCleanup(CustomCleanupFn customCleanup) {
        this->customCleanup = std::move(customCleanup);
    }

    void setPeerAllocationsFn(PeerAllocationsFn peerAllocationsFn) {
        this->peerAllocationsFn = std::move(peerAllocationsFn);
    }

    static constexpr auto chunkAlignment = 512u;
    static constexpr auto poolAlignment = MemoryConstants::pageSize2M;

  protected:
    MemoryOperationsStatus releasePoolResidency(Device *targetDevice, GraphicsAllocation *targetAllocation);
    MemoryOperationsStatus makePoolResident(Device *targetDevice, GraphicsAllocation *targetAllocation);
    // Caller must hold mtx.
    MemoryOperationsStatus dropPoolResidency(Device *targetDevice);
    // Caller must hold mtx.
    bool isEmptyImpl() const;
    // Caller must hold mtx.
    void drainDeferredFreeChunks();
    // Releases a freed chunk and the residency it held. Caller must hold mtx.
    void releaseChunk(const AllocationInfo &allocationInfo);
    CustomCleanupFn customCleanup = nullptr;
    PeerAllocationsFn peerAllocationsFn = nullptr;
    std::unique_ptr<HeapAllocator> chunkAllocator;
    void *pool{};
    void *poolEnd{};
    SVMAllocsManager *svmMemoryManager{};
    AllocationsInfoStorage allocations;
    std::vector<DeferredFreeInfo> deferredFreeChunks;
    mutable std::mutex mtx;
    RootDeviceIndicesContainer rootDeviceIndices;
    std::map<uint32_t, NEO::DeviceBitfield> deviceBitFields;
    Device *device{};
    InternalMemoryType poolMemoryType = InternalMemoryType::notSpecified;
    GraphicsAllocation *allocation{};
    SvmAllocationData *allocationData{};
    PoolInfo poolInfo{};
    std::unordered_map<Device *, uint64_t> residencyCounts;
    // handles are per adapter, so a peer device is resident through its own allocation
    std::unordered_map<Device *, GraphicsAllocation *> residentPoolAllocations;
    bool trackResidency{false};
};

class UsmMemAllocPoolsManager : NEO::NonCopyableAndNonMovableClass {
  public:
    using CustomCleanupFn = UsmMemAllocPool::CustomCleanupFn;
    using PeerAllocationsFn = UsmMemAllocPool::PeerAllocationsFn;
    static constexpr size_t maxEmptyPoolsPerBucket = 1u;

    UsmMemAllocPoolsManager(InternalMemoryType memoryType,
                            const RootDeviceIndicesContainer &rootDeviceIndices,
                            const std::map<uint32_t, DeviceBitfield> &subdeviceBitfields,
                            Device *device,
                            bool uncompressed) : device(device), poolMemoryType(memoryType),
                                                 poolMemoryProperties(UsmMemAllocPool::createPoolMemoryProperties(memoryType, rootDeviceIndices, subdeviceBitfields, device, false, uncompressed)) {};
    MOCKABLE_VIRTUAL ~UsmMemAllocPoolsManager() = default;
    bool initialize(SVMAllocsManager *svmMemoryManager);
    bool isInitialized() const;
    void cleanup();
    void *createUnifiedMemoryAllocation(size_t size, const UnifiedMemoryProperties &memoryProperties);
    const std::array<const PoolInfo, 3> getPoolInfos();
    size_t getMaxPoolableSize();
    MOCKABLE_VIRTUAL bool canAddPool(PoolInfo poolInfo);
    void trimEmptyPools(PoolInfo poolInfo);
    bool freeSVMAlloc(const void *ptr, FreePolicyType policy);
    UsmPoolLookupResult getPoolContainingAlloc(const void *ptr);
    void enableResidencyTracking() { this->trackResidency = true; }
    void setCustomCleanup(CustomCleanupFn customCleanup) {
        this->customCleanup = std::move(customCleanup);
    }
    void setPeerAllocationsFn(PeerAllocationsFn peerAllocationsFn) {
        this->peerAllocationsFn = std::move(peerAllocationsFn);
    }

  protected:
    bool canBePooled(size_t size, const UnifiedMemoryProperties &memoryProperties);
    std::unique_ptr<UsmMemAllocPool> createPool(PoolInfo poolInfo);
    void insertPool(PoolInfo poolInfo, std::unique_ptr<UsmMemAllocPool> pool);
    CustomCleanupFn customCleanup = nullptr;
    PeerAllocationsFn peerAllocationsFn = nullptr;
    SVMAllocsManager *svmMemoryManager{};
    MemoryManager *memoryManager{};
    Device *device{nullptr};
    const InternalMemoryType poolMemoryType;
    UnifiedMemoryProperties poolMemoryProperties;
    size_t totalSize{};
    std::mutex mtx;
    std::map<PoolInfo, std::vector<std::unique_ptr<UsmMemAllocPool>>> pools;
    std::optional<bool> poolsCompressed;
    bool trackResidency{false};
};

} // namespace NEO
