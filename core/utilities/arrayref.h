/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstddef>
#include <type_traits>
#include <utility>

template <typename DataType>
class ArrayRef {
  public:
    using iterator = DataType *;
    using const_iterator = const DataType *;

    template <typename IteratorType>
    ArrayRef(IteratorType begIt, IteratorType endIt) {
        if (begIt != nullptr) {
            this->begIt = &*begIt;
            this->endIt = &*(endIt - 1) + 1;
        } else {
            this->begIt = nullptr;
            this->endIt = nullptr;
        }
    }

    template <typename IteratorType>
    ArrayRef(IteratorType first, size_t size)
        : ArrayRef(first, (size > 0) ? (first + size) : first) {
    }

    template <typename SequentialContainerType>
    ArrayRef(SequentialContainerType &ctr)
        : begIt((ctr.size() > 0) ? &*ctr.begin() : nullptr), endIt((ctr.size() > 0) ? (&*(ctr.end() - 1) + 1) : nullptr) {
    }

    template <size_t Size>
    ArrayRef(DataType (&array)[Size])
        : begIt(&array[0]), endIt(&array[Size]) {
    }

    ArrayRef() = default;

    size_t size() const {
        return endIt - begIt;
    }

    DataType &operator[](std::size_t idx) {
        return begIt[idx];
    }

    const DataType &operator[](std::size_t idx) const {
        return begIt[idx];
    }

    iterator begin() {
        return begIt;
    }

    const_iterator begin() const {
        return begIt;
    }

    iterator end() {
        return endIt;
    }

    const_iterator end() const {
        return endIt;
    }

    void swap(ArrayRef &rhs) {
        std::swap(begIt, rhs.begIt);
        std::swap(endIt, rhs.endIt);
    }

    operator ArrayRef<const DataType>() {
        return ArrayRef<const DataType>(begIt, endIt);
    }

  private:
    DataType *begIt = nullptr;
    DataType *endIt = nullptr;
};

template <typename T>
bool operator==(const ArrayRef<T> &lhs,
                const ArrayRef<T> &rhs) {
    if (lhs.size() != rhs.size()) {
        return false;
    }

    auto lhsIt = lhs.begin();
    auto lhsEnd = lhs.end();
    auto rhsIt = rhs.begin();

    for (; lhsIt != lhsEnd; ++lhsIt, ++rhsIt) {
        if (*lhsIt != *rhsIt) {
            return false;
        }
    }

    return true;
}
