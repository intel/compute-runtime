/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/helpers/get_info_status_mapper.h"

#include "gtest/gtest.h"

TEST(getInfoStatusMapper, GivenValidGetInfoStatusReturnExpectedCLCode) {
    auto getInfoStatus = changeGetInfoStatusToCLResultType(GetInfoStatus::SUCCESS);
    EXPECT_EQ(CL_SUCCESS, getInfoStatus);

    getInfoStatus = changeGetInfoStatusToCLResultType(GetInfoStatus::INVALID_CONTEXT);
    EXPECT_EQ(CL_INVALID_CONTEXT, getInfoStatus);

    getInfoStatus = changeGetInfoStatusToCLResultType(GetInfoStatus::INVALID_VALUE);
    EXPECT_EQ(CL_INVALID_VALUE, getInfoStatus);
}

TEST(getInfoStatusMapper, GivenInvalidGetStatusReturnCLInvalidValue) {
    auto getInfoStatus = changeGetInfoStatusToCLResultType(static_cast<GetInfoStatus>(1));
    EXPECT_EQ(CL_INVALID_VALUE, getInfoStatus);
}
