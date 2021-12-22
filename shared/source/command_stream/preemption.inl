/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/sip.h"
#include "shared/source/command_stream/preemption.h"
#include "shared/source/device/device.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/pipe_control_args.h"
#include "shared/source/helpers/preamble.h"
#include "shared/source/memory_manager/graphics_allocation.h"

namespace NEO {

template <typename GfxFamily>
void PreemptionHelper::programCsrBaseAddress(LinearStream &preambleCmdStream, Device &device, const GraphicsAllocation *preemptionCsr) {
    using GPGPU_CSR_BASE_ADDRESS = typename GfxFamily::GPGPU_CSR_BASE_ADDRESS;
    bool isMidThreadPreemption = device.getPreemptionMode() == PreemptionMode::MidThread;
    if (isMidThreadPreemption) {
        UNRECOVERABLE_IF(nullptr == preemptionCsr);

        auto csr = reinterpret_cast<GPGPU_CSR_BASE_ADDRESS *>(preambleCmdStream.getSpace(sizeof(GPGPU_CSR_BASE_ADDRESS)));
        GPGPU_CSR_BASE_ADDRESS cmd = GfxFamily::cmdInitGpgpuCsrBaseAddress;
        cmd.setGpgpuCsrBaseAddress(preemptionCsr->getGpuAddressToPatch());
        *csr = cmd;
    }
}

template <typename GfxFamily>
void PreemptionHelper::programStateSip(LinearStream &preambleCmdStream, Device &device) {
    using STATE_SIP = typename GfxFamily::STATE_SIP;
    bool debuggingEnabled = device.getDebugger() != nullptr || device.isDebuggerActive();
    bool isMidThreadPreemption = device.getPreemptionMode() == PreemptionMode::MidThread;

    if (isMidThreadPreemption || debuggingEnabled) {
        auto sipAllocation = SipKernel::getSipKernel(device).getSipAllocation();

        auto sip = reinterpret_cast<STATE_SIP *>(preambleCmdStream.getSpace(sizeof(STATE_SIP)));
        STATE_SIP cmd = GfxFamily::cmdInitStateSip;
        cmd.setSystemInstructionPointer(sipAllocation->getGpuAddressToPatch());
        *sip = cmd;
    }
}

template <typename GfxFamily>
void PreemptionHelper::programStateSipEndWa(LinearStream &cmdStream, Device &device) {}

template <typename GfxFamily>
void PreemptionHelper::programCmdStream(LinearStream &cmdStream, PreemptionMode newPreemptionMode,
                                        PreemptionMode oldPreemptionMode, GraphicsAllocation *preemptionCsr) {
    if (oldPreemptionMode == newPreemptionMode) {
        return;
    }

    uint32_t regVal = 0;
    if (newPreemptionMode == PreemptionMode::MidThread) {
        regVal = PreemptionConfig<GfxFamily>::midThreadVal | PreemptionConfig<GfxFamily>::mask;
    } else if (newPreemptionMode == PreemptionMode::ThreadGroup) {
        regVal = PreemptionConfig<GfxFamily>::threadGroupVal | PreemptionConfig<GfxFamily>::mask;
    } else {
        regVal = PreemptionConfig<GfxFamily>::cmdLevelVal | PreemptionConfig<GfxFamily>::mask;
    }

    LriHelper<GfxFamily>::program(&cmdStream, PreemptionConfig<GfxFamily>::mmioAddress, regVal, true);
}

template <typename GfxFamily>
size_t PreemptionHelper::getRequiredCmdStreamSize(PreemptionMode newPreemptionMode, PreemptionMode oldPreemptionMode) {
    if (newPreemptionMode == oldPreemptionMode) {
        return 0;
    }
    return sizeof(typename GfxFamily::MI_LOAD_REGISTER_IMM);
}

template <typename GfxFamily>
size_t PreemptionHelper::getRequiredPreambleSize(const Device &device) {
    if (device.getPreemptionMode() == PreemptionMode::MidThread) {
        return sizeof(typename GfxFamily::GPGPU_CSR_BASE_ADDRESS);
    }
    return 0;
}

template <typename GfxFamily>
size_t PreemptionHelper::getRequiredStateSipCmdSize(Device &device, bool isRcs) {
    size_t size = 0;
    bool isMidThreadPreemption = device.getPreemptionMode() == PreemptionMode::MidThread;
    bool debuggingEnabled = device.getDebugger() != nullptr || device.isDebuggerActive();

    if (isMidThreadPreemption || debuggingEnabled) {
        size += sizeof(typename GfxFamily::STATE_SIP);
    }
    return size;
}

template <typename GfxFamily>
size_t PreemptionHelper::getPreemptionWaCsSize(const Device &device) {
    return 0u;
}
template <typename GfxFamily>
void PreemptionHelper::applyPreemptionWaCmdsBegin(LinearStream *pCommandStream, const Device &device) {
}

template <typename GfxFamily>
void PreemptionHelper::applyPreemptionWaCmdsEnd(LinearStream *pCommandStream, const Device &device) {
}

template <typename GfxFamily>
void PreemptionHelper::programInterfaceDescriptorDataPreemption(INTERFACE_DESCRIPTOR_DATA<GfxFamily> *idd, PreemptionMode preemptionMode) {
    using INTERFACE_DESCRIPTOR_DATA = typename GfxFamily::INTERFACE_DESCRIPTOR_DATA;
    if (preemptionMode == PreemptionMode::MidThread) {
        idd->setThreadPreemptionDisable(INTERFACE_DESCRIPTOR_DATA::THREAD_PREEMPTION_DISABLE_DISABLE);
    } else {
        idd->setThreadPreemptionDisable(INTERFACE_DESCRIPTOR_DATA::THREAD_PREEMPTION_DISABLE_ENABLE);
    }
}

template <typename GfxFamily>
constexpr uint32_t PreemptionConfig<GfxFamily>::mmioAddress = 0x2580;

template <typename GfxFamily>
constexpr uint32_t PreemptionConfig<GfxFamily>::mask = ((1 << 1) | (1 << 2)) << 16;

template <typename GfxFamily>
constexpr uint32_t PreemptionConfig<GfxFamily>::threadGroupVal = (1 << 1);

template <typename GfxFamily>
constexpr uint32_t PreemptionConfig<GfxFamily>::cmdLevelVal = (1 << 2);

template <typename GfxFamily>
constexpr uint32_t PreemptionConfig<GfxFamily>::midThreadVal = 0;

} // namespace NEO
