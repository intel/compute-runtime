/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/common_types.h"
#include "shared/source/memory_manager/multi_graphics_allocation.h"
#include "shared/source/memory_manager/residency_container.h"
#include "shared/source/unified_memory/unified_memory.h"

#include "memory_properties_flags.h"

#include <atomic>
#include <cstdint>
#include <map>
#include <mutex>
#include <set>
#include <shared_mutex>

namespace NEO {
class CommandStreamReceiver;
class GraphicsAllocation;
class MemoryManager;
class Device;

struct SvmAllocationData {
    SvmAllocationData(uint32_t maxRootDeviceIndex) : gpuAllocations(maxRootDeviceIndex), maxRootDeviceIndex(maxRootDeviceIndex){};
    SvmAllocationData(const SvmAllocationData &svmAllocData) : SvmAllocationData(svmAllocData.maxRootDeviceIndex) {
        this->allocationFlagsProperty = svmAllocData.allocationFlagsProperty;
        this->cpuAllocation = svmAllocData.cpuAllocation;
        this->device = svmAllocData.device;
        this->size = svmAllocData.size;
        this->memoryType = svmAllocData.memoryType;
        this->allocId = svmAllocData.allocId;
        this->pageSizeForAlignment = svmAllocData.pageSizeForAlignment;
        this->isImportedAllocation = svmAllocData.isImportedAllocation;
        for (auto allocation : svmAllocData.gpuAllocations.getGraphicsAllocations()) {
            if (allocation) {
                this->gpuAllocations.addAllocation(allocation);
            }
        }
    }
    SvmAllocationData &operator=(const SvmAllocationData &) = delete;
    GraphicsAllocation *cpuAllocation = nullptr;
    MultiGraphicsAllocation gpuAllocations;
    size_t size = 0;
    size_t pageSizeForAlignment = 0;
    InternalMemoryType memoryType = InternalMemoryType::SVM;
    MemoryProperties allocationFlagsProperty;
    Device *device = nullptr;
    bool isImportedAllocation = false;
    void setAllocId(uint32_t id) {
        allocId = id;
    }

    uint32_t getAllocId() {
        return allocId;
    }

    static constexpr uint32_t uninitializedAllocId = std::numeric_limits<uint32_t>::max();

  protected:
    const uint32_t maxRootDeviceIndex;
    uint32_t allocId = uninitializedAllocId;
};

struct SvmMapOperation {
    void *regionSvmPtr = nullptr;
    size_t regionSize = 0;
    void *baseSvmPtr = nullptr;
    size_t offset = 0;
    bool readOnlyMap = false;
};

class SVMAllocsManager {
  public:
    class MapBasedAllocationTracker {
        friend class SVMAllocsManager;

      public:
        using SvmAllocationContainer = std::map<const void *, SvmAllocationData>;
        void insert(SvmAllocationData);
        void remove(SvmAllocationData);
        SvmAllocationData *get(const void *);
        size_t getNumAllocs() const { return allocations.size(); };

        SvmAllocationContainer allocations;
    };

    struct MapOperationsTracker {
        using SvmMapOperationsContainer = std::map<const void *, SvmMapOperation>;
        void insert(SvmMapOperation);
        void remove(const void *);
        SvmMapOperation *get(const void *);
        size_t getNumMapOperations() const { return operations.size(); };

      protected:
        SvmMapOperationsContainer operations;
    };

    struct SvmAllocationProperties {
        bool coherent = false;
        bool hostPtrReadOnly = false;
        bool readOnly = false;
    };

    struct InternalAllocationsTracker {
        uint32_t latestSentTaskCount = 0lu;
        uint32_t latestResidentObjectId = 0lu;
    };

    struct UnifiedMemoryProperties {
        UnifiedMemoryProperties(InternalMemoryType memoryType,
                                const RootDeviceIndicesContainer &rootDeviceIndices,
                                const std::map<uint32_t, DeviceBitfield> &subdeviceBitfields) : memoryType(memoryType),
                                                                                                rootDeviceIndices(rootDeviceIndices),
                                                                                                subdeviceBitfields(subdeviceBitfields){};
        InternalMemoryType memoryType = InternalMemoryType::NOT_SPECIFIED;
        MemoryProperties allocationFlags;
        Device *device = nullptr;
        const RootDeviceIndicesContainer &rootDeviceIndices;
        const std::map<uint32_t, DeviceBitfield> &subdeviceBitfields;
    };

    struct SvmCacheAllocationInfo {
        size_t allocationSize;
        void *allocation;
        SvmCacheAllocationInfo(size_t allocationSize, void *allocation) : allocationSize(allocationSize), allocation(allocation) {}
        bool operator<(SvmCacheAllocationInfo const &other) {
            return allocationSize < other.allocationSize;
        }
        bool operator<(size_t const &size) {
            return allocationSize < size;
        }
    };

    struct SvmAllocationCache {
        void insert(size_t size, void *);
        void *get(size_t size, const UnifiedMemoryProperties &unifiedMemoryProperties, SVMAllocsManager *svmAllocsManager);
        void trim(SVMAllocsManager *svmAllocsManager);
        std::vector<SvmCacheAllocationInfo> allocations;
        std::mutex mtx;
    };

    SVMAllocsManager(MemoryManager *memoryManager, bool multiOsContextSupport);
    MOCKABLE_VIRTUAL ~SVMAllocsManager();
    void *createSVMAlloc(size_t size,
                         const SvmAllocationProperties svmProperties,
                         const RootDeviceIndicesContainer &rootDeviceIndices,
                         const std::map<uint32_t, DeviceBitfield> &subdeviceBitfields);
    MOCKABLE_VIRTUAL void *createHostUnifiedMemoryAllocation(size_t size,
                                                             const UnifiedMemoryProperties &svmProperties);
    MOCKABLE_VIRTUAL void *createUnifiedMemoryAllocation(size_t size,
                                                         const UnifiedMemoryProperties &svmProperties);
    MOCKABLE_VIRTUAL void *createSharedUnifiedMemoryAllocation(size_t size,
                                                               const UnifiedMemoryProperties &svmProperties,
                                                               void *cmdQ);
    void *createUnifiedKmdMigratedAllocation(size_t size,
                                             const SvmAllocationProperties &svmProperties,
                                             const UnifiedMemoryProperties &unifiedMemoryProperties);
    void setUnifiedAllocationProperties(GraphicsAllocation *allocation, const SvmAllocationProperties &svmProperties);
    SvmAllocationData *getSVMAlloc(const void *ptr);
    MOCKABLE_VIRTUAL bool freeSVMAlloc(void *ptr, bool blocking);
    MOCKABLE_VIRTUAL void freeSVMAllocImpl(void *ptr, bool blocking, SvmAllocationData *svmData);
    bool freeSVMAlloc(void *ptr) { return freeSVMAlloc(ptr, false); }
    void trimUSMDeviceAllocCache();
    void insertSVMAlloc(const SvmAllocationData &svmData);
    void removeSVMAlloc(const SvmAllocationData &svmData);
    size_t getNumAllocs() const { return SVMAllocs.getNumAllocs(); }
    MapBasedAllocationTracker *getSVMAllocs() { return &SVMAllocs; }

    MOCKABLE_VIRTUAL void insertSvmMapOperation(void *regionSvmPtr, size_t regionSize, void *baseSvmPtr, size_t offset, bool readOnlyMap);
    void removeSvmMapOperation(const void *regionSvmPtr);
    SvmMapOperation *getSvmMapOperation(const void *regionPtr);
    MOCKABLE_VIRTUAL void addInternalAllocationsToResidencyContainer(uint32_t rootDeviceIndex,
                                                                     ResidencyContainer &residencyContainer,
                                                                     uint32_t requestedTypesMask);
    void makeInternalAllocationsResident(CommandStreamReceiver &commandStreamReceiver, uint32_t requestedTypesMask);
    void *createUnifiedAllocationWithDeviceStorage(size_t size, const SvmAllocationProperties &svmProperties, const UnifiedMemoryProperties &unifiedMemoryProperties);
    void freeSvmAllocationWithDeviceStorage(SvmAllocationData *svmData);
    bool hasHostAllocations();
    std::atomic<uint32_t> allocationsCounter = 0;
    MOCKABLE_VIRTUAL void makeIndirectAllocationsResident(CommandStreamReceiver &commandStreamReceiver, uint32_t taskCount);
    void prepareIndirectAllocationForDestruction(SvmAllocationData *);
    void prefetchMemory(Device &device, SvmAllocationData &svmData);
    std::unique_lock<std::mutex> obtainOwnership();

    std::map<CommandStreamReceiver *, InternalAllocationsTracker> indirectAllocationsResidency;

    using NonGpuDomainAllocsContainer = std::vector<void *>;
    NonGpuDomainAllocsContainer nonGpuDomainAllocs;

  protected:
    void *createZeroCopySvmAllocation(size_t size, const SvmAllocationProperties &svmProperties,
                                      const RootDeviceIndicesContainer &rootDeviceIndices,
                                      const std::map<uint32_t, DeviceBitfield> &subdeviceBitfields);
    AllocationType getGraphicsAllocationTypeAndCompressionPreference(const UnifiedMemoryProperties &unifiedMemoryProperties, bool &compressionEnabled) const;

    void freeZeroCopySvmAllocation(SvmAllocationData *svmData);

    void initUsmDeviceAllocationsCache();

    MapBasedAllocationTracker SVMAllocs;
    MapOperationsTracker svmMapOperations;
    MemoryManager *memoryManager;
    std::shared_mutex mtx;
    std::mutex mtxForIndirectAccess;
    bool multiOsContextSupport;
    SvmAllocationCache usmDeviceAllocationsCache;
    bool usmDeviceAllocationsCacheEnabled = false;
};
} // namespace NEO
