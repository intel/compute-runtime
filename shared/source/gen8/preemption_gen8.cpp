/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/preemption.h"
#include "shared/source/command_stream/preemption.inl"
#include "shared/source/gen8/hw_cmds_base.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/register_offsets.h"

namespace NEO {

using GfxFamily = Gen8Family;

template <>
struct PreemptionConfig<GfxFamily> {
    static constexpr uint32_t mmioAddress = 0x2248;
    static constexpr uint32_t maskVal = 0;
    static constexpr uint32_t maskShift = 0;
    static constexpr uint32_t mask = 0;

    static constexpr uint32_t threadGroupVal = 0;
    static constexpr uint32_t cmdLevelVal = (1 << 2);
    static constexpr uint32_t midThreadVal = 0;
};

template <>
void PreemptionHelper::programCmdStream<GfxFamily>(LinearStream &cmdStream, PreemptionMode newPreemptionMode, PreemptionMode oldPreemptionMode,
                                                   GraphicsAllocation *preemptionCsr) {
    if (newPreemptionMode == oldPreemptionMode) {
        return;
    }

    uint32_t regVal = 0;
    if (newPreemptionMode == PreemptionMode::ThreadGroup) {
        regVal = PreemptionConfig<GfxFamily>::threadGroupVal;
    } else {
        regVal = PreemptionConfig<GfxFamily>::cmdLevelVal;
    }

    LriHelper<GfxFamily>::program(&cmdStream, PreemptionConfig<GfxFamily>::mmioAddress, regVal, false);
}

template <>
size_t PreemptionHelper::getRequiredPreambleSize<GfxFamily>(const Device &device) {
    return 0;
}

template <>
size_t PreemptionHelper::getRequiredStateSipCmdSize<GfxFamily>(Device &device, bool isRcs) {
    return 0;
}

template <>
void PreemptionHelper::programCsrBaseAddress<GfxFamily>(LinearStream &preambleCmdStream, Device &device, const GraphicsAllocation *preemptionCsr) {
}

template <>
void PreemptionHelper::programCsrBaseAddressCmd<GfxFamily>(LinearStream &preambleCmdStream, const GraphicsAllocation *preemptionCsr) {
}

template <>
void PreemptionHelper::programStateSip<GfxFamily>(LinearStream &preambleCmdStream, Device &device, OsContext *context) {
}

template <>
void PreemptionHelper::programStateSipCmd<GfxFamily>(LinearStream &preambleCmdStream, GraphicsAllocation *sipAllocation) {
}

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
                                          false);
        }
    }
}

template <>
void PreemptionHelper::programInterfaceDescriptorDataPreemption<GfxFamily>(INTERFACE_DESCRIPTOR_DATA<GfxFamily> *idd, PreemptionMode preemptionMode) {
}

template size_t PreemptionHelper::getRequiredCmdStreamSize<GfxFamily>(PreemptionMode newPreemptionMode, PreemptionMode oldPreemptionMode);
} // namespace NEO
