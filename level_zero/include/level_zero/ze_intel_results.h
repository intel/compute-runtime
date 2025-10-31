/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _ZE_INTEL_RESULTS_H
#define _ZE_INTEL_RESULTS_H

#include <level_zero/ze_api.h>
#include <level_zero/zes_api.h>
#include <level_zero/zet_api.h>

#include <cstdint>

// Metrics experimental return codes
#define ZE_INTEL_RESULT_WARNING_DATA_NOT_PROCESSED_EXP static_cast<ze_result_t>(0x40000000)             // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)
#define ZE_INTEL_RESULT_WARNING_NOT_CALCULABLE_METRICS_IGNORED_EXP static_cast<ze_result_t>(0x40000001) // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)
#define ZE_INTEL_RESULT_WARNING_TIME_PARAMS_IGNORED_EXP static_cast<ze_result_t>(0x40000002)            // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)

#endif // _ZE_INTEL_RESULTS_H