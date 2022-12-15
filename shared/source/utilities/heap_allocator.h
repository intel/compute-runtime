/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/utilities/logger.h"

#include <cstdint>
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

    HeapAllocator(uint64_t address, uint64_t size, size_t allocationAlignment, size_t threshold) : size(size), availableSize(size), allocationAlignment(allocationAlignment), sizeThreshold(threshold) {
        pLeftBound = address;
        pRightBound = address + size;
        freedChunksBig.reserve(10);
        freedChunksSmall.reserve(50);
    }

    uint64_t allocate(size_t &sizeToAllocate) {
        return allocateWithCustomAlignment(sizeToAllocate, 0u);
    }

    uint64_t allocateWithCustomAlignment(size_t &sizeToAllocate, size_t alignment);

    void free(uint64_t ptr, size_t size) {
        if (ptr == 0llu)
            return;

        std::lock_guard<std::mutex> lock(mtx);
        DBG_LOG(LogAllocationMemoryPool, __FUNCTION__, "Allocator usage == ", this->getUsage());

        if (ptr == pRightBound) {
            pRightBound = ptr + size;
            mergeLastFreedSmall();
        } else if (ptr == pLeftBound - size) {
            pLeftBound = ptr;
            mergeLastFreedBig();
        } else if (ptr < pLeftBound) {
            DEBUG_BREAK_IF(size <= sizeThreshold);
            storeInFreedChunks(ptr, size, freedChunksBig);
        } else {
            storeInFreedChunks(ptr, size, freedChunksSmall);
        }
        availableSize += size;
    }

    uint64_t getLeftSize() const {
        return availableSize;
    }

    uint64_t getUsedSize() const {
        return size - availableSize;
    }

    NO_SANITIZE
    double getUsage() const {
        return static_cast<double>(size - availableSize) / size;
    }

  protected:
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
            if ((freedChunk.ptr + freedChunk.size) == (ptr + size)) {
                if (ptr < freedChunk.ptr) {
                    freedChunk.ptr = ptr;
                    freedChunk.size = size;
                    return;
                }
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

    void defragment() {

        if (freedChunksSmall.size() > 1) {
            std::sort(freedChunksSmall.rbegin(), freedChunksSmall.rend());
            size_t maxSize = freedChunksSmall.size();
            for (size_t i = maxSize - 1; i > 0; --i) {
                auto ptr = freedChunksSmall[i].ptr;
                size_t chunkSize = freedChunksSmall[i].size;

                if (freedChunksSmall[i - 1].ptr == ptr + chunkSize) {
                    freedChunksSmall[i - 1].ptr = ptr;
                    freedChunksSmall[i - 1].size += chunkSize;
                    freedChunksSmall.erase(freedChunksSmall.begin() + i);
                }
            }
        }
        mergeLastFreedSmall();
        if (freedChunksBig.size() > 1) {
            std::sort(freedChunksBig.begin(), freedChunksBig.end());

            size_t maxSize = freedChunksBig.size();
            for (size_t i = maxSize - 1; i > 0; --i) {
                auto ptr = freedChunksBig[i].ptr;
                size_t chunkSize = freedChunksBig[i].size;
                if ((freedChunksBig[i - 1].ptr + freedChunksBig[i - 1].size) == ptr) {
                    freedChunksBig[i - 1].size += chunkSize;
                    freedChunksBig.erase(freedChunksBig.begin() + i);
                }
            }
        }
        mergeLastFreedBig();
        DBG_LOG(LogAllocationMemoryPool, __FUNCTION__, "Allocator usage == ", this->getUsage());
    }
};
} // namespace NEO
