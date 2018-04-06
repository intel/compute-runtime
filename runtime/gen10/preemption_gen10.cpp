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

#include "runtime/built_ins/built_ins.h"
#include "runtime/command_stream/preemption.h"
#include "runtime/command_stream/preemption.inl"
#include "runtime/memory_manager/graphics_allocation.h"

namespace OCLRT {

typedef CNLFamily GfxFamily;

template <>
struct PreemptionConfig<GfxFamily> {
    static constexpr uint32_t mmioAddress = 0x2580;
    static constexpr uint32_t maskVal = (1 << 1) | (1 << 2);
    static constexpr uint32_t maskShift = 16;
    static constexpr uint32_t mask = maskVal << maskShift;

    static constexpr uint32_t threadGroupVal = (1 << 1);
    static constexpr uint32_t cmdLevelVal = (1 << 2);
    static constexpr uint32_t midThreadVal = 0;
};

template <>
size_t PreemptionHelper::getPreemptionWaCsSize<GfxFamily>(const Device &device) {
    return 0;
}

template <>
void PreemptionHelper::applyPreemptionWaCmdsBegin<GfxFamily>(LinearStream *pCommandStream, const Device &device) {
}

template <>
void PreemptionHelper::applyPreemptionWaCmdsEnd<GfxFamily>(LinearStream *pCommandStream, const Device &device) {
}

template void PreemptionHelper::programCmdStream<GfxFamily>(LinearStream &cmdStream,
                                                            PreemptionMode newPreemptionMode, PreemptionMode oldPreemptionMode,
                                                            GraphicsAllocation *preemptionCsr, Device &device);
template void PreemptionHelper::programPreamble<GfxFamily>(LinearStream &preambleCmdStream, Device &device, const GraphicsAllocation *preemptionCsr);
template size_t PreemptionHelper::getRequiredPreambleSize<GfxFamily>(const Device &device);
template size_t PreemptionHelper::getRequiredCmdStreamSize<GfxFamily>(PreemptionMode newPreemptionMode, PreemptionMode oldPreemptionMode);

template <>
void PreemptionHelper::programInterfaceDescriptorDataPreemption<GfxFamily>(INTERFACE_DESCRIPTOR_DATA<GfxFamily> *idd, PreemptionMode preemptionMode) {
    using INTERFACE_DESCRIPTOR_DATA = typename GfxFamily::INTERFACE_DESCRIPTOR_DATA;
    if (preemptionMode == PreemptionMode::MidThread) {
        idd->setThreadPreemptionDisable(INTERFACE_DESCRIPTOR_DATA::THREAD_PREEMPTION_DISABLE_DISABLE);
    } else {
        idd->setThreadPreemptionDisable(INTERFACE_DESCRIPTOR_DATA::THREAD_PREEMPTION_DISABLE_ENABLE);
    }
}
} // namespace OCLRT
