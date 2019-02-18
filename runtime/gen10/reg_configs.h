/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/helpers/preamble.h"

namespace OCLRT {

struct CNLFamily;
template <>
struct L3CNTLREGConfig<IGFX_CANNONLAKE> {
    static const uint32_t valueForSLM = 0xA0000321u;
    static const uint32_t valueForNoSLM = 0xc0000340u;
};

template <>
struct L3CNTLRegisterOffset<CNLFamily> {
    static const uint32_t registerOffset = 0x7034;
};

const uint32_t gen10HdcModeRegisterAddresss = 0xE5F0;

} // namespace OCLRT
