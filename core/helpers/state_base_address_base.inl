/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/gmm_helper/gmm_helper.h"
#include "core/helpers/cache_policy.h"
#include "core/helpers/hw_cmds.h"
#include "core/helpers/state_base_address.h"
#include "core/indirect_heap/indirect_heap.h"
#include "core/memory_manager/memory_constants.h"

namespace NEO {
template <typename GfxFamily>
void StateBaseAddressHelper<GfxFamily>::programStateBaseAddress(
    LinearStream &commandStream,
    const IndirectHeap &dsh,
    const IndirectHeap &ioh,
    const IndirectHeap &ssh,
    uint64_t generalStateBase,
    uint32_t statelessMocsIndex,
    uint64_t internalHeapBase,
    GmmHelper *gmmHelper,
    DispatchFlags &dispatchFlags) {

    auto pCmd = static_cast<STATE_BASE_ADDRESS *>(commandStream.getSpace(sizeof(STATE_BASE_ADDRESS)));
    *pCmd = GfxFamily::cmdInitStateBaseAddress;

    pCmd->setDynamicStateBaseAddressModifyEnable(true);
    pCmd->setGeneralStateBaseAddressModifyEnable(true);
    pCmd->setSurfaceStateBaseAddressModifyEnable(true);
    pCmd->setIndirectObjectBaseAddressModifyEnable(true);
    pCmd->setInstructionBaseAddressModifyEnable(true);

    pCmd->setDynamicStateBaseAddress(dsh.getHeapGpuBase());
    // GSH must be set to 0 for stateless
    pCmd->setGeneralStateBaseAddress(GmmHelper::decanonize(generalStateBase));

    pCmd->setSurfaceStateBaseAddress(ssh.getHeapGpuBase());
    pCmd->setInstructionBaseAddress(internalHeapBase);

    pCmd->setDynamicStateBufferSizeModifyEnable(true);
    pCmd->setGeneralStateBufferSizeModifyEnable(true);
    pCmd->setIndirectObjectBufferSizeModifyEnable(true);
    pCmd->setInstructionBufferSizeModifyEnable(true);

    pCmd->setDynamicStateBufferSize(dsh.getHeapSizeInPages());
    pCmd->setGeneralStateBufferSize(0xfffff);

    pCmd->setIndirectObjectBaseAddress(ioh.getHeapGpuBase());
    pCmd->setIndirectObjectBufferSize(ioh.getHeapSizeInPages());

    pCmd->setInstructionBufferSize(MemoryConstants::sizeOf4GBinPageEntities);

    if (DebugManager.flags.OverrideStatelessMocsIndex.get() != -1) {
        statelessMocsIndex = DebugManager.flags.OverrideStatelessMocsIndex.get();
    }

    statelessMocsIndex = statelessMocsIndex << 1;

    pCmd->setStatelessDataPortAccessMemoryObjectControlState(statelessMocsIndex);
    pCmd->setInstructionMemoryObjectControlState(gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_STATE_HEAP_BUFFER));

    appendStateBaseAddressParameters(pCmd, dsh, ioh, ssh, generalStateBase, internalHeapBase, gmmHelper, dispatchFlags);
}

} // namespace NEO
