/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/constants.h"

#include <cstdint>
#include <mutex>
#include <vector>

namespace NEO {

struct HeapChunk {
    HeapChunk(uint64_t ptr, size_t size) : ptr(ptr), size(size) {}
    uint64_t ptr;
    size_t size;
};

bool operator<(const HeapChunk &hc1, const HeapChunk &hc2);

class HeapAllocator {
  public:
    HeapAllocator(uint64_t address, uint64_t size) : HeapAllocator(address, size, MemoryConstants::pageSize) {
    }

    HeapAllocator(uint64_t address, uint64_t size, size_t allocationAlignment) : HeapAllocator(address, size, allocationAlignment, 4 * MemoryConstants::megaByte) {
    }

    HeapAllocator(uint64_t address, uint64_t size, size_t allocationAlignment, size_t threshold) : baseAddress(address), size(size), availableSize(size), allocationAlignment(allocationAlignment), sizeThreshold(threshold) {
        pLeftBound = address;
        pRightBound = address + size;
        freedChunksBig.reserve(10);
        freedChunksSmall.reserve(50);
    }

    MOCKABLE_VIRTUAL ~HeapAllocator() = default;

    uint64_t allocate(size_t &sizeToAllocate) {
        return allocateWithCustomAlignment(sizeToAllocate, 0u);
    }

    uint64_t allocateWithCustomAlignment(size_t &sizeToAllocate, size_t alignment);

    MOCKABLE_VIRTUAL void free(uint64_t ptr, size_t size);

    uint64_t getLeftSize() const {
        return availableSize;
    }

    uint64_t getUsedSize() const {
        return size - availableSize;
    }

    double getUsage() const;

    uint64_t getBaseAddress() const {
        return this->baseAddress;
    }

  protected:
    const uint64_t baseAddress;
    const uint64_t size;
    uint64_t availableSize;
    uint64_t pLeftBound;
    uint64_t pRightBound;
    size_t allocationAlignment;
    const size_t sizeThreshold;

    std::vector<HeapChunk> freedChunksSmall;
    std::vector<HeapChunk> freedChunksBig;
    std::mutex mtx;

    uint64_t getFromFreedChunks(size_t size, std::vector<HeapChunk> &freedChunks, size_t &sizeOfFreedChunk, size_t requiredAlignment);

    void storeInFreedChunks(uint64_t ptr, size_t size, std::vector<HeapChunk> &freedChunks) {
        for (auto &freedChunk : freedChunks) {
            if (freedChunk.ptr == ptr + size) {
                freedChunk.ptr = ptr;
                freedChunk.size += size;
                return;
            }
            if (freedChunk.ptr + freedChunk.size == ptr) {
                freedChunk.size += size;
                return;
            }
        }

        freedChunks.emplace_back(ptr, size);
    }

    void mergeLastFreedSmall() {
        size_t maxSizeOfSmallChunks = freedChunksSmall.size();

        if (maxSizeOfSmallChunks > 0) {
            auto ptr = freedChunksSmall[maxSizeOfSmallChunks - 1].ptr;
            size_t chunkSize = freedChunksSmall[maxSizeOfSmallChunks - 1].size;
            if (ptr == pRightBound) {
                pRightBound = ptr + chunkSize;
                freedChunksSmall.pop_back();
            }
        }
    }

    void mergeLastFreedBig() {
        size_t maxSizeOfBigChunks = freedChunksBig.size();

        if (maxSizeOfBigChunks > 0) {
            auto ptr = freedChunksBig[maxSizeOfBigChunks - 1].ptr;
            size_t chunkSize = freedChunksBig[maxSizeOfBigChunks - 1].size;
            if (ptr == pLeftBound - chunkSize) {
                pLeftBound = ptr;
                freedChunksBig.pop_back();
            }
        }
    }

    void defragment();
};
} // namespace NEO
