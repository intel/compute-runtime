/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/kernel_commands.h"
#include "runtime/helpers/kernel_commands.inl"
#include "runtime/helpers/kernel_commands_base.inl"

#include "hw_cmds.h"

namespace OCLRT {
template struct KernelCommandsHelper<CNLFamily>;

} // namespace OCLRT
