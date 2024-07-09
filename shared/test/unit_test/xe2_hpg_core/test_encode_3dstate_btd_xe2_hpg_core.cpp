/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/unit_test/fixtures/command_container_fixture.h"

using namespace NEO;

using CommandEncodeEnableRayTracing = Test<CommandEncodeStatesFixture>;

HWTEST2_F(CommandEncodeEnableRayTracing, givenDefaultDebugFlagsWhenProgramEnableRayTracingThenBtdStateBodyIsSetProperly, IsXe2HpgCore) {
    using _3DSTATE_BTD = typename FamilyType::_3DSTATE_BTD;
    DebugManagerStateRestore restore;
    debugManager.flags.ForceTheMaximumNumberOfOutstandingRayqueriesPerSs.set(-1);
    debugManager.flags.ForceDispatchTimeoutCounter.set(-1);

    uint32_t pCmdBuffer[1024];
    uint32_t pMemoryBackedBuffer[1024];

    MockGraphicsAllocation gfxAllocation(static_cast<void *>(pCmdBuffer), sizeof(pCmdBuffer));
    LinearStream stream(&gfxAllocation);

    uint64_t memoryBackedBuffer = reinterpret_cast<uint64_t>(&pMemoryBackedBuffer);

    EncodeEnableRayTracing<FamilyType>::programEnableRayTracing(stream, memoryBackedBuffer);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, stream.getCpuBase(), stream.getUsed());

    auto iterator3dStateBtd = find<_3DSTATE_BTD *>(commands.begin(), commands.end());
    ASSERT_NE(iterator3dStateBtd, commands.end());

    auto cmd3dStateBtd = genCmdCast<_3DSTATE_BTD *>(*iterator3dStateBtd);
    EXPECT_EQ(static_cast<int32_t>(cmd3dStateBtd->getBtdStateBody().getControlsTheMaximumNumberOfOutstandingRayqueriesPerSs()), 0);
    EXPECT_EQ(static_cast<int32_t>(cmd3dStateBtd->getBtdStateBody().getDispatchTimeoutCounter()), 0);
}

HWTEST2_F(CommandEncodeEnableRayTracing, givenDebugFlagsWhenProgramEnableRayTracingThenBtdStateBodyIsSetProperly, IsXe2HpgCore) {
    using _3DSTATE_BTD = typename FamilyType::_3DSTATE_BTD;
    DebugManagerStateRestore restore;
    debugManager.flags.ForceTheMaximumNumberOfOutstandingRayqueriesPerSs.set(1);
    debugManager.flags.ForceDispatchTimeoutCounter.set(2);

    uint32_t pCmdBuffer[1024];
    uint32_t pMemoryBackedBuffer[1024];

    MockGraphicsAllocation gfxAllocation(static_cast<void *>(pCmdBuffer), sizeof(pCmdBuffer));
    LinearStream stream(&gfxAllocation);

    uint64_t memoryBackedBuffer = reinterpret_cast<uint64_t>(&pMemoryBackedBuffer);

    EncodeEnableRayTracing<FamilyType>::programEnableRayTracing(stream, memoryBackedBuffer);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, stream.getCpuBase(), stream.getUsed());

    auto iterator3dStateBtd = find<_3DSTATE_BTD *>(commands.begin(), commands.end());
    ASSERT_NE(iterator3dStateBtd, commands.end());

    auto cmd3dStateBtd = genCmdCast<_3DSTATE_BTD *>(*iterator3dStateBtd);
    EXPECT_EQ(static_cast<int32_t>(cmd3dStateBtd->getBtdStateBody().getControlsTheMaximumNumberOfOutstandingRayqueriesPerSs()), debugManager.flags.ForceTheMaximumNumberOfOutstandingRayqueriesPerSs.get());
    EXPECT_EQ(static_cast<int32_t>(cmd3dStateBtd->getBtdStateBody().getDispatchTimeoutCounter()), debugManager.flags.ForceDispatchTimeoutCounter.get());
}
