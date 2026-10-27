/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/string.h"

#include <cstddef>
#include <cstring>
#include <utility>

namespace NEO {
namespace BuiltIn {

struct Resource {
    Resource() = default;

    Resource(const char *ptr, size_t size, bool persistentMemory)
        : size(size), persistentMemory(persistentMemory) {
        if (ptr == nullptr || size == 0) {
            return;
        }

        if (persistentMemory) {
            data = ptr;
        } else {
            auto copy = new char[size];
            memcpy_s(copy, size, ptr, size);
            data = copy;
        }
    }

    Resource(const Resource &rhs)
        : Resource(rhs.data, rhs.size, rhs.persistentMemory) {
    }

    Resource(Resource &&rhs) noexcept
        : size(rhs.size), data(rhs.data), persistentMemory(rhs.persistentMemory) {
        rhs.size = 0;
        rhs.data = nullptr;
        rhs.persistentMemory = false;
    }

    bool empty() const {
        return size == 0;
    }

    friend bool operator==(const Resource &lhs, const Resource &rhs) {
        if (lhs.size != rhs.size) {
            return false;
        }

        if (lhs.persistentMemory && rhs.persistentMemory) {
            return lhs.data == rhs.data;
        }

        if (lhs.size == 0) {
            return true;
        }

        UNRECOVERABLE_IF(lhs.data == nullptr || rhs.data == nullptr);

        return std::memcmp(lhs.data, rhs.data, lhs.size) == 0;
    }

    Resource &operator=(const Resource &rhs) {
        if (this == &rhs) {
            return *this;
        }

        Resource copy(rhs);
        *this = std::move(copy);
        return *this;
    }

    Resource &operator=(Resource &&rhs) noexcept {
        if (this == &rhs) {
            return *this;
        }

        if (!persistentMemory) {
            delete[] data;
        }

        size = rhs.size;
        data = rhs.data;
        persistentMemory = rhs.persistentMemory;

        rhs.size = 0;
        rhs.data = nullptr;
        rhs.persistentMemory = false;
        return *this;
    }

    ~Resource() {
        if (!persistentMemory) {
            delete[] data;
        }
    }

    size_t size = 0;
    const char *data = nullptr;
    bool persistentMemory = false;
};

Resource createResource(const char *ptr, size_t size, bool persistentSrcMemory);
Resource createResource(const Resource &r);
inline Resource createResource(const char *ptr, size_t size) {
    return createResource(ptr, size, false);
};

} // namespace BuiltIn
} // namespace NEO
