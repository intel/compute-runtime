/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/helpers/preamble.h"

namespace NEO {

struct ICLFamily;

template <>
struct L3CNTLREGConfig<IGFX_ICELAKE_LP> {
    static const uint32_t valueForSLM = 0xA0000720u;
    static const uint32_t valueForNoSLM = 0xA0000720u;
};

template <>
struct L3CNTLRegisterOffset<ICLFamily> {
    static const uint32_t registerOffset = 0x7034;
};

namespace gen11HdcModeRegister {
const uint32_t address = 0xE5F4;
const uint32_t forceNonCoherentEnableBit = 4;
} // namespace gen11HdcModeRegister

namespace gen11PowerClockStateRegister {
const uint32_t address = 0x20C8;
const uint32_t minEuCountShift = 0;
const uint32_t maxEuCountShift = 4;
const uint32_t subSliceCountShift = 8;
const uint32_t sliceCountShift = 12;
const uint32_t vmeSliceCount = 1;
const uint32_t enabledValue = 0x80040800u;
const uint32_t disabledValue = 0x80040800u;
} // namespace gen11PowerClockStateRegister
} // namespace NEO
