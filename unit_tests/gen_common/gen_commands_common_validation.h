/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstdint>
#include "runtime/memory_manager/memory_constants.h"
#include "runtime/indirect_heap/indirect_heap.h"
#include "unit_tests/gen_common/gen_cmd_parse.h"
#include "unit_tests/helpers/unit_test_helper.h"
#include "gtest/gtest.h"

namespace OCLRT {
template <typename FamilyType>
void validateStateBaseAddress(uint64_t internalHeapBase, IndirectHeap *pDSH,
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

    EXPECT_EQ(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(pDSH->getCpuBase())), cmd->getDynamicStateBaseAddress());
    // Stateless accesses require GSH.base to be 0.
    EXPECT_EQ(expectedGeneralStateHeapBaseAddress, cmd->getGeneralStateBaseAddress());
    EXPECT_EQ(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(pSSH->getCpuBase())), cmd->getSurfaceStateBaseAddress());
    EXPECT_EQ(pIOH->getGraphicsAllocation()->gpuBaseAddress, cmd->getIndirectObjectBaseAddress());
    EXPECT_EQ(internalHeapBase, cmd->getInstructionBaseAddress());

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
        auto L3ClientPool = (l3Cntlreg >> 25) & 0x7f;
        EXPECT_NE(0u, numURBWays);
        EXPECT_NE(0u, L3ClientPool);
    } else {
        ASSERT_EQ(itorWalker, itorCmd);
    }
}

template <typename FamilyType>
void validateMediaVFEState(const HardwareInfo *hwInfo, void *cmdMediaVfeState, GenCmdList &cmdList, GenCmdList::iterator itorMediaVfeState) {
    typedef typename FamilyType::MEDIA_VFE_STATE MEDIA_VFE_STATE;

    auto *cmd = (MEDIA_VFE_STATE *)cmdMediaVfeState;
    ASSERT_NE(nullptr, cmd);

    uint32_t threadPerEU = (hwInfo->pSysInfo->ThreadCount / hwInfo->pSysInfo->EUCount) + hwInfo->capabilityTable.extraQuantityThreadsPerEU;

    uint32_t expected = hwInfo->pSysInfo->EUCount * threadPerEU;
    EXPECT_EQ(expected, cmd->getMaximumNumberOfThreads());
    EXPECT_NE(0u, cmd->getNumberOfUrbEntries());
    EXPECT_NE(0u, cmd->getUrbEntryAllocationSize());
    EXPECT_EQ(0u, cmd->getScratchSpaceBasePointer());
    EXPECT_EQ(0u, cmd->getPerThreadScratchSpace());
    EXPECT_EQ(0u, cmd->getStackSize());

    // Generically validate this command
    FamilyType::PARSE::template validateCommand<MEDIA_VFE_STATE *>(cmdList.begin(), itorMediaVfeState);
}
} // namespace OCLRT
