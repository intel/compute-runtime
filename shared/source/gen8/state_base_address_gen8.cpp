/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen8/hw_cmds_base.h"
#include "shared/source/helpers/state_base_address.h"
#include "shared/source/helpers/state_base_address_bdw.inl"
#include "shared/source/helpers/state_base_address_bdw_and_later.inl"

namespace NEO {

template <>
void StateBaseAddressHelper<BDWFamily>::programStateBaseAddress(
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
    bool areMultipleSubDevicesInContext,
    LogicalStateHelper *logicalStateHelper) {

    *stateBaseAddress = BDWFamily::cmdInitStateBaseAddress;

    if (dsh) {
        stateBaseAddress->setDynamicStateBaseAddressModifyEnable(true);
        stateBaseAddress->setDynamicStateBufferSizeModifyEnable(true);
        stateBaseAddress->setDynamicStateBaseAddress(dsh->getHeapGpuBase());
        stateBaseAddress->setDynamicStateBufferSize(dsh->getHeapSizeInPages());
    }

    if (ioh) {
        stateBaseAddress->setIndirectObjectBaseAddressModifyEnable(true);
        stateBaseAddress->setIndirectObjectBufferSizeModifyEnable(true);
        stateBaseAddress->setIndirectObjectBaseAddress(ioh->getHeapGpuBase());
        stateBaseAddress->setIndirectObjectBufferSize(ioh->getHeapSizeInPages());
    }

    if (ssh) {
        stateBaseAddress->setSurfaceStateBaseAddressModifyEnable(true);
        stateBaseAddress->setSurfaceStateBaseAddress(ssh->getHeapGpuBase());
    }

    if (setInstructionStateBaseAddress) {
        stateBaseAddress->setInstructionBaseAddressModifyEnable(true);
        stateBaseAddress->setInstructionBaseAddress(instructionHeapBaseAddress);
        stateBaseAddress->setInstructionBufferSizeModifyEnable(true);
        stateBaseAddress->setInstructionBufferSize(MemoryConstants::sizeOf4GBinPageEntities);
        stateBaseAddress->setInstructionMemoryObjectControlState(gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_STATE_HEAP_BUFFER));
    }

    if (setGeneralStateBaseAddress) {
        stateBaseAddress->setGeneralStateBaseAddressModifyEnable(true);
        stateBaseAddress->setGeneralStateBufferSizeModifyEnable(true);
        // GSH must be set to 0 for stateless
        stateBaseAddress->setGeneralStateBaseAddress(gmmHelper->decanonize(generalStateBase));
        stateBaseAddress->setGeneralStateBufferSize(0xfffff);
    }

    if (DebugManager.flags.OverrideStatelessMocsIndex.get() != -1) {
        statelessMocsIndex = DebugManager.flags.OverrideStatelessMocsIndex.get();
    }

    statelessMocsIndex = statelessMocsIndex << 1;

    stateBaseAddress->setStatelessDataPortAccessMemoryObjectControlState(statelessMocsIndex);

    appendStateBaseAddressParameters(stateBaseAddress, ssh, setGeneralStateBaseAddress, indirectObjectHeapBaseAddress,
                                     gmmHelper, isMultiOsContextCapable, memoryCompressionState, true, useGlobalAtomics, areMultipleSubDevicesInContext);
}
template struct StateBaseAddressHelper<BDWFamily>;
} // namespace NEO
