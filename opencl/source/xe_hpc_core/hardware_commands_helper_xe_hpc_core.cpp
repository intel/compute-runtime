/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/cache_flush_xehp_and_later.inl"
#include "shared/source/xe_hpc_core/hw_cmds_xe_hpc_core_base.h"

#include "opencl/source/helpers/hardware_commands_helper.h"
#include "opencl/source/helpers/hardware_commands_helper_base.inl"
#include "opencl/source/helpers/hardware_commands_helper_xehp_and_later.inl"

namespace NEO {
using FamilyType = XeHpcCoreFamily;
} // namespace NEO

template struct NEO::HardwareCommandsHelperWithHeap<NEO::FamilyType>;

#include "opencl/source/helpers/enable_hardware_commands_helper_cw.inl"
