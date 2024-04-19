/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/built_ins.h"
#include "shared/source/command_stream/csr_definitions.h"
#include "shared/source/command_stream/preemption.h"
#include "shared/source/command_stream/preemption.inl"
#include "shared/source/gen9/hw_cmds_base.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/register_offsets.h"

#include <cstring>

namespace NEO {

using GfxFamily = Gen9Family;

template <>
size_t PreemptionHelper::getPreemptionWaCsSize<GfxFamily>(const Device &device) {
    typedef typename GfxFamily::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;
    size_t size = 0;
    PreemptionMode preemptionMode = device.getPreemptionMode();
    if (preemptionMode == PreemptionMode::ThreadGroup ||
        preemptionMode == PreemptionMode::MidThread) {
        if (device.getHardwareInfo().workaroundTable.flags.waModifyVFEStateAfterGPGPUPreemption) {
            size += 2 * sizeof(MI_LOAD_REGISTER_IMM);
        }
    }
    return size;
}

template <>
void PreemptionHelper::applyPreemptionWaCmdsBegin<GfxFamily>(LinearStream *pCommandStream, const Device &device) {
    typedef typename GfxFamily::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;
    PreemptionMode preemptionMode = device.getPreemptionMode();
    if (preemptionMode == PreemptionMode::ThreadGroup ||
        preemptionMode == PreemptionMode::MidThread) {
        if (device.getHardwareInfo().workaroundTable.flags.waModifyVFEStateAfterGPGPUPreemption) {
            LriHelper<GfxFamily>::program(pCommandStream,
                                          RegisterOffsets::csGprR0,
                                          RegisterConstants::gpgpuWalkerCookieValueBeforeWalker,
                                          false,
                                          false);
        }
    }
}

template <>
void PreemptionHelper::applyPreemptionWaCmdsEnd<GfxFamily>(LinearStream *pCommandStream, const Device &device) {
    typedef typename GfxFamily::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;
    PreemptionMode preemptionMode = device.getPreemptionMode();
    if (preemptionMode == PreemptionMode::ThreadGroup ||
        preemptionMode == PreemptionMode::MidThread) {
        if (device.getHardwareInfo().workaroundTable.flags.waModifyVFEStateAfterGPGPUPreemption) {
            LriHelper<GfxFamily>::program(pCommandStream,
                                          RegisterOffsets::csGprR0,
                                          RegisterConstants::gpgpuWalkerCookieValueAfterWalker,
                                          false,
                                          false);
        }
    }
}

template <>
void PreemptionHelper::programInterfaceDescriptorDataPreemption<GfxFamily>(INTERFACE_DESCRIPTOR_DATA<GfxFamily> *idd, PreemptionMode preemptionMode) {
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

} // namespace NEO
