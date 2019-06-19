/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#if defined(__linux__)

#include <cstring>
#include <errno.h>
#include <string>

inline int strcpy_s(char *dst, size_t dstSize, const char *src) {
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

inline int strncpy_s(char *dst, size_t numberOfElements, const char *src, size_t count) {
    if ((dst == nullptr) || (src == nullptr)) {
        return -EINVAL;
    }
    if (numberOfElements < count) {
        return -ERANGE;
    }

    size_t length = strlen(src);
    if (length > count) {
        length = count;
    }
    memcpy(dst, src, length);

    if (length < numberOfElements) {
        numberOfElements = length;
    }
    dst[numberOfElements] = '\0';

    return 0;
}

inline size_t strnlen_s(const char *str, size_t count) {
    if (str == nullptr) {
        return 0;
    }

    for (size_t i = 0; i < count; ++i) {
        if (str[i] == '\0')
            return i;
    }

    return count;
}

inline int memcpy_s(void *dst, size_t destSize, const void *src, size_t count) {
    if ((dst == nullptr) || (src == nullptr)) {
        return -EINVAL;
    }
    if (destSize < count) {
        return -ERANGE;
    }

    memcpy(dst, src, count);

    return 0;
}

inline int memmove_s(void *dst, size_t numberOfElements, const void *src, size_t count) {
    if ((dst == nullptr) || (src == nullptr)) {
        return -EINVAL;
    }
    if (numberOfElements < count) {
        return -ERANGE;
    }

    memmove(dst, src, count);

    return 0;
}

#endif
