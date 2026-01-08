/*
 * Copyright (C) 2023-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/memory_manager/memory_operations_status.h"
#include "shared/source/memory_manager/pool_info.h"
#include "shared/source/memory_manager/unified_memory_properties.h"
#include "shared/source/unified_memory/unified_memory.h"
#include "shared/source/utilities/heap_allocator.h"
#include "shared/source/utilities/sorted_vector.h"

#include <functional>
#include <map>
namespace NEO {
class GraphicsAllocation;
class MemoryManager;
class MemoryOperationsHandler;
class SVMAllocsManager;
struct SvmAllocationData;

class UsmMemAllocPool : NEO::NonCopyableAndNonMovableClass {
  public:
    struct AllocationInfo {
        uint64_t address;
        size_t size;
        size_t requestedSize;
        bool isResident = false;
    };

    enum class ResidencyOperationType {
        makeResident,
        evict
    };
    using AllocationsInfoStorage = BaseSortedPointerWithValueVector<AllocationInfo>;
    using CustomCleanupFn = std::function<void(const void *)>;

    UsmMemAllocPool() = default;
    MOCKABLE_VIRTUAL ~UsmMemAllocPool() = default;
    MOCKABLE_VIRTUAL bool initialize(SVMAllocsManager *svmMemoryManager, const UnifiedMemoryProperties &memoryProperties, size_t poolSize, size_t minServicedSize, size_t maxServicedSize);
    bool initialize(SVMAllocsManager *svmMemoryManager, void *ptr, SvmAllocationData *svmData, size_t minServicedSize, size_t maxServicedSize);
    bool isInitialized() const;
    size_t getPoolSize() const;
    MOCKABLE_VIRTUAL void cleanup();
    static bool alignmentIsAllowed(size_t alignment);
    static bool flagsAreAllowed(const UnifiedMemoryProperties &memoryProperties);
    static double getPercentOfFreeMemoryForRecycling(InternalMemoryType memoryType);
    bool sizeIsAllowed(size_t size);
    bool canBePooled(size_t size, const UnifiedMemoryProperties &memoryProperties);
    MOCKABLE_VIRTUAL void *createUnifiedMemoryAllocation(size_t size, const UnifiedMemoryProperties &memoryProperties);
    bool isInPool(const void *ptr) const;
    bool isEmpty() const;
    MOCKABLE_VIRTUAL bool freeSVMAlloc(const void *ptr, bool blocking);
    size_t getPooledAllocationSize(const void *ptr);
    void *getPooledAllocationBasePtr(const void *ptr);
    size_t getOffsetInPool(const void *ptr) const;
    uint64_t getPoolAddress() const;
    PoolInfo getPoolInfo() const;
    std::mutex &getMutex() noexcept { return mtx; }
    void enableResidencyTracking() { this->trackResidency = true; }
    bool isTrackingResidency() { return this->trackResidency; }

    template <ResidencyOperationType op>
    MemoryOperationsStatus residencyOperation(const void *ptr) {
        OPTIONAL_UNRECOVERABLE_IF(nullptr == device || nullptr == allocation);
        std::unique_lock<std::mutex> lock(mtx);
        auto allocationInfo = allocations.get(ptr);
        if (allocationInfo) {
            if constexpr (ResidencyOperationType::makeResident == op) {
                if (false == std::exchange(allocationInfo->isResident, true) && 0u == this->residencyCount++) {
                    return makePoolResident();
                }
            } else { // evict
                if (true == std::exchange(allocationInfo->isResident, false) && 1u == this->residencyCount--) {
                    return evictPool();
                }
            }
            return MemoryOperationsStatus::success;
        } else { // not allocated chunk
            return MemoryOperationsStatus::memoryNotFound;
        }
    }

    void setCustomCleanup(CustomCleanupFn customCleanup) {
        this->customCleanup = std::move(customCleanup);
    }

    static constexpr auto chunkAlignment = 512u;
    static constexpr auto poolAlignment = MemoryConstants::pageSize2M;

  protected:
    MemoryOperationsStatus evictPool();
    MemoryOperationsStatus makePoolResident();
    CustomCleanupFn customCleanup = nullptr;
    std::unique_ptr<HeapAllocator> chunkAllocator;
    void *pool{};
    void *poolEnd{};
    SVMAllocsManager *svmMemoryManager{};
    AllocationsInfoStorage allocations;
    std::mutex mtx;
    RootDeviceIndicesContainer rootDeviceIndices;
    std::map<uint32_t, NEO::DeviceBitfield> deviceBitFields;
    Device *device{};
    InternalMemoryType poolMemoryType = InternalMemoryType::notSpecified;
    GraphicsAllocation *allocation{};
    SvmAllocationData *allocationData{};
    MemoryOperationsHandler *memoryOperationsIface{};
    PoolInfo poolInfo{};
    uint64_t residencyCount{};
    bool trackResidency{false};
};

class UsmMemAllocPoolsManager : NEO::NonCopyableAndNonMovableClass {
  public:
    using CustomCleanupFn = UsmMemAllocPool::CustomCleanupFn;
    static constexpr size_t maxEmptyPoolsPerBucket = 1u;

    UsmMemAllocPoolsManager(InternalMemoryType memoryType,
                            const RootDeviceIndicesContainer &rootDeviceIndices,
                            const std::map<uint32_t, DeviceBitfield> &subdeviceBitfields,
                            Device *device) : device(device), poolMemoryType(memoryType), poolMemoryProperties(memoryType, UsmMemAllocPool::poolAlignment, rootDeviceIndices, subdeviceBitfields) {
        poolMemoryProperties.device = device;
    };
    MOCKABLE_VIRTUAL ~UsmMemAllocPoolsManager() = default;
    bool initialize(SVMAllocsManager *svmMemoryManager);
    bool isInitialized() const;
    void cleanup();
    void *createUnifiedMemoryAllocation(size_t size, const UnifiedMemoryProperties &memoryProperties);
    const std::array<const PoolInfo, 3> getPoolInfos();
    size_t getMaxPoolableSize();
    UsmMemAllocPool *tryAddPool(PoolInfo poolInfo);
    MOCKABLE_VIRTUAL bool canAddPool(PoolInfo poolInfo);
    void trimEmptyPools(PoolInfo poolInfo);
    bool freeSVMAlloc(const void *ptr, bool blocking);
    size_t getPooledAllocationSize(const void *ptr);
    void *getPooledAllocationBasePtr(const void *ptr);
    size_t getOffsetInPool(const void *ptr);
    UsmMemAllocPool *getPoolContainingAlloc(const void *ptr);
    void enableResidencyTracking() { this->trackResidency = true; }
    void setCustomCleanup(CustomCleanupFn customCleanup) {
        this->customCleanup = std::move(customCleanup);
    }

  protected:
    bool canBePooled(size_t size, const UnifiedMemoryProperties &memoryProperties);
    CustomCleanupFn customCleanup = nullptr;
    SVMAllocsManager *svmMemoryManager{};
    MemoryManager *memoryManager{};
    Device *device{nullptr};
    const InternalMemoryType poolMemoryType;
    UnifiedMemoryProperties poolMemoryProperties;
    size_t totalSize{};
    std::mutex mtx;
    std::map<PoolInfo, std::vector<std::unique_ptr<UsmMemAllocPool>>> pools;
    bool trackResidency{false};
};

class UsmMemAllocPoolsFacade : NEO::NonCopyableAndNonMovableClass {
  public:
    bool initialize(InternalMemoryType memoryType, const RootDeviceIndicesContainer &rootDeviceIndices, const std::map<uint32_t, DeviceBitfield> &subdeviceBitfields, Device *device, SVMAllocsManager *svmMemoryManager);
    bool isInitialized() const;
    void cleanup();
    void *createUnifiedMemoryAllocation(size_t size, const UnifiedMemoryProperties &memoryProperties);
    bool freeSVMAlloc(const void *ptr, bool blocking);
    size_t getPooledAllocationSize(const void *ptr);
    void *getPooledAllocationBasePtr(const void *ptr);
    UsmMemAllocPool *getPoolContainingAlloc(const void *ptr);

  protected:
    std::unique_ptr<UsmMemAllocPool> pool;
    std::unique_ptr<UsmMemAllocPoolsManager> poolManager;
};

} // namespace NEO
