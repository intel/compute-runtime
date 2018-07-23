/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
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
    GmmHelper *gmmHelper) {

    auto pCmd = static_cast<STATE_BASE_ADDRESS *>(commandStream.getSpace(sizeof(STATE_BASE_ADDRESS)));
    *pCmd = STATE_BASE_ADDRESS::sInit();

    pCmd->setDynamicStateBaseAddressModifyEnable(true);
    pCmd->setGeneralStateBaseAddressModifyEnable(true);
    pCmd->setSurfaceStateBaseAddressModifyEnable(true);
    pCmd->setIndirectObjectBaseAddressModifyEnable(true);
    pCmd->setInstructionBaseAddressModifyEnable(true);

    pCmd->setDynamicStateBaseAddress(reinterpret_cast<uintptr_t>(dsh.getCpuBase()));
    // GSH must be set to 0 for stateless
    pCmd->setGeneralStateBaseAddress(generalStateBase);
    pCmd->setSurfaceStateBaseAddress(reinterpret_cast<uintptr_t>(ssh.getCpuBase()));
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

    appendStateBaseAddressParameters(pCmd, dsh, ioh, ssh, generalStateBase, internalHeapBase);
}

template <typename GfxFamily>
void StateBaseAddressHelper<GfxFamily>::appendStateBaseAddressParameters(
    STATE_BASE_ADDRESS *stateBaseAddress,
    const IndirectHeap &dsh,
    const IndirectHeap &ioh,
    const IndirectHeap &ssh,
    uint64_t generalStateBase,
    uint64_t internalHeapBase) {
}
} // namespace OCLRT
