/*
 * Copyright (c) 2018, Intel Corporation
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

#pragma once

#include <cstdint>
#include "runtime/memory_manager/memory_constants.h"
#include "runtime/indirect_heap/indirect_heap.h"
#include "unit_tests/gen_common/gen_cmd_parse.h"
#include "unit_tests/gen_common/gen_commands_common_validation.h"
#include "test.h"

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
    EXPECT_EQ(pIOH->getGpuBase(), cmd->getIndirectObjectBaseAddress());
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
} // namespace OCLRT
