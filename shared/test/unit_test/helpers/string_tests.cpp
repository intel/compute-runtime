/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hash.h"
#include "shared/source/helpers/string.h"

#include "gtest/gtest.h"

#if defined(__linux__)

TEST(StringHelpers, GivenParamsWhenUsingStrncpyThenReturnIsCorrect) {
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

TEST(StringHelpers, GivenParamsWhenUsingMemmoveThenReturnIsCorrect) {
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

TEST(StringHelpers, GivenParamsWhenUsingStrcpyThenReturnIsCorrect) {
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

TEST(StringHelpers, GivenParamsWhenUsingStrnlenThenReturnIsCorrect) {
    char src[1024] = "HelloWorld";

    auto ret = strnlen_s(nullptr, sizeof(src));
    EXPECT_EQ(ret, 0u);

    ret = strnlen_s(src, 0);
    EXPECT_EQ(ret, 0u);

    ret = strnlen_s(src, sizeof(src));
    EXPECT_EQ(ret, strlen(src));
}

TEST(StringHelpers, GivenParamsWhenUsingMemcpyThenReturnIsCorrect) {
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

TEST(StringHelpers, GivenParamsWhenUsingSnprintfsThenReturnIsCorrect) {
    char buffer[15] = "";
    const char *fmtStr = "World!";

    int retVal1 = snprintf_s(buffer, sizeof(buffer), sizeof(buffer), "Hello %s", fmtStr);
    ASSERT_EQ(12, retVal1);
    ASSERT_EQ(0, std::strcmp("Hello World!", buffer));

    int retVal2 = snprintf_s(nullptr, sizeof(buffer), sizeof(buffer), "Hello %s", fmtStr);
    ASSERT_EQ(-EINVAL, retVal2);

    int retVal3 = snprintf_s(buffer, sizeof(buffer), sizeof(buffer), nullptr, fmtStr);
    ASSERT_EQ(-EINVAL, retVal3);

    int retVal4 = snprintf_s(nullptr, sizeof(buffer), sizeof(buffer), nullptr, fmtStr);
    ASSERT_EQ(-EINVAL, retVal4);
}
