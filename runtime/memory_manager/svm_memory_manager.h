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

class SVMAllocsManager {
  public:
    class MapBasedAllocationTracker {
      public:
        void insert(GraphicsAllocation &);
        void remove(GraphicsAllocation &);
        GraphicsAllocation *get(const void *);
        size_t getNumAllocs() const { return allocs.size(); };

      protected:
        std::map<const void *, GraphicsAllocation *> allocs;
    };

    SVMAllocsManager(MemoryManager *memoryManager);
    void *createSVMAlloc(size_t size, cl_mem_flags flags);
    GraphicsAllocation *getSVMAlloc(const void *ptr);
    void freeSVMAlloc(void *ptr);
    size_t getNumAllocs() const { return SVMAllocs.getNumAllocs(); }
    static bool memFlagIsReadOnly(cl_svm_mem_flags flags);

  protected:
    MapBasedAllocationTracker SVMAllocs;
    MemoryManager *memoryManager;
    std::mutex mtx;
};
} // namespace NEO
