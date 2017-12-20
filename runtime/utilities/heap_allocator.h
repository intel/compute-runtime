/*
 * Copyright (c) 2017, Intel Corporation
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
#include "runtime/helpers/aligned_memory.h"
#include "runtime/helpers/debug_helpers.h"

#include <cstdint>
#include <algorithm>

#include <vector>
#include <unordered_map>

namespace OCLRT {

struct HeapChunk {
    HeapChunk(void *ptr, size_t size) : ptr(ptr), size(size) {}
    void *ptr;
    size_t size;
};

bool operator<(const HeapChunk &hc1, const HeapChunk &hc2);

class HeapAllocator {
  public:
    HeapAllocator(void *address, uint64_t size) : address(address), size(size), availableSize(size), sizeThreshold(defaultSizeThreshold) {
        pLeftBound = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(address));
        pRightBound = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(address) + (size_t)size);
        freedChunksBig.reserve(10);
        freedChunksSmall.reserve(50);
    }

    HeapAllocator(void *address, uint64_t size, size_t threshold) : address(address), size(size), availableSize(size), sizeThreshold(threshold) {
        pLeftBound = reinterpret_cast<uint64_t>(address);
        pRightBound = reinterpret_cast<uint64_t>(address) + size;
        freedChunksBig.reserve(10);
        freedChunksSmall.reserve(50);
    }

    ~HeapAllocator() {
    }

    void *allocate(size_t &sizeToAllocate) {
        std::lock_guard<std::mutex> lock(mtx);
        sizeToAllocate = alignUp(sizeToAllocate, allocationAlignment);
        void *ptrReturn = nullptr;

        DBG_LOG(PrintDebugMessages, __FUNCTION__, "Allocator usage == ", this->getUsage());

        if (availableSize < sizeToAllocate) {
            return nullptr;
        }

        std::vector<HeapChunk> &freedChunks = (sizeToAllocate > sizeThreshold) ? freedChunksBig : freedChunksSmall;
        size_t sizeOfFreedChunk = 0;
        uint32_t defragmentCount = 0;

        while (ptrReturn == nullptr) {

            ptrReturn = getFromFreedChunks(sizeToAllocate, freedChunks, sizeOfFreedChunk);

            if (ptrReturn == nullptr) {
                if (sizeToAllocate > sizeThreshold) {
                    if (pLeftBound + sizeToAllocate <= pRightBound) {
                        ptrReturn = reinterpret_cast<void *>(pLeftBound);
                        pLeftBound += sizeToAllocate;
                    }
                } else {
                    if (pRightBound - sizeToAllocate >= pLeftBound) {
                        pRightBound -= sizeToAllocate;
                        ptrReturn = reinterpret_cast<void *>(pRightBound);
                    }
                }
            }

            if (ptrReturn != nullptr) {
                if (sizeOfFreedChunk > 0) {
                    availableSize -= sizeOfFreedChunk;
                    sizeToAllocate = sizeOfFreedChunk;
                } else {
                    availableSize -= sizeToAllocate;
                }
            }

            if (ptrReturn == nullptr) {
                if (defragmentCount == 1)
                    break;
                defragment();
                defragmentCount++;
            }
        }

        return ptrReturn;
    }

    void free(void *ptr, size_t size) {
        std::lock_guard<std::mutex> lock(mtx);
        uintptr_t ptrIn = reinterpret_cast<uintptr_t>(ptr);
        if (ptrIn == 0u)
            return;

        DBG_LOG(PrintDebugMessages, __FUNCTION__, "Allocator usage == ", this->getUsage());

        if (ptrIn == pRightBound) {
            pRightBound = ptrIn + size;
            mergeLastFreedSmall();
        } else if (ptrIn == (pLeftBound - size)) {
            pLeftBound = (pLeftBound - size);
            mergeLastFreedBig();
        } else {
            if (ptrIn < pLeftBound) {
                DEBUG_BREAK_IF(size <= sizeThreshold);
                storeInFreedChunks(ptr, size, freedChunksBig);
            } else {
                storeInFreedChunks(ptr, size, freedChunksSmall);
            }
        }
        availableSize += size;
    }

    uint64_t getLeftSize() {
        return availableSize;
    }

    uint64_t getUsedSize() {
        return size - availableSize;
    }

    NO_SANITIZE
    double getUsage() {
        return 1.0 * (size - availableSize) / (size * 1.0);
    }

  protected:
    void *address;
    uint64_t size;
    uint64_t availableSize;
    uint64_t pLeftBound, pRightBound;
    const size_t defaultSizeThreshold = 4096 * 1024;
    const size_t sizeThreshold;
    size_t allocationAlignment = MemoryConstants::pageSize;

    std::vector<HeapChunk> freedChunksSmall;
    std::vector<HeapChunk> freedChunksBig;
    std::mutex mtx;

    void *getFromFreedChunks(size_t size, std::vector<HeapChunk> &freedChunks, size_t &sizeOfFreedChunk) {
        size_t elements = freedChunks.size();
        size_t bestFitIndex = -1;
        size_t bestFitSize = 0;
        sizeOfFreedChunk = 0;

        for (size_t i = 0; i < elements; i++) {
            if (freedChunks[i].size == size) {
                void *ptr = freedChunks[i].ptr;
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
                void *ptr = freedChunks[bestFitIndex].ptr;
                sizeOfFreedChunk = freedChunks[bestFitIndex].size;
                freedChunks.erase(freedChunks.begin() + bestFitIndex);
                return ptr;
            } else {
                size_t sizeDelta = freedChunks[bestFitIndex].size - size;

                DEBUG_BREAK_IF(!((size <= sizeThreshold) || ((size > sizeThreshold) && (sizeDelta > sizeThreshold))));

                void *ptr = reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(freedChunks[bestFitIndex].ptr) + sizeDelta);
                freedChunks[bestFitIndex].size = sizeDelta;
                return ptr;
            }
        }
        return nullptr;
    }

    void storeInFreedChunks(void *ptr, size_t size, std::vector<HeapChunk> &freedChunks) {
        size_t elements = freedChunks.size();
        uintptr_t pLeft = reinterpret_cast<uintptr_t>(ptr);
        uintptr_t pRight = reinterpret_cast<uintptr_t>(ptr) + size;
        bool freedChunkStored = false;

        for (size_t i = 0; i < elements; i++) {
            if (freedChunks[i].ptr == reinterpret_cast<void *>(pRight)) {
                freedChunks[i].ptr = reinterpret_cast<void *>(pLeft);
                freedChunks[i].size += size;
                freedChunkStored = true;
            } else if ((reinterpret_cast<uintptr_t>(freedChunks[i].ptr) + freedChunks[i].size) == pLeft) {
                freedChunks[i].size += size;
                freedChunkStored = true;
            }

            if (freedChunkStored == true) {
                break;
            }
        }

        if (freedChunkStored == false) {
            freedChunks.emplace_back(ptr, size);
        }
    }

    void mergeLastFreedSmall() {

        size_t maxSizeOfSmallChunks = freedChunksSmall.size();

        if (maxSizeOfSmallChunks > 0) {
            uintptr_t ptr = reinterpret_cast<uintptr_t>(freedChunksSmall[maxSizeOfSmallChunks - 1].ptr);
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
            uintptr_t ptr = reinterpret_cast<uintptr_t>(freedChunksBig[maxSizeOfBigChunks - 1].ptr);
            size_t chunkSize = freedChunksBig[maxSizeOfBigChunks - 1].size;

            if (ptr == (pLeftBound - chunkSize)) {
                pLeftBound = (pLeftBound - chunkSize);
                freedChunksBig.pop_back();
            }
        }
    }

    void defragment() {

        if (freedChunksSmall.size() > 1) {
            std::sort(freedChunksSmall.rbegin(), freedChunksSmall.rend());
            size_t maxSize = freedChunksSmall.size();
            for (size_t i = maxSize - 1; i > 0; --i) {
                uintptr_t ptr = reinterpret_cast<uintptr_t>(freedChunksSmall[i].ptr);
                size_t chunkSize = freedChunksSmall[i].size;

                if (reinterpret_cast<uintptr_t>(freedChunksSmall[i - 1].ptr) == ptr + chunkSize) {
                    freedChunksSmall[i - 1].ptr = reinterpret_cast<void *>(ptr);
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
                uintptr_t ptr = reinterpret_cast<uintptr_t>(freedChunksBig[i].ptr);
                size_t chunkSize = freedChunksBig[i].size;
                if ((reinterpret_cast<uintptr_t>(freedChunksBig[i - 1].ptr) + freedChunksBig[i - 1].size) == ptr) {
                    freedChunksBig[i - 1].size += chunkSize;
                    freedChunksBig.erase(freedChunksBig.begin() + i);
                }
            }
        }
        mergeLastFreedBig();
        DBG_LOG(PrintDebugMessages, __FUNCTION__, "Allocator usage == ", this->getUsage());
    }
};
} // namespace OCLRT
