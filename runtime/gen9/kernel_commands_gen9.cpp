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

#include <cstdint>

namespace OCLRT {

template <>
bool KernelCommandsHelper<SKLFamily>::isPipeControlWArequired() { return true; }

template struct KernelCommandsHelper<SKLFamily>;
} // namespace OCLRT
