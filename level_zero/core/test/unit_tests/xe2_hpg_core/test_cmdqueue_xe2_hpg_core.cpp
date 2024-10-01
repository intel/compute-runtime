/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/cmdlist/cmdlist.h"
#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"

namespace L0 {
namespace ult {

using CommandQueueCommandsXe2HpgCore = Test<DeviceFixture>;

HWTEST2_F(CommandQueueCommandsXe2HpgCore, givenCommandQueueWhenExecutingCommandListsThenStateSystemMemFenceAddressCmdIsGenerated, IsXe2HpgCore) {
    using STATE_SYSTEM_MEM_FENCE_ADDRESS = typename FamilyType::STATE_SYSTEM_MEM_FENCE_ADDRESS;
    ze_command_queue_desc_t desc = {};
    auto csr = neoDevice->getDefaultEngine().commandStreamReceiver;

    auto commandQueue = new MockCommandQueueHw<gfxCoreFamily>(device, csr, &desc);
    commandQueue->initialize(false, false, false);

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::compute, 0u, returnValue, false));
    auto commandListHandle = commandList->toHandle();
    commandList->close();

    commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false, nullptr);

    auto globalFence = csr->getGlobalFenceAllocation();

    auto used = commandQueue->commandStream.getUsed();
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, commandQueue->commandStream.getCpuBase(), used));

    auto itor = find<STATE_SYSTEM_MEM_FENCE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itor);

    auto systemMemFenceAddressCmd = genCmdCast<STATE_SYSTEM_MEM_FENCE_ADDRESS *>(*itor);
    EXPECT_EQ(globalFence->getGpuAddress(), systemMemFenceAddressCmd->getSystemMemoryFenceAddress());

    commandQueue->destroy();
}

HWTEST2_F(CommandQueueCommandsXe2HpgCore, givenCommandQueueWhenExecutingCommandListsForTheSecondTimeThenStateSystemMemFenceAddressCmdIsNotGenerated, IsXe2HpgCore) {
    using STATE_SYSTEM_MEM_FENCE_ADDRESS = typename FamilyType::STATE_SYSTEM_MEM_FENCE_ADDRESS;
    ze_command_queue_desc_t desc = {};
    auto csr = neoDevice->getDefaultEngine().commandStreamReceiver;

    auto commandQueue = new MockCommandQueueHw<gfxCoreFamily>(device, csr, &desc);
    commandQueue->initialize(false, false, false);

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::compute, 0u, returnValue, false));
    auto commandListHandle = commandList->toHandle();
    commandList->close();

    commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false, nullptr);
    auto usedSpaceAfter1stExecute = commandQueue->commandStream.getUsed();
    commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false, nullptr);
    auto usedSpaceOn2ndExecute = commandQueue->commandStream.getUsed() - usedSpaceAfter1stExecute;

    GenCmdList cmdList;
    auto cmdBufferAddress = ptrOffset(commandQueue->commandStream.getCpuBase(), usedSpaceAfter1stExecute);
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, cmdBufferAddress, usedSpaceOn2ndExecute));

    auto itor = find<STATE_SYSTEM_MEM_FENCE_ADDRESS *>(cmdList.begin(), cmdList.end());
    EXPECT_EQ(cmdList.end(), itor);

    commandQueue->destroy();
}

} // namespace ult
} // namespace L0
