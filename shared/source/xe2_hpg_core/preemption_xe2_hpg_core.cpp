/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/command_stream/preemption.h"
#include "shared/source/command_stream/preemption.inl"
#include "shared/source/xe2_hpg_core/hw_cmds_base.h"

namespace NEO {

using GfxFamily = Xe2HpgCoreFamily;

template <>
void PreemptionHelper::programInterfaceDescriptorDataPreemption<GfxFamily>(INTERFACE_DESCRIPTOR_DATA<GfxFamily> *idd, PreemptionMode preemptionMode) {
    if (preemptionMode == PreemptionMode::MidThread) {
        idd->setThreadPreemption(true);
    } else {
        idd->setThreadPreemption(false);
    }
}

template <>
void PreemptionHelper::programCsrBaseAddressCmd<GfxFamily>(LinearStream &preambleCmdStream, const GraphicsAllocation *preemptionCsr) {
    using STATE_CONTEXT_DATA_BASE_ADDRESS = typename GfxFamily::STATE_CONTEXT_DATA_BASE_ADDRESS;

    auto stateContextBaseAddressCmd = preambleCmdStream.getSpaceForCmd<STATE_CONTEXT_DATA_BASE_ADDRESS>();
    STATE_CONTEXT_DATA_BASE_ADDRESS cmd = GfxFamily::cmdInitStateContextDataBaseAddress;
    cmd.setContextDataBaseAddress(preemptionCsr->getGpuAddressToPatch());
    *stateContextBaseAddressCmd = cmd;
}

template <>
size_t PreemptionHelper::getRequiredPreambleSize<GfxFamily>(const Device &device) {
    using STATE_CONTEXT_DATA_BASE_ADDRESS = typename GfxFamily::STATE_CONTEXT_DATA_BASE_ADDRESS;
    bool debuggingEnabled = device.getDebugger() != nullptr;
    if ((device.getPreemptionMode() == PreemptionMode::MidThread) && !debuggingEnabled) {
        return sizeof(STATE_CONTEXT_DATA_BASE_ADDRESS);
    }
    return 0u;
}

#include "shared/source/command_stream/preemption_xe2_and_later.inl"

template void PreemptionHelper::programStateSip<GfxFamily>(LinearStream &preambleCmdStream, Device &device, OsContext *context);
template void PreemptionHelper::programStateSipCmd<GfxFamily>(LinearStream &preambleCmdStream, GraphicsAllocation *sipAllocation, bool useFullAddress);
template size_t PreemptionHelper::getRequiredStateSipCmdSize<GfxFamily>(Device &device, bool isRcs);
template void PreemptionHelper::programCsrBaseAddress<GfxFamily>(LinearStream &cmdStream, Device &device, const GraphicsAllocation *preemptionCsr);
} // namespace NEO
