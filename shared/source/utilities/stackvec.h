/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/debug_helpers.h"

#include <algorithm>
#include <cinttypes>
#include <cstddef>
#include <iterator>
#include <limits>
#include <type_traits>
#include <utility>
#include <vector>

template <size_t OnStackCapacity>
struct StackVecSize {
    static constexpr size_t max32 = std::numeric_limits<uint32_t>::max();
    static constexpr size_t max16 = std::numeric_limits<uint16_t>::max();
    static constexpr size_t max8 = std::numeric_limits<uint8_t>::max();

    using SizeT = std::conditional_t<(OnStackCapacity < max8), uint8_t,
                                     std::conditional_t<(OnStackCapacity < max16), uint16_t,
                                                        std::conditional_t<(OnStackCapacity < max32), uint32_t, size_t>>>;
};

template <typename DataType, size_t OnStackCapacity,
          typename StackSizeT = typename StackVecSize<OnStackCapacity>::SizeT>
class StackVec { // NOLINT(clang-analyzer-optin.performance.Padding)
  public:
    using value_type = DataType;
    using SizeT = StackSizeT;
    using iterator = DataType *;
    using const_iterator = const DataType *;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    static constexpr SizeT onStackCaps = OnStackCapacity;

    StackVec() {
        onStackMem = reinterpret_cast<DataType *const>(onStackMemRawBytes);
    }

    template <typename ItType>
    StackVec(ItType beginIt, ItType endIt) {
        onStackMem = reinterpret_cast<DataType *const>(onStackMemRawBytes);
        size_t count = (endIt - beginIt);
        if (count > OnStackCapacity) {
            dynamicMem = new std::vector<DataType>(beginIt, endIt);
            setUsesDynamicMem();
            return;
        }

        while (beginIt != endIt) {
            push_back(*beginIt);
            ++beginIt;
        }
        onStackSize = static_cast<SizeT>(count);
    }

    StackVec(const StackVec &rhs) {
        onStackMem = reinterpret_cast<DataType *const>(onStackMemRawBytes);
        if (onStackCaps < rhs.size()) {
            dynamicMem = new std::vector<DataType>(rhs.begin(), rhs.end());
            setUsesDynamicMem();
            return;
        }

        for (const auto &v : rhs) {
            push_back(v);
        }
    }

    explicit StackVec(size_t initialSize)
        : StackVec() {
        onStackMem = reinterpret_cast<DataType *const>(onStackMemRawBytes);
        resize(initialSize);
    }

    StackVec(std::initializer_list<DataType> init) {
        onStackMem = reinterpret_cast<DataType *const>(onStackMemRawBytes);
        reserve(init.size());
        for (const auto &obj : init) {
            push_back(obj);
        }
    }

    StackVec &operator=(const StackVec &rhs) {
        if (this == &rhs) {
            return *this;
        }
        clear();

        if (usesDynamicMem()) {
            this->dynamicMem->assign(rhs.begin(), rhs.end());
            return *this;
        }

        if (onStackCaps < rhs.size()) {
            this->dynamicMem = new std::vector<DataType>(rhs.begin(), rhs.end());
            setUsesDynamicMem();
            return *this;
        }

        for (const auto &v : rhs) {
            push_back(v);
        }

        return *this;
    }

    StackVec(StackVec &&rhs) {
        onStackMem = reinterpret_cast<DataType *const>(onStackMemRawBytes);
        if (rhs.usesDynamicMem()) {
            this->dynamicMem = rhs.dynamicMem;
            setUsesDynamicMem();
            rhs.onStackSize = 0U;
            return;
        }

        for (const auto &v : rhs) {
            push_back(v);
        }
        rhs.clear();
    }

    StackVec &operator=(StackVec &&rhs) {
        if (this == &rhs) {
            return *this;
        }

        clear();

        if (rhs.usesDynamicMem()) {
            if (usesDynamicMem()) {
                delete this->dynamicMem;
            }
            this->dynamicMem = rhs.dynamicMem;
            this->setUsesDynamicMem();
            rhs.onStackSize = 0U;
            return *this;
        }

        if (usesDynamicMem()) {
            this->dynamicMem->assign(rhs.begin(), rhs.end());
            return *this;
        }

        for (const auto &v : rhs) {
            push_back(v);
        }
        rhs.clear();

        return *this;
    }

    template <typename RhsT>
    void swap(RhsT &rhs) {
        if (this->usesDynamicMem() && rhs.usesDynamicMem()) {
            this->dynamicMem->swap(*rhs.dynamicMem);
            return;
        }
        size_t smallerSize = this->size() < rhs.size() ? this->size() : rhs.size();
        size_t i = 0;
        for (; i < smallerSize; ++i) {
            std::swap((*this)[i], rhs[i]);
        }
        if (this->size() == smallerSize) {
            auto biggerSize = rhs.size();
            for (; i < biggerSize; ++i) {
                this->push_back(std::move(rhs[i]));
            }
            rhs.resize(smallerSize);
        } else {
            auto biggerSize = this->size();
            for (; i < biggerSize; ++i) {
                rhs.push_back(std::move((*this)[i]));
            }
            this->resize(smallerSize);
        }
    }

    ~StackVec() {
        if (usesDynamicMem()) {
            delete dynamicMem;
            return;
        }
        clearStackObjects();
    }

    size_t size() const {
        if (usesDynamicMem()) {
            return dynamicMem->size();
        }
        return onStackSize;
    }

    bool empty() const {
        return 0U == size();
    }

    size_t capacity() const {
        if (usesDynamicMem()) {
            return dynamicMem->capacity();
        }
        return OnStackCapacity;
    }

    void reserve(size_t newCapacity) {
        if (newCapacity > onStackCaps) {
            ensureDynamicMem();
            dynamicMem->reserve(newCapacity);
        }
    }

    void clear() {
        if (usesDynamicMem()) {
            dynamicMem->clear();
            return;
        }
        clearStackObjects();
    }

    void push_back(const DataType &v) { // NOLINT(readability-identifier-naming)
        if (onStackSize == onStackCaps) {
            ensureDynamicMem();
        }

        if (usesDynamicMem()) {
            dynamicMem->push_back(v);
            return;
        }

        new (reinterpret_cast<DataType *>(onStackMemRawBytes) + onStackSize) DataType(v);
        ++onStackSize;
    }

    void sort() {
        std::sort(this->begin(), this->end());
    }

    void remove_duplicates() { // NOLINT(readability-identifier-naming)
        if (1 >= this->size()) {
            return;
        }
        this->sort();
        const auto last = std::unique(this->begin(), this->end());
        auto currentEnd = this->end();
        while (last != currentEnd--) {
            this->pop_back();
        }
    }

    void pop_back() { // NOLINT(readability-identifier-naming)
        if (usesDynamicMem()) {
            dynamicMem->pop_back();
            return;
        }

        UNRECOVERABLE_IF(0 == onStackSize);

        clearStackObjects(onStackSize - 1, 1U);
        --onStackSize;
    }

    DataType &operator[](std::size_t idx) {
        if (usesDynamicMem()) {
            return (*dynamicMem)[idx];
        }
        return *(reinterpret_cast<DataType *>(onStackMemRawBytes) + idx);
    }

    DataType &at(std::size_t idx) { return this->operator[](idx); }

    const DataType &at(std::size_t idx) const { return this->operator[](idx); }

    const DataType &operator[](std::size_t idx) const {
        if (usesDynamicMem()) {
            return (*dynamicMem)[idx];
        }
        return *(reinterpret_cast<const DataType *>(onStackMemRawBytes) + idx);
    }

    iterator begin() {
        if (usesDynamicMem()) {
            return dynamicMem->data();
        }

        return reinterpret_cast<DataType *>(onStackMemRawBytes);
    }

    reverse_iterator rbegin() {
        return reverse_iterator(end());
    }

    const_reverse_iterator crbegin() const {
        return const_reverse_iterator(end());
    }

    const_iterator begin() const {
        if (usesDynamicMem()) {
            return dynamicMem->data();
        }

        return reinterpret_cast<const DataType *>(onStackMemRawBytes);
    }

    iterator end() {
        if (usesDynamicMem()) {
            return dynamicMem->data() + dynamicMem->size();
        }

        return reinterpret_cast<DataType *>(onStackMemRawBytes) + onStackSize;
    }

    reverse_iterator rend() {
        return reverse_iterator(begin());
    }

    const_reverse_iterator crend() const {
        return const_reverse_iterator(begin());
    }

    const_iterator end() const {
        if (usesDynamicMem()) {
            return dynamicMem->data() + dynamicMem->size();
        }

        return reinterpret_cast<const DataType *>(onStackMemRawBytes) + onStackSize;
    }

    void resize(size_t newSize) {
        this->resizeImpl(newSize, nullptr);
    }

    void resize(size_t newSize, const DataType &value) {
        resizeImpl(newSize, &value);
    }

    bool usesDynamicMem() const {
        return std::numeric_limits<decltype(onStackSize)>::max() == this->onStackSize;
    }

    auto data() {
        if (usesDynamicMem()) {
            return dynamicMem->data();
        }
        return reinterpret_cast<DataType *>(onStackMemRawBytes);
    }

  private:
    template <typename RhsDataType, size_t RhsOnStackCapacity, typename RhsStackSizeT>
    friend class StackVec;
    void setUsesDynamicMem() {
        this->onStackSize = std::numeric_limits<decltype(onStackSize)>::max();
    }

    void resizeImpl(size_t newSize, const DataType *value) {
        // new size does not fit into internal mem
        if (newSize > onStackCaps) {
            ensureDynamicMem();
        }

        // memory already backed by stl vector
        if (usesDynamicMem()) {
            if (value != nullptr) {
                dynamicMem->resize(newSize, *value);
            } else {
                dynamicMem->resize(newSize);
            }
            return;
        }

        if (newSize <= onStackSize) {
            // trim elements
            clearStackObjects(newSize, onStackSize - newSize);
            onStackSize = static_cast<SizeT>(newSize);
            return;
        }

        // add new elements
        if (value != nullptr) {
            // copy-construct elements
            while (onStackSize < newSize) {
                new (reinterpret_cast<DataType *>(onStackMemRawBytes) + onStackSize) DataType(*value);
                ++onStackSize;
            }
        } else {
            // default-construct elements
            while (onStackSize < newSize) {
                new (reinterpret_cast<DataType *>(onStackMemRawBytes) + onStackSize) DataType();
                ++onStackSize;
            }
        }
    }

    void ensureDynamicMem() {
        if (usesDynamicMem()) {
            return;
        }
        dynamicMem = new std::vector<DataType>();
        if (onStackSize > 0) {
            dynamicMem->reserve(onStackSize);
            for (auto it = reinterpret_cast<DataType *>(onStackMemRawBytes), end = reinterpret_cast<DataType *>(onStackMemRawBytes) + onStackSize; it != end; ++it) {
                dynamicMem->push_back(std::move(*it));
            }
            clearStackObjects();
        }
        setUsesDynamicMem();
    }

    void clearStackObjects() {
        clearStackObjects(0, onStackSize);
        onStackSize = 0;
    }

    void clearStackObjects(size_t offset, size_t count) {
        UNRECOVERABLE_IF(offset + count > onStackSize);
        for (auto it = reinterpret_cast<DataType *>(onStackMemRawBytes) + offset, end = reinterpret_cast<DataType *>(onStackMemRawBytes) + offset + count; it != end; ++it) {
            it->~DataType();
        }
    }

    union {
        std::vector<DataType> *dynamicMem;
        DataType *onStackMem;
    };

    alignas(alignof(DataType)) char onStackMemRawBytes[sizeof(DataType[onStackCaps])];
    SizeT onStackSize = 0U;
};

namespace {
static_assert(sizeof(StackVec<char, 1U>::SizeT) == 1u, "");
static_assert(sizeof(StackVec<char, 7U>) <= 16u, "");
static_assert(sizeof(StackVec<uint32_t, 3U>) <= 24u, "");
} // namespace

template <typename T, size_t LhsStackCaps, size_t RhsStackCaps>
bool operator==(const StackVec<T, LhsStackCaps> &lhs,
                const StackVec<T, RhsStackCaps> &rhs) {
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

template <typename T, size_t LhsStackCaps, size_t RhsStackCaps>
bool operator!=(const StackVec<T, LhsStackCaps> &lhs,
                const StackVec<T, RhsStackCaps> &rhs) {
    return false == (lhs == rhs);
}

using RootDeviceIndicesContainer = StackVec<uint32_t, 16>;
