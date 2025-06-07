/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/cmdlist/cmdlist_launch_params.h"
#include "level_zero/core/source/kernel/kernel_imp.h"

namespace L0 {

KernelExt *KernelImp::getExtension(uint32_t extensionType) { return nullptr; }

void KernelImp::patchRegionParams(const CmdListKernelLaunchParams &launchParams, const ze_group_count_t &threadGroupDimensions) {}

} // namespace L0
