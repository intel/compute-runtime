/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <memory>

template <typename T>
using ExtUniquePtrT = std::unique_ptr<T, void (*)(T *)>;

template <typename T>
void cloneExt(ExtUniquePtrT<T> &dst, const T *src);

template <typename T>
void destroyExt(T *dst);

template <typename T>
struct Ext {
    Ext(T *ptr) : ptr(ptr, destroyExt) {}
    Ext() = default;
    Ext(const Ext &rhs) {
        cloneExt(this->ptr, rhs.ptr.get());
    }

    Ext &operator=(const Ext &rhs) {
        if (this == &rhs) {
            return *this;
        }
        cloneExt(this->ptr, rhs.ptr.get());
        return *this;
    }

    ExtUniquePtrT<T> ptr{nullptr, destroyExt};
};

template <typename T>
struct Clonable {
    Clonable(T *ptr) : ptr(ptr) {}
    Clonable() = default;
    Clonable(const Clonable &rhs) {
        if (rhs.ptr != nullptr) {
            this->ptr = std::make_unique<T>(*rhs.ptr);
        }
    }

    Clonable &operator=(const Clonable &rhs) {
        if (this == &rhs) {
            return *this;
        }
        if (rhs.ptr == nullptr) {
            ptr.reset();
            return *this;
        }

        this->ptr = std::make_unique<T>(*rhs.ptr);
        return *this;
    }

    std::unique_ptr<T> ptr;
};
