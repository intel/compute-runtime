/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/constants.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/utilities/heap_allocator.h"
#include "shared/source/utilities/sorted_vector.h"

#include <array>
#include <map>

namespace NEO {
class UsmMemAllocPool : NEO::NonCopyableAndNonMovableClass {
  public:
    struct PoolInfo {
        size_t minServicedSize;
        size_t maxServicedSize;
        size_t poolSize;
        bool operator<(const PoolInfo &rhs) const {
            return this->minServicedSize < rhs.minServicedSize;
        }
    };
    using UnifiedMemoryProperties = SVMAllocsManager::UnifiedMemoryProperties;
    struct AllocationInfo {
        uint64_t address;
        size_t size;
        size_t requestedSize;
    };
    using AllocationsInfoStorage = BaseSortedPointerWithValueVector<AllocationInfo>;

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

    static constexpr auto chunkAlignment = 512u;
    static constexpr auto poolAlignment = MemoryConstants::pageSize2M;

  protected:
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
    PoolInfo poolInfo{};
};

class UsmMemAllocPoolsManager : NEO::NonCopyableAndNonMovableClass {
  public:
    using PoolInfo = UsmMemAllocPool::PoolInfo;
    static constexpr uint64_t KB = MemoryConstants::kiloByte; // NOLINT(readability-identifier-naming)
    static constexpr uint64_t MB = MemoryConstants::megaByte; // NOLINT(readability-identifier-naming)
    static constexpr uint64_t maxPoolableSize = 2 * MB;
    // clang-format off
    static constexpr std::array<const PoolInfo, 3> poolInfos = {
        PoolInfo{ 0,          4 * KB,  2 * MB},
        PoolInfo{ 4 * KB+1,  64 * KB,  2 * MB},
        PoolInfo{64 * KB+1,   2 * MB, 16 * MB}};
    // clang-format on
    static constexpr size_t maxEmptyPoolsPerBucket = 1u;

    using UnifiedMemoryProperties = SVMAllocsManager::UnifiedMemoryProperties;
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
    UsmMemAllocPool *tryAddPool(PoolInfo poolInfo);
    MOCKABLE_VIRTUAL bool canAddPool(PoolInfo poolInfo);
    void trimEmptyPools(PoolInfo poolInfo);
    bool freeSVMAlloc(const void *ptr, bool blocking);
    size_t getPooledAllocationSize(const void *ptr);
    void *getPooledAllocationBasePtr(const void *ptr);
    size_t getOffsetInPool(const void *ptr);
    UsmMemAllocPool *getPoolContainingAlloc(const void *ptr);

  protected:
    static bool canBePooled(size_t size, const UnifiedMemoryProperties &memoryProperties) {
        return size <= maxPoolableSize &&
               UsmMemAllocPool::alignmentIsAllowed(memoryProperties.alignment) &&
               UsmMemAllocPool::flagsAreAllowed(memoryProperties);
    }

    SVMAllocsManager *svmMemoryManager{};
    MemoryManager *memoryManager{};
    Device *device{nullptr};
    const InternalMemoryType poolMemoryType;
    UnifiedMemoryProperties poolMemoryProperties;
    size_t totalSize{};
    std::mutex mtx;
    std::map<PoolInfo, std::vector<std::unique_ptr<UsmMemAllocPool>>> pools;
};

} // namespace NEO
