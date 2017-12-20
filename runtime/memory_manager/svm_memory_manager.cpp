/*
 * Copyright (c) 2017, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "runtime/memory_manager/memory_manager.h"
#include "runtime/memory_manager/svm_memory_manager.h"
#include "runtime/helpers/aligned_memory.h"
#include "runtime/command_stream/command_stream_receiver.h"

namespace OCLRT {

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

void *SVMAllocsManager::createSVMAlloc(size_t size, bool coherent) {
    if (size == 0)
        return nullptr;

    std::unique_lock<std::mutex> lock(mtx);
    GraphicsAllocation *GA = memoryManager->allocateGraphicsMemoryForSVM(size, 4096, coherent);
    if (!GA) {
        return nullptr;
    }
    this->SVMAllocs.insert(*GA);

    return GA->getUnderlyingBuffer();
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
} // namespace OCLRT
