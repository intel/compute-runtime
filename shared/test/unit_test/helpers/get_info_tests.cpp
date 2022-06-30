/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/get_info.h"

#include "gtest/gtest.h"

TEST(getInfo, GivenSrcSizeLessThanOrEqualDstSizeWhenGettingInfoThenSrcCopiedToDst) {
    float dest = 0.0f;
    float src = 1.0f;

    auto retVal = GetInfo::getInfo(&dest, sizeof(dest), &src, sizeof(src));
    EXPECT_EQ(GetInfoStatus::SUCCESS, retVal);
    EXPECT_EQ(src, dest);
}

TEST(getInfo, GivenSrcSizeGreaterThanEqualDstSizeAndDstNullPtrWhenGettingInfoThenSrcNotCopiedToDstAndSuccessReturned) {
    float dest = 0.0f;
    float src = 1.0f;

    auto retVal = GetInfo::getInfo(nullptr, 0, &src, sizeof(src));
    EXPECT_EQ(GetInfoStatus::SUCCESS, retVal);
    EXPECT_NE(src, dest);
}

TEST(getInfo, GivenSrcSizeLessThanOrEqualDstSizeAndDstIsNullPtrWhenGettingInfoThenSuccessIsReturned) {
    float dest = 0.0f;
    float src = 1.0f;

    auto retVal = GetInfo::getInfo(nullptr, sizeof(dest), &src, sizeof(src));
    EXPECT_EQ(GetInfoStatus::SUCCESS, retVal);
    EXPECT_NE(src, dest);
}

TEST(getInfo, GivenSrcSizeGreaterThanDstSizeAndDstIsNotNullPtrWhenGettingInfoThenInvalidValueIsReturned) {
    float dest = 0.0f;
    float src = 1.0f;

    auto retVal = GetInfo::getInfo(&dest, 0, &src, sizeof(src));
    EXPECT_EQ(GetInfoStatus::INVALID_VALUE, retVal);
    EXPECT_NE(src, dest);
}

TEST(getInfo, GivenNullSrcPtrWhenGettingInfoThenInvalidValueErrorIsReturned) {
    float dest = 0.0f;
    float src = 1.0f;

    auto retVal = GetInfo::getInfo(&dest, sizeof(dest), nullptr, sizeof(src));
    EXPECT_EQ(GetInfoStatus::INVALID_VALUE, retVal);
    EXPECT_NE(src, dest);
}

TEST(getInfo, GivenZeroSrcSizeWhenGettingInfoThenSuccessIsReturned) {
    float dest = 0.0f;
    float src = 1.0f;

    auto retVal = GetInfo::getInfo(&dest, sizeof(dest), &src, 0);
    EXPECT_EQ(GetInfoStatus::SUCCESS, retVal);
    EXPECT_NE(src, dest);
}

TEST(getInfo, GivenInvalidSrcSizeWhenGettingInfoThenInvalidValueErrorIsReturned) {
    float dest = 0.0f;
    float src = 1.0f;

    auto retVal = GetInfo::getInfo(&dest, sizeof(dest), &src, GetInfo::invalidSourceSize);
    EXPECT_EQ(GetInfoStatus::INVALID_VALUE, retVal);
    EXPECT_NE(src, dest);
}

TEST(getInfo, GivenInvalidInputWhenSettingParamValueReturnSizeThenNothingHappens) {
    size_t paramValueReturnSize = 0u;

    GetInfo::setParamValueReturnSize(nullptr, 1, GetInfoStatus::SUCCESS);
    GetInfo::setParamValueReturnSize(&paramValueReturnSize, 1, GetInfoStatus::INVALID_VALUE);
    EXPECT_EQ(0u, paramValueReturnSize);
}

TEST(getInfo, GivenValidInputWhenSettingParamValueReturnSizeThenValueIsUpdated) {
    size_t paramValueReturnSize = 0u;

    GetInfo::setParamValueReturnSize(&paramValueReturnSize, 1, GetInfoStatus::SUCCESS);
    EXPECT_EQ(1u, paramValueReturnSize);
}

TEST(getInfoHelper, GivenInstanceOfGetInfoHelperAndNullPtrParamsThenSuccessIsReturned) {
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
    ASSERT_NE(nullptr, getValue); // NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)
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
