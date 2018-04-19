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
#include <cstdint>
#include <cstddef>

static const int ptrGarbageContent[16] = {
    0x0131, 0x133, 0xA, 0xEF,
    0x0131, 0x133, 0xA, 0xEF,
    0x0131, 0x133, 0xA, 0xEF,
    0x0131, 0x133, 0xA, 0xEF};
static const auto ptrGarbage = (void *)ptrGarbageContent;

template <typename T>
inline T ptrOffset(T ptrBefore, size_t offset) {
    auto addrBefore = (uintptr_t)ptrBefore;
    auto addrAfter = addrBefore + offset;
    return (T)addrAfter;
}

template <typename TA, typename TB>
inline size_t ptrDiff(TA ptrAfter, TB ptrBefore) {
    auto addrBefore = (uintptr_t)ptrBefore;
    auto addrAfter = (uintptr_t)ptrAfter;
    return addrAfter - addrBefore;
}

template <typename IntegerAddressType>
inline void *addrToPtr(IntegerAddressType addr) {
    uintptr_t correctBitnessAddress = static_cast<uintptr_t>(addr);
    void *ptrReturn = reinterpret_cast<void *>(correctBitnessAddress);
    return ptrReturn;
}

inline void patchWithRequiredSize(void *memoryToBePatched, uint32_t patchSize, uintptr_t patchValue) {
    if (patchSize == sizeof(uint64_t)) {
        uint64_t *curbeAddress = (uint64_t *)memoryToBePatched;
        *curbeAddress = patchValue;
    } else {
        uint32_t *curbeAddress = (uint32_t *)memoryToBePatched;
        *curbeAddress = (uint32_t)patchValue;
    }
}

inline uint64_t castToUint64(void *address) {
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(address));
}
