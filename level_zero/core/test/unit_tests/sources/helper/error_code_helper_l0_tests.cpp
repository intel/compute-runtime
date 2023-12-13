/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/submission_status.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/helpers/error_code_helper_l0.h"

namespace L0 {
namespace ult {

TEST(ErrorCodeHelperTest, givenSubmissionStatusWhenGettingErrorCodeThenProperValueIsReturned) {
    EXPECT_EQ(ZE_RESULT_SUCCESS, getErrorCodeForSubmissionStatus(NEO::SubmissionStatus::success));
    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY, getErrorCodeForSubmissionStatus(NEO::SubmissionStatus::outOfMemory));
    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY, getErrorCodeForSubmissionStatus(NEO::SubmissionStatus::outOfHostMemory));
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, getErrorCodeForSubmissionStatus(NEO::SubmissionStatus::failed));
}

} // namespace ult
} // namespace L0
