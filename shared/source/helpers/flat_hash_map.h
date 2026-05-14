/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace NEO {

template <typename Key, typename Value, typename Hash>
class FlatHashMap : NEO::NonCopyableAndNonMovableClass {
  public:
    FlatHashMap() {
        hashes.resize(initialCapacity, emptySlot);
        entries.resize(initialCapacity);
    }

    template <typename KeyComparator>
    const Value *findBy(uint64_t precomputedHash, KeyComparator &&matches) const {
        const auto h = precomputedHash != emptySlot ? precomputedHash : emptySlot + 1;
        const size_t mask = hashes.size() - 1;
        size_t idx = h & mask;
        for (size_t i = 0; i < hashes.size(); ++i) {
            const auto sh = hashes[idx];
            if (sh == emptySlot) {
                return nullptr;
            }
            if (sh == h && matches(entries[idx].first)) {
                return &entries[idx].second;
            }
            idx = (idx + 1) & mask;
        }
        return nullptr;
    }

    template <typename KeyComparator>
    Value *findBy(uint64_t precomputedHash, KeyComparator &&matches) {
        return const_cast<Value *>(static_cast<const FlatHashMap *>(this)->findBy(precomputedHash, std::forward<KeyComparator>(matches)));
    }

    template <typename Func>
    void forEach(Func &&func) const {
        for (size_t i = 0; i < hashes.size(); ++i) {
            if (hashes[i] != emptySlot) {
                func(entries[i].first, entries[i].second);
            }
        }
    }

    void insert(Key key, Value value) {
        ensureCapacity();
        const auto h = makeHash(key);
        insertInto(hashes, entries, h, std::move(key), std::move(value));
        ++occupiedCount;
    }

    void clear() {
        for (size_t i = 0; i < hashes.size(); ++i) {
            if (hashes[i] != emptySlot) {
                hashes[i] = emptySlot;
                entries[i].first = Key{};
            }
        }
        occupiedCount = 0;
    }

    bool isEmpty() const {
        return occupiedCount == 0;
    }

  private:
    using Entry = std::pair<Key, Value>;

    static constexpr size_t initialCapacity = 8;
    static constexpr size_t maxLoadFactorPercent = 75;
    static constexpr size_t emptySlot = 0;
    static_assert(initialCapacity && (initialCapacity & (initialCapacity - 1)) == 0, "Initial capacity must be a power of two.");

    std::vector<uint64_t> hashes;
    std::vector<Entry> entries;
    size_t occupiedCount = 0;
    Hash hasher;

    uint64_t makeHash(const Key &key) const {
        const uint64_t h = hasher(key);
        return h != emptySlot ? h : emptySlot + 1;
    }

    static void insertInto(std::vector<uint64_t> &hs, std::vector<Entry> &es,
                           uint64_t hash, Key key, Value value) {
        const size_t mask = hs.size() - 1;
        size_t idx = hash & mask;
        for (size_t i = 0; i < hs.size(); ++i) {
            if (hs[idx] == emptySlot) {
                hs[idx] = hash;
                es[idx].first = std::move(key);
                es[idx].second = std::move(value);
                return;
            }
            idx = (idx + 1) & mask;
        }
    }

    void ensureCapacity() {
        if ((occupiedCount + 1) * 100 < hashes.size() * maxLoadFactorPercent) {
            return;
        }
        const size_t newCap = hashes.size() * 2;
        std::vector<uint64_t> newHashes(newCap, emptySlot);
        std::vector<Entry> newEntries(newCap);
        for (size_t i = 0; i < hashes.size(); ++i) {
            if (hashes[i] != emptySlot) {
                insertInto(newHashes, newEntries, hashes[i],
                           std::move(entries[i].first), std::move(entries[i].second));
            }
        }
        hashes = std::move(newHashes);
        entries = std::move(newEntries);
    }
};

} // namespace NEO
