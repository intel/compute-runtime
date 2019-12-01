/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cinttypes>
#include <string>

constexpr size_t constLength(const char *string) {
    if (nullptr == string) {
        return 0U;
    }
    auto it = string;
    for (; *it != '\0'; ++it) {
    }
    return it - string;
}

class ConstStringRef {
  public:
    template <size_t Length>
    constexpr ConstStringRef(const char (&array)[Length]) noexcept
        : ptr(array), len(((Length > 0) && (array[Length - 1] == '\0')) ? (Length - 1) : Length) {
    }

    constexpr ConstStringRef(const char *const ptr, const size_t length) noexcept
        : ptr(ptr), len(length) {
    }

    ConstStringRef(const std::string &str) noexcept
        : ptr(str.data()), len(str.length()) {
    }

    constexpr const char *data() const noexcept {
        return ptr;
    }

    constexpr operator const char *() const noexcept {
        return ptr;
    }

    constexpr size_t size() const noexcept {
        return len;
    }

    constexpr size_t length() const noexcept {
        return len;
    }

    constexpr bool empty() const noexcept {
        return len == 0;
    }

    constexpr const char *begin() const noexcept {
        return ptr;
    }

    constexpr const char *end() const noexcept {
        return ptr + len;
    }

  protected:
    const char *const ptr;
    const size_t len;
};

constexpr bool equals(const ConstStringRef &lhs, const ConstStringRef &rhs) {
    if (lhs.size() != rhs.size()) {
        return false;
    }

    for (size_t i = 0, e = lhs.size(); i < e; ++i) {
        if (lhs[i] != rhs[i]) {
            return false;
        }
    }

    return true;
}

template <typename T = char>
constexpr bool equals(const ConstStringRef &lhs, const T *rhs) {
    return equals(lhs, ConstStringRef(rhs, constLength(rhs)));
}

inline bool equals(const ConstStringRef &lhs, const std::string &rhs) {
    return equals(lhs, ConstStringRef(rhs.data(), rhs.length()));
}

constexpr bool operator==(const ConstStringRef &lhs, const ConstStringRef &rhs) {
    return equals(lhs, rhs);
}

template <typename RhsT>
constexpr bool operator==(const ConstStringRef &lhs, const RhsT &rhs) {
    return equals(lhs, rhs);
}

template <typename LhsT>
constexpr bool operator==(const LhsT &lhs, const ConstStringRef &rhs) {
    return equals(rhs, lhs);
}

constexpr bool operator!=(const ConstStringRef &lhs, const ConstStringRef &rhs) {
    return false == equals(lhs, rhs);
}

template <typename RhsT>
constexpr bool operator!=(const ConstStringRef &lhs, const RhsT &rhs) {
    return false == (lhs == rhs);
}

template <typename LhsT>
constexpr bool operator!=(const LhsT &lhs, const ConstStringRef &rhs) {
    return rhs != lhs;
}
