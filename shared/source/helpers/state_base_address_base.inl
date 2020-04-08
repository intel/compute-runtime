/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/cache_policy.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/hw_cmds.h"
#include "shared/source/helpers/state_base_address.h"
#include "shared/source/indirect_heap/indirect_heap.h"

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
    STATE_BASE_ADDRESS cmd = GfxFamily::cmdInitStateBaseAddress;

    if (dsh) {
        cmd.setDynamicStateBaseAddressModifyEnable(true);
        cmd.setDynamicStateBufferSizeModifyEnable(true);
        cmd.setDynamicStateBaseAddress(dsh->getHeapGpuBase());
        cmd.setDynamicStateBufferSize(dsh->getHeapSizeInPages());
    }

    if (ioh) {
        cmd.setIndirectObjectBaseAddressModifyEnable(true);
        cmd.setIndirectObjectBufferSizeModifyEnable(true);
        cmd.setIndirectObjectBaseAddress(ioh->getHeapGpuBase());
        cmd.setIndirectObjectBufferSize(ioh->getHeapSizeInPages());
    }

    if (ssh) {
        cmd.setSurfaceStateBaseAddressModifyEnable(true);
        cmd.setSurfaceStateBaseAddress(ssh->getHeapGpuBase());
    }

    if (setInstructionStateBaseAddress) {
        cmd.setInstructionBaseAddressModifyEnable(true);
        cmd.setInstructionBaseAddress(internalHeapBase);
        cmd.setInstructionBufferSizeModifyEnable(true);
        cmd.setInstructionBufferSize(MemoryConstants::sizeOf4GBinPageEntities);
        cmd.setInstructionMemoryObjectControlState(gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_STATE_HEAP_BUFFER));
    }

    if (setGeneralStateBaseAddress) {
        cmd.setGeneralStateBaseAddressModifyEnable(true);
        cmd.setGeneralStateBufferSizeModifyEnable(true);
        // GSH must be set to 0 for stateless
        cmd.setGeneralStateBaseAddress(GmmHelper::decanonize(generalStateBase));
        cmd.setGeneralStateBufferSize(0xfffff);
    }

    if (DebugManager.flags.OverrideStatelessMocsIndex.get() != -1) {
        statelessMocsIndex = DebugManager.flags.OverrideStatelessMocsIndex.get();
    }

    statelessMocsIndex = statelessMocsIndex << 1;

    cmd.setStatelessDataPortAccessMemoryObjectControlState(statelessMocsIndex);

    appendStateBaseAddressParameters(&cmd, ssh, setGeneralStateBaseAddress, internalHeapBase, gmmHelper, isMultiOsContextCapable);

    *pCmd = cmd;
}

} // namespace NEO
