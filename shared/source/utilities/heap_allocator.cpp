/*
 * Copyright (C) 2019-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/heap_allocator.h"

#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/utilities/logger.h"

#include <algorithm>

namespace NEO {

bool operator<(const HeapChunk &hc1, const HeapChunk &hc2) {
    return hc1.ptr < hc2.ptr;
}

uint64_t HeapAllocator::allocateWithCustomAlignmentWithStartAddressHint(const uint64_t requiredStartAddress, size_t &sizeToAllocate, size_t alignment) {

    if (alignment < this->allocationAlignment) {
        alignment = this->allocationAlignment;
    }

    UNRECOVERABLE_IF(alignment % allocationAlignment != 0); // custom alignment have to be a multiple of allocator alignment
    sizeToAllocate = alignUp(sizeToAllocate, allocationAlignment);

    uint64_t ptrReturn = 0llu;

    {
        std::lock_guard<std::mutex> lock(mtx);
        DBG_LOG(LogAllocationMemoryPool, __FUNCTION__, "Allocator usage == ", this->getUsage());
        if (availableSize < sizeToAllocate) {
            return 0llu;
        }

        if (requiredStartAddress >= pLeftBound && requiredStartAddress <= pRightBound) {

            const uint64_t misalignment = requiredStartAddress - pLeftBound;
            if (pLeftBound + misalignment + sizeToAllocate <= pRightBound) {
                if (misalignment) {
                    storeInFreedChunks(pLeftBound, static_cast<size_t>(misalignment), freedChunksBig);
                    pLeftBound += misalignment;
                }
                ptrReturn = pLeftBound;
                pLeftBound += sizeToAllocate;
                availableSize -= sizeToAllocate;
            }
        } else { // Try to find in freed chunks

            defragment();

            if (requiredStartAddress >= this->baseAddress && requiredStartAddress < this->pLeftBound) {
                // If between baseAddress and pLeftBound, get from freedChunksBig
                ptrReturn = getFromFreedChunksWithStartAddressHint(requiredStartAddress, sizeToAllocate, freedChunksBig);
            } else {
                // If between pRightBound and (baseAddress + size), get from freedChunksSmall
                ptrReturn = getFromFreedChunksWithStartAddressHint(requiredStartAddress, sizeToAllocate, freedChunksSmall);
            }

            if (ptrReturn != 0llu) {
                availableSize -= sizeToAllocate;
            }
        }
    }

    if (ptrReturn == 0llu) {
        return allocateWithCustomAlignment(sizeToAllocate, alignment);
    }

    UNRECOVERABLE_IF(!isAligned(ptrReturn, alignment));
    return ptrReturn;
}

uint64_t HeapAllocator::allocateWithCustomAlignment(size_t &sizeToAllocate, size_t alignment) {
    if (alignment < this->allocationAlignment) {
        alignment = this->allocationAlignment;
    }

    UNRECOVERABLE_IF(alignment % allocationAlignment != 0); // custom alignment have to be a multiple of allocator alignment
    sizeToAllocate = alignUp(sizeToAllocate, allocationAlignment);

    std::lock_guard<std::mutex> lock(mtx);
    DBG_LOG(LogAllocationMemoryPool, __FUNCTION__, "Allocator usage == ", this->getUsage());
    if (availableSize < sizeToAllocate) {
        return 0llu;
    }

    auto getMemoryFromPool = [&](std::vector<HeapChunk> &freedChunks) {
        uint64_t ptrReturn = 0llu;

        if (sizeToAllocate > sizeThreshold) {
            const uint64_t misalignment = alignUp(pLeftBound, alignment) - pLeftBound;
            if (pLeftBound + misalignment + sizeToAllocate <= pRightBound) {
                if (misalignment) {
                    storeInFreedChunks(pLeftBound, static_cast<size_t>(misalignment), freedChunksBig);
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
                    storeInFreedChunks(pRightBound, static_cast<size_t>(misalignment), freedChunksSmall);
                }
                pRightBound -= sizeToAllocate;
                ptrReturn = pRightBound;
            }
        }

        size_t sizeOfFreedChunk = 0;
        if (ptrReturn == 0llu) {
            ptrReturn = getFromFreedChunks(sizeToAllocate, freedChunks, sizeOfFreedChunk, alignment);
        }

        if (ptrReturn != 0llu) {
            if (sizeOfFreedChunk > 0) {
                availableSize -= sizeOfFreedChunk;
                sizeToAllocate = sizeOfFreedChunk;
            } else {
                availableSize -= sizeToAllocate;
            }
            UNRECOVERABLE_IF(!isAligned(ptrReturn, alignment));
            return ptrReturn;
        }
        return ptrReturn;
    };
    std::vector<HeapChunk> &primaryPool = (sizeToAllocate > sizeThreshold) ? freedChunksBig : freedChunksSmall;

    auto ptr = getMemoryFromPool(primaryPool);
    if (ptr != 0llu) {
        return ptr;
    }

    defragment();
    ptr = getMemoryFromPool(primaryPool);
    if (ptr != 0llu) {
        return ptr;
    }

    std::vector<HeapChunk> &secondaryPool = (sizeToAllocate <= sizeThreshold) ? freedChunksBig : freedChunksSmall;
    ptr = getMemoryFromPool(secondaryPool);
    if (ptr != 0llu) {
        return ptr;
    }

    if (alignment > 2 * MemoryConstants::megaByte && pRightBound - pLeftBound >= sizeToAllocate) {
        alignment = Math::prevPowerOfTwo(static_cast<size_t>(pRightBound - pLeftBound - 1 - sizeToAllocate + 2 * MemoryConstants::pageSize64k));
        ptr = getMemoryFromPool(primaryPool);
        if (ptr != 0llu) {
            return ptr;
        }
        ptr = getMemoryFromPool(secondaryPool);
        if (ptr != 0llu) {
            return ptr;
        }
    }

    return 0llu;
}

void HeapAllocator::free(uint64_t ptr, size_t size) {
    if (ptr == 0llu) {
        return;
    }

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

uint64_t HeapAllocator::getFromFreedChunksWithStartAddressHint(const uint64_t requiredStartAddress, size_t size, std::vector<HeapChunk> &freedChunks) {

    for (size_t i = 0; i < freedChunks.size(); i++) {
        uint64_t chunkStart = freedChunks[i].ptr;
        uint64_t chunkEnd = chunkStart + freedChunks[i].size;

        if (requiredStartAddress >= chunkStart && requiredStartAddress + size <= chunkEnd) {
            size_t leadingSize = static_cast<size_t>(requiredStartAddress - chunkStart);
            size_t trailingSize = static_cast<size_t>(chunkEnd - (requiredStartAddress + size));

            // Chunk splitting
            if (leadingSize > 0) {

                freedChunks[i].size = leadingSize;

                if (trailingSize > 0) {
                    freedChunks.emplace_back(requiredStartAddress + size, trailingSize);
                }
            } else {

                if (trailingSize > 0) {

                    freedChunks[i].ptr = requiredStartAddress + size;
                    freedChunks[i].size = trailingSize;
                } else {

                    freedChunks.erase(freedChunks.begin() + i);
                }
            }

            return requiredStartAddress;
        }
    }

    return 0llu;
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
            if (!isAligned(ptr, requiredAlignment)) {
                auto alignedPtr = alignDown(ptr, requiredAlignment);
                auto alignedDelta = ptr - alignedPtr;

                sizeOfFreedChunk = size + static_cast<size_t>(alignedDelta);
                freedChunks[bestFitIndex].size = sizeDelta - static_cast<size_t>(alignedDelta);
                if (freedChunks[bestFitIndex].size == 0) {
                    freedChunks.erase(freedChunks.begin() + bestFitIndex);
                }
                return alignedPtr;
            }

            freedChunks[bestFitIndex].size = sizeDelta;
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
