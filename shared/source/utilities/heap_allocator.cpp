/*
 * Copyright (C) 2019-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/heap_allocator.h"

#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/utilities/logger.h"

namespace NEO {

bool operator<(const HeapChunk &hc1, const HeapChunk &hc2) {
    return hc1.ptr < hc2.ptr;
}

uint64_t HeapAllocator::allocateWithCustomAlignment(size_t &sizeToAllocate, size_t alignment) {
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

void HeapAllocator::free(uint64_t ptr, size_t size) {
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

NO_SANITIZE
double HeapAllocator::getUsage() const {
    return static_cast<double>(size - availableSize) / size;
}

uint64_t HeapAllocator::getFromFreedChunks(size_t size, std::vector<HeapChunk> &freedChunks, size_t &sizeOfFreedChunk, size_t requiredAlignment) {
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

            if (!isAligned(ptr, requiredAlignment)) {
                return 0llu;
            }
            return ptr;
        }
    }
    return 0llu;
}

void HeapAllocator::defragment() {

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

} // namespace NEO
