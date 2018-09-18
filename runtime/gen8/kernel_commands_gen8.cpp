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

#include "hw_cmds_generated.h"

namespace OCLRT {

template <>
bool KernelCommandsHelper<BDWFamily>::isPipeControlWArequired() { return false; }

static uint32_t slmSizeId[] = {0, 1, 2, 4, 4, 8, 8, 8, 8, 16, 16, 16, 16, 16, 16, 16};

template <>
uint32_t KernelCommandsHelper<BDWFamily>::computeSlmValues(uint32_t valueIn) {
    valueIn += (4 * KB - 1);
    valueIn = valueIn >> 12;
    valueIn = std::min(valueIn, 15u);
    valueIn = slmSizeId[valueIn];
    return valueIn;
}

// Explicitly instantiate KernelCommandsHelper for BDW device family
template struct KernelCommandsHelper<BDWFamily>;
} // namespace OCLRT
