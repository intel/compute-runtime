/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/get_info.h"

#include "gtest/gtest.h"

TEST(getInfo, GivenSrcSizeLessThanOrEqualDstSizeWhenGettingInfoThenSrcCopiedToDst) {
    float dest = 0.0f;
    float src = 1.0f;

    auto retVal = getInfo(&dest, sizeof(dest), &src, sizeof(src));
    EXPECT_EQ(GetInfoStatus::SUCCESS, retVal);
    EXPECT_EQ(src, dest);
}

TEST(getInfo, GivenSrcSizeGreaterThanEqualDstSizeAndDstNullPtrWhenGettingInfoThenSrcNotCopiedToDstAndSuccessReturned) {
    float dest = 0.0f;
    float src = 1.0f;

    auto retVal = getInfo(nullptr, 0, &src, sizeof(src));
    EXPECT_EQ(GetInfoStatus::SUCCESS, retVal);
    EXPECT_NE(src, dest);
}

TEST(getInfo, GivenSrcSizeLessThanOrEqualDstSizeAndDstIsNullPtrWhenGettingInfoThenSuccessIsReturned) {
    float dest = 0.0f;
    float src = 1.0f;

    auto retVal = getInfo(nullptr, sizeof(dest), &src, sizeof(src));
    EXPECT_EQ(GetInfoStatus::SUCCESS, retVal);
    EXPECT_NE(src, dest);
}

TEST(getInfo, GivenSrcSizeLessThanOrEqualDstSizeAndDstIsNotNullPtrWhenGettingInfoThenInvalidValueIsReturned) {
    float dest = 0.0f;
    float src = 1.0f;

    auto retVal = getInfo(&dest, 0, &src, sizeof(src));
    EXPECT_EQ(GetInfoStatus::INVALID_VALUE, retVal);
    EXPECT_NE(src, dest);
}

TEST(getInfo, GivenNullSrcPtrWhenGettingInfoThenInvalidValueErrorIsReturned) {
    float dest = 0.0f;
    float src = 1.0f;

    auto retVal = getInfo(&dest, sizeof(dest), nullptr, sizeof(src));
    EXPECT_EQ(GetInfoStatus::INVALID_VALUE, retVal);
    EXPECT_NE(src, dest);
}

TEST(getInfo, GivenZeroSrcSizeWhenGettingInfoThenInvalidValueErrorIsReturned) {
    float dest = 0.0f;
    float src = 1.0f;

    auto retVal = getInfo(&dest, sizeof(dest), &src, 0);
    EXPECT_EQ(GetInfoStatus::INVALID_VALUE, retVal);
    EXPECT_NE(src, dest);
}

TEST(getInfoHelper, GivenInstanceOfGetInfoHelperAndNullPtrParamsSuccessIsReturned) {
    GetInfoStatus retVal;
    GetInfoHelper info(nullptr, 0, nullptr, &retVal);

    info.set(1);
    EXPECT_EQ(GetInfoStatus::SUCCESS, retVal);
}

TEST(getInfoHelper, GivenPointerWhenSettingValueThenValueIsSetCorrectly) {
    uint32_t *getValue = nullptr;
    uint32_t expectedValue = 1;

    GetInfoHelper::set(getValue, expectedValue);
    EXPECT_EQ(nullptr, getValue);

    getValue = new uint32_t(0);
    GetInfoHelper::set(getValue, expectedValue);
    ASSERT_NE(nullptr, getValue);
    EXPECT_EQ(*getValue, expectedValue);

    delete getValue;
}

TEST(errorCodeHelper, GivenLocalVariableWhenSettingValueThenValueIsSetCorrectly) {
    int errCode = 0;
    ErrorCodeHelper err(&errCode, 1);
    EXPECT_EQ(1, errCode);
    EXPECT_EQ(1, err.localErrcode);

    err.set(2);
    EXPECT_EQ(2, errCode);
    EXPECT_EQ(2, err.localErrcode);
}
