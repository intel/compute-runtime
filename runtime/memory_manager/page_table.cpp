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
#include "runtime/memory_manager/page_table.inl"

namespace OCLRT {

const uint32_t PTE::initialPage = 1;
std::atomic<uint32_t> PTE::nextPage(PTE::initialPage);

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

template class PageTable<class PDP, 3, 9>;
template class PageTable<class PDE, 2, 2>;
} // namespace OCLRT
