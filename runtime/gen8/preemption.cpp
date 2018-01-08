/*
 * Copyright (c) 2018, Intel Corporation
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

#include "runtime/command_stream/preemption.h"
#include "runtime/command_stream/preemption.inl"

namespace OCLRT {

typedef BDWFamily GfxFamily;

namespace PreemptionBDW {
static constexpr uint32_t mmioAddress = 0x2248;

static constexpr uint32_t threadGroupVal = 0;
static constexpr uint32_t cmdLevelVal = (1 << 2);
}; // namespace PreemptionBDW

template <>
void PreemptionHelper::programCmdStream<GfxFamily>(LinearStream *cmdStream, PreemptionMode &preemptionMode, GraphicsAllocation *preemptionCsr, GraphicsAllocation *sipKernel) {
    uint32_t regVal = 0;
    if (preemptionMode == PreemptionMode::ThreadGroup) {
        regVal = PreemptionBDW::threadGroupVal;
    } else {
        regVal = PreemptionBDW::cmdLevelVal;
    }

    LriHelper<GfxFamily>::program(cmdStream, PreemptionBDW::mmioAddress, regVal);
}

template <>
size_t PreemptionHelper::getRequiredCmdStreamSize<GfxFamily>(PreemptionMode preemptionMode) {
    return sizeof(typename GfxFamily::MI_LOAD_REGISTER_IMM);
}

template size_t PreemptionHelper::getPreemptionWaCsSize<GfxFamily>(const Device &device);
template void PreemptionHelper::applyPreemptionWaCmdsBegin<GfxFamily>(LinearStream *pCommandStream, const Device &device);
template void PreemptionHelper::applyPreemptionWaCmdsEnd<GfxFamily>(LinearStream *pCommandStream, const Device &device);
} // namespace OCLRT
