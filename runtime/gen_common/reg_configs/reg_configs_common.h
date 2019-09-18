/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#ifdef SUPPORT_GEN8
#include "runtime/gen8/reg_configs.h"
#endif
#ifdef SUPPORT_GEN9
#include "runtime/gen9/reg_configs.h"
#endif
#ifdef SUPPORT_GEN11
#include "runtime/gen11/reg_configs.h"
#endif
#ifdef SUPPORT_GEN12LP
#include "runtime/gen12lp/reg_configs.h"
#endif
#include <cstdint>

namespace NEO {
namespace RowChickenReg4 {
const uint32_t address = 0xE48C;
const uint32_t regDataForArbitrationPolicy[3] = {
    0xC0000, // Age Based
    0xC0004, // Round Robin
    0xC0008, // Round Robin after dependency
};
} // namespace RowChickenReg4
namespace FfSliceCsChknReg2 {
constexpr uint32_t address = 0x20E4;

constexpr uint32_t regUpdate = (1 << 5);
constexpr uint32_t maskShift = 16;
constexpr uint32_t maskUpdate = regUpdate << maskShift;

constexpr uint32_t regVal = regUpdate | maskUpdate;

} // namespace FfSliceCsChknReg2
} // namespace NEO
