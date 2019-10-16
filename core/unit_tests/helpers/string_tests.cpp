/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/helpers/hash.h"
#include "core/helpers/string.h"

#include "gtest/gtest.h"

#if defined(__linux__)

TEST(StringHelpers, strncpy) {
    char dst[1024] = "";
    char src[1024] = "HelloWorld";

    //preconditions
    ASSERT_EQ(sizeof(dst), sizeof(src));
    //String must be smaller than array capacity
    ASSERT_LT(strlen(src), sizeof(src));

    auto ret = strncpy_s(nullptr, 1024, src, 1024);
    EXPECT_EQ(ret, -EINVAL);

    ret = strncpy_s(dst, 1024, nullptr, 1024);
    EXPECT_EQ(ret, -EINVAL);

    ret = strncpy_s(dst, 512, src, 1024);
    EXPECT_EQ(ret, -ERANGE);

    memset(dst, 0, sizeof(dst));
    ret = strncpy_s(dst, 1024, src, strlen(src) / 2);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(0, memcmp(dst, src, strlen(src) / 2));
    for (size_t i = strlen(src) / 2; i < sizeof(dst); i++)
        EXPECT_EQ(0, dst[i]);

    memset(dst, 0, sizeof(dst));
    ret = strncpy_s(dst, strlen(src) / 2, src, strlen(src) / 2);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(0, memcmp(dst, src, strlen(src) / 2));
    for (size_t i = strlen(src) / 2; i < sizeof(dst); i++)
        EXPECT_EQ(0, dst[i]);

    strncpy_s(dst, 1024, src, 1024);
    EXPECT_EQ(0, memcmp(dst, src, strlen(src)));
    for (size_t i = strlen(src); i < sizeof(dst); i++)
        EXPECT_EQ(0, dst[i]);
}

TEST(StringHelpers, memmove) {
    char dst[1024] = "";
    char src[1024] = "HelloWorld";

    ASSERT_EQ(sizeof(dst), sizeof(src));

    auto ret = memmove_s(nullptr, sizeof(dst), src, sizeof(src));
    EXPECT_EQ(ret, -EINVAL);

    ret = memmove_s(dst, sizeof(dst), nullptr, sizeof(src));
    EXPECT_EQ(ret, -EINVAL);

    ret = memmove_s(dst, sizeof(src) / 2, src, sizeof(src));
    EXPECT_EQ(ret, -ERANGE);

    memset(dst, 0, sizeof(dst));
    ret = memmove_s(dst, sizeof(dst), src, sizeof(src));
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(0, memcmp(dst, src, sizeof(dst)));
}

TEST(StringHelpers, strcpy) {
    char dst[1024] = "";
    char src[1024] = "HelloWorld";

    ASSERT_EQ(sizeof(dst), sizeof(src));

    auto ret = strcpy_s(nullptr, 0, src);
    EXPECT_EQ(ret, -EINVAL);

    ret = strcpy_s(nullptr, sizeof(dst), src);
    EXPECT_EQ(ret, -EINVAL);

    ret = strcpy_s(nullptr, 0, nullptr);
    EXPECT_EQ(ret, -EINVAL);

    ret = strcpy_s(nullptr, sizeof(dst), nullptr);
    EXPECT_EQ(ret, -EINVAL);

    ret = strcpy_s(dst, 0, nullptr);
    EXPECT_EQ(ret, -EINVAL);

    ret = strcpy_s(dst, strlen(src) / 2, src);
    EXPECT_EQ(ret, -ERANGE);

    ret = strcpy_s(dst, strlen(src), src);
    EXPECT_EQ(ret, -ERANGE);

    char pattern = 0x5a;
    memset(dst, pattern, sizeof(dst));
    ret = strcpy_s(dst, sizeof(dst), src);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(0, memcmp(dst, src, strlen(src)));
    EXPECT_EQ(0, dst[strlen(src)]);
    for (size_t i = strlen(src) + 1; i < sizeof(dst); i++)
        EXPECT_EQ(pattern, dst[i]);
}

TEST(StringHelpers, strnlen) {
    char src[1024] = "HelloWorld";

    auto ret = strnlen_s(nullptr, sizeof(src));
    EXPECT_EQ(ret, 0u);

    ret = strnlen_s(src, 0);
    EXPECT_EQ(ret, 0u);

    ret = strnlen_s(src, sizeof(src));
    EXPECT_EQ(ret, strlen(src));
}

TEST(StringHelpers, memcpy) {
    char dst[1024] = "";
    char src[1024] = "HelloWorld";

    //preconditions
    ASSERT_EQ(sizeof(dst), sizeof(src));
    //String must be smaller than array capacity
    ASSERT_LT(strlen(src), sizeof(src));

    auto ret = memcpy_s(nullptr, sizeof(dst), src, sizeof(src));
    EXPECT_EQ(ret, -EINVAL);

    ret = memcpy_s(dst, sizeof(dst), nullptr, sizeof(src));
    EXPECT_EQ(ret, -EINVAL);

    ret = memcpy_s(dst, sizeof(dst) / 2, src, sizeof(src));
    EXPECT_EQ(ret, -ERANGE);

    memset(dst, 0, sizeof(dst));
    ret = memcpy_s(dst, sizeof(dst), src, strlen(src) / 2);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(0, memcmp(dst, src, strlen(src) / 2));
    for (size_t i = strlen(src) / 2; i < sizeof(dst); i++)
        EXPECT_EQ(0, dst[i]);
}

#endif
