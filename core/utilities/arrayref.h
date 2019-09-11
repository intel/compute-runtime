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
    ArrayRef(IteratorType b, IteratorType e) {
        if (b != nullptr) {
            this->b = &*b;
            this->e = &*(e - 1) + 1;
        } else {
            this->b = nullptr;
            this->e = nullptr;
        }
    }

    template <typename IteratorType>
    ArrayRef(IteratorType b, size_t s)
        : ArrayRef(b, b + s) {
    }

    template <typename SequentialContainerType>
    ArrayRef(SequentialContainerType &ctr)
        : b(&*ctr.begin()), e(&*(ctr.end() - 1) + 1) {
    }

    template <size_t Size>
    ArrayRef(DataType (&array)[Size])
        : b(&array[0]), e(&array[Size]) {
    }

    ArrayRef() = default;

    size_t size() const {
        return e - b;
    }

    DataType &operator[](std::size_t idx) {
        return b[idx];
    }

    const DataType &operator[](std::size_t idx) const {
        return b[idx];
    }

    iterator begin() {
        return b;
    }

    const_iterator begin() const {
        return b;
    }

    iterator end() {
        return e;
    }

    const_iterator end() const {
        return e;
    }

    void swap(ArrayRef &rhs) {
        std::swap(b, rhs.b);
        std::swap(e, rhs.e);
    }

    operator ArrayRef<const DataType>() {
        return ArrayRef<const DataType>(b, e);
    }

  private:
    DataType *b = nullptr;
    DataType *e = nullptr;
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
