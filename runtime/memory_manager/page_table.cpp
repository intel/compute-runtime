/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/memory_manager/page_table.h"

#include "runtime/aub_mem_dump/page_table_entry_bits.h"
#include "runtime/memory_manager/page_table.inl"

namespace NEO {

uintptr_t PTE::map(uintptr_t vm, size_t size, uint64_t entryBits, uint32_t memoryBank) {
    const size_t shift = 12;
    const auto mask = static_cast<uint32_t>(maxNBitValue(bits));
    size_t indexStart = (vm >> shift) & mask;
    size_t indexEnd = ((vm + size - 1) >> shift) & mask;
    uintptr_t res = -1;
    bool updateEntryBits = entryBits != PageTableEntry::nonValidBits;
    uint64_t newEntryBits = entryBits & MemoryConstants::pageMask;
    newEntryBits |= 0x1;

    for (size_t index = indexStart; index <= indexEnd; index++) {
        if (entries[index] == 0x0) {
            uint64_t tmp = allocator->reserve4kPage(memoryBank);
            entries[index] = reinterpret_cast<void *>(tmp | newEntryBits);
        } else if (updateEntryBits) {
            entries[index] = reinterpret_cast<void *>((reinterpret_cast<uintptr_t>(entries[index]) & MemoryConstants::page4kEntryMask) | newEntryBits);
        }
        res = std::min(reinterpret_cast<uintptr_t>(entries[index]) & MemoryConstants::page4kEntryMask, res);
    }
    return (res & ~newEntryBits) + (vm & (pageSize - 1));
}

void PTE::pageWalk(uintptr_t vm, size_t size, size_t offset, uint64_t entryBits, PageWalker &pageWalker, uint32_t memoryBank) {
    static const uint32_t bits = 9;
    const size_t shift = 12;
    const auto mask = static_cast<uint32_t>(maxNBitValue(bits));
    size_t indexStart = (vm >> shift) & mask;
    size_t indexEnd = ((vm + size - 1) >> shift) & mask;
    uint64_t res = -1;
    uintptr_t rem = vm & (pageSize - 1);
    bool updateEntryBits = entryBits != PageTableEntry::nonValidBits;
    uint64_t newEntryBits = entryBits & MemoryConstants::pageMask;
    newEntryBits |= 0x1;

    for (size_t index = indexStart; index <= indexEnd; index++) {
        if (entries[index] == 0x0) {
            uint64_t tmp = allocator->reserve4kPage(memoryBank);
            entries[index] = reinterpret_cast<void *>(tmp | newEntryBits);
        } else if (updateEntryBits) {
            entries[index] = reinterpret_cast<void *>((reinterpret_cast<uintptr_t>(entries[index]) & MemoryConstants::page4kEntryMask) | newEntryBits);
        }
        res = reinterpret_cast<uintptr_t>(entries[index]) & MemoryConstants::page4kEntryMask;

        size_t lSize = std::min(pageSize - rem, size);
        pageWalker((res & ~0x1) + rem, lSize, offset, reinterpret_cast<uintptr_t>(entries[index]) & MemoryConstants::pageMask);

        size -= lSize;
        offset += lSize;
        rem = 0;
    }
}

template class PageTable<class PDP, 3, 9>;
template class PageTable<class PDE, 2, 2>;
} // namespace NEO
