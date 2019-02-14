/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "hw_cmds.h"
#include "runtime/indirect_heap/indirect_heap.h"
#include "runtime/helpers/cache_policy.h"
#include "runtime/helpers/state_base_address.h"
#include "runtime/gmm_helper/gmm_helper.h"
#include "runtime/memory_manager/memory_constants.h"

namespace OCLRT {
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

    pCmd->setDynamicStateBufferSize(static_cast<uint32_t>((dsh.getMaxAvailableSpace() + MemoryConstants::pageMask) / MemoryConstants::pageSize));
    pCmd->setGeneralStateBufferSize(static_cast<uint32_t>(-1));

    pCmd->setIndirectObjectBaseAddress(ioh.getHeapGpuBase());
    pCmd->setIndirectObjectBufferSize(ioh.getHeapSizeInPages());

    pCmd->setInstructionBufferSize(MemoryConstants::sizeOf4GBinPageEntities);

    //set cache settings
    pCmd->setStatelessDataPortAccessMemoryObjectControlState(gmmHelper->getMOCS(statelessMocsIndex));
    pCmd->setInstructionMemoryObjectControlState(gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_STATE_HEAP_BUFFER));

    appendStateBaseAddressParameters(pCmd, dsh, ioh, ssh, generalStateBase, internalHeapBase, gmmHelper, dispatchFlags);
}

template <typename GfxFamily>
void StateBaseAddressHelper<GfxFamily>::appendStateBaseAddressParameters(
    STATE_BASE_ADDRESS *stateBaseAddress,
    const IndirectHeap &dsh,
    const IndirectHeap &ioh,
    const IndirectHeap &ssh,
    uint64_t generalStateBase,
    uint64_t internalHeapBase,
    GmmHelper *gmmHelper,
    DispatchFlags &dispatchFlags) {
}

template <typename GfxFamily>
void StateBaseAddressHelper<GfxFamily>::programBindingTableBaseAddress(LinearStream &commandStream, const IndirectHeap &ssh,
                                                                       size_t stateBaseAddressCmdOffset, GmmHelper *gmmHelper) {

    auto sbaCommand = static_cast<STATE_BASE_ADDRESS *>(ptrOffset(commandStream.getCpuBase(), stateBaseAddressCmdOffset));
    UNRECOVERABLE_IF(sbaCommand->getSurfaceStateBaseAddress() != ssh.getGraphicsAllocation()->getGpuAddress());
}

} // namespace OCLRT
