/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/cache_policy.h"
#include "shared/source/helpers/hw_cmds.h"
#include "shared/source/helpers/state_base_address.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/source/memory_manager/memory_constants.h"

namespace NEO {
template <typename GfxFamily>
void StateBaseAddressHelper<GfxFamily>::programStateBaseAddress(
    LinearStream &commandStream,
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

    auto pCmd = static_cast<STATE_BASE_ADDRESS *>(commandStream.getSpace(sizeof(STATE_BASE_ADDRESS)));
    *pCmd = GfxFamily::cmdInitStateBaseAddress;

    if (dsh) {
        pCmd->setDynamicStateBaseAddressModifyEnable(true);
        pCmd->setDynamicStateBufferSizeModifyEnable(true);
        pCmd->setDynamicStateBaseAddress(dsh->getHeapGpuBase());
        pCmd->setDynamicStateBufferSize(dsh->getHeapSizeInPages());
    }

    if (ioh) {
        pCmd->setIndirectObjectBaseAddressModifyEnable(true);
        pCmd->setIndirectObjectBufferSizeModifyEnable(true);
        pCmd->setIndirectObjectBaseAddress(ioh->getHeapGpuBase());
        pCmd->setIndirectObjectBufferSize(ioh->getHeapSizeInPages());
    }

    if (ssh) {
        pCmd->setSurfaceStateBaseAddressModifyEnable(true);
        pCmd->setSurfaceStateBaseAddress(ssh->getHeapGpuBase());
    }

    if (setInstructionStateBaseAddress) {
        pCmd->setInstructionBaseAddressModifyEnable(true);
        pCmd->setInstructionBaseAddress(internalHeapBase);
        pCmd->setInstructionBufferSizeModifyEnable(true);
        pCmd->setInstructionBufferSize(MemoryConstants::sizeOf4GBinPageEntities);
        pCmd->setInstructionMemoryObjectControlState(gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_STATE_HEAP_BUFFER));
    }

    if (setGeneralStateBaseAddress) {
        pCmd->setGeneralStateBaseAddressModifyEnable(true);
        pCmd->setGeneralStateBufferSizeModifyEnable(true);
        // GSH must be set to 0 for stateless
        pCmd->setGeneralStateBaseAddress(GmmHelper::decanonize(generalStateBase));
        pCmd->setGeneralStateBufferSize(0xfffff);
    }

    if (DebugManager.flags.OverrideStatelessMocsIndex.get() != -1) {
        statelessMocsIndex = DebugManager.flags.OverrideStatelessMocsIndex.get();
    }

    statelessMocsIndex = statelessMocsIndex << 1;

    pCmd->setStatelessDataPortAccessMemoryObjectControlState(statelessMocsIndex);

    appendStateBaseAddressParameters(pCmd, ssh, setGeneralStateBaseAddress, internalHeapBase, gmmHelper, isMultiOsContextCapable);
}

} // namespace NEO
