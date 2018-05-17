/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#pragma once
#if defined(__linux__)

#include <errno.h>
#include <cstring>
#include <string>

inline int strcpy_s(char *dst, size_t dstSize, const char *src) {
    if ((dst == nullptr) || (src == nullptr)) {
        return -EINVAL;
    }
    size_t length = strlen(src);
    if (dstSize <= length) {
        return -ERANGE;
    }

    strncpy(dst, src, length);
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
    strncpy(dst, src, length);

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
