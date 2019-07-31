/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/built_ins/built_ins.h"
#include "runtime/command_stream/csr_definitions.h"
#include "runtime/command_stream/preemption.h"
#include "runtime/command_stream/preemption.inl"

#include <cstring>

namespace NEO {

typedef SKLFamily GfxFamily;

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

template void PreemptionHelper::programCmdStream<GfxFamily>(LinearStream &cmdStream, PreemptionMode newPreemptionMode,
                                                            PreemptionMode oldPreemptionMode, GraphicsAllocation *preemptionCsr);

template size_t PreemptionHelper::getRequiredPreambleSize<GfxFamily>(const Device &device);
template void PreemptionHelper::programCsrBaseAddress<GfxFamily>(LinearStream &preambleCmdStream, Device &device, const GraphicsAllocation *preemptionCsr);
template void PreemptionHelper::programStateSip<GfxFamily>(LinearStream &preambleCmdStream, Device &device);
template size_t PreemptionHelper::getRequiredStateSipCmdSize<GfxFamily>(const Device &device);
template size_t PreemptionHelper::getRequiredCmdStreamSize<GfxFamily>(PreemptionMode newPreemptionMode, PreemptionMode oldPreemptionMode);
template size_t PreemptionHelper::getPreemptionWaCsSize<GfxFamily>(const Device &device);
template void PreemptionHelper::applyPreemptionWaCmdsBegin<GfxFamily>(LinearStream *pCommandStream, const Device &device);
template void PreemptionHelper::applyPreemptionWaCmdsEnd<GfxFamily>(LinearStream *pCommandStream, const Device &device);
template void PreemptionHelper::programInterfaceDescriptorDataPreemption<GfxFamily>(INTERFACE_DESCRIPTOR_DATA<GfxFamily> *idd, PreemptionMode preemptionMode);
} // namespace NEO
