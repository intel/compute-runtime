/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

template <>
void PreemptionHelper::programCsrBaseAddress<GfxFamily>(LinearStream &preambleCmdStream, Device &device, const GraphicsAllocation *preemptionCsr, LogicalStateHelper *logicalStateHelper) {
}

template <>
void PreemptionHelper::programStateSip<GfxFamily>(LinearStream &preambleCmdStream, Device &device, LogicalStateHelper *logicalStateHelper) {
    using STATE_SIP = typename GfxFamily::STATE_SIP;
    using MI_LOAD_REGISTER_IMM = typename GfxFamily::MI_LOAD_REGISTER_IMM;

    auto &hwInfo = device.getHardwareInfo();
    bool debuggingEnabled = device.getDebugger() != nullptr;

    if (debuggingEnabled) {
        auto &gfxCoreHelper = device.getGfxCoreHelper();
        auto sipAllocation = SipKernel::getSipKernel(device).getSipAllocation();

        if (gfxCoreHelper.isSipWANeeded(hwInfo)) {
            auto mmio = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(preambleCmdStream.getSpace(sizeof(MI_LOAD_REGISTER_IMM)));
            MI_LOAD_REGISTER_IMM cmd = GfxFamily::cmdInitLoadRegisterImm;

            UNRECOVERABLE_IF((sipAllocation->getGpuAddressToPatch() & uint64_t(0xffffffff00000000)) != 0);

            uint32_t globalSip = static_cast<uint32_t>(sipAllocation->getGpuAddressToPatch() & uint32_t(-1));
            globalSip &= 0xfffffff8;
            globalSip |= 1;
            cmd.setDataDword(globalSip);
            cmd.setRegisterOffset(GlobalSipRegister<GfxFamily>::registerOffset);
            *mmio = cmd;
        } else {
            auto sip = reinterpret_cast<STATE_SIP *>(preambleCmdStream.getSpace(sizeof(STATE_SIP)));
            STATE_SIP cmd = GfxFamily::cmdInitStateSip;
            cmd.setSystemInstructionPointer(sipAllocation->getGpuAddressToPatch());
            *sip = cmd;
        }
    }
}

template <>
void PreemptionHelper::programStateSipEndWa<GfxFamily>(LinearStream &cmdStream, const HardwareInfo &hwInfo, bool debuggerActive) {
    using MI_LOAD_REGISTER_IMM = typename GfxFamily::MI_LOAD_REGISTER_IMM;

    if (debuggerActive) {
        GfxCoreHelper &gfxCoreHelper = GfxCoreHelper::get(hwInfo.platform.eRenderCoreFamily);
        if (gfxCoreHelper.isSipWANeeded(hwInfo)) {

            NEO::PipeControlArgs args;
            NEO::MemorySynchronizationCommands<GfxFamily>::addSingleBarrier(cmdStream, args);

            auto mmio = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(cmdStream.getSpace(sizeof(MI_LOAD_REGISTER_IMM)));
            MI_LOAD_REGISTER_IMM cmd = GfxFamily::cmdInitLoadRegisterImm;
            uint32_t globalSip = 0;
            cmd.setDataDword(globalSip);
            cmd.setRegisterOffset(GlobalSipRegister<GfxFamily>::registerOffset);
            *mmio = cmd;
        }
    }
}

template <>
size_t PreemptionHelper::getRequiredPreambleSize<GfxFamily>(const Device &device) {
    return 0u;
}

template <>
size_t PreemptionHelper::getRequiredStateSipCmdSize<GfxFamily>(Device &device, bool isRcs) {
    size_t size = 0;
    bool debuggingEnabled = device.getDebugger() != nullptr || device.isDebuggerActive();
    auto &hwInfo = device.getHardwareInfo();

    if (debuggingEnabled) {
        auto &gfxCoreHelper = device.getGfxCoreHelper();

        if (gfxCoreHelper.isSipWANeeded(hwInfo)) {
            size += MemorySynchronizationCommands<GfxFamily>::getSizeForSingleBarrier(false);
            size += 2 * sizeof(typename GfxFamily::MI_LOAD_REGISTER_IMM);
        } else {
            auto hwInfoConfig = HwInfoConfig::get(hwInfo.platform.eProductFamily);
            const auto &[isBasicWARequired, isExtendedWARequired] = hwInfoConfig->isPipeControlPriorToNonPipelinedStateCommandsWARequired(hwInfo, isRcs);
            const auto isWARequired = isBasicWARequired || isExtendedWARequired;

            if (isWARequired) {
                size += MemorySynchronizationCommands<GfxFamily>::getSizeForSingleBarrier(false);
            }
            size += sizeof(typename GfxFamily::STATE_SIP);
        }
    }
    return size;
}
