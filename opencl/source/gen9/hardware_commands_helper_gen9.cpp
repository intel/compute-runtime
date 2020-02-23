/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen9/hw_cmds.h"
#include "opencl/source/helpers/hardware_commands_helper.h"
#include "opencl/source/helpers/hardware_commands_helper.inl"
#include "opencl/source/helpers/hardware_commands_helper_base.inl"

#include <cstdint>

namespace NEO {

template <>
bool HardwareCommandsHelper<SKLFamily>::isPipeControlWArequired(const HardwareInfo &hwInfo) { return true; }

template struct HardwareCommandsHelper<SKLFamily>;
} // namespace NEO
