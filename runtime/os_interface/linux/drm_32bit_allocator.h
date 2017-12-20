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

#pragma once
#include "runtime/os_interface/32bit_memory.h"
#include "runtime/helpers/aligned_memory.h"
#include "runtime/helpers/ptr_math.h"
#include <sys/mman.h>

namespace OCLRT {

class Allocator32bit::OsInternals::Drm32BitAllocator {
  protected:
    Allocator32bit::OsInternals &outer;

  public:
    Drm32BitAllocator(Allocator32bit::OsInternals &outer) : outer(outer) {
    }

    void *allocate(size_t size) {
        auto ptr = outer.mmapFunction(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);

        // In case we failed, retry with address provided as a hint
        if (ptr == MAP_FAILED) {
            ptr = outer.mmapFunction((void *)outer.upperRangeAddress, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            if (((uintptr_t)ptr + alignUp(size, 4096)) >= max32BitAddress || ptr == MAP_FAILED) {
                outer.munmapFunction(ptr, size);

                // Try to use lower range
                ptr = outer.mmapFunction((void *)outer.lowerRangeAddress, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
                if ((uintptr_t)ptr >= max32BitAddress) {
                    outer.munmapFunction(ptr, size);
                    return nullptr;
                }

                outer.lowerRangeAddress = (uintptr_t)ptr + alignUp(size, 4096);
                return ptr;
            }

            outer.upperRangeAddress = (uintptr_t)ptr + alignUp(size, 4096);
        }
        return ptr;
    }

    int free(void *ptr, uint64_t size) {
        if ((ptr == MAP_FAILED) || (ptr == nullptr))
            return 0;

        auto alignedSize = alignUp(size, 4096);
        auto offsetedPtr = (uintptr_t)ptrOffset(ptr, alignedSize);

        if (offsetedPtr == outer.upperRangeAddress) {
            outer.upperRangeAddress -= alignedSize;
        } else if (offsetedPtr == outer.lowerRangeAddress) {
            outer.lowerRangeAddress -= alignedSize;
        }
        return outer.munmapFunction(ptr, size);
    }

    ~Drm32BitAllocator() = default;
}
