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
class UsmMemAllocPool {
  public:
    using UnifiedMemoryProperties = SVMAllocsManager::UnifiedMemoryProperties;
    struct AllocationInfo {
        uint64_t address;
        size_t size;
        size_t requestedSize;
    };
    using AllocationsInfoStorage = BaseSortedPointerWithValueVector<AllocationInfo>;

    UsmMemAllocPool() = default;
    virtual ~UsmMemAllocPool() = default;
    bool initialize(SVMAllocsManager *svmMemoryManager, const UnifiedMemoryProperties &memoryProperties, size_t poolSize, size_t minServicedSize, size_t maxServicedSize);
    bool initialize(SVMAllocsManager *svmMemoryManager, void *ptr, SvmAllocationData *svmData, size_t minServicedSize, size_t maxServicedSize);
    bool isInitialized() const;
    size_t getPoolSize() const;
    MOCKABLE_VIRTUAL void cleanup();
    static bool alignmentIsAllowed(size_t alignment);
    static bool flagsAreAllowed(const UnifiedMemoryProperties &memoryProperties);
    static double getPercentOfFreeMemoryForRecycling(InternalMemoryType memoryType);
    bool sizeIsAllowed(size_t size);
    bool canBePooled(size_t size, const UnifiedMemoryProperties &memoryProperties);
    void *createUnifiedMemoryAllocation(size_t size, const UnifiedMemoryProperties &memoryProperties);
    bool isInPool(const void *ptr) const;
    bool isEmpty();
    bool freeSVMAlloc(const void *ptr, bool blocking);
    size_t getPooledAllocationSize(const void *ptr);
    void *getPooledAllocationBasePtr(const void *ptr);
    size_t getOffsetInPool(const void *ptr) const;
    uint64_t getPoolAddress() const;

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
    Device *device;
    InternalMemoryType poolMemoryType;
    size_t poolSize{};
    size_t minServicedSize;
    size_t maxServicedSize;
};

class UsmMemAllocPoolsManager {
  public:
    struct PoolInfo {
        size_t minServicedSize;
        size_t maxServicedSize;
        size_t preallocateSize;
        bool isPreallocated() const;
        bool operator<(const PoolInfo &rhs) const {
            return this->minServicedSize < rhs.minServicedSize;
        }
    };
    // clang-format off
    const std::array<const PoolInfo, 6> poolInfos = {
        PoolInfo{ 0,          4 * KB,  2 * MB},
        PoolInfo{ 4 * KB+1,  64 * KB,  2 * MB},
        PoolInfo{64 * KB+1,   2 * MB, 16 * MB},
        PoolInfo{ 2 * MB+1,  16 * MB,  0},
        PoolInfo{16 * MB+1,  64 * MB,  0},
        PoolInfo{64 * MB+1, 256 * MB,  0}};
    // clang-format on
    static constexpr size_t firstNonPreallocatedIndex = 3u;

    using UnifiedMemoryProperties = SVMAllocsManager::UnifiedMemoryProperties;
    static constexpr uint64_t KB = MemoryConstants::kiloByte; // NOLINT(readability-identifier-naming)
    static constexpr uint64_t MB = MemoryConstants::megaByte; // NOLINT(readability-identifier-naming)
    static constexpr uint64_t maxPoolableSize = 256 * MB;
    UsmMemAllocPoolsManager(MemoryManager *memoryManager,
                            const RootDeviceIndicesContainer &rootDeviceIndices,
                            const std::map<uint32_t, NEO::DeviceBitfield> &deviceBitFields,
                            Device *device,
                            InternalMemoryType poolMemoryType) : memoryManager(memoryManager), rootDeviceIndices(rootDeviceIndices), deviceBitFields(deviceBitFields), device(device), poolMemoryType(poolMemoryType){};
    MOCKABLE_VIRTUAL ~UsmMemAllocPoolsManager() = default;
    bool ensureInitialized(SVMAllocsManager *svmMemoryManager);
    bool isInitialized() const;
    void trim();
    void trim(std::vector<std::unique_ptr<UsmMemAllocPool>> &poolVector);
    void cleanup();
    void *createUnifiedMemoryAllocation(size_t size, const UnifiedMemoryProperties &memoryProperties);
    bool freeSVMAlloc(const void *ptr, bool blocking);
    MOCKABLE_VIRTUAL uint64_t getFreeMemory();
    bool recycleSVMAlloc(void *ptr, bool blocking);
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
    bool belongsInPreallocatedPool(size_t size) {
        return size <= poolInfos[firstNonPreallocatedIndex - 1].maxServicedSize;
    }

    SVMAllocsManager *svmMemoryManager{};
    MemoryManager *memoryManager;
    RootDeviceIndicesContainer rootDeviceIndices;
    std::map<uint32_t, NEO::DeviceBitfield> deviceBitFields;
    Device *device;
    InternalMemoryType poolMemoryType;
    size_t totalSize{};
    std::mutex mtx;
    std::map<PoolInfo, std::vector<std::unique_ptr<UsmMemAllocPool>>> pools;
};

} // namespace NEO
