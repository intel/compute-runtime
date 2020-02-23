/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen8/hw_cmds.h"
#include "opencl/source/helpers/hardware_commands_helper.h"
#include "opencl/source/helpers/hardware_commands_helper.inl"
#include "opencl/source/helpers/hardware_commands_helper_base.inl"

#include <cstdint>

namespace NEO {

static uint32_t slmSizeId[] = {0, 1, 2, 4, 4, 8, 8, 8, 8, 16, 16, 16, 16, 16, 16, 16};

template <>
uint32_t HardwareCommandsHelper<BDWFamily>::alignSlmSize(uint32_t slmSize) {
    if (slmSize == 0u) {
        return 0u;
    }
    slmSize = std::max(slmSize, 4096u);
    slmSize = Math::nextPowerOfTwo(slmSize);
    return slmSize;
}

template <>
uint32_t HardwareCommandsHelper<BDWFamily>::computeSlmValues(uint32_t slmSize) {
    slmSize += (4 * KB - 1);
    slmSize = slmSize >> 12;
    slmSize = std::min(slmSize, 15u);
    slmSize = slmSizeId[slmSize];
    return slmSize;
}

// Explicitly instantiate HardwareCommandsHelper for BDW device family
template struct HardwareCommandsHelper<BDWFamily>;
} // namespace NEO
