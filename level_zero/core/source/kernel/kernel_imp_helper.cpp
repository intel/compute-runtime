/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/cmdlist/cmdlist_launch_params.h"
#include "level_zero/core/source/kernel/kernel_imp.h"

namespace L0 {

void KernelImp::patchRegionParams(const CmdListKernelLaunchParams &launchParams, const ze_group_count_t &threadGroupDimensions) {}

ze_result_t KernelImp::validateWorkgroupSize() const {
    return ZE_RESULT_SUCCESS;
}
} // namespace L0
