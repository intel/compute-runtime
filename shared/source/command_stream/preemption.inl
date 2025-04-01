/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/sip.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/command_stream/preemption.h"
#include "shared/source/command_stream/preemption_mode.h"
#include "shared/source/device/device.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/pipe_control_args.h"
#include "shared/source/helpers/preamble.h"
#include "shared/source/memory_manager/graphics_allocation.h"

namespace NEO {

template <typename GfxFamily>
void PreemptionHelper::programCsrBaseAddress(LinearStream &preambleCmdStream, Device &device, const GraphicsAllocation *preemptionCsr) {
    bool debuggingEnabled = device.getDebugger() != nullptr;
    bool isMidThreadPreemption = device.getPreemptionMode() == PreemptionMode::MidThread;
    if (isMidThreadPreemption && !debuggingEnabled) {

        programCsrBaseAddressCmd<GfxFamily>(preambleCmdStream, preemptionCsr);
    }
}

template <typename GfxFamily>
void PreemptionHelper::programCsrBaseAddressCmd(LinearStream &preambleCmdStream, const GraphicsAllocation *preemptionCsr) {
    using GPGPU_CSR_BASE_ADDRESS = typename GfxFamily::GPGPU_CSR_BASE_ADDRESS;

    auto csr = reinterpret_cast<GPGPU_CSR_BASE_ADDRESS *>(preambleCmdStream.getSpace(sizeof(GPGPU_CSR_BASE_ADDRESS)));
    GPGPU_CSR_BASE_ADDRESS cmd = GfxFamily::cmdInitGpgpuCsrBaseAddress;
    cmd.setGpgpuCsrBaseAddress(preemptionCsr->getGpuAddressToPatch());
    *csr = cmd;
}

template <typename GfxFamily>
void PreemptionHelper::programStateSip(LinearStream &preambleCmdStream, Device &device, OsContext *context) {
    auto &helper = device.getGfxCoreHelper();
    if (!helper.isStateSipRequired()) {
        return;
    }
    bool debuggingEnabled = device.getDebugger() != nullptr;
    bool isMidThreadPreemption = device.getPreemptionMode() == PreemptionMode::MidThread;

    auto &compilerProductHelper = device.getCompilerProductHelper();
    bool useFullAddress = compilerProductHelper.isHeaplessModeEnabled(device.getHardwareInfo());

    if (isMidThreadPreemption || debuggingEnabled) {
        GraphicsAllocation *sipAllocation = SipKernel::getSipKernel(device, context).getSipAllocation();
        programStateSipCmd<GfxFamily>(preambleCmdStream, sipAllocation, useFullAddress);
    }
}

template <typename GfxFamily>
void PreemptionHelper::programStateSipCmd(LinearStream &preambleCmdStream, GraphicsAllocation *sipAllocation, bool useFullAddress) {
    using STATE_SIP = typename GfxFamily::STATE_SIP;

    auto sip = reinterpret_cast<STATE_SIP *>(preambleCmdStream.getSpace(sizeof(STATE_SIP)));
    STATE_SIP cmd = GfxFamily::cmdInitStateSip;
    if (useFullAddress) {
        cmd.setSystemInstructionPointer(sipAllocation->getGpuAddress());

    } else {
        cmd.setSystemInstructionPointer(sipAllocation->getGpuAddressToPatch());
    }

    *sip = cmd;
}

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

    LriHelper<GfxFamily>::program(&cmdStream, PreemptionConfig<GfxFamily>::mmioAddress, regVal, true, false);
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
    bool debuggingEnabled = device.getDebugger() != nullptr;
    bool isMidThreadPreemption = device.getPreemptionMode() == PreemptionMode::MidThread;
    if (isMidThreadPreemption || debuggingEnabled) {
        return sizeof(typename GfxFamily::GPGPU_CSR_BASE_ADDRESS);
    }
    return 0;
}

template <typename GfxFamily>
size_t PreemptionHelper::getRequiredStateSipCmdSize(Device &device, bool isRcs) {
    auto &helper = device.getGfxCoreHelper();
    if (!helper.isStateSipRequired()) {
        return 0;
    }
    size_t size = 0;
    bool isMidThreadPreemption = device.getPreemptionMode() == PreemptionMode::MidThread;
    bool debuggingEnabled = device.getDebugger() != nullptr;

    if (isMidThreadPreemption || debuggingEnabled) {
        size += sizeof(typename GfxFamily::STATE_SIP);
    }
    return size;
}

template <typename GfxFamily, typename InterfaceDescriptorType>
void PreemptionHelper::programInterfaceDescriptorDataPreemption(InterfaceDescriptorType *idd, PreemptionMode preemptionMode) {}

template <typename GfxFamily>
const uint32_t PreemptionConfig<GfxFamily>::mmioAddress = 0x2580;

template <typename GfxFamily>
const uint32_t PreemptionConfig<GfxFamily>::mask = ((1 << 1) | (1 << 2)) << 16;

template <typename GfxFamily>
const uint32_t PreemptionConfig<GfxFamily>::threadGroupVal = (1 << 1);

template <typename GfxFamily>
const uint32_t PreemptionConfig<GfxFamily>::cmdLevelVal = (1 << 2);

template <typename GfxFamily>
const uint32_t PreemptionConfig<GfxFamily>::midThreadVal = 0;

} // namespace NEO
