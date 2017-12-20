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

#include "runtime/os_interface/32bit_memory.h"
#include "runtime/helpers/aligned_memory.h"
using namespace OCLRT;

bool OCLRT::is32BitOsAllocatorAvailable = is64bit ? true : false;

class Allocator32bit::OsInternals {
  public:
    void *allocatedRange;
};

Allocator32bit::Allocator32bit(uint64_t base, uint64_t size) {
    this->base = base;
    this->size = size;
    heapAllocator = std::unique_ptr<HeapAllocator>(new HeapAllocator((void *)base, size));
}

OCLRT::Allocator32bit::Allocator32bit() {
    size_t sizeToMap = 100 * 4096;
    this->base = (uint64_t)alignedMalloc(sizeToMap, 4096);
    osInternals = std::unique_ptr<OsInternals>(new OsInternals);
    osInternals.get()->allocatedRange = (void *)((uintptr_t)this->base);

    heapAllocator = std::unique_ptr<HeapAllocator>(new HeapAllocator((void *)this->base, sizeToMap));
}

OCLRT::Allocator32bit::~Allocator32bit() {
    if (this->osInternals.get() != nullptr) {
        alignedFree(this->osInternals->allocatedRange);
    }
}

void *Allocator32bit::allocate(size_t &size) {
    if (size >= 0xfffff000)
        return nullptr;
    return this->heapAllocator->allocate(size);
}

int Allocator32bit::free(void *ptr, size_t size) {
    this->heapAllocator->free(ptr, size);
    return 0;
}

uintptr_t Allocator32bit::getBase() {
    return (uintptr_t)base;
}
