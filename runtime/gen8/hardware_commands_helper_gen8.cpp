/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/gen8/hw_cmds.h"
#include "runtime/helpers/hardware_commands_helper.h"
#include "runtime/helpers/hardware_commands_helper.inl"
#include "runtime/helpers/hardware_commands_helper_base.inl"

#include <cstdint>

namespace NEO {

static uint32_t slmSizeId[] = {0, 1, 2, 4, 4, 8, 8, 8, 8, 16, 16, 16, 16, 16, 16, 16};

template <>
uint32_t HardwareCommandsHelper<BDWFamily>::computeSlmValues(uint32_t valueIn) {
    valueIn += (4 * KB - 1);
    valueIn = valueIn >> 12;
    valueIn = std::min(valueIn, 15u);
    valueIn = slmSizeId[valueIn];
    return valueIn;
}

// Explicitly instantiate HardwareCommandsHelper for BDW device family
template struct HardwareCommandsHelper<BDWFamily>;
} // namespace NEO
