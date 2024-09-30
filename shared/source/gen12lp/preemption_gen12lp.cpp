/*
 * Copyright (C) 2019-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/preemption.h"
#include "shared/source/command_stream/preemption.inl"
#include "shared/source/gen12lp/hw_cmds_base.h"

namespace NEO {

using GfxFamily = Gen12LpFamily;

template <>
void PreemptionHelper::programCsrBaseAddress<GfxFamily>(LinearStream &preambleCmdStream, Device &device, const GraphicsAllocation *preemptionCsr) {}

template <>
size_t PreemptionHelper::getRequiredPreambleSize<GfxFamily>(const Device &device) {
    return 0;
}

template void PreemptionHelper::programCmdStream<GfxFamily>(LinearStream &cmdStream, PreemptionMode newPreemptionMode,
                                                            PreemptionMode oldPreemptionMode, GraphicsAllocation *preemptionCsr);
template size_t PreemptionHelper::getRequiredPreambleSize<GfxFamily>(const Device &device);
template void PreemptionHelper::programCsrBaseAddress<GfxFamily>(LinearStream &preambleCmdStream, Device &device, const GraphicsAllocation *preemptionCsr);
template void PreemptionHelper::programCsrBaseAddressCmd<GfxFamily>(LinearStream &preambleCmdStream, const GraphicsAllocation *preemptionCsr);
template void PreemptionHelper::programStateSip<GfxFamily>(LinearStream &preambleCmdStream, Device &device, OsContext *context);
template void PreemptionHelper::programStateSipCmd<GfxFamily>(LinearStream &preambleCmdStream, GraphicsAllocation *sipAllocation, bool useFullAddress);
template size_t PreemptionHelper::getRequiredStateSipCmdSize<GfxFamily>(Device &device, bool isRcs);
template size_t PreemptionHelper::getRequiredCmdStreamSize<GfxFamily>(PreemptionMode newPreemptionMode, PreemptionMode oldPreemptionMode);
template void PreemptionHelper::programInterfaceDescriptorDataPreemption<GfxFamily>(INTERFACE_DESCRIPTOR_DATA<GfxFamily> *idd, PreemptionMode preemptionMode);
} // namespace NEO
