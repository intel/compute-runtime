/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/get_info_status.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/source/helpers/get_info_status_mapper.h"

namespace NEO {
namespace LEO {
namespace ult {

TEST(GetInfoStatusMapperTests, givenSuccessStatusWhenConvertToCLResultThenReturnsSuccess) {
    EXPECT_EQ(CL_SUCCESS, changeGetInfoStatusToCLResultType(GetInfoStatus::success));
}

TEST(GetInfoStatusMapperTests, givenInvalidContextStatusWhenConvertToCLResultThenReturnsCLInvalidContext) {
    EXPECT_EQ(CL_INVALID_CONTEXT, changeGetInfoStatusToCLResultType(GetInfoStatus::invalidContext));
}

TEST(GetInfoStatusMapperTests, givenInvalidValueStatusWhenConvertToCLResultThenReturnsCLInvalidValue) {
    EXPECT_EQ(CL_INVALID_VALUE, changeGetInfoStatusToCLResultType(GetInfoStatus::invalidValue));
}

} // namespace ult
} // namespace LEO
} // namespace NEO
