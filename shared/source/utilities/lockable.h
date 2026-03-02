/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <mutex>
#include <utility>

namespace NEO {

template <typename ObjT>
class Lockable {
  public:
    using ThisT = Lockable<ObjT>;

    Lockable() = default;
    Lockable &operator=(Lockable &) = delete;

    template <typename... Args>
    Lockable(Args &&...args)
        : obj(std::forward<Args>(args)...) {
    }

    Lockable(ThisT &&rhs) {
        auto rhsLock = rhs.lock();
        this->obj = std::move(rhs.obj);
    }

    Lockable(const ThisT &rhs) {
        auto rhsLock = rhs.lock();
        this->obj = rhs.obj;
    }

    [[nodiscard]] std::unique_lock<std::mutex> lock() const {
        return std::unique_lock<std::mutex>(mutex);
    }

    ObjT *operator->() {
        return &obj;
    }

    const ObjT *operator->() const {
        return &obj;
    }

    ObjT &operator*() {
        return obj;
    }

    const ObjT &operator*() const {
        return obj;
    }

    void swap(ObjT &obj) {
        std::swap(this->obj, obj);
    }

  protected:
    mutable std::mutex mutex;
    ObjT obj = {};
};

} // namespace NEO
