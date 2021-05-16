/*
 * Copyright (C) 2019-2021 Intel Corporation
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
#include <set>

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
    Device *device = nullptr;

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
        UnifiedMemoryProperties(InternalMemoryType memoryType,
                                const std::set<uint32_t> &rootDeviceIndices,
                                const std::map<uint32_t, DeviceBitfield> &subdeviceBitfields) : memoryType(memoryType),
                                                                                                rootDeviceIndices(rootDeviceIndices),
                                                                                                subdeviceBitfields(subdeviceBitfields){};
        InternalMemoryType memoryType = InternalMemoryType::NOT_SPECIFIED;
        MemoryProperties allocationFlags;
        Device *device = nullptr;
        const std::set<uint32_t> &rootDeviceIndices;
        const std::map<uint32_t, DeviceBitfield> &subdeviceBitfields;
    };

    SVMAllocsManager(MemoryManager *memoryManager, bool multiOsContextSupport);
    MOCKABLE_VIRTUAL ~SVMAllocsManager() = default;
    void *createSVMAlloc(size_t size,
                         const SvmAllocationProperties svmProperties,
                         const std::set<uint32_t> &rootDeviceIndices,
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
    void *createUnifiedAllocationWithDeviceStorage(size_t size, const SvmAllocationProperties &svmProperties, const UnifiedMemoryProperties &unifiedMemoryProperties);
    void freeSvmAllocationWithDeviceStorage(SvmAllocationData *svmData);
    bool hasHostAllocations();

  protected:
    void *createZeroCopySvmAllocation(size_t size, const SvmAllocationProperties &svmProperties,
                                      const std::set<uint32_t> &rootDeviceIndices,
                                      const std::map<uint32_t, DeviceBitfield> &subdeviceBitfields);
    GraphicsAllocation::AllocationType getGraphicsAllocationType(const UnifiedMemoryProperties &unifiedMemoryProperties) const;

    void freeZeroCopySvmAllocation(SvmAllocationData *svmData);

    MapBasedAllocationTracker SVMAllocs;
    MapOperationsTracker svmMapOperations;
    MemoryManager *memoryManager;
    SpinLock mtx;
    bool multiOsContextSupport;
};
} // namespace NEO
