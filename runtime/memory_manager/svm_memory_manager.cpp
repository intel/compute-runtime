/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/memory_manager/svm_memory_manager.h"

#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/helpers/aligned_memory.h"
#include "runtime/mem_obj/mem_obj_helper.h"
#include "runtime/memory_manager/memory_manager.h"

namespace NEO {

void SVMAllocsManager::MapBasedAllocationTracker::insert(GraphicsAllocation &ga) {
    allocs.insert(std::make_pair(ga.getUnderlyingBuffer(), &ga));
}

void SVMAllocsManager::MapBasedAllocationTracker::remove(GraphicsAllocation &ga) {
    std::map<const void *, GraphicsAllocation *>::iterator iter;
    iter = allocs.find(ga.getUnderlyingBuffer());
    allocs.erase(iter);
}

GraphicsAllocation *SVMAllocsManager::MapBasedAllocationTracker::get(const void *ptr) {
    std::map<const void *, GraphicsAllocation *>::iterator Iter, End;
    GraphicsAllocation *GA;
    if (ptr == nullptr)
        return nullptr;
    End = allocs.end();
    Iter = allocs.lower_bound(ptr);
    if (((Iter != End) && (Iter->first != ptr)) ||
        (Iter == End)) {
        if (Iter == allocs.begin()) {
            Iter = End;
        } else {
            Iter--;
        }
    }
    if (Iter != End) {
        GA = Iter->second;
        if (ptr < ((char *)GA->getUnderlyingBuffer() + GA->getUnderlyingBufferSize())) {
            return GA;
        }
    }
    return nullptr;
}

SVMAllocsManager::SVMAllocsManager(MemoryManager *memoryManager) : memoryManager(memoryManager) {
}

void *SVMAllocsManager::createSVMAlloc(size_t size, cl_mem_flags flags) {
    if (size == 0)
        return nullptr;

    std::unique_lock<std::mutex> lock(mtx);
    AllocationProperties properties{true, size, GraphicsAllocation::AllocationType::SVM};
    MemObjHelper::fillCachePolicyInProperties(properties, flags);
    GraphicsAllocation *allocation = memoryManager->allocateGraphicsMemoryWithProperties(properties);
    if (!allocation) {
        return nullptr;
    }
    allocation->setMemObjectsAllocationWithWritableFlags(!SVMAllocsManager::memFlagIsReadOnly(flags));
    allocation->setCoherent(isValueSet(flags, CL_MEM_SVM_FINE_GRAIN_BUFFER));
    this->SVMAllocs.insert(*allocation);

    return allocation->getUnderlyingBuffer();
}

GraphicsAllocation *SVMAllocsManager::getSVMAlloc(const void *ptr) {
    std::unique_lock<std::mutex> lock(mtx);
    return SVMAllocs.get(ptr);
}

void SVMAllocsManager::freeSVMAlloc(void *ptr) {
    GraphicsAllocation *GA = getSVMAlloc(ptr);
    if (GA) {
        std::unique_lock<std::mutex> lock(mtx);
        SVMAllocs.remove(*GA);
        memoryManager->freeGraphicsMemory(GA);
    }
}

bool SVMAllocsManager::memFlagIsReadOnly(cl_svm_mem_flags flags) {
    return (flags & (CL_MEM_READ_ONLY | CL_MEM_HOST_READ_ONLY | CL_MEM_HOST_NO_ACCESS)) != 0;
}
} // namespace NEO
