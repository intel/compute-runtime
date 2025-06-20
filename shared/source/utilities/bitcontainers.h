/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/ptr_math.h"

#include <algorithm>
#include <bit>
#include <bitset>
#include <functional>
#include <vector>

// Variable-length bitarray with support for ffz (find first zero)
class BitArray final {
    static inline constexpr size_t chunkSize = 64;
    using Chunk = std::bitset<chunkSize>;
    static_assert(sizeof(Chunk) == sizeof(uint64_t));

    std::vector<Chunk> data;
    size_t arrayLength;

  public:
    static inline constexpr int64_t npos = -1;

    BitArray(size_t length) : arrayLength(length) {
        data.resize(alignUp(length, chunkSize) / chunkSize);
    }

    Chunk::reference operator[](size_t pos) {
        DEBUG_BREAK_IF(pos >= arrayLength);
        return data[pos / chunkSize][pos % chunkSize];
    }

    int64_t ffz() const {
        auto chunkIt = std::find_if(cbegin(data), cend(data), [](auto &chunk) { return false == chunk.all(); });
        if (cend(data) == chunkIt) {
            return npos;
        }

        auto offset = std::countr_one(chunkIt->to_ullong());
        auto block = chunkIt - cbegin(data);
        auto pos = block * chunkSize + offset;
        if (pos >= arrayLength) {
            return npos;
        }
        return static_cast<int64_t>(pos);
    }

    size_t length() const {
        return arrayLength;
    }
};

// Variable-length allocator of bits (positions)
class BitAllocator final {
    BitArray bitArray;

  public:
    static inline constexpr int64_t npos = BitArray::npos;

    BitAllocator(size_t capacity) : bitArray(capacity) {
    }

    int64_t allocate() {
        auto pos = bitArray.ffz();
        if (BitArray::npos == pos) {
            return npos;
        }
        bitArray[static_cast<size_t>(pos)] = true;
        return pos;
    }

    void free(int64_t pos) {
        if (pos < 0) {
            DEBUG_BREAK_IF(true);
            return;
        }
        DEBUG_BREAK_IF(false == bitArray[static_cast<size_t>(pos)]);
        bitArray[static_cast<size_t>(pos)] = false;
    }

    size_t sizeInBits() const {
        return bitArray.length();
    }
};

// Array of opaque elements
template <typename UnderlyingMemoryHandleT = void *>
class OpaqueArray {
    using ElementCount = size_t;

    const UnderlyingMemoryHandleT userHandle;
    void *const array = nullptr;
    const size_t arraySizeInBytes = 0;
    const size_t elementStrideInBytes = 0;

  protected:
    size_t idx(void *ptr) const {
        DEBUG_BREAK_IF(false == contains(ptr));
        auto byteOffset = ptrDiff(ptr, array);
        return byteOffset / elementStrideInBytes;
    }

  public:
    template <typename T>
    OpaqueArray(T &&handle, void *array, size_t elementStrideInBytes, size_t numElementsInArray)
        : userHandle(std::forward<T>(handle)), array(reinterpret_cast<uint8_t *>(array)),
          arraySizeInBytes(numElementsInArray * elementStrideInBytes), elementStrideInBytes(elementStrideInBytes) {
    }

    const UnderlyingMemoryHandleT &handle() const {
        return userHandle;
    }

    void *element(size_t pos) {
        return ptrOffset(array, pos * elementStrideInBytes);
    }

    void *base() const {
        return array;
    }

    bool contains(void *ptr) const {
        if (byteRangeContains(array, arraySizeInBytes, ptr)) {
            DEBUG_BREAK_IF(ptrDiff(ptr, array) + elementStrideInBytes > arraySizeInBytes);
            return true;
        }
        return false;
    }
};

// Fast fixed-size allocator of opaque elements (of uniform size)
template <typename UnderlyingMemoryHandleT = void *>
class OpaqueArrayElementAllocator final : public OpaqueArray<UnderlyingMemoryHandleT> {
    BitAllocator bitAllocator;

  public:
    template <typename T>
    OpaqueArrayElementAllocator(T &&handle, void *array, size_t elementStrideInBytes, size_t numElementsInArray)
        : OpaqueArray<UnderlyingMemoryHandleT>(std::forward<T>(handle), array, elementStrideInBytes, numElementsInArray),
          bitAllocator(numElementsInArray) {
    }

    void *allocate() {
        auto pos = bitAllocator.allocate();
        if (BitAllocator::npos == pos) {
            return nullptr;
        }
        return this->element(static_cast<size_t>(pos));
    }

    bool free(void *el) {
        if (this->contains(el) == false) {
            return false;
        }

        auto pos = this->idx(el);
        bitAllocator.free(pos);
        return true;
    }
};

template <typename UnderlyingMemoryHandleT>
struct UnderlyingAllocator {
    using AllocationT = std::pair<UnderlyingMemoryHandleT, void *>;

    std::function<AllocationT(size_t size, size_t alignment)> allocate;
    std::function<void(AllocationT)> free;
};

// Fast dynamic-size allocator of opaque elements (of uniform size)
template <typename UnderlyingMemoryHandleT = void *>
class OpaqueElementAllocator final {
  public:
    using UnderlyingAllocatorT = UnderlyingAllocator<UnderlyingMemoryHandleT>;
    using AllocationT = UnderlyingAllocatorT::AllocationT;

  private:
    const size_t chunkSize;
    const size_t alignedElementSize;

    UnderlyingAllocatorT underlyingAllocator;
    using ChunkT = OpaqueArrayElementAllocator<UnderlyingMemoryHandleT>;
    std::vector<ChunkT> chunks;

  public:
    template <typename GivenAllocatorT = UnderlyingAllocatorT>
    OpaqueElementAllocator(size_t chunkSize, size_t alignedElementSize,
                           GivenAllocatorT &&underlyingAllocator) : chunkSize(chunkSize), alignedElementSize(alignedElementSize),
                                                                    underlyingAllocator(std::forward<GivenAllocatorT>(underlyingAllocator)) {
        UNRECOVERABLE_IF(chunkSize < alignedElementSize);
        DEBUG_BREAK_IF((chunkSize % alignedElementSize) != 0);
    }

    ~OpaqueElementAllocator() {
        for (auto &chunk : chunks) {
            underlyingAllocator.free({std::move(chunk.handle()), chunk.base()});
        }
    }

    OpaqueElementAllocator(OpaqueElementAllocator &&) = default;
    OpaqueElementAllocator &operator=(OpaqueElementAllocator &&) = default;
    OpaqueElementAllocator(const OpaqueElementAllocator &) = delete;
    OpaqueElementAllocator &operator=(const OpaqueElementAllocator &) = delete;

    AllocationT allocate() {
        for (auto &chunk : chunks) {
            auto *va = chunk.allocate();
            if (va) {
                return {chunk.handle(), va};
            }
        }

        auto alloc = underlyingAllocator.allocate(chunkSize, alignedElementSize);
        if (nullptr == alloc.second) {
            return {};
        }
        chunks.emplace_back(std::move(alloc.first), alloc.second, alignedElementSize, chunkSize / alignedElementSize);

        return {chunks.rbegin()->handle(), chunks.rbegin()->allocate()};
    }

    bool free(void *ptr) {
        return std::ranges::any_of(chunks,
                                   [=](auto &chunk) { return chunk.free(ptr); });
    }

    bool contains(void *ptr) const {
        return std::ranges::any_of(chunks,
                                   [=](const auto &chunk) { return chunk.contains(ptr); });
    }
};
