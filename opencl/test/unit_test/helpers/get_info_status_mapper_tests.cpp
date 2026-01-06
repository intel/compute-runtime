/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/helpers/get_info_status_mapper.h"

#include "gtest/gtest.h"

TEST(getInfoStatusMapper, GivenValidGetInfoStatusWhenTranslatingThenExpectedClCodeIsReturned) {
    auto getInfoStatus = changeGetInfoStatusToCLResultType(GetInfoStatus::success);
    EXPECT_EQ(CL_SUCCESS, getInfoStatus);

    getInfoStatus = changeGetInfoStatusToCLResultType(GetInfoStatus::invalidContext);
    EXPECT_EQ(CL_INVALID_CONTEXT, getInfoStatus);

    getInfoStatus = changeGetInfoStatusToCLResultType(GetInfoStatus::invalidValue);
    EXPECT_EQ(CL_INVALID_VALUE, getInfoStatus);
}

TEST(getInfoStatusMapper, GivenInvalidGetInfoStatusWhenTranslatingThenClInvalidValueIsReturned) {
    auto getInfoStatus = changeGetInfoStatusToCLResultType(static_cast<GetInfoStatus>(1)); // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)
    EXPECT_EQ(CL_INVALID_VALUE, getInfoStatus);
}
