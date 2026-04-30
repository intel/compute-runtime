/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <level_zero/zes_api.h>
#include <level_zero/zes_intel_gpu_sysman.h>

#include <map>
#include <string_view>

namespace L0 {
namespace Sysman {

inline const std::map<zes_ras_error_category_exp_t, std::string_view> categoryToErrorNameMap = {
    {ZES_RAS_ERROR_CATEGORY_EXP_COMPUTE_ERRORS, "core-compute"},
    {ZES_RAS_ERROR_CATEGORY_EXP_MEMORY_ERRORS, "device-memory"},
    {ZES_RAS_ERROR_CATEGORY_EXP_SCALE_ERRORS, "scale"},
    {static_cast<zes_ras_error_category_exp_t>(ZES_INTEL_RAS_ERROR_CATEGORY_EXP_FABRIC_ERRORS), "fabric"},
    {static_cast<zes_ras_error_category_exp_t>(ZES_INTEL_RAS_ERROR_CATEGORY_EXP_PCIE_ERRORS), "pcie"},
    {static_cast<zes_ras_error_category_exp_t>(ZES_INTEL_RAS_ERROR_CATEGORY_EXP_SOC_INTERNAL_ERRORS), "soc-internal"}};

} // namespace Sysman
} // namespace L0
