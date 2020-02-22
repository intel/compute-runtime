/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/debug_settings/debug_settings_manager.h"
#include "core/gen11/hw_cmds.h"
#include "opencl/source/helpers/hardware_commands_helper.h"
#include "opencl/source/helpers/hardware_commands_helper.inl"
#include "opencl/source/helpers/hardware_commands_helper_base.inl"

namespace NEO {

template <>
bool HardwareCommandsHelper<ICLFamily>::doBindingTablePrefetch() {
    return false;
}

template struct HardwareCommandsHelper<ICLFamily>;
} // namespace NEO
