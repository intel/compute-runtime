/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/preamble.h"

namespace NEO {

struct Gen12LpFamily;
template <>
struct L3CNTLREGConfig<IGFX_TIGERLAKE_LP> {
    static const uint32_t valueForSLM = 0xD0000020u;
    static const uint32_t valueForNoSLM = 0xD0000020u;
};

template <>
struct L3CNTLRegisterOffset<Gen12LpFamily> {
    static const uint32_t registerOffset = 0xB134;
    static const uint32_t registerOffsetCCS = 0xB234;
};

template <>
struct DebugModeRegisterOffset<Gen12LpFamily> {
    enum {
        registerOffset = 0x20d8,
        debugEnabledValue = (1 << 5) | (1 << 21)
    };
};

} // namespace NEO
