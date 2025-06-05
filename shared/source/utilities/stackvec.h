/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/debug_helpers.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <tuple>
#include <vector>

template <size_t onStackCapacity>
struct StackVecSize {
    static constexpr size_t max32 = std::numeric_limits<uint32_t>::max();
    static constexpr size_t max16 = std::numeric_limits<uint16_t>::max();
    static constexpr size_t max8 = std::numeric_limits<uint8_t>::max();

    using SizeT = std::conditional_t<(onStackCapacity < max8), uint8_t,
                                     std::conditional_t<(onStackCapacity < max16), uint16_t,
                                                        std::conditional_t<(onStackCapacity < max32), uint32_t, size_t>>>;
};

template <typename DataType, size_t onStackCapacity,
          typename StackSizeT = typename StackVecSize<onStackCapacity>::SizeT>
class StackVec { // NOLINT(clang-analyzer-optin.performance.Padding)
  public:
    using value_type = DataType; // NOLINT(readability-identifier-naming)
    using SizeT = StackSizeT;
    using iterator = DataType *;                                          // NOLINT(readability-identifier-naming)
    using const_iterator = const DataType *;                              // NOLINT(readability-identifier-naming)
    using reverse_iterator = std::reverse_iterator<iterator>;             // NOLINT(readability-identifier-naming)
    using const_reverse_iterator = std::reverse_iterator<const_iterator>; // NOLINT(readability-identifier-naming)

    static constexpr SizeT onStackCaps = onStackCapacity;

    StackVec() {
        switchToStackMem();
    }

    template <typename ItType>
    StackVec(ItType beginIt, ItType endIt) {
        switchToStackMem();
        size_t count = (endIt - beginIt);
        if (count > onStackCapacity) {
            dynamicMem = new std::vector<DataType>(beginIt, endIt);
            return;
        }

        while (beginIt != endIt) {
            push_back(*beginIt);
            ++beginIt;
        }
        onStackSize = static_cast<SizeT>(count);
    }

    StackVec(const StackVec &rhs) {
        switchToStackMem();
        if (onStackCaps < rhs.size()) {
            dynamicMem = new std::vector<DataType>(rhs.begin(), rhs.end());
            return;
        }

        for (const auto &v : rhs) {
            push_back(v);
        }
    }

    explicit StackVec(size_t initialSize)
        : StackVec() {
        switchToStackMem();
        resize(initialSize);
    }

    StackVec(std::initializer_list<DataType> init) {
        switchToStackMem();
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
            return *this;
        }

        for (const auto &v : rhs) {
            push_back(v);
        }

        return *this;
    }

    StackVec(StackVec &&rhs) noexcept {
        onStackMem = reinterpret_cast<DataType *const>(onStackMemRawBytes);
        if (rhs.usesDynamicMem()) {
            this->dynamicMem = rhs.dynamicMem;
            rhs.switchToStackMem();
            return;
        }

        for (const auto &v : rhs) {
            push_back(v);
        }
        rhs.clear();
    }

    StackVec &operator=(StackVec &&rhs) noexcept {
        if (this == &rhs) {
            return *this;
        }

        clear();

        if (rhs.usesDynamicMem()) {
            if (usesDynamicMem()) {
                delete this->dynamicMem;
            }
            this->dynamicMem = rhs.dynamicMem;
            rhs.switchToStackMem();
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

    template <typename T>
    constexpr iterator insert(const_iterator pos, T &&value) {
        auto offset = pos - begin();
        push_back(std::forward<T>(value));
        std::rotate(begin() + offset, end() - 1, end());
        return begin() + offset;
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
        return onStackCapacity;
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
        isDynamicMemNeeded();

        if (usesDynamicMem()) {
            dynamicMem->push_back(v);
            return;
        }

        new (reinterpret_cast<DataType *>(onStackMemRawBytes) + onStackSize) DataType(v);
        ++onStackSize;
    }

    void push_back(DataType &&v) { // NOLINT(readability-identifier-naming)
        isDynamicMemNeeded();

        if (usesDynamicMem()) {
            dynamicMem->push_back(std::move(v));
            return;
        }

        new (reinterpret_cast<DataType *>(onStackMemRawBytes) + onStackSize) DataType(std::move(v));
        ++onStackSize;
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
        return reinterpret_cast<uintptr_t>(this->onStackMem) != reinterpret_cast<uintptr_t>(onStackMemRawBytes) && this->dynamicMem;
    }

    DataType *data() {
        if (usesDynamicMem()) {
            return dynamicMem->data();
        }
        return reinterpret_cast<DataType *>(onStackMemRawBytes);
    }

    const DataType *data() const {
        if (usesDynamicMem()) {
            return dynamicMem->data();
        }
        return reinterpret_cast<const DataType *>(onStackMemRawBytes);
    }

  private:
    template <typename RhsDataType, size_t rhsOnStackCapacity, typename RhsStackSizeT>
    friend class StackVec;

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

        auto currentSize = std::min(onStackSize, onStackCaps);
        if (newSize <= currentSize) {
            // trim elements
            clearStackObjects(newSize, currentSize - newSize);
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

    void isDynamicMemNeeded() {
        if (onStackSize == onStackCaps) {
            ensureDynamicMem();
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
    void switchToStackMem() {
        onStackMem = reinterpret_cast<DataType *const>(onStackMemRawBytes);
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

template <typename T, size_t lhsStackCaps, size_t rhsStackCaps>
bool operator==(const StackVec<T, lhsStackCaps> &lhs,
                const StackVec<T, rhsStackCaps> &rhs) {
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

template <typename T, size_t lhsStackCaps, size_t rhsStackCaps>
bool operator!=(const StackVec<T, lhsStackCaps> &lhs,
                const StackVec<T, rhsStackCaps> &rhs) {
    return false == (lhs == rhs);
}

constexpr size_t maxRootDeviceIndices = 16;
class RootDeviceIndicesContainer : protected StackVec<uint32_t, maxRootDeviceIndices> {
  public:
    using StackVec<uint32_t, maxRootDeviceIndices>::StackVec;
    using StackVec<uint32_t, maxRootDeviceIndices>::at;
    using StackVec<uint32_t, maxRootDeviceIndices>::begin;
    using StackVec<uint32_t, maxRootDeviceIndices>::end;
    using StackVec<uint32_t, maxRootDeviceIndices>::size;
    using StackVec<uint32_t, maxRootDeviceIndices>::operator[];

    inline void pushUnique(uint32_t rootDeviceIndex) {
        if (indexPresent.size() <= rootDeviceIndex) {
            indexPresent.resize(rootDeviceIndex + 1);
        }
        if (!indexPresent[rootDeviceIndex]) {
            push_back(rootDeviceIndex);
            indexPresent[rootDeviceIndex] = 1;
        }
    }

  protected:
    StackVec<int8_t, maxRootDeviceIndices> indexPresent;
};
using RootDeviceIndicesMap = StackVec<std::tuple<uint32_t, uint32_t>, maxRootDeviceIndices>;
