/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <level_zero/ze_api.h>

namespace NEO {
enum class SubmissionStatus : uint32_t;
}

namespace L0 {
ze_result_t getErrorCodeForSubmissionStatus(NEO::SubmissionStatus status);

} // namespace L0
