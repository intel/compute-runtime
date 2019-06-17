/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/unified_memory/unified_memory.h"
#include "runtime/utilities/spinlock.h"

#include <cstdint>
#include <map>
#include <mutex>

namespace NEO {
class CommandStreamReceiver;
class Device;
class GraphicsAllocation;
class MemoryManager;

struct SvmAllocationData {
    GraphicsAllocation *cpuAllocation = nullptr;
    GraphicsAllocation *gpuAllocation = nullptr;
    size_t size = 0;
    InternalMemoryType memoryType = InternalMemoryType::SVM;
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
        UnifiedMemoryProperties() = default;
        UnifiedMemoryProperties(InternalMemoryType memoryType) : memoryType(memoryType){};
        InternalMemoryType memoryType = InternalMemoryType::NOT_SPECIFIED;
    };

    SVMAllocsManager(MemoryManager *memoryManager);
    void *createSVMAlloc(size_t size, const SvmAllocationProperties svmProperties);
    void *createUnifiedMemoryAllocation(size_t size, const UnifiedMemoryProperties svmProperties);
    SvmAllocationData *getSVMAlloc(const void *ptr);
    void freeSVMAlloc(void *ptr);
    size_t getNumAllocs() const { return SVMAllocs.getNumAllocs(); }

    void insertSvmMapOperation(void *regionSvmPtr, size_t regionSize, void *baseSvmPtr, size_t offset, bool readOnlyMap);
    void removeSvmMapOperation(const void *regionSvmPtr);
    SvmMapOperation *getSvmMapOperation(const void *regionPtr);
    void makeInternalAllocationsResident(CommandStreamReceiver &commandStreamReceiver, uint32_t requestedTypesMask);

  protected:
    void *createZeroCopySvmAllocation(size_t size, const SvmAllocationProperties &svmProperties);
    void *createSvmAllocationWithDeviceStorage(size_t size, const SvmAllocationProperties &svmProperties);

    void freeZeroCopySvmAllocation(SvmAllocationData *svmData);
    void freeSvmAllocationWithDeviceStorage(SvmAllocationData *svmData);

    MapBasedAllocationTracker SVMAllocs;
    MapOperationsTracker svmMapOperations;
    MemoryManager *memoryManager;
    SpinLock mtx;
};
} // namespace NEO
