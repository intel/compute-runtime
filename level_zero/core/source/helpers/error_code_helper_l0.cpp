/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/helpers/error_code_helper_l0.h"

#include "shared/source/command_stream/submission_status.h"

namespace L0 {
ze_result_t getErrorCodeForSubmissionStatus(NEO::SubmissionStatus status) {
    switch (status) {
    case NEO::SubmissionStatus::SUCCESS:
        return ZE_RESULT_SUCCESS;
        break;
    case NEO::SubmissionStatus::OUT_OF_MEMORY:
        return ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY;
        break;
    case NEO::SubmissionStatus::OUT_OF_HOST_MEMORY:
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
        break;
    default:
        return ZE_RESULT_ERROR_UNKNOWN;
        break;
    }
}

} // namespace L0
