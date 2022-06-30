/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/debug_helpers.h"

#include <algorithm>
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

    uint64_t allocateWithCustomAlignment(size_t &sizeToAllocate, size_t alignment) {
        if (alignment == 0) {
            alignment = this->allocationAlignment;
        }

        UNRECOVERABLE_IF(alignment % allocationAlignment != 0); // custom alignment have to be a multiple of allocator alignment
        sizeToAllocate = alignUp(sizeToAllocate, allocationAlignment);

        std::lock_guard<std::mutex> lock(mtx);
        DBG_LOG(LogAllocationMemoryPool, __FUNCTION__, "Allocator usage == ", this->getUsage());
        if (availableSize < sizeToAllocate) {
            return 0llu;
        }

        std::vector<HeapChunk> &freedChunks = (sizeToAllocate > sizeThreshold) ? freedChunksBig : freedChunksSmall;
        uint32_t defragmentCount = 0;

        for (;;) {
            size_t sizeOfFreedChunk = 0;
            uint64_t ptrReturn = getFromFreedChunks(sizeToAllocate, freedChunks, sizeOfFreedChunk, alignment);

            if (ptrReturn == 0llu) {
                if (sizeToAllocate > sizeThreshold) {
                    const uint64_t misalignment = alignUp(pLeftBound, alignment) - pLeftBound;
                    if (pLeftBound + misalignment + sizeToAllocate <= pRightBound) {
                        if (misalignment) {
                            storeInFreedChunks(pLeftBound, static_cast<size_t>(misalignment), freedChunks);
                            pLeftBound += misalignment;
                        }
                        ptrReturn = pLeftBound;
                        pLeftBound += sizeToAllocate;
                    }
                } else {
                    const uint64_t pStart = pRightBound - sizeToAllocate;
                    const uint64_t misalignment = pStart - alignDown(pStart, alignment);
                    if (pLeftBound + sizeToAllocate + misalignment <= pRightBound) {
                        if (misalignment) {
                            pRightBound -= misalignment;
                            storeInFreedChunks(pRightBound, static_cast<size_t>(misalignment), freedChunks);
                        }
                        pRightBound -= sizeToAllocate;
                        ptrReturn = pRightBound;
                    }
                }
            }

            if (ptrReturn != 0llu) {
                if (sizeOfFreedChunk > 0) {
                    availableSize -= sizeOfFreedChunk;
                    sizeToAllocate = sizeOfFreedChunk;
                } else {
                    availableSize -= sizeToAllocate;
                }
                DEBUG_BREAK_IF(!isAligned(ptrReturn, alignment));
                return ptrReturn;
            }

            if (defragmentCount == 1)
                return 0llu;
            defragment();
            defragmentCount++;
        }
    }

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

    uint64_t getFromFreedChunks(size_t size, std::vector<HeapChunk> &freedChunks, size_t &sizeOfFreedChunk, size_t requiredAlignment) {
        size_t elements = freedChunks.size();
        size_t bestFitIndex = -1;
        size_t bestFitSize = 0;
        sizeOfFreedChunk = 0;

        for (size_t i = 0; i < elements; i++) {
            const bool chunkAligned = isAligned(freedChunks[i].ptr, requiredAlignment);
            if (!chunkAligned) {
                continue;
            }

            if (freedChunks[i].size == size) {
                auto ptr = freedChunks[i].ptr;
                freedChunks.erase(freedChunks.begin() + i);
                return ptr;
            }

            if (freedChunks[i].size > size) {
                if (freedChunks[i].size < bestFitSize || bestFitSize == 0) {
                    bestFitIndex = i;
                    bestFitSize = freedChunks[i].size;
                }
            }
        }

        if (bestFitSize != 0) {
            if (bestFitSize < (size << 1)) {
                auto ptr = freedChunks[bestFitIndex].ptr;
                sizeOfFreedChunk = freedChunks[bestFitIndex].size;
                freedChunks.erase(freedChunks.begin() + bestFitIndex);
                return ptr;
            } else {
                size_t sizeDelta = freedChunks[bestFitIndex].size - size;

                DEBUG_BREAK_IF(!(size <= sizeThreshold || (size > sizeThreshold && sizeDelta > sizeThreshold)));

                auto ptr = freedChunks[bestFitIndex].ptr + sizeDelta;
                freedChunks[bestFitIndex].size = sizeDelta;
                return ptr;
            }
        }
        return 0llu;
    }

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
