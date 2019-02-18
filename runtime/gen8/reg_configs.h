/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/helpers/preamble.h"

namespace OCLRT {

struct BDWFamily;
template <>
struct L3CNTLREGConfig<IGFX_BROADWELL> {
    static const uint32_t valueForSLM = 0x60000321u;
    static const uint32_t valueForNoSLM = 0x80000340u;
};

template <>
struct L3CNTLRegisterOffset<BDWFamily> {
    static const uint32_t registerOffset = 0x7034;
};
} // namespace OCLRT
