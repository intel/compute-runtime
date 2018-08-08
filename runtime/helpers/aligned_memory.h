/*
 * Copyright (c) 2017 - 2018, Intel Corporation
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
#include "runtime/helpers/debug_helpers.h"
#include "runtime/os_interface/debug_settings_manager.h"
#include "runtime/memory_manager/memory_constants.h"
#include <new>
#include <cstdint>
#include <cstddef>
#include <algorithm>
#include <memory>
#include <functional>

#ifdef _MSC_VER
#define ALIGNAS(x) __declspec(align(x))
#else
#define ALIGNAS(x) alignas(x)
#endif

template <typename T>
constexpr inline T alignUp(T before, size_t alignment) {
    return static_cast<T>((static_cast<size_t>(before) + alignment - 1) & ~(alignment - 1));
}

template <typename T>
constexpr inline T *alignUp(T *ptrBefore, size_t alignment) {
    return reinterpret_cast<T *>(alignUp(reinterpret_cast<uintptr_t>(ptrBefore), alignment));
}

template <typename T>
constexpr inline T alignDown(T before, size_t alignment) {
    return static_cast<T>(static_cast<size_t>(before) & ~(alignment - 1));
}

template <typename T>
constexpr inline T *alignDown(T *ptrBefore, size_t alignment) {
    return reinterpret_cast<T *>(alignDown(reinterpret_cast<uintptr_t>(ptrBefore), alignment));
}

inline void *alignedMalloc(size_t bytes, size_t alignment) {
    DEBUG_BREAK_IF(alignment <= 0);

    if (bytes == 0) {
        bytes = sizeof(void *);
    }

    // Make sure our alignment is at least the size of a pointer
    alignment = std::max(alignment, sizeof(void *));

    // Allocate _bytes + _alignment
    size_t sizeToAlloc = bytes + alignment;
    auto pOriginalMemory = new (std::nothrow) char[sizeToAlloc];

    // Add in the alignment
    auto pAlignedMemory = reinterpret_cast<uintptr_t>(pOriginalMemory);
    if (pAlignedMemory) {
        pAlignedMemory += alignment;
        pAlignedMemory -= pAlignedMemory % alignment;

        // Store the original pointer to facilitate deallocation
        reinterpret_cast<void **>(pAlignedMemory)[-1] = pOriginalMemory;
    }

    DBG_LOG(LogAlignedAllocations, __FUNCTION__, "Pointer:", reinterpret_cast<void *>(pOriginalMemory), "size:", sizeToAlloc);
    // Return result
    return reinterpret_cast<void *>(pAlignedMemory);
}

inline void alignedFree(void *ptr) {
    if (ptr) {
        auto originalPtr = reinterpret_cast<char **>(ptr)[-1];
        DBG_LOG(LogAlignedAllocations, __FUNCTION__, "Pointer:", reinterpret_cast<void *>(originalPtr));
        delete[] originalPtr;
    }
}

inline size_t alignSizeWholePage(const void *ptr, size_t size) {
    uintptr_t startPageMisalignedAddressOffset = reinterpret_cast<uintptr_t>(ptr) & MemoryConstants::pageMask;
    size_t alignedSizeToPage = alignUp(startPageMisalignedAddressOffset + size, MemoryConstants::pageSize);
    return alignedSizeToPage;
}

template <size_t alignment, typename T>
inline constexpr bool isAligned(T val) {
    return (static_cast<size_t>(val) % alignment) == 0;
}

template <size_t alignment, typename T>
inline bool isAligned(T *ptr) {
    return ((reinterpret_cast<uintptr_t>(ptr)) % alignment) == 0;
}

template <typename T>
inline bool isAligned(T *ptr) {
    return (reinterpret_cast<uintptr_t>(ptr) & (alignof(T) - 1)) == 0;
}
inline auto allocateAlignedMemory(size_t bytes, size_t alignment) {
    return std::unique_ptr<void, std::function<decltype(alignedFree)>>(alignedMalloc(bytes, alignment), alignedFree);
}
