/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/common_types.h"
#include "shared/source/memory_manager/multi_graphics_allocation.h"
#include "shared/source/memory_manager/residency_container.h"
#include "shared/source/unified_memory/unified_memory.h"
#include "shared/source/utilities/spinlock.h"

#include "memory_properties_flags.h"

#include <cstdint>
#include <map>
#include <mutex>

namespace NEO {
class CommandStreamReceiver;
class GraphicsAllocation;
class MemoryManager;

struct SvmAllocationData {
    SvmAllocationData(uint32_t maxRootDeviceIndex) : gpuAllocations(maxRootDeviceIndex), maxRootDeviceIndex(maxRootDeviceIndex){};
    SvmAllocationData(const SvmAllocationData &svmAllocData) : SvmAllocationData(svmAllocData.maxRootDeviceIndex) {
        this->allocationFlagsProperty = svmAllocData.allocationFlagsProperty;
        this->cpuAllocation = svmAllocData.cpuAllocation;
        this->device = svmAllocData.device;
        this->size = svmAllocData.size;
        this->memoryType = svmAllocData.memoryType;
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
    InternalMemoryType memoryType = InternalMemoryType::SVM;
    MemoryProperties allocationFlagsProperty;
    void *device = nullptr;

  protected:
    const uint32_t maxRootDeviceIndex;
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

      protected:
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

    struct UnifiedMemoryProperties {
        UnifiedMemoryProperties(InternalMemoryType memoryType, DeviceBitfield subdeviceBitfield) : memoryType(memoryType), subdeviceBitfield(subdeviceBitfield){};
        InternalMemoryType memoryType = InternalMemoryType::NOT_SPECIFIED;
        MemoryProperties allocationFlags;
        void *device = nullptr;
        DeviceBitfield subdeviceBitfield;
    };

    SVMAllocsManager(MemoryManager *memoryManager);
    MOCKABLE_VIRTUAL ~SVMAllocsManager() = default;
    void *createSVMAlloc(uint32_t rootDeviceIndex,
                         size_t size,
                         const SvmAllocationProperties svmProperties,
                         const DeviceBitfield &deviceBitfield);
    void *createHostUnifiedMemoryAllocation(uint32_t maxRootDeviceIndex,
                                            size_t size,
                                            const UnifiedMemoryProperties &svmProperties);
    void *createUnifiedMemoryAllocation(uint32_t rootDeviceIndex,
                                        size_t size,
                                        const UnifiedMemoryProperties &svmProperties);
    void *createSharedUnifiedMemoryAllocation(uint32_t rootDeviceIndex,
                                              size_t size,
                                              const UnifiedMemoryProperties &svmProperties,
                                              void *cmdQ);
    void *createUnifiedKmdMigratedAllocation(uint32_t rootDeviceIndex,
                                             size_t size,
                                             const SvmAllocationProperties &svmProperties,
                                             const UnifiedMemoryProperties &unifiedMemoryProperties);
    void setUnifiedAllocationProperties(GraphicsAllocation *allocation, const SvmAllocationProperties &svmProperties);
    SvmAllocationData *getSVMAlloc(const void *ptr);
    bool freeSVMAlloc(void *ptr, bool blocking);
    bool freeSVMAlloc(void *ptr) { return freeSVMAlloc(ptr, false); }
    void insertSVMAlloc(const SvmAllocationData &svmData);
    void removeSVMAlloc(const SvmAllocationData &svmData);
    size_t getNumAllocs() const { return SVMAllocs.getNumAllocs(); }
    MapBasedAllocationTracker *getSVMAllocs() { return &SVMAllocs; }

    MOCKABLE_VIRTUAL void insertSvmMapOperation(void *regionSvmPtr, size_t regionSize, void *baseSvmPtr, size_t offset, bool readOnlyMap);
    void removeSvmMapOperation(const void *regionSvmPtr);
    SvmMapOperation *getSvmMapOperation(const void *regionPtr);
    void addInternalAllocationsToResidencyContainer(uint32_t rootDeviceIndex,
                                                    ResidencyContainer &residencyContainer,
                                                    uint32_t requestedTypesMask);
    void makeInternalAllocationsResident(CommandStreamReceiver &commandStreamReceiver, uint32_t requestedTypesMask);
    void *createUnifiedAllocationWithDeviceStorage(uint32_t rootDeviceIndex, size_t size, const SvmAllocationProperties &svmProperties, const UnifiedMemoryProperties &unifiedMemoryProperties);
    void freeSvmAllocationWithDeviceStorage(SvmAllocationData *svmData);

  protected:
    void *createZeroCopySvmAllocation(uint32_t rootDeviceIndex, size_t size, const SvmAllocationProperties &svmProperties, const DeviceBitfield &deviceBitfield);

    void freeZeroCopySvmAllocation(SvmAllocationData *svmData);

    MapBasedAllocationTracker SVMAllocs;
    MapOperationsTracker svmMapOperations;
    MemoryManager *memoryManager;
    SpinLock mtx;
};
} // namespace NEO
