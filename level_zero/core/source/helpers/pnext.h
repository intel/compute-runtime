/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <level_zero/ze_api.h>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <ranges>

namespace L0 {

struct PNextRange {
    struct PNextIterator {
        using difference_type = ptrdiff_t;
        using value_type = ze_base_desc_t;
        using pointer = const ze_base_desc_t *;
        using reference = const ze_base_desc_t &;
        using iterator_category = std::forward_iterator_tag;

        PNextIterator(const ze_base_desc_t *curr)
            : curr(curr) {
        }

        bool operator==(const PNextIterator &rhs) const {
            return (curr == rhs.curr);
        }

        bool operator!=(const PNextIterator &rhs) const {
            return false == (*this == rhs);
        }

        reference operator*() {
            return *curr;
        }

        pointer operator->() {
            return curr;
        }

        PNextIterator &operator++() {
            if (nullptr != curr) {
                curr = static_cast<const ze_base_desc_t *>(curr->pNext);
            }
            return *this;
        }

        PNextIterator operator++(int) {
            if (nullptr == curr) {
                return *this;
            }

            PNextIterator old = *this;
            ++*this;
            return old;
        }

      protected:
        const ze_base_desc_t *curr;
    };

    using iterator = PNextIterator;

    PNextRange(const ze_base_desc_t *first) : first(first) {
    }

    PNextRange(const void *first) : PNextRange(static_cast<const ze_base_desc_t *>(first)) {
    }

    size_t size() const {
        if (-1 != cachedSize) {
            return cachedSize;
        }
        auto countedSize = count();
        cachedSize = countedSize;
        return countedSize;
    }

    iterator begin() const {
        return PNextIterator{first};
    }

    iterator end() const {
        return PNextIterator{nullptr};
    }

    template <typename T = ze_base_desc_t>
    const T *get(ze_structure_type_t requested) const {
        auto it = std::find_if(begin(), end(), [&](const auto &entry) { return entry.stype == requested; });
        if (end() == it) {
            return nullptr;
        } else {
            return reinterpret_cast<const T *>(&*it);
        }
    }

    bool contains(ze_structure_type_t requested) const {
        return this->get(requested) != nullptr;
    }

  protected:
    int32_t count() const {
        int32_t num = 0;
        const ze_base_desc_t *curr = first;
        while (curr) {
            ++num;
            curr = static_cast<const ze_base_desc_t *>(curr->pNext);
        }
        return num;
    }

    const ze_base_desc_t *first;
    mutable int32_t cachedSize = -1;
};

} // namespace L0
