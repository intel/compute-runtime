/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_bindless_heaps_helper.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/cmdqueue/cmdqueue_hw.h"
#include "level_zero/core/source/device/device.h"
#include "level_zero/core/test/unit_tests/sources/debugger/l0_debugger_fixture.h"

#include "test_traits_common.h"

namespace L0 {
namespace ult {

using L0CmdQueueDebuggerTest = Test<L0DebuggerPerContextAddressSpaceFixture>;
HWTEST2_F(L0CmdQueueDebuggerTest, givenDebuggingEnabledAndRequiredGsbaWhenBindlessModeEnabledThenProgramGsbaWritesToSbaTrackingBuffer, IsAtLeastXeHpCore) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.UseBindlessMode.set(1);
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->createBindlessHeapsHelper(neoDevice->getMemoryManager(),
                                                                                                                             neoDevice->getNumGenericSubDevices() > 1,
                                                                                                                             neoDevice->getRootDeviceIndex(),
                                                                                                                             neoDevice->getDeviceBitfield());
    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue;
    auto cmdQ = CommandQueue::create(productFamily, device, neoDevice->getDefaultEngine().commandStreamReceiver, &queueDesc, false, false, false, returnValue);
    ASSERT_NE(nullptr, cmdQ);

    auto cmdQHw = static_cast<CommandQueueHw<gfxCoreFamily> *>(cmdQ);
    StackVec<char, 4096> buffer(4096);
    NEO::LinearStream cmdStream(buffer.begin(), buffer.size());

    auto usedSpaceBefore = cmdStream.getUsed();

    cmdQHw->programStateBaseAddress(0u, false, cmdStream, true, nullptr);

    auto usedSpaceAfter = cmdStream.getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(cmdStream.getCpuBase(), 0), usedSpaceAfter));

    auto pcItor = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), pcItor);

    auto sbaItor = find<STATE_BASE_ADDRESS *>(pcItor, cmdList.end());
    ASSERT_NE(cmdList.end(), sbaItor);
    auto cmdSba = genCmdCast<STATE_BASE_ADDRESS *>(*sbaItor);

    auto sdiItor = find<MI_STORE_DATA_IMM *>(sbaItor, cmdList.end());
    ASSERT_NE(cmdList.end(), sdiItor);
    auto cmdSdi = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);

    auto gmmHelper = neoDevice->getGmmHelper();
    uint64_t gsbaGpuVa = gmmHelper->canonize(cmdSba->getGeneralStateBaseAddress());

    EXPECT_EQ(static_cast<uint32_t>(gsbaGpuVa & 0x0000FFFFFFFFULL), cmdSdi->getDataDword0());
    EXPECT_EQ(static_cast<uint32_t>(gsbaGpuVa >> 32), cmdSdi->getDataDword1());

    auto expectedGpuVa = gmmHelper->decanonize(device->getL0Debugger()->getSbaTrackingGpuVa()) + offsetof(NEO::SbaTrackedAddresses, generalStateBaseAddress);
    EXPECT_EQ(expectedGpuVa, cmdSdi->getAddress());
    cmdQ->destroy();
}

HWTEST2_F(L0CmdQueueDebuggerTest, givenDebuggingEnabledAndRequiredGsbaWhenInternalCommandQueueThenProgramGsbaDoesNotWritesToSbaTrackingBuffer, IsAtLeastXeHpCore) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.UseBindlessMode.set(1);
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->createBindlessHeapsHelper(neoDevice->getMemoryManager(),
                                                                                                                             neoDevice->getNumGenericSubDevices() > 1,
                                                                                                                             neoDevice->getRootDeviceIndex(),
                                                                                                                             neoDevice->getDeviceBitfield());
    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue;
    auto cmdQ = CommandQueue::create(productFamily, device, neoDevice->getDefaultEngine().commandStreamReceiver, &queueDesc, false, true, false, returnValue);
    ASSERT_NE(nullptr, cmdQ);

    auto cmdQHw = static_cast<CommandQueueHw<gfxCoreFamily> *>(cmdQ);
    StackVec<char, 4096> buffer(4096);
    NEO::LinearStream cmdStream(buffer.begin(), buffer.size());

    auto usedSpaceBefore = cmdStream.getUsed();

    cmdQHw->programStateBaseAddress(0u, false, cmdStream, true, nullptr);

    auto usedSpaceAfter = cmdStream.getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(cmdStream.getCpuBase(), 0), usedSpaceAfter));

    auto pcItor = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), pcItor);

    auto sbaItor = find<STATE_BASE_ADDRESS *>(pcItor, cmdList.end());
    ASSERT_NE(cmdList.end(), sbaItor);

    auto sdiItor = find<MI_STORE_DATA_IMM *>(sbaItor, cmdList.end());
    EXPECT_EQ(cmdList.end(), sdiItor);

    cmdQ->destroy();
}

} // namespace ult

} // namespace L0
