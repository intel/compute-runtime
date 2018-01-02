/*
 * Copyright (c) 2017, Intel Corporation
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
#include "runtime/memory_manager/graphics_allocation.h"

namespace OCLRT {

typedef SKLFamily GfxFamily;

namespace PreemptionSKL {
static constexpr uint32_t mmioAddress = 0x2580;
static constexpr uint32_t maskVal = (1 << 1) | (1 << 2);
static constexpr uint32_t maskShift = 16;
static constexpr uint32_t mask = PreemptionSKL::maskVal << PreemptionSKL::maskShift;

static constexpr uint32_t threadGroupVal = (1 << 1);
static constexpr uint32_t cmdLevelVal = (1 << 2);
static constexpr uint32_t midThreadVal = 0;
}; // namespace PreemptionSKL

template <>
void PreemptionHelper::programPreemptionMode<GfxFamily>(LinearStream *cmdStream, PreemptionMode &preemptionMode, GraphicsAllocation *preemptionCsr, GraphicsAllocation *sipKernel) {
    uint32_t regVal = 0;
    if (preemptionMode == PreemptionMode::MidThread) {
        regVal = PreemptionSKL::midThreadVal | PreemptionSKL::mask;
    } else if (preemptionMode == PreemptionMode::ThreadGroup) {
        regVal = PreemptionSKL::threadGroupVal | PreemptionSKL::mask;
    } else {
        regVal = PreemptionSKL::cmdLevelVal | PreemptionSKL::mask;
    }

    LriHelper<GfxFamily>::program(cmdStream, PreemptionSKL::mmioAddress, regVal);

    if (preemptionMode == PreemptionMode::MidThread) {
        typedef typename GfxFamily::GPGPU_CSR_BASE_ADDRESS GPGPU_CSR_BASE_ADDRESS;
        auto csr = (GPGPU_CSR_BASE_ADDRESS *)cmdStream->getSpace(sizeof(GPGPU_CSR_BASE_ADDRESS));
        *csr = GPGPU_CSR_BASE_ADDRESS::sInit();
        csr->setGpgpuCsrBaseAddress(preemptionCsr->getGpuAddressToPatch());
    }
}

template <>
size_t PreemptionHelper::getRequiredCsrSize<GfxFamily>(PreemptionMode preemptionMode) {
    size_t size = sizeof(typename GfxFamily::MI_LOAD_REGISTER_IMM);
    if (preemptionMode == PreemptionMode::MidThread) {
        size += sizeof(typename GfxFamily::GPGPU_CSR_BASE_ADDRESS);
    }
    return size;
}

template size_t PreemptionHelper::getPreemptionWaCsSize<GfxFamily>(const Device &device);
template void PreemptionHelper::applyPreemptionWaCmdsBegin<GfxFamily>(LinearStream *pCommandStream, const Device &device);
template void PreemptionHelper::applyPreemptionWaCmdsEnd<GfxFamily>(LinearStream *pCommandStream, const Device &device);
} // namespace OCLRT
