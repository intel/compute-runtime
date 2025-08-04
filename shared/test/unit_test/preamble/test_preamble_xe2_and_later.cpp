/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/helpers/pipeline_select_args.h"
#include "shared/source/helpers/preamble.h"
#include "shared/test/common/fixtures/preamble_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"

using namespace NEO;

using atLeastXe2HpgCore = IsAtLeastXe2HpgCore;

using PreambleTest = ::testing::Test;

HWTEST2_F(PreambleTest, givenAtLeastXe2HpgCoreAndNotSetDebugFlagWhenPreambleCfeStateIsProgrammedThenStackIDControlIsDefault, IsXe2HpgCore) {
    using CFE_STATE = typename FamilyType::CFE_STATE;
    using STACK_ID_CONTROL = typename CFE_STATE::STACK_ID_CONTROL;

    char buffer[64];
    MockGraphicsAllocation graphicsAllocation(buffer, sizeof(buffer));
    LinearStream preambleStream(&graphicsAllocation, graphicsAllocation.getUnderlyingBuffer(), graphicsAllocation.getUnderlyingBufferSize());

    auto feCmdPtr = PreambleHelper<FamilyType>::getSpaceForVfeState(&preambleStream, *defaultHwInfo, EngineGroupType::renderCompute, nullptr);
    StreamProperties streamProperties{};

    MockExecutionEnvironment executionEnvironment{};
    PreambleHelper<FamilyType>::programVfeState(feCmdPtr, *executionEnvironment.rootDeviceEnvironments[0], 0u, 0, 0, streamProperties);

    auto &cfeState = *reinterpret_cast<CFE_STATE *>(feCmdPtr);
    EXPECT_EQ(cfeState.getStackIdControl(), static_cast<STACK_ID_CONTROL>(0b00u));
}

HWTEST2_F(PreambleTest, givenAtLeastXe2HpgCoreAndSetDebugFlagWhenPreambleCfeStateIsProgrammedThenStackIDControlIsChanged, IsXe2HpgCore) {
    using CFE_STATE = typename FamilyType::CFE_STATE;
    using STACK_ID_CONTROL = typename CFE_STATE::STACK_ID_CONTROL;
    char buffer[64];
    MockGraphicsAllocation graphicsAllocation(buffer, sizeof(buffer));
    LinearStream preambleStream(&graphicsAllocation, graphicsAllocation.getUnderlyingBuffer(), graphicsAllocation.getUnderlyingBufferSize());
    DebugManagerStateRestore debugRestore;

    uint64_t expectedBufferGpuAddress = preambleStream.getCurrentGpuAddressPosition();
    uint64_t cmdBufferGpuAddress = 0;

    debugManager.flags.CFEStackIDControl.set(0b10u);

    auto feCmdPtr = PreambleHelper<FamilyType>::getSpaceForVfeState(&preambleStream, *defaultHwInfo, EngineGroupType::renderCompute, &cmdBufferGpuAddress);
    StreamProperties streamProperties{};
    EXPECT_EQ(expectedBufferGpuAddress, cmdBufferGpuAddress);

    MockExecutionEnvironment executionEnvironment{};
    PreambleHelper<FamilyType>::programVfeState(feCmdPtr, *executionEnvironment.rootDeviceEnvironments[0], 0u, 0, 0, streamProperties);

    auto &cfeState = *reinterpret_cast<CFE_STATE *>(feCmdPtr);
    EXPECT_EQ(cfeState.getStackIdControl(), static_cast<STACK_ID_CONTROL>(0b10u));
}

HWTEST2_F(PreambleTest, givenPreambleHelperThenSystolicModeIsDisabled, atLeastXe2HpgCore) {
    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &rootDeviceEnvironment = *mockExecutionEnvironment.rootDeviceEnvironments[0];
    EXPECT_FALSE(PreambleHelper<FamilyType>::isSystolicModeConfigurable(rootDeviceEnvironment));
}

HWTEST2_F(PreambleTest, givenPreambleHelperThenGetCmdSizeForPipelineSelectReturnsZero, atLeastXe2HpgCore) {
    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &rootDeviceEnvironment = *mockExecutionEnvironment.rootDeviceEnvironments[0];
    EXPECT_EQ(0u, PreambleHelper<FamilyType>::getCmdSizeForPipelineSelect(rootDeviceEnvironment));
}

HWTEST2_F(PreambleTest, givenPreambleHelperAndDebugFlagThenGetCmdSizeForPipelineSelectReturnsCorrectSize, atLeastXe2HpgCore) {
    DebugManagerStateRestore restorer{};
    debugManager.flags.PipelinedPipelineSelect.set(true);

    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &rootDeviceEnvironment = *mockExecutionEnvironment.rootDeviceEnvironments[0];

    EXPECT_EQ(sizeof(typename FamilyType::PIPELINE_SELECT), PreambleHelper<FamilyType>::getCmdSizeForPipelineSelect(rootDeviceEnvironment));
}

HWTEST2_F(PreambleFixture, givenPreambleHelperWhenCallingProgramPipelineSelectThenStreamDoesNotChange, atLeastXe2HpgCore) {
    const auto initialStreamSize = linearStream.getUsed();
    PipelineSelectArgs pipelineArgs;
    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &rootDeviceEnvironment = *mockExecutionEnvironment.rootDeviceEnvironments[0];

    PreambleHelper<FamilyType>::programPipelineSelect(&linearStream, pipelineArgs, rootDeviceEnvironment);
    EXPECT_EQ(initialStreamSize, linearStream.getUsed());
}

HWTEST2_F(PreambleFixture, givenPreambleHelperAndDebugFlagEnabledWhenCallingProgramPipelineSelectThenPipelineSelectionIsProgrammed, atLeastXe2HpgCore) {
    using PIPELINE_SELECT = typename FamilyType::PIPELINE_SELECT;

    DebugManagerStateRestore restorer{};
    debugManager.flags.PipelinedPipelineSelect.set(true);

    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &rootDeviceEnvironment = *mockExecutionEnvironment.rootDeviceEnvironments[0];

    PipelineSelectArgs pipelineArgs;
    PreambleHelper<FamilyType>::programPipelineSelect(&linearStream, pipelineArgs, rootDeviceEnvironment);

    parseCommands<FamilyType>(linearStream);
    const auto cmdIt = find<PIPELINE_SELECT *>(cmdList.begin(), cmdList.end());

    ASSERT_NE(cmdIt, cmdList.cend());

    auto cmd = genCmdCast<PIPELINE_SELECT *>(*cmdIt);
    EXPECT_EQ(cmd->getPipelineSelection(), PIPELINE_SELECT::PIPELINE_SELECTION_GPGPU);
}
