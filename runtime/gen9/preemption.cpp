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

#include <cstring>

#include "runtime/built_ins/built_ins.h"
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
void PreemptionHelper::programCmdStream<GfxFamily>(LinearStream &cmdStream,
                                                   PreemptionMode newPreemptionMode, PreemptionMode oldPreemptionMode,
                                                   GraphicsAllocation *preemptionCsr,
                                                   const LinearStream &ih, const Device &device) {
    if (oldPreemptionMode == newPreemptionMode) {
        DEBUG_BREAK_IF((newPreemptionMode == PreemptionMode::MidThread) && (false == isValidInstructionHeapForMidThreadPreemption(ih, device)));
        return;
    }

    uint32_t regVal = 0;
    if (newPreemptionMode == PreemptionMode::MidThread) {
        regVal = PreemptionSKL::midThreadVal | PreemptionSKL::mask;
    } else if (newPreemptionMode == PreemptionMode::ThreadGroup) {
        regVal = PreemptionSKL::threadGroupVal | PreemptionSKL::mask;
    } else {
        regVal = PreemptionSKL::cmdLevelVal | PreemptionSKL::mask;
    }

    LriHelper<GfxFamily>::program(&cmdStream, PreemptionSKL::mmioAddress, regVal);
}

template <>
size_t PreemptionHelper::getRequiredCmdStreamSize<GfxFamily>(PreemptionMode newPreemptionMode, PreemptionMode oldPreemptionMode) {
    if (newPreemptionMode == oldPreemptionMode) {
        return 0;
    }
    return sizeof(typename GfxFamily::MI_LOAD_REGISTER_IMM);
}

template <>
size_t PreemptionHelper::getRequiredPreambleSize<GfxFamily>(const Device &device) {
    if (device.getPreemptionMode() != PreemptionMode::MidThread) {
        return 0;
    }

    return sizeof(typename GfxFamily::GPGPU_CSR_BASE_ADDRESS) + sizeof(typename GfxFamily::STATE_SIP);
}

template <>
void PreemptionHelper::programPreamble<GfxFamily>(LinearStream &preambleCmdStream, const Device &device,
                                                  const GraphicsAllocation *preemptionCsr) {
    if (device.getPreemptionMode() != PreemptionMode::MidThread) {
        return;
    }

    UNRECOVERABLE_IF(nullptr == preemptionCsr);
    using GPGPU_CSR_BASE_ADDRESS = typename GfxFamily::GPGPU_CSR_BASE_ADDRESS;
    using STATE_SIP = typename GfxFamily::STATE_SIP;

    auto csr = reinterpret_cast<GPGPU_CSR_BASE_ADDRESS *>(preambleCmdStream.getSpace(sizeof(GPGPU_CSR_BASE_ADDRESS)));
    csr->init();
    csr->setGpgpuCsrBaseAddress(preemptionCsr->getGpuAddressToPatch());

    auto sip = reinterpret_cast<STATE_SIP *>(preambleCmdStream.getSpace(sizeof(STATE_SIP)));
    sip->init();
    sip->setSystemInstructionPointer(0);
}

template size_t PreemptionHelper::getPreemptionWaCsSize<GfxFamily>(const Device &device);
template void PreemptionHelper::applyPreemptionWaCmdsBegin<GfxFamily>(LinearStream *pCommandStream, const Device &device);
template void PreemptionHelper::applyPreemptionWaCmdsEnd<GfxFamily>(LinearStream *pCommandStream, const Device &device);
} // namespace OCLRT
