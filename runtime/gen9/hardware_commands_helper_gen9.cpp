/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/hardware_commands_helper.h"
#include "runtime/helpers/hardware_commands_helper.inl"
#include "runtime/helpers/hardware_commands_helper_base.inl"

#include "hw_cmds.h"

#include <cstdint>

namespace NEO {

template <>
bool HardwareCommandsHelper<SKLFamily>::isPipeControlWArequired() { return true; }

template struct HardwareCommandsHelper<SKLFamily>;
} // namespace NEO
