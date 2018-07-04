/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#pragma once
#ifdef SUPPORT_GEN8
#include "runtime/gen8/reg_configs.h"
#endif
#ifdef SUPPORT_GEN9
#include "runtime/gen9/reg_configs.h"
#endif
#ifdef SUPPORT_GEN10
#include "runtime/gen10/reg_configs.h"
#endif

#include <cstdint>

namespace OCLRT {
namespace ThreadArbitrationPolicy {
const uint32_t RoundRobinAfterDependency = 2;
}
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
} // namespace OCLRT
