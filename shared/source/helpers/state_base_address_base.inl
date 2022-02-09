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

#include "hw_cmds.h"

namespace NEO {
template <typename GfxFamily>
void StateBaseAddressHelper<GfxFamily>::programStateBaseAddress(
    STATE_BASE_ADDRESS *stateBaseAddress,
    const IndirectHeap *dsh,
    const IndirectHeap *ioh,
    const IndirectHeap *ssh,
    uint64_t generalStateBase,
    bool setGeneralStateBaseAddress,
    uint32_t statelessMocsIndex,
    uint64_t indirectObjectHeapBaseAddress,
    uint64_t instructionHeapBaseAddress,
    uint64_t globalHeapsBaseAddress,
    bool setInstructionStateBaseAddress,
    bool useGlobalHeapsBaseAddress,
    GmmHelper *gmmHelper,
    bool isMultiOsContextCapable,
    MemoryCompressionState memoryCompressionState,
    bool useGlobalAtomics,
    bool areMultipleSubDevicesInContext) {

    *stateBaseAddress = GfxFamily::cmdInitStateBaseAddress;
    bool overrideBindlessSurfaceStateBase = true;

    const auto surfaceStateCount = getMaxBindlessSurfaceStates();
    stateBaseAddress->setBindlessSurfaceStateSize(surfaceStateCount);

    if (useGlobalHeapsBaseAddress) {
        stateBaseAddress->setDynamicStateBaseAddressModifyEnable(true);
        stateBaseAddress->setDynamicStateBufferSizeModifyEnable(true);
        stateBaseAddress->setDynamicStateBaseAddress(globalHeapsBaseAddress);
        stateBaseAddress->setDynamicStateBufferSize(MemoryConstants::pageSize64k);

        stateBaseAddress->setSurfaceStateBaseAddressModifyEnable(true);
        stateBaseAddress->setSurfaceStateBaseAddress(globalHeapsBaseAddress);

        stateBaseAddress->setBindlessSurfaceStateBaseAddressModifyEnable(true);
        stateBaseAddress->setBindlessSurfaceStateBaseAddress(globalHeapsBaseAddress);

        overrideBindlessSurfaceStateBase = false;
    } else {
        if (dsh) {
            stateBaseAddress->setDynamicStateBaseAddressModifyEnable(true);
            stateBaseAddress->setDynamicStateBufferSizeModifyEnable(true);
            stateBaseAddress->setDynamicStateBaseAddress(dsh->getHeapGpuBase());
            stateBaseAddress->setDynamicStateBufferSize(dsh->getHeapSizeInPages());
        }

        if (ssh) {
            stateBaseAddress->setSurfaceStateBaseAddressModifyEnable(true);
            stateBaseAddress->setSurfaceStateBaseAddress(ssh->getHeapGpuBase());
        }
    }

    appendIohParameters(stateBaseAddress, ioh, useGlobalHeapsBaseAddress, indirectObjectHeapBaseAddress);

    if (setInstructionStateBaseAddress) {
        stateBaseAddress->setInstructionBaseAddressModifyEnable(true);
        stateBaseAddress->setInstructionBaseAddress(instructionHeapBaseAddress);
        stateBaseAddress->setInstructionBufferSizeModifyEnable(true);
        stateBaseAddress->setInstructionBufferSize(MemoryConstants::sizeOf4GBinPageEntities);

        auto resourceUsage = CacheSettingsHelper::getGmmUsageType(AllocationType::INTERNAL_HEAP, DebugManager.flags.DisableCachingForHeaps.get(), *gmmHelper->getHardwareInfo());

        stateBaseAddress->setInstructionMemoryObjectControlState(gmmHelper->getMOCS(resourceUsage));
    }

    if (setGeneralStateBaseAddress) {
        stateBaseAddress->setGeneralStateBaseAddressModifyEnable(true);
        stateBaseAddress->setGeneralStateBufferSizeModifyEnable(true);
        // GSH must be set to 0 for stateless
        stateBaseAddress->setGeneralStateBaseAddress(GmmHelper::decanonize(generalStateBase));
        stateBaseAddress->setGeneralStateBufferSize(0xfffff);
    }

    if (DebugManager.flags.OverrideStatelessMocsIndex.get() != -1) {
        statelessMocsIndex = DebugManager.flags.OverrideStatelessMocsIndex.get();
    }

    statelessMocsIndex = statelessMocsIndex << 1;

    stateBaseAddress->setStatelessDataPortAccessMemoryObjectControlState(statelessMocsIndex);

    appendStateBaseAddressParameters(stateBaseAddress, ssh, setGeneralStateBaseAddress, indirectObjectHeapBaseAddress, gmmHelper,
                                     isMultiOsContextCapable, memoryCompressionState, overrideBindlessSurfaceStateBase, useGlobalAtomics, areMultipleSubDevicesInContext);
}

template <typename GfxFamily>
void StateBaseAddressHelper<GfxFamily>::appendExtraCacheSettings(STATE_BASE_ADDRESS *stateBaseAddress, GmmHelper *gmmHelper) {}

} // namespace NEO
