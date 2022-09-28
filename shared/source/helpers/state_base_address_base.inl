/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/memory_compression_state.h"
#include "shared/source/gmm_helper/cache_settings_helper.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/cache_policy.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/state_base_address.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/source/os_interface/hw_info_config.h"

namespace NEO {

template <typename GfxFamily>
void StateBaseAddressHelper<GfxFamily>::programStateBaseAddressIntoCommandStream(StateBaseAddressHelperArgs<GfxFamily> &args, NEO::LinearStream &commandStream) {
    StateBaseAddressHelper<GfxFamily>::programStateBaseAddress(args);
    auto cmdSpace = StateBaseAddressHelper<GfxFamily>::getSpaceForSbaCmd(commandStream);
    *cmdSpace = *args.stateBaseAddressCmd;

    auto &hwInfoConfig = *HwInfoConfig::get(args.hwInfo->platform.eProductFamily);
    if (hwInfoConfig.isAdditionalStateBaseAddressWARequired(*args.hwInfo)) {
        auto cmdSpace = StateBaseAddressHelper<GfxFamily>::getSpaceForSbaCmd(commandStream);
        *cmdSpace = *args.stateBaseAddressCmd;
    }
}

template <typename GfxFamily>
void StateBaseAddressHelper<GfxFamily>::programStateBaseAddress(
    StateBaseAddressHelperArgs<GfxFamily> &args) {

    *args.stateBaseAddressCmd = GfxFamily::cmdInitStateBaseAddress;
    bool overrideBindlessSurfaceStateBase = true;

    const auto surfaceStateCount = getMaxBindlessSurfaceStates();
    args.stateBaseAddressCmd->setBindlessSurfaceStateSize(surfaceStateCount);

    if (args.useGlobalHeapsBaseAddress) {
        args.stateBaseAddressCmd->setDynamicStateBaseAddressModifyEnable(true);
        args.stateBaseAddressCmd->setDynamicStateBufferSizeModifyEnable(true);
        args.stateBaseAddressCmd->setDynamicStateBaseAddress(args.globalHeapsBaseAddress);
        args.stateBaseAddressCmd->setDynamicStateBufferSize(MemoryConstants::pageSize64k);

        args.stateBaseAddressCmd->setSurfaceStateBaseAddressModifyEnable(true);
        args.stateBaseAddressCmd->setSurfaceStateBaseAddress(args.globalHeapsBaseAddress);

        args.stateBaseAddressCmd->setBindlessSurfaceStateBaseAddressModifyEnable(true);
        args.stateBaseAddressCmd->setBindlessSurfaceStateBaseAddress(args.globalHeapsBaseAddress);

        overrideBindlessSurfaceStateBase = false;
    } else {
        if (args.dsh) {
            args.stateBaseAddressCmd->setDynamicStateBaseAddressModifyEnable(true);
            args.stateBaseAddressCmd->setDynamicStateBufferSizeModifyEnable(true);
            args.stateBaseAddressCmd->setDynamicStateBaseAddress(args.dsh->getHeapGpuBase());
            args.stateBaseAddressCmd->setDynamicStateBufferSize(args.dsh->getHeapSizeInPages());
        }

        if (args.ssh) {
            args.stateBaseAddressCmd->setSurfaceStateBaseAddressModifyEnable(true);
            args.stateBaseAddressCmd->setSurfaceStateBaseAddress(args.ssh->getHeapGpuBase());
        }
    }

    appendIohParameters(args);

    if (args.setInstructionStateBaseAddress) {
        args.stateBaseAddressCmd->setInstructionBaseAddressModifyEnable(true);
        args.stateBaseAddressCmd->setInstructionBaseAddress(args.instructionHeapBaseAddress);
        args.stateBaseAddressCmd->setInstructionBufferSizeModifyEnable(true);
        args.stateBaseAddressCmd->setInstructionBufferSize(MemoryConstants::sizeOf4GBinPageEntities);

        auto resourceUsage = CacheSettingsHelper::getGmmUsageType(AllocationType::INTERNAL_HEAP, DebugManager.flags.DisableCachingForHeaps.get(), *args.gmmHelper->getHardwareInfo());

        args.stateBaseAddressCmd->setInstructionMemoryObjectControlState(args.gmmHelper->getMOCS(resourceUsage));
    }

    if (args.setGeneralStateBaseAddress) {
        args.stateBaseAddressCmd->setGeneralStateBaseAddressModifyEnable(true);
        args.stateBaseAddressCmd->setGeneralStateBufferSizeModifyEnable(true);
        // GSH must be set to 0 for stateless
        args.stateBaseAddressCmd->setGeneralStateBaseAddress(args.gmmHelper->decanonize(args.generalStateBase));
        args.stateBaseAddressCmd->setGeneralStateBufferSize(0xfffff);
    }

    if (args.overrideSurfaceStateBaseAddress) {
        args.stateBaseAddressCmd->setSurfaceStateBaseAddressModifyEnable(true);
        args.stateBaseAddressCmd->setSurfaceStateBaseAddress(args.surfaceStateBaseAddress);
    }

    if (DebugManager.flags.OverrideStatelessMocsIndex.get() != -1) {
        args.statelessMocsIndex = DebugManager.flags.OverrideStatelessMocsIndex.get();
    }

    args.statelessMocsIndex = args.statelessMocsIndex << 1;

    GmmHelper::applyMocsEncryptionBit(args.statelessMocsIndex);

    args.stateBaseAddressCmd->setStatelessDataPortAccessMemoryObjectControlState(args.statelessMocsIndex);

    appendStateBaseAddressParameters(args, overrideBindlessSurfaceStateBase);
}

template <typename GfxFamily>
typename GfxFamily::STATE_BASE_ADDRESS *StateBaseAddressHelper<GfxFamily>::getSpaceForSbaCmd(LinearStream &cmdStream) {
    return cmdStream.getSpaceForCmd<typename GfxFamily::STATE_BASE_ADDRESS>();
}

template <typename GfxFamily>
void StateBaseAddressHelper<GfxFamily>::programBindingTableBaseAddress(LinearStream &commandStream, const IndirectHeap &ssh, GmmHelper *gmmHelper) {
    StateBaseAddressHelper<GfxFamily>::programBindingTableBaseAddress(commandStream, ssh.getHeapGpuBase(), ssh.getHeapSizeInPages(), gmmHelper);
}

} // namespace NEO
