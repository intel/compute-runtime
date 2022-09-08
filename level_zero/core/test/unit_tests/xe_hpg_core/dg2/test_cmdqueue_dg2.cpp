/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_l0_debugger.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"

namespace L0 {
namespace ult {
using CommandQueueTestDG2 = Test<DeviceFixture>;

HWTEST2_F(CommandQueueTestDG2, givenBindlessEnabledWhenEstimateStateBaseAddressCmdSizeCalledOnDG2ThenReturnedSizeOf2SBAAndPCAnd3DBindingTablePoolPool, IsDG2) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.UseBindlessMode.set(1);
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using STATE_BASE_ADDRESS = typename GfxFamily::STATE_BASE_ADDRESS;
    using PIPE_CONTROL = typename GfxFamily::PIPE_CONTROL;
    using _3DSTATE_BINDING_TABLE_POOL_ALLOC = typename GfxFamily::_3DSTATE_BINDING_TABLE_POOL_ALLOC;
    ze_command_queue_desc_t desc = {};
    auto csr = std::unique_ptr<NEO::CommandStreamReceiver>(neoDevice->createCommandStreamReceiver());
    auto commandQueue = std::make_unique<MockCommandQueueHw<gfxCoreFamily>>(device, csr.get(), &desc);
    auto size = commandQueue->estimateStateBaseAddressCmdSize();
    auto expectedSize = 2 * sizeof(STATE_BASE_ADDRESS) + sizeof(PIPE_CONTROL) + sizeof(_3DSTATE_BINDING_TABLE_POOL_ALLOC);
    EXPECT_EQ(size, expectedSize);
}

HWTEST2_F(CommandQueueTestDG2, givenBindlessEnabledWhenProgramStateBaseAddressCalledOnDG2ThenProgramCorrectL1CachePolicy, IsDG2) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.UseBindlessMode.set(1);

    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->createBindlessHeapsHelper(neoDevice->getMemoryManager(),
                                                                                                                             neoDevice->getNumGenericSubDevices() > 1,
                                                                                                                             neoDevice->getRootDeviceIndex(),
                                                                                                                             neoDevice->getDeviceBitfield());

    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using STATE_BASE_ADDRESS = typename GfxFamily::STATE_BASE_ADDRESS;
    ze_command_queue_desc_t desc = {};
    auto &csr = neoDevice->getGpgpuCommandStreamReceiver();
    auto commandQueue = std::make_unique<MockCommandQueueHw<gfxCoreFamily>>(device, &csr, &desc);
    void *linearStreamBuffer = alignedMalloc(4096, 0x1000);
    {
        NEO::LinearStream cmdStream(linearStreamBuffer, 4096);
        auto usedSpaceBefore = cmdStream.getUsed();
        commandQueue->programStateBaseAddress(0, false, cmdStream, false);
        auto usedSpaceAfter = cmdStream.getUsed();
        ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
            cmdList, ptrOffset(cmdStream.getCpuBase(), 0), usedSpaceAfter));

        auto sbaItor = find<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
        ASSERT_NE(cmdList.end(), sbaItor);
        auto cmdSba = genCmdCast<STATE_BASE_ADDRESS *>(*sbaItor);

        EXPECT_EQ(STATE_BASE_ADDRESS::L1_CACHE_POLICY_WB, cmdSba->getL1CachePolicyL1CacheControl());
    }
    {
        const_cast<NEO::DeviceInfo &>(device->getDeviceInfo()).debuggerActive = true;
        NEO::LinearStream cmdStream(linearStreamBuffer, 4096);
        auto usedSpaceBefore = cmdStream.getUsed();
        commandQueue->programStateBaseAddress(0, false, cmdStream, false);
        auto usedSpaceAfter = cmdStream.getUsed();
        ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
            cmdList, ptrOffset(cmdStream.getCpuBase(), 0), usedSpaceAfter));

        auto sbaItor = find<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
        ASSERT_NE(cmdList.end(), sbaItor);
        auto cmdSba = genCmdCast<STATE_BASE_ADDRESS *>(*sbaItor);

        EXPECT_EQ(STATE_BASE_ADDRESS::L1_CACHE_POLICY_WBP, cmdSba->getL1CachePolicyL1CacheControl());
    }
    {
        auto debugger = MockDebuggerL0Hw<FamilyType>::allocate(neoDevice);
        neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->debugger.reset(debugger);
        const_cast<NEO::DeviceInfo &>(device->getDeviceInfo()).debuggerActive = false;
        NEO::LinearStream cmdStream(linearStreamBuffer, 4096);
        auto usedSpaceBefore = cmdStream.getUsed();
        commandQueue->programStateBaseAddress(0, false, cmdStream, false);
        auto usedSpaceAfter = cmdStream.getUsed();
        ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
            cmdList, ptrOffset(cmdStream.getCpuBase(), 0), usedSpaceAfter));

        auto sbaItor = find<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
        ASSERT_NE(cmdList.end(), sbaItor);
        auto cmdSba = genCmdCast<STATE_BASE_ADDRESS *>(*sbaItor);

        EXPECT_EQ(STATE_BASE_ADDRESS::L1_CACHE_POLICY_WBP, cmdSba->getL1CachePolicyL1CacheControl());
    }

    alignedFree(linearStreamBuffer);
}

} // namespace ult
} // namespace L0
