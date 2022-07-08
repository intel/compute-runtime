/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hpc_core/hw_cmds_pvc.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/event/event.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/xe_hpc_core/xe_hpc_core_test_l0_fixtures.h"

namespace L0 {
namespace ult {

using CommandListStatePrefetchPvcXt = Test<CommandListStatePrefetchXeHpcCore>;

PVCTEST_F(CommandListStatePrefetchPvcXt, givenCommandBufferIsExhaustedWhenPrefetchApiCalledAndIsPvcXtNotBaseDieA0ThenProgramStatePrefetch) {
    auto hwInfo = device->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo();
    const auto &hwInfoConfig = *HwInfoConfig::get(hwInfo->platform.eProductFamily);
    hwInfo->platform.usDeviceID = pvcXtDeviceIds.front();
    hwInfo->platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(REVISION_B, *hwInfo); // not BD A0
    checkIfCommandBufferIsExhaustedWhenPrefetchApiCalledThenStatePrefetchProgrammed(hwInfo);
}

PVCTEST_F(CommandListStatePrefetchPvcXt, givenDebugFlagSetWhenPrefetchApiCalledAndIsPvcXtNotBaseDieA0ThenProgramStatePrefetch) {
    auto hwInfo = device->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo();
    const auto &hwInfoConfig = *HwInfoConfig::get(hwInfo->platform.eProductFamily);
    hwInfo = device->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo->platform.usDeviceID = pvcXtDeviceIds.front();
    hwInfo->platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(REVISION_B, *hwInfo); // not BD A0
    checkIfDebugFlagSetWhenPrefetchApiCalledAThenStatePrefetchProgrammed(hwInfo);
}

using CommandListEventFenceTestsPvc = Test<ModuleFixture>;

PVCTEST_F(CommandListEventFenceTestsPvc, givenCommandListWithProfilingEventAfterCommandOnPvcRev00ThenMiFenceIsNotAdded) {
    using GfxFamily = typename NEO::GfxFamilyMapper<IGFX_XE_HPC_CORE>::GfxFamily;
    using MI_MEM_FENCE = typename FamilyType::MI_MEM_FENCE;

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<IGFX_XE_HPC_CORE>>>();
    commandList->initialize(device, NEO::EngineGroupType::Compute, 0u);
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    auto hwInfo = commandList->commandContainer.getDevice()->getExecutionEnvironment()->rootDeviceEnvironments[0]->getMutableHardwareInfo();
    hwInfo->platform.usRevId = 0x00;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = 0;
    eventDesc.wait = 0;
    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, device));
    commandList->appendEventForProfiling(event.get(), false, false);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), 0), commandList->commandContainer.getCommandStream()->getUsed()));

    auto itor = find<MI_MEM_FENCE *>(cmdList.begin(), cmdList.end());
    EXPECT_EQ(cmdList.end(), itor);
}

using CommandListAppendBarrierXeHpcCore = Test<DeviceFixture>;

PVCTEST_F(CommandListAppendBarrierXeHpcCore, givenCommandListWhenAppendingBarrierThenPipeControlIsProgrammedAndHdcAndUnTypedFlushesAreSet) {
    using GfxFamily = typename NEO::GfxFamilyMapper<IGFX_XE_HPC_CORE>::GfxFamily;
    using PIPE_CONTROL = typename GfxFamily::PIPE_CONTROL;
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<IGFX_XE_HPC_CORE>>>();
    commandList->initialize(device, NEO::EngineGroupType::RenderCompute, 0u);
    ze_result_t returnValue = commandList->appendBarrier(nullptr, 0, nullptr);
    EXPECT_EQ(returnValue, ZE_RESULT_SUCCESS);
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), 0), commandList->commandContainer.getCommandStream()->getUsed()));

    // PC for STATE_BASE_ADDRESS from list initialization
    auto itor = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    itor++;

    // PC for appendBarrier
    itor = find<PIPE_CONTROL *>(itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);

    auto pipeControlCmd = reinterpret_cast<typename FamilyType::PIPE_CONTROL *>(*itor);
    EXPECT_TRUE(pipeControlCmd->getHdcPipelineFlush());
    EXPECT_TRUE(pipeControlCmd->getUnTypedDataPortCacheFlush());
}

} // namespace ult
} // namespace L0
