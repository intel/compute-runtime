/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstdint>
#include <cstring>
#include <regex>
#include <stddef.h>
#include <string>

#define EXPECT_EQ_VAL(a, b)      \
    {                            \
        auto evalA = (a);        \
        auto evalB = (b);        \
        EXPECT_EQ(evalA, evalB); \
    }

#define EXPECT_NE_VAL(a, b)      \
    {                            \
        auto evalA = (a);        \
        auto evalB = (b);        \
        EXPECT_NE(evalA, evalB); \
    }

#define EXPECT_GT_VAL(a, b)      \
    {                            \
        auto evalA = (a);        \
        auto evalB = (b);        \
        EXPECT_GT(evalA, evalB); \
    }

#define EXPECT_EQ_CONST(a, b)       \
    {                               \
        decltype(b) expected = (a); \
        EXPECT_EQ_VAL(expected, b); \
    }

inline bool memoryZeroed(const void *mem, const size_t size) {
    size_t sizeLeft = (size_t)size;
    bool memoryZeroed = true;
    while (sizeLeft--) {
        uint8_t *pMem = (uint8_t *)mem;
        if (pMem[sizeLeft] != 0) {
            memoryZeroed = false;
            break;
        }
    }
    return memoryZeroed;
}

inline bool memoryEqualsPointer(const void *mem, const uintptr_t expectedPointer) {
    auto ptrToExpectedPointer = &expectedPointer;
    return 0 == memcmp(mem, ptrToExpectedPointer, sizeof(uintptr_t));
}

inline bool hasSubstr(const std::string &str, const std::string &subStr) {
    return std::string::npos != str.find(subStr);
}

inline bool startsWith(const std::string &str, const std::string &subStr) {
    return 0 == str.find(subStr, 0);
}

inline bool endsWith(const std::string &str, const std::string &subStr) {
    if (subStr.size() > str.size()) {
        return false;
    }

    return 0 == str.compare(str.size() - subStr.size(), subStr.size(), subStr);
}

inline bool isEmpty(const std::string &str) {
    return str.empty();
}

inline bool containsRegex(const std::string &str, const std::string &regex) {
    return std::regex_search(str, std::regex(regex));
}
