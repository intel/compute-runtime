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
}
