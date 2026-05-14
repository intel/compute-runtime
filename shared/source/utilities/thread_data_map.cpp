/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/thread_data_map.h"

#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/helpers/heap_helper.h"
#include "shared/source/indirect_heap/heap_size.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/source/utilities/thread_data_hash.h"

namespace NEO {

/**
 * @brief Registers thread data (combined cross and per-thread data) for a given hash.
 * If the same hash is registered multiple times, occurrence count is increased.
 *
 * @param[in] hash hash of the thread data
 * @param[in] threadData thread data to be registered
 */
void ThreadDataTracker::registerThreadData(uint64_t hash, std::span<const uint8_t> threadData) {
    auto *existing = threadDataOccurences.findBy(hash, [hash](const uint64_t &key) { return key == hash; });
    if (existing != nullptr) {
        existing->count++;
        return;
    }
    threadDataOccurences.insert(hash, {1, threadData});
}

/**
 * @brief Finds most common thread data based on the occurrence count.
 * After this call, the tracker is cleared.
 *
 * @return A pair containing the hash and the most common thread data.
 */
std::pair<uint64_t, std::span<const uint8_t>> ThreadDataTracker::getCommonThreadData() {
    ThreadDataCounter mostFrequent = {0, {}};
    uint64_t hash = 0;
    threadDataOccurences.forEach([&mostFrequent, &hash](uint64_t h, const ThreadDataCounter &marker) {
        if (marker.count > mostFrequent.count ||
            (marker.count == mostFrequent.count && marker.threadData.size() > mostFrequent.threadData.size())) {
            mostFrequent = marker;
            hash = h;
        }
    });
    threadDataOccurences.clear();
    return {hash, mostFrequent.threadData};
}

ThreadDataMap::ThreadDataMap(HeapHelper *heapHelper, uint32_t rootDeviceIndex, size_t heapAlignment) : heapHelper(heapHelper), rootDeviceIndex(rootDeviceIndex), heapAlignment(heapAlignment) {
    this->storage = std::make_unique<IndirectHeap>(nullptr, true);
}

ThreadDataMap::~ThreadDataMap() {
    heapHelper->storeHeapAllocation(storage->getGraphicsAllocation());
}

/**
 * @brief Inserts hash and thread data into the map.
 * Thread data is stored in a GPU-accessible heap, and the offset to that data is stored in the map.
 * Key to the offset consists of both hash and CPU-copy of thread data, to ensure that hash collisions are handled correctly.
 *
 * If there is not enough space in the current allocation, the map is reallocated.
 * In that case, all existing offsets are invalidated, and map is cleared.
 *
 * @param[in] hash hash of the thread data
 * @param[in] threadData thread data to be added to the map
 */
void ThreadDataMap::insert(uint64_t hash, std::span<const uint8_t> threadData) {
    storage->align(heapAlignment);
    if (storage->getAvailableSpace() < threadData.size()) {
        reallocateMap();
    }
    auto gpuThreadData = std::span<uint8_t>(reinterpret_cast<uint8_t *>(storage->getSpace(threadData.size())), threadData.size());
    std::copy(threadData.begin(), threadData.end(), gpuThreadData.begin());

    uint64_t threadDataOffset = 0;
    if constexpr (is64bit) {
        threadDataOffset = storage->getHeapGpuStartOffset() + (storage->getUsed() - threadData.size());
    } else {
        threadDataOffset = storage->getHeapGpuBase() + (storage->getUsed() - threadData.size());
    }
    ThreadDataKey key{hash, std::vector<uint8_t>(threadData.begin(), threadData.end())};
    threadDataMap.insert(std::move(key), threadDataOffset);
}

/**
 * @brief Finds offset to the thread data in GPU-accessible heap based on the hash and thread data.
 *
 * @param[in] hash hash of the thread data
 * @param[in] crossThreadData cross-thread data to be matched
 * @param[in] perThreadData per-thread data to be matched
 *
 * @return Offset to the thread data in GPU-accessible heap if found, std::nullopt otherwise
 */
std::optional<uint64_t> ThreadDataMap::find(uint64_t hash, std::span<const uint8_t> crossThreadData, std::span<const uint8_t> perThreadData) const {
    auto keyComparator = [crossThreadData, perThreadData](const ThreadDataKey &key) {
        if (key.threadData.size() != (crossThreadData.size() + perThreadData.size())) {
            return false;
        }
        return std::equal(crossThreadData.begin(), crossThreadData.end(), key.threadData.begin()) &&
               std::equal(perThreadData.begin(), perThreadData.end(), key.threadData.begin() + crossThreadData.size(), key.threadData.end());
    };
    auto result = threadDataMap.findBy(hash, keyComparator);
    return result != nullptr ? std::optional<uint64_t>{*result} : std::nullopt;
}

std::optional<uint64_t> ThreadDataMap::find(std::span<const uint8_t> crossThreadData, std::span<const uint8_t> perThreadData) const {
    return this->find(ThreadDataHash::computeThreadDataHash(crossThreadData, perThreadData), crossThreadData, perThreadData);
}

/**
 * @brief Clears map, allocates new heap storage, and stores old allocation for future reusage.
 */
void ThreadDataMap::reallocateMap() {
    constexpr auto heapType = IndirectHeap::Type::indirectObject;
    auto heapSize = HeapSize::getDefaultHeapSize(heapType);

    auto newAlloc = heapHelper->getHeapAllocation(heapType, heapSize, MemoryConstants::pageSize64k, rootDeviceIndex);
    auto oldAlloc = storage->getGraphicsAllocation();

    storage->replaceGraphicsAllocation(newAlloc);
    storage->replaceBuffer(newAlloc->getUnderlyingBuffer(), newAlloc->getUnderlyingBufferSize());
    heapHelper->storeHeapAllocation(oldAlloc);

    threadDataMap.clear();
}

} // namespace NEO
