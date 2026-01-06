/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

template <>
void PreemptionHelper::programCsrBaseAddress<GfxFamily>(LinearStream &preambleCmdStream, Device &device, const GraphicsAllocation *preemptionCsr) {
}

template <>
void PreemptionHelper::programStateSip<GfxFamily>(LinearStream &preambleCmdStream, Device &device, OsContext *context) {
    using STATE_SIP = typename GfxFamily::STATE_SIP;

    bool debuggingEnabled = device.getDebugger() != nullptr;

    if (debuggingEnabled) {
        GraphicsAllocation *sipAllocation = SipKernel::getSipKernel(device, context).getSipAllocation();

        auto sip = reinterpret_cast<STATE_SIP *>(preambleCmdStream.getSpace(sizeof(STATE_SIP)));
        STATE_SIP cmd = GfxFamily::cmdInitStateSip;
        cmd.setSystemInstructionPointer(sipAllocation->getGpuAddressToPatch());
        *sip = cmd;
    }
}

template <>
size_t PreemptionHelper::getRequiredPreambleSize<GfxFamily>(const Device &device) {
    return 0u;
}

template <>
size_t PreemptionHelper::getRequiredStateSipCmdSize<GfxFamily>(Device &device, bool isRcs) {
    size_t size = 0;
    bool debuggingEnabled = device.getDebugger() != nullptr;
    auto &hwInfo = device.getHardwareInfo();

    if (debuggingEnabled) {

        auto &productHelper = device.getProductHelper();
        auto *releaseHelper = device.getReleaseHelper();
        const auto &[isBasicWARequired, isExtendedWARequired] = productHelper.isPipeControlPriorToNonPipelinedStateCommandsWARequired(hwInfo, isRcs, releaseHelper);
        const auto isWARequired = isBasicWARequired || isExtendedWARequired;

        if (isWARequired) {
            size += MemorySynchronizationCommands<GfxFamily>::getSizeForSingleBarrier();
        }
        size += sizeof(typename GfxFamily::STATE_SIP);
    }
    return size;
}
