/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <memory>
#include <type_traits>

#if defined(__linux__)

#include <cstring>
#include <errno.h>
#include <string>

inline int strcpy_s(char *dst, size_t dstSize, const char *src) { // NOLINT(readability-identifier-naming)
    if ((dst == nullptr) || (src == nullptr)) {
        return -EINVAL;
    }
    size_t length = strlen(src);
    if (dstSize <= length) {
        return -ERANGE;
    }

    memcpy(dst, src, length);
    dst[length] = '\0';

    return 0;
}

inline size_t strnlen_s(const char *str, size_t count) { // NOLINT(readability-identifier-naming)
    if (str == nullptr) {
        return 0;
    }

    for (size_t i = 0; i < count; ++i) {
        if (str[i] == '\0')
            return i;
    }

    return count;
}

inline int strncpy_s(char *dst, size_t numberOfElements, const char *src, size_t count) { // NOLINT(readability-identifier-naming)
    if ((dst == nullptr) || (src == nullptr)) {
        return -EINVAL;
    }

    size_t length = strnlen_s(src, count);
    if (numberOfElements <= count && numberOfElements <= length) {
        return -ERANGE;
    }

    memcpy(dst, src, length);
    dst[length] = '\0';

    return 0;
}

inline int memcpy_s(void *dst, size_t destSize, const void *src, size_t count) { // NOLINT(readability-identifier-naming)
    if ((dst == nullptr) || (src == nullptr)) {
        return -EINVAL;
    }
    if (destSize < count) {
        return -ERANGE;
    }

    memcpy(dst, src, count);

    return 0;
}

inline int memmove_s(void *dst, size_t numberOfElements, const void *src, size_t count) { // NOLINT(readability-identifier-naming)
    if ((dst == nullptr) || (src == nullptr)) {
        return -EINVAL;
    }
    if (numberOfElements < count) {
        return -ERANGE;
    }

    memmove(dst, src, count);

    return 0;
}

template <typename... Args>
inline int snprintf_s(char *buffer, size_t sizeOfBuffer, size_t count, const char *format, Args &&...args) { // NOLINT(readability-identifier-naming)
    if ((buffer == nullptr) || (format == nullptr)) {
        return -EINVAL;
    }

    return snprintf(buffer, sizeOfBuffer, format, std::forward<Args>(args)...);
}

#endif

#if defined(_WIN32)

template <typename... Args>
inline int snprintf_s(char *buffer, size_t sizeOfBuffer, size_t count, const char *format, Args &&...args) { // NOLINT(readability-identifier-naming)
    if ((buffer == nullptr) || (format == nullptr)) {
        return -EINVAL;
    }

    return _snprintf_s(buffer, sizeOfBuffer, count, format, std::forward<Args>(args)...);
}

#endif

template <typename T = char>
inline std::unique_ptr<T[]> makeCopy(const void *src, size_t size) {
    if (size == 0) {
        return nullptr;
    }

    static_assert(sizeof(T) == 1u && std::is_trivially_copyable_v<T>);

    auto copiedData = std::make_unique<T[]>(size);
    memcpy_s(copiedData.get(), size, src, size);

    return copiedData;
}
