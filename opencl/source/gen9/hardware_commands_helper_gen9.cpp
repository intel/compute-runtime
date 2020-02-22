/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/gen9/hw_cmds.h"

#include "helpers/hardware_commands_helper.h"
#include "helpers/hardware_commands_helper.inl"
#include "helpers/hardware_commands_helper_base.inl"

#include <cstdint>

namespace NEO {

template <>
bool HardwareCommandsHelper<SKLFamily>::isPipeControlWArequired(const HardwareInfo &hwInfo) { return true; }

template struct HardwareCommandsHelper<SKLFamily>;
} // namespace NEO
