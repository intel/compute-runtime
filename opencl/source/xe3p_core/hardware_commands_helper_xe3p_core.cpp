/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/cache_flush_xehp_and_later.inl"

#include "opencl/source/helpers/hardware_commands_helper.h"
#include "opencl/source/helpers/hardware_commands_helper_base.inl"
#include "opencl/source/helpers/hardware_commands_helper_xehp_and_later.inl"

#include "hw_cmds_xe3p_core.h"

namespace NEO {
using FamilyType = Xe3pCoreFamily;

} // namespace NEO

template struct NEO::HardwareCommandsHelper<NEO::FamilyType>;
template struct NEO::HardwareCommandsHelperNoHeap<NEO::FamilyType>;

#include "opencl/source/helpers/hardware_commands_helper_xe3p_and_later.inl"

#include "enable_hardware_commands_helper_xe3p.inl"
