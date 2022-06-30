/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/constants.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/test/common/helpers/unit_test_helper.h"

#include <cstdint>

namespace NEO {
template <typename FamilyType>
void validateStateBaseAddress(uint64_t indirectObjectHeapBase,
                              uint64_t instructionHeapBaseAddress,
                              IndirectHeap *pDSH,
                              IndirectHeap *pIOH,
                              IndirectHeap *pSSH,
                              GenCmdList::iterator &startCommand,
                              GenCmdList::iterator &endCommand,
                              GenCmdList &cmdList,
                              uint64_t expectedGeneralStateHeapBaseAddress) {
    typedef typename FamilyType::STATE_BASE_ADDRESS STATE_BASE_ADDRESS;
    // All state should be programmed before walker
    auto itorCmd = find<STATE_BASE_ADDRESS *>(startCommand, endCommand);
    ASSERT_NE(endCommand, itorCmd);

    auto *cmd = (STATE_BASE_ADDRESS *)*itorCmd;

    // Verify all addresses are getting programmed
    EXPECT_TRUE(cmd->getDynamicStateBaseAddressModifyEnable());
    EXPECT_TRUE(cmd->getGeneralStateBaseAddressModifyEnable());
    EXPECT_TRUE(cmd->getSurfaceStateBaseAddressModifyEnable());
    EXPECT_TRUE(cmd->getIndirectObjectBaseAddressModifyEnable());
    EXPECT_TRUE(cmd->getInstructionBaseAddressModifyEnable());

    EXPECT_EQ(pDSH->getGraphicsAllocation()->getGpuAddress(), cmd->getDynamicStateBaseAddress());
    // Stateless accesses require GSH.base to be 0.
    EXPECT_EQ(expectedGeneralStateHeapBaseAddress, cmd->getGeneralStateBaseAddress());
    EXPECT_EQ(pSSH->getGraphicsAllocation()->getGpuAddress(), cmd->getSurfaceStateBaseAddress());
    EXPECT_EQ(pIOH->getGraphicsAllocation()->getGpuBaseAddress(), cmd->getIndirectObjectBaseAddress());
    EXPECT_EQ(instructionHeapBaseAddress, cmd->getInstructionBaseAddress());

    // Verify all sizes are getting programmed
    EXPECT_TRUE(cmd->getDynamicStateBufferSizeModifyEnable());
    EXPECT_TRUE(cmd->getGeneralStateBufferSizeModifyEnable());
    EXPECT_TRUE(cmd->getIndirectObjectBufferSizeModifyEnable());
    EXPECT_TRUE(cmd->getInstructionBufferSizeModifyEnable());

    EXPECT_EQ(pDSH->getMaxAvailableSpace(), cmd->getDynamicStateBufferSize() * MemoryConstants::pageSize);
    EXPECT_NE(0u, cmd->getGeneralStateBufferSize());
    EXPECT_EQ(MemoryConstants::sizeOf4GBinPageEntities, cmd->getIndirectObjectBufferSize());
    EXPECT_EQ(MemoryConstants::sizeOf4GBinPageEntities, cmd->getInstructionBufferSize());

    // Generically validate this command
    FamilyType::PARSE::template validateCommand<STATE_BASE_ADDRESS *>(cmdList.begin(), itorCmd);
}

template <typename FamilyType>
void validateL3Programming(GenCmdList &cmdList, GenCmdList::iterator &itorWalker) {
    typedef typename FamilyType::PARSE PARSE;
    typedef typename PARSE::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;

    auto itorCmd = findMmio<FamilyType>(cmdList.begin(), itorWalker, L3CNTLRegisterOffset<FamilyType>::registerOffset);
    if (UnitTestHelper<FamilyType>::isL3ConfigProgrammable()) {
        // All state should be programmed before walker
        ASSERT_NE(itorWalker, itorCmd);

        auto *cmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(*itorCmd);
        ASSERT_NE(nullptr, cmd);

        auto registerOffset = L3CNTLRegisterOffset<FamilyType>::registerOffset;
        EXPECT_EQ(registerOffset, cmd->getRegisterOffset());
        auto l3Cntlreg = cmd->getDataDword();
        auto numURBWays = (l3Cntlreg >> 1) & 0x7f;
        auto l3ClientPool = (l3Cntlreg >> 25) & 0x7f;
        EXPECT_NE(0u, numURBWays);
        EXPECT_NE(0u, l3ClientPool);
    } else {
        ASSERT_EQ(itorWalker, itorCmd);
    }
}

template <typename FamilyType>
void validateMediaVFEState(const HardwareInfo *hwInfo, void *cmdMediaVfeState, GenCmdList &cmdList, GenCmdList::iterator itorMediaVfeState) {
    typedef typename FamilyType::MEDIA_VFE_STATE MEDIA_VFE_STATE;

    auto *cmd = (MEDIA_VFE_STATE *)cmdMediaVfeState;
    ASSERT_NE(nullptr, cmd);

    uint32_t threadPerEU = (hwInfo->gtSystemInfo.ThreadCount / hwInfo->gtSystemInfo.EUCount) + hwInfo->capabilityTable.extraQuantityThreadsPerEU;

    uint32_t expected = hwInfo->gtSystemInfo.EUCount * threadPerEU;
    EXPECT_EQ(expected, cmd->getMaximumNumberOfThreads());
    EXPECT_NE(0u, cmd->getNumberOfUrbEntries());
    EXPECT_NE(0u, cmd->getUrbEntryAllocationSize());
    EXPECT_EQ(0u, cmd->getScratchSpaceBasePointer());
    EXPECT_EQ(0u, cmd->getPerThreadScratchSpace());
    EXPECT_EQ(0u, cmd->getStackSize());

    // Generically validate this command
    FamilyType::PARSE::template validateCommand<MEDIA_VFE_STATE *>(cmdList.begin(), itorMediaVfeState);
}
} // namespace NEO
