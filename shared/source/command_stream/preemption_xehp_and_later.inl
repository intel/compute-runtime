/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

template <>
void PreemptionHelper::programCsrBaseAddress<GfxFamily>(LinearStream &preambleCmdStream, Device &device, const GraphicsAllocation *preemptionCsr) {
}

template <>
void PreemptionHelper::programStateSip<GfxFamily>(LinearStream &preambleCmdStream, Device &device) {
    using STATE_SIP = typename GfxFamily::STATE_SIP;
    using MI_LOAD_REGISTER_IMM = typename GfxFamily::MI_LOAD_REGISTER_IMM;

    auto &hwInfo = device.getHardwareInfo();
    bool debuggingEnabled = device.getDebugger() != nullptr;

    if (debuggingEnabled) {
        HwHelper &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);
        auto sipAllocation = SipKernel::getSipKernel(device).getSipAllocation();

        if (hwHelper.isSipWANeeded(hwInfo)) {
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
void PreemptionHelper::programStateSipEndWa<GfxFamily>(LinearStream &cmdStream, Device &device) {
    using MI_LOAD_REGISTER_IMM = typename GfxFamily::MI_LOAD_REGISTER_IMM;
    bool debuggingEnabled = device.getDebugger() != nullptr;

    if (debuggingEnabled) {
        HwHelper &hwHelper = HwHelper::get(device.getHardwareInfo().platform.eRenderCoreFamily);
        if (hwHelper.isSipWANeeded(device.getHardwareInfo())) {

            NEO::PipeControlArgs args;
            NEO::MemorySynchronizationCommands<GfxFamily>::addPipeControl(cmdStream, args);

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
        HwHelper &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);

        if (hwHelper.isSipWANeeded(hwInfo)) {
            size += sizeof(typename GfxFamily::PIPE_CONTROL);
            size += 2 * sizeof(typename GfxFamily::MI_LOAD_REGISTER_IMM);
        } else {
            auto hwInfoConfig = HwInfoConfig::get(hwInfo.platform.eProductFamily);
            const auto &[isBasicWARequired, isExtendedWARequired] = hwInfoConfig->isPipeControlPriorToNonPipelinedStateCommandsWARequired(hwInfo, isRcs);
            const auto isWARequired = isBasicWARequired || isExtendedWARequired;

            if (isWARequired) {
                size += sizeof(typename GfxFamily::PIPE_CONTROL);
            }
            size += sizeof(typename GfxFamily::STATE_SIP);
        }
    }
    return size;
}
