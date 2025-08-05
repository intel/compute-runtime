/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/xe_hpc_core/hw_cmds_pvc.h"
#include "shared/source/xe_hpc_core/pvc/device_ids_configs_pvc.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/event/event.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/xe_hpc_core/xe_hpc_core_test_l0_fixtures.h"

#include "implicit_args.h"

namespace L0 {
namespace ult {

using CommandListEventFenceTestsPvc = Test<ModuleFixture>;

PVCTEST_F(CommandListEventFenceTestsPvc, givenCommandListWithProfilingEventAfterCommandOnPvcRev00ThenMiFenceIsNotAdded) {
    using MI_MEM_FENCE = typename FamilyType::MI_MEM_FENCE;

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<IGFX_XE_HPC_CORE>>>();
    commandList->initialize(device, NEO::EngineGroupType::compute, 0u);
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
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device, result));
    commandList->appendEventForProfiling(event.get(), nullptr, false, false, false, false);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), 0), commandList->commandContainer.getCommandStream()->getUsed()));

    auto itor = find<MI_MEM_FENCE *>(cmdList.begin(), cmdList.end());
    EXPECT_EQ(cmdList.end(), itor);
}

using CommandListAppendBarrierXeHpcCore = Test<DeviceFixture>;

PVCTEST_F(CommandListAppendBarrierXeHpcCore, givenCommandListWhenAppendingBarrierThenPipeControlIsProgrammedAndHdcAndUnTypedFlushesAreSet) {
    using GfxFamily = typename NEO::GfxFamilyMapper<IGFX_XE_HPC_CORE>::GfxFamily;
    using PIPE_CONTROL = typename GfxFamily::PIPE_CONTROL;
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<IGFX_XE_HPC_CORE>>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    ze_result_t returnValue = commandList->appendBarrier(nullptr, 0, nullptr, false);
    EXPECT_EQ(returnValue, ZE_RESULT_SUCCESS);
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), 0), commandList->commandContainer.getCommandStream()->getUsed()));

    bool correctPcFound = false;
    auto itorPcs = findAll<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(0u, itorPcs.size());
    for (auto &itor : itorPcs) {
        auto pipeControlCmd = reinterpret_cast<typename FamilyType::PIPE_CONTROL *>(*itor);
        if (pipeControlCmd->getHdcPipelineFlush() && pipeControlCmd->getUnTypedDataPortCacheFlush()) {
            correctPcFound = true;
        }
    }

    EXPECT_TRUE(correctPcFound);
}

} // namespace ult
} // namespace L0
