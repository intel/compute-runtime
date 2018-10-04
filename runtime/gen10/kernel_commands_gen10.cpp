/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/kernel_commands.h"
#include "hw_cmds.h"
#include "runtime/helpers/kernel_commands.inl"
#include "runtime/helpers/kernel_commands_base.inl"

namespace OCLRT {
template struct KernelCommandsHelper<CNLFamily>;

template <>
bool KernelCommandsHelper<CNLFamily>::isPipeControlWArequired() { return true; }
} // namespace OCLRT
