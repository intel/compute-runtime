/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

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
    uint64_t internalHeapBase,
    bool setInstructionStateBaseAddress,
    GmmHelper *gmmHelper,
    bool isMultiOsContextCapable) {

    *stateBaseAddress = GfxFamily::cmdInitStateBaseAddress;

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
        stateBaseAddress->setInstructionBaseAddress(internalHeapBase);
        stateBaseAddress->setInstructionBufferSizeModifyEnable(true);
        stateBaseAddress->setInstructionBufferSize(MemoryConstants::sizeOf4GBinPageEntities);
        stateBaseAddress->setInstructionMemoryObjectControlState(gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_STATE_HEAP_BUFFER));
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

    appendStateBaseAddressParameters(stateBaseAddress, ssh, setGeneralStateBaseAddress, internalHeapBase, gmmHelper, isMultiOsContextCapable);
}

} // namespace NEO
