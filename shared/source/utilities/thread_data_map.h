/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/flat_hash_map.h"

#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace NEO {

class HeapHelper;
class IndirectHeap;

struct ThreadDataCounter {
    size_t count;
    std::span<const uint8_t> threadData; // combined CTD and PTD
};

class ThreadDataTracker : NEO::NonCopyableAndNonMovableClass {
  public:
    void registerThreadData(uint64_t hash, std::span<const uint8_t> threadData);
    std::pair<uint64_t, std::span<const uint8_t>> getCommonThreadData();
    bool isEmpty() const { return threadDataOccurences.isEmpty(); }

  private:
    struct Hash {
        uint64_t operator()(const uint64_t &key) const noexcept {
            return key;
        }
    };
    FlatHashMap<uint64_t, ThreadDataCounter, Hash> threadDataOccurences;
};

class ThreadDataMap : NEO::NonCopyableAndNonMovableClass {
  public:
    ThreadDataMap(HeapHelper *heapHelper, uint32_t rootDeviceIndex, size_t heapAlignment);
    ~ThreadDataMap();

    void insert(uint64_t hash, std::span<const uint8_t> threadData);
    std::optional<uint64_t> find(uint64_t hash, std::span<const uint8_t> crossThreadData, std::span<const uint8_t> perThreadData) const;
    std::optional<uint64_t> find(std::span<const uint8_t> crossThreadData, std::span<const uint8_t> perThreadData) const;
    void reallocateMap();
    IndirectHeap *getStorage() const { return storage.get(); }

  private:
    struct ThreadDataKey {
        uint64_t hash = 0;
        std::vector<uint8_t> threadData; // combined CTD and PTD
    };
    struct ThreadDataKeyHash {
        uint64_t operator()(const ThreadDataKey &key) const noexcept {
            return key.hash;
        }
    };

    FlatHashMap<ThreadDataKey, uint64_t, ThreadDataKeyHash> threadDataMap;
    std::unique_ptr<IndirectHeap> storage;
    HeapHelper *heapHelper = nullptr;
    uint32_t rootDeviceIndex = 0;
    size_t heapAlignment = 0;
};

static_assert(NEO::NonCopyableAndNonMovable<ThreadDataTracker>);
static_assert(NEO::NonCopyableAndNonMovable<ThreadDataMap>);

} // namespace NEO
