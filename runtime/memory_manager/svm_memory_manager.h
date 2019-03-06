/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "CL/cl.h"

#include <cstdint>
#include <map>
#include <mutex>

namespace NEO {
class Device;
class GraphicsAllocation;
class CommandStreamReceiver;
class MemoryManager;

struct SvmAllocationData {
    GraphicsAllocation *cpuAllocation = nullptr;
    GraphicsAllocation *gpuAllocation = nullptr;
    size_t size = 0;
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

    SVMAllocsManager(MemoryManager *memoryManager);
    void *createSVMAlloc(size_t size, cl_mem_flags flags);
    SvmAllocationData *getSVMAlloc(const void *ptr);
    void freeSVMAlloc(void *ptr);
    size_t getNumAllocs() const { return SVMAllocs.getNumAllocs(); }

    static bool memFlagIsReadOnly(cl_svm_mem_flags flags) {
        return (flags & (CL_MEM_READ_ONLY | CL_MEM_HOST_READ_ONLY | CL_MEM_HOST_NO_ACCESS)) != 0;
    }

    static bool mapFlagIsReadOnly(cl_map_flags mapFlags) {
        return !((mapFlags & (CL_MAP_WRITE | CL_MAP_WRITE_INVALIDATE_REGION)) != 0);
    }

    void insertSvmMapOperation(void *regionSvmPtr, size_t regionSize, void *baseSvmPtr, size_t offset, bool readOnlyMap);
    void removeSvmMapOperation(const void *regionSvmPtr);
    SvmMapOperation *getSvmMapOperation(const void *regionPtr);

  protected:
    void *createZeroCopySvmAllocation(size_t size, cl_mem_flags flags);
    void *createSvmAllocationWithDeviceStorage(size_t size, cl_mem_flags flags);
    void freeZeroCopySvmAllocation(SvmAllocationData *svmData);
    void freeSvmAllocationWithDeviceStorage(SvmAllocationData *svmData);

    MapBasedAllocationTracker SVMAllocs;
    MapOperationsTracker svmMapOperations;
    MemoryManager *memoryManager;
    std::mutex mtx;
};
} // namespace NEO
