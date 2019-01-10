/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/kernel_commands.h"
#include "runtime/helpers/kernel_commands.inl"
#include "runtime/helpers/kernel_commands_base.inl"
#include "runtime/os_interface/debug_settings_manager.h"

#include "hw_cmds.h"

namespace NEO {

template <>
bool KernelCommandsHelper<ICLFamily>::doBindingTablePrefetch() {
    return false;
}

template struct KernelCommandsHelper<ICLFamily>;
} // namespace NEO
