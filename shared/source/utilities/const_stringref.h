/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cinttypes>
#include <cstddef>
#include <cstring>
#include <string>

namespace NEO {

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
    constexpr ConstStringRef() {
    }

    constexpr ConstStringRef(const ConstStringRef &rhs) : ptr(rhs.ptr), len(rhs.len) {
    }

    ConstStringRef &operator=(const ConstStringRef &rhs) {
        this->ptr = rhs.ptr;
        this->len = rhs.len;
        return *this;
    }

    constexpr ConstStringRef(const char &c) noexcept
        : ptr(&c), len(1) {
    }

    constexpr ConstStringRef(const char *const ptr) noexcept
        : ptr(ptr), len(constLength(ptr)) {
    }

    constexpr ConstStringRef(const char *const ptr, const size_t length) noexcept
        : ptr(ptr), len(length) {
    }

    template <size_t Length>
    static constexpr ConstStringRef fromArray(const char (&array)[Length]) noexcept {
        return ConstStringRef(array, Length);
    }

    ConstStringRef(const std::string &str) noexcept
        : ptr(str.data()), len(str.length()) {
    }

    template <typename SizeT>
    constexpr ConstStringRef substr(SizeT offset, SizeT len) const noexcept {
        if (len >= 0) {
            return ConstStringRef(this->ptr + offset, len);
        } else {
            return ConstStringRef(this->ptr + offset, this->len + len - offset);
        }
    }

    template <typename SizeT>
    constexpr ConstStringRef substr(SizeT offset) const noexcept {
        return ConstStringRef(this->ptr + offset, this->len - offset);
    }

    constexpr ConstStringRef truncated(int len) const noexcept {
        if (len >= 0) {
            return ConstStringRef(this->ptr, len);
        } else {
            return ConstStringRef(this->ptr, this->len + len);
        }
    }

    constexpr const char *data() const noexcept {
        return ptr;
    }

    constexpr char operator[](size_t pos) const noexcept {
        return ptr[pos];
    }

    constexpr char operator[](int pos) const noexcept {
        return ptr[pos];
    }

    explicit operator std::string() const {
        return str();
    }

    std::string str() const {
        return std::string(ptr, len);
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

    constexpr bool contains(const char *subString) const noexcept {
        const char *findBeg = ptr;
        const char *findEnd = ptr + len;
        while (findBeg != findEnd) {
            const char *lhs = findBeg;
            const char *rhs = subString;
            while ((lhs < findEnd) && (*lhs == *rhs) && ('\0' != *rhs)) {
                ++lhs;
                ++rhs;
            }
            if ('\0' == *rhs) {
                return true;
            }
            ++findBeg;
        }
        return false;
    }

    constexpr bool containsCaseInsensitive(const char *subString) const noexcept {
        const char *findBeg = ptr;
        const char *findEnd = ptr + len;
        while (findBeg != findEnd) {
            const char *lhs = findBeg;
            const char *rhs = subString;
            while ((lhs < findEnd) && (std::tolower(*lhs) == std::tolower(*rhs)) && ('\0' != *rhs)) {
                ++lhs;
                ++rhs;
            }
            if ('\0' == *rhs) {
                return true;
            }
            ++findBeg;
        }
        return false;
    }

    constexpr bool startsWith(const char *subString) const noexcept {
        const char *findEnd = ptr + len;
        const char *lhs = ptr;
        const char *rhs = subString;
        while ((lhs < findEnd) && (*lhs == *rhs) && ('\0' != *rhs)) {
            ++lhs;
            ++rhs;
        }
        return ('\0' == *rhs);
    }

    constexpr bool startsWith(ConstStringRef subString) const noexcept {
        if (subString.length() > len) {
            return false;
        }
        const char *findEnd = ptr + subString.length();
        const char *lhs = ptr;
        const char *rhs = subString.begin();
        while ((lhs < findEnd)) {
            if (*lhs != *rhs) {
                return false;
            }
            lhs++;
            rhs++;
        }
        return true;
    }

    constexpr bool isEqualWithoutSeparator(const char separator, const char *subString) const noexcept {
        const char *end = ptr + len;
        const char *lhs = ptr;
        const char *rhs = subString;

        for (auto i = lhs; i != end; i++) {
            if (*i == separator) {
                continue;
            }
            if (*i != *rhs)
                return false;
            ++rhs;
        }
        return ('\0' == *rhs);
    }

  protected:
    ConstStringRef(std::nullptr_t) = delete;

    const char *ptr = nullptr;
    size_t len = 0U;
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

constexpr bool equals(const ConstStringRef &lhs, const char *rhs) {
    size_t i = 0;
    for (size_t e = lhs.size(); i < e; ++i) {
        if (lhs[i] != rhs[i]) {
            return false;
        }
        if ((rhs[i] == '\0') && (i + 1 < e)) {
            return false;
        }
    }

    return (rhs[i] == '\0');
}

constexpr bool operator==(const ConstStringRef &lhs, const ConstStringRef &rhs) {
    return equals(lhs, rhs);
}

constexpr bool operator==(const ConstStringRef &lhs, const char *rhs) {
    return equals(lhs, rhs);
}

constexpr bool operator==(const char *lhs, const ConstStringRef &rhs) {
    return equals(rhs, lhs);
}

constexpr bool operator!=(const ConstStringRef &lhs, const ConstStringRef &rhs) {
    return false == equals(lhs, rhs);
}

constexpr bool operator!=(const ConstStringRef &lhs, const char *rhs) {
    return false == equals(lhs, rhs);
}

constexpr bool operator!=(const char *lhs, const ConstStringRef &rhs) {
    return false == equals(rhs, lhs);
}

constexpr bool equalsCaseInsensitive(const ConstStringRef &lhs, const ConstStringRef &rhs) {
    if (lhs.size() != rhs.size()) {
        return false;
    }

    constexpr auto caseDiff = 'a' - 'A';
    for (size_t i = 0, e = lhs.size(); i < e; ++i) {

        if ((lhs[i] != rhs[i]) && (lhs[i] + caseDiff != rhs[i]) && (lhs[i] != rhs[i] + caseDiff)) {
            return false;
        }
    }

    return true;
}

} // namespace NEO
