/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <iterator>

namespace NEO {

template <typename DataType>
struct Range {
    using iterator = DataType *;
    using const_iterator = const DataType *;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    Range(DataType *base, size_t count)
        : begIt(base), endIt(base + count) {
    }

    template <typename SequentialContainerT, typename BeginT = decltype(((SequentialContainerT *)nullptr)->size())>
    Range(SequentialContainerT &container)
        : Range(&*container.begin(), container.size()) {
    }

    template <typename T, size_t S>
    Range(T (&base)[S])
        : Range(&base[0], S) {
    }

    iterator begin() {
        return begIt;
    }

    iterator end() {
        return endIt;
    }

    const_iterator begin() const {
        return begIt;
    }

    const_iterator end() const {
        return endIt;
    }

    reverse_iterator rbegin() {
        return reverse_iterator(end());
    }

    const_reverse_iterator rbegin() const {
        return const_reverse_iterator(end());
    }

    reverse_iterator rend() {
        return reverse_iterator(end()) + (endIt - begIt);
    }

    const_reverse_iterator rend() const {
        return const_reverse_iterator(end()) + (endIt - begIt);
    }

    bool empty() const {
        return begIt == endIt;
    }

    size_t size() const {
        return endIt - begIt;
    }

    iterator begIt;
    iterator endIt;
};

template <typename T>
inline Range<T> CreateRange(T *base, size_t count) {
    return Range<T>(base, count);
}
} // namespace NEO