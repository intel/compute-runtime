/*
 * Copyright (C) 2025 Intel Corporation
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

HWTEST2_F(CommandEncodeEnableRayTracing, givenDefaultDebugFlagsWhenProgramEnableRayTracingThenIsSetProperly, IsXe3Core) {
    using _3DSTATE_BTD = typename FamilyType::_3DSTATE_BTD;
    DebugManagerStateRestore restore;
    debugManager.flags.ForceTheMaximumNumberOfOutstandingRayqueriesPerSs.set(-1);
    debugManager.flags.ForceDispatchTimeoutCounter.set(-1);

    uint32_t pCmdBuffer[1024];
    uint32_t pMemoryBackedBuffer[1024];

    MockGraphicsAllocation gfxAllocation(static_cast<void *>(pCmdBuffer), sizeof(pCmdBuffer));
    LinearStream stream(&gfxAllocation);

    uint64_t memoryBackedBuffer = reinterpret_cast<uint64_t>(pMemoryBackedBuffer);

    EncodeEnableRayTracing<FamilyType>::programEnableRayTracing(stream, memoryBackedBuffer);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, stream.getCpuBase(), stream.getUsed());

    auto iterator3dStateBtd = find<_3DSTATE_BTD *>(commands.begin(), commands.end());
    ASSERT_NE(iterator3dStateBtd, commands.end());

    auto cmd3dStateBtd = genCmdCast<_3DSTATE_BTD *>(*iterator3dStateBtd);
    EXPECT_EQ(static_cast<int32_t>(cmd3dStateBtd->getControlsTheMaximumNumberOfOutstandingRayqueriesPerSs()), 0);
    EXPECT_EQ(static_cast<int32_t>(cmd3dStateBtd->getDispatchTimeoutCounter()), 0);
}

HWTEST2_F(CommandEncodeEnableRayTracing, givenDebugFlagsWhenProgramEnableRayTracingThenIsSetProperly, IsXe3Core) {
    using _3DSTATE_BTD = typename FamilyType::_3DSTATE_BTD;
    DebugManagerStateRestore restore;
    debugManager.flags.ForceTheMaximumNumberOfOutstandingRayqueriesPerSs.set(1);
    debugManager.flags.ForceDispatchTimeoutCounter.set(2);

    uint32_t pCmdBuffer[1024];
    uint32_t pMemoryBackedBuffer[1024];

    MockGraphicsAllocation gfxAllocation(static_cast<void *>(pCmdBuffer), sizeof(pCmdBuffer));
    LinearStream stream(&gfxAllocation);

    uint64_t memoryBackedBuffer = reinterpret_cast<uint64_t>(pMemoryBackedBuffer);

    EncodeEnableRayTracing<FamilyType>::programEnableRayTracing(stream, memoryBackedBuffer);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, stream.getCpuBase(), stream.getUsed());

    auto iterator3dStateBtd = find<_3DSTATE_BTD *>(commands.begin(), commands.end());
    ASSERT_NE(iterator3dStateBtd, commands.end());

    auto cmd3dStateBtd = genCmdCast<_3DSTATE_BTD *>(*iterator3dStateBtd);
    EXPECT_EQ(static_cast<int32_t>(cmd3dStateBtd->getControlsTheMaximumNumberOfOutstandingRayqueriesPerSs()), debugManager.flags.ForceTheMaximumNumberOfOutstandingRayqueriesPerSs.get());
    EXPECT_EQ(static_cast<int32_t>(cmd3dStateBtd->getDispatchTimeoutCounter()), debugManager.flags.ForceDispatchTimeoutCounter.get());
}

struct CommandEncodeEnableRayTracingAndEnable64bAddressingForRayTracing
    : public CommandEncodeStatesFixture,
      public ::testing::TestWithParam<int32_t> {

    void SetUp() override {
        CommandEncodeStatesFixture::setUp();
    }

    void TearDown() override {
        CommandEncodeStatesFixture::tearDown();
    }
};

HWTEST2_P(CommandEncodeEnableRayTracingAndEnable64bAddressingForRayTracing, givenXe364bAddresingForRTDebugFlagsWhenProgramEnableRayTracingThenIsSetProperlyXe3, IsXe3Core) {
    using _3DSTATE_BTD = typename FamilyType::_3DSTATE_BTD;
    DebugManagerStateRestore restore;
    int32_t enable64bAddressingForRayTracing = GetParam();
    debugManager.flags.Enable64bAddressingForRayTracing.set(enable64bAddressingForRayTracing);

    uint32_t pCmdBuffer[1024]{};
    uint32_t pMemoryBackedBuffer[1024]{};

    MockGraphicsAllocation gfxAllocation(static_cast<void *>(pCmdBuffer), sizeof(pCmdBuffer));
    LinearStream stream(&gfxAllocation);

    uint64_t memoryBackedBuffer = reinterpret_cast<uint64_t>(pMemoryBackedBuffer);

    EncodeEnableRayTracing<FamilyType>::programEnableRayTracing(stream, memoryBackedBuffer);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, stream.getCpuBase(), stream.getUsed());

    auto iterator3dStateBtd = find<_3DSTATE_BTD *>(commands.begin(), commands.end());
    ASSERT_NE(iterator3dStateBtd, commands.end());

    auto cmd3dStateBtd = genCmdCast<_3DSTATE_BTD *>(*iterator3dStateBtd);
    EXPECT_EQ(static_cast<bool>(cmd3dStateBtd->getRtMemStructures64BModeEnable()), enable64bAddressingForRayTracing == -1 ? true : static_cast<bool>(enable64bAddressingForRayTracing));
}

INSTANTIATE_TEST_SUITE_P(
    Enable64bAddressingForRayTracingValues,
    CommandEncodeEnableRayTracingAndEnable64bAddressingForRayTracing,
    ::testing::Values(-1, 0, 1));
