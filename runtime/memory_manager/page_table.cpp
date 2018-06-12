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

#include "runtime/memory_manager/page_table.h"

#include <inttypes.h>
#include <iostream>

namespace OCLRT {

const uint32_t PTE::initialPage = 1;
std::atomic<uint32_t> PTE::nextPage(PTE::initialPage);

template <>
uintptr_t PageTable<void, 0, 9>::map(uintptr_t vm, size_t size) {
    return 0;
}

template <>
size_t PageTable<void, 0, 9>::getBits() {
    return 9;
}

uintptr_t PTE::map(uintptr_t vm, size_t size) {
    const size_t shift = 12;
    const uint32_t mask = (1 << bits) - 1;
    size_t indexStart = (vm >> shift) & mask;
    size_t indexEnd = ((vm + size - 1) >> shift) & mask;
    uintptr_t res = -1;

    for (size_t index = indexStart; index <= indexEnd; index++) {
        if (entries[index] == 0x0) {
            uint64_t tmp = nextPage.fetch_add(1);
            entries[index] = reinterpret_cast<void *>(tmp * pageSize | 0x1);
        }
        res = std::min(reinterpret_cast<uintptr_t>(entries[index]) & 0xfffffffeu, res);
    }
    return (res & ~0x1) + (vm & (pageSize - 1));
}

template <class T, uint32_t level, uint32_t bits>
uintptr_t PageTable<T, level, bits>::map(uintptr_t vm, size_t size) {
    const size_t shift = T::getBits() + 12;
    const uintptr_t mask = (1 << bits) - 1;
    size_t indexStart = (vm >> shift) & mask;
    size_t indexEnd = ((vm + size - 1) >> shift) & mask;
    uintptr_t res = -1;
    uintptr_t vmMask = (uintptr_t(-1) >> (sizeof(void *) * 8 - shift - bits));
    auto maskedVm = vm & vmMask;

    for (size_t index = indexStart; index <= indexEnd; index++) {
        uintptr_t vmStart = (uintptr_t(1) << shift) * index;
        vmStart = std::max(vmStart, maskedVm);
        uintptr_t vmEnd = (uintptr_t(1) << shift) * (index + 1) - 1;
        vmEnd = std::min(vmEnd, maskedVm + size - 1);

        if (entries[index] == nullptr) {
            entries[index] = new T;
        }
        res = std::min((entries[index])->map(vmStart, vmEnd - vmStart + 1), res);
    }
    return res;
}

void PTE::pageWalk(uintptr_t vm, size_t size, size_t offset, PageWalker &pageWalker) {
    static const uint32_t bits = 9;
    const size_t shift = 12;
    const uint32_t mask = (1 << bits) - 1;
    size_t indexStart = (vm >> shift) & mask;
    size_t indexEnd = ((vm + size - 1) >> shift) & mask;
    uint64_t res = -1;
    uintptr_t rem = vm & (pageSize - 1);

    for (size_t index = indexStart; index <= indexEnd; index++) {
        if (entries[index] == 0x0) {
            uint64_t tmp = nextPage.fetch_add(1);
            entries[index] = reinterpret_cast<void *>(tmp * pageSize | 0x1);
        }
        res = reinterpret_cast<uintptr_t>(entries[index]) & 0xfffffffeu;

        size_t lSize = std::min(pageSize - rem, size);
        pageWalker((res & ~0x1) + rem, lSize, offset);

        size -= lSize;
        offset += lSize;
        rem = 0;
    }
}

template <>
void PageTable<void, 0, 9>::pageWalk(uintptr_t vm, size_t size, size_t offset, PageWalker &pageWalker) {
}

template <class T, uint32_t level, uint32_t bits>
void PageTable<T, level, bits>::pageWalk(uintptr_t vm, size_t size, size_t offset, PageWalker &pageWalker) {
    const size_t shift = T::getBits() + 12;
    const uintptr_t mask = (1 << bits) - 1;
    size_t indexStart = (vm >> shift) & mask;
    size_t indexEnd = ((vm + size - 1) >> shift) & mask;
    uintptr_t vmMask = (uintptr_t(-1) >> (sizeof(void *) * 8 - shift - bits));
    auto maskedVm = vm & vmMask;

    for (size_t index = indexStart; index <= indexEnd; index++) {
        uintptr_t vmStart = (uintptr_t(1) << shift) * index;
        vmStart = std::max(vmStart, maskedVm);
        uintptr_t vmEnd = (uintptr_t(1) << shift) * (index + 1) - 1;
        vmEnd = std::min(vmEnd, maskedVm + size - 1);

        if (entries[index] == nullptr) {
            entries[index] = new T;
        }
        entries[index]->pageWalk(vmStart, vmEnd - vmStart + 1, offset, pageWalker);

        offset += (vmEnd - vmStart + 1);
    }
}

template <>
PageTable<void, 0, 9>::~PageTable() {
}

template <class T, uint32_t level, uint32_t bits>
PageTable<T, level, bits>::~PageTable() {
    for (auto &e : entries)
        delete e;
}

template class PageTable<class PDP, 3, 9>;
template class PageTable<class PDE, 2, 2>;
} // namespace OCLRT
