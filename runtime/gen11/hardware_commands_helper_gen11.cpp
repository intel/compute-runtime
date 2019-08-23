/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/gen11/hw_cmds.h"
#include "runtime/helpers/hardware_commands_helper.h"
#include "runtime/helpers/hardware_commands_helper.inl"
#include "runtime/helpers/hardware_commands_helper_base.inl"
#include "runtime/os_interface/debug_settings_manager.h"

namespace NEO {

template <>
bool HardwareCommandsHelper<ICLFamily>::doBindingTablePrefetch() {
    return false;
}

template struct HardwareCommandsHelper<ICLFamily>;
} // namespace NEO
