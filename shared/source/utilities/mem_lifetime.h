/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <memory>

template <typename T>
using ExtUniquePtrT = std::unique_ptr<T, void (*)(T *)>;

template <typename T>
void cloneExt(ExtUniquePtrT<T> &dst, const T &src);

template <typename T>
void allocateExt(ExtUniquePtrT<T> &dst);

template <typename T>
void destroyExt(T *dst);

namespace Impl {

template <typename ParentT>
struct UniquePtrWrapperOps {
    auto operator*() const noexcept(noexcept(std::declval<decltype(ParentT::ptr)>().operator*())) {
        return *static_cast<const ParentT *>(this)->ptr;
    }

    auto operator->() const noexcept {
        return static_cast<const ParentT *>(this)->ptr.operator->();
    }

    explicit operator bool() const noexcept {
        return static_cast<bool>(static_cast<const ParentT *>(this)->ptr);
    }

    template <typename Rhs>
    friend bool operator==(const ParentT &lhs, const Rhs &rhs) {
        return lhs.ptr == rhs;
    }

    template <typename Lhs>
    friend bool operator==(const Lhs &lhs, const ParentT &rhs) {
        return lhs == rhs.ptr;
    }

    friend bool operator==(const ParentT &lhs, const ParentT &rhs) {
        return lhs.ptr == rhs.ptr;
    }
};

} // namespace Impl

template <typename T, bool allocateAtInit = false>
struct Ext : Impl::UniquePtrWrapperOps<Ext<T>> {
    Ext(T *ptr) : ptr(ptr, destroyExt) {}
    Ext() {
        if constexpr (allocateAtInit) {
            allocateExt(ptr);
        }
    }
    Ext(const Ext &rhs) {
        if (rhs.ptr.get()) {
            cloneExt(this->ptr, *rhs.ptr.get());
        }
    }

    Ext &operator=(const Ext &rhs) {
        if (this == &rhs) {
            return *this;
        }
        if (rhs.ptr.get()) {
            cloneExt(this->ptr, *rhs.ptr.get());
        } else {
            ptr.reset();
        }
        return *this;
    }

    ~Ext() = default;
    Ext(Ext &&rhs) noexcept = default;
    Ext &operator=(Ext &&rhs) noexcept = default;

    ExtUniquePtrT<T> ptr{nullptr, destroyExt};
};

template <typename T>
struct Clonable : Impl::UniquePtrWrapperOps<Clonable<T>> {
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

    ~Clonable() = default;
    Clonable(Clonable &&rhs) noexcept = default;
    Clonable &operator=(Clonable &&rhs) noexcept = default;

    std::unique_ptr<T> ptr;
};

template <typename T>
struct WeaklyShared {
    WeaklyShared() = default;
    ~WeaklyShared() {
        if (owned) {
            delete ptr;
        }
    }

    explicit WeaklyShared(T *ptr) : ptr(ptr), owned(true) {
    }

    WeaklyShared(const WeaklyShared &rhs) : ptr(rhs.get()) {
    }

    WeaklyShared(WeaklyShared &&rhs) : ptr(rhs.get()), owned(rhs.owned) {
        rhs.ptr = nullptr;
        rhs.owned = false;
    }

    WeaklyShared &operator=(const WeaklyShared &rhs) {
        if (this == &rhs) {
            return *this;
        }
        if (this->owned) {
            delete this->ptr;
        }
        this->ptr = rhs.ptr;
        this->owned = false;
        return *this;
    }

    WeaklyShared &operator=(WeaklyShared &&rhs) {
        if (this == &rhs) {
            return *this;
        }
        this->ptr = rhs.ptr;
        this->owned = rhs.owned;
        rhs.ptr = nullptr;
        rhs.owned = false;
        return *this;
    }

    WeaklyShared share() const noexcept {
        return WeaklyShared(ptr, false);
    }

    T *get() const noexcept {
        return ptr;
    }

    bool empty() const noexcept {
        return nullptr == ptr;
    }

    T &operator*() const noexcept {
        return *ptr;
    }

    T *operator->() const noexcept {
        return ptr;
    }

  protected:
    WeaklyShared(T *ptr, bool owned) : ptr(ptr), owned(owned) {
    }

    T *ptr = nullptr;
    bool owned = false;
};
