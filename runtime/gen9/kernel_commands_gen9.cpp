/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include <cstdint>
#include "runtime/helpers/kernel_commands.h"
#include "hw_cmds.h"
#include "runtime/helpers/kernel_commands.inl"
#include "runtime/helpers/kernel_commands_base.inl"

namespace OCLRT {

template <>
bool KernelCommandsHelper<SKLFamily>::isPipeControlWArequired() { return true; }

template struct KernelCommandsHelper<SKLFamily>;
} // namespace OCLRT
