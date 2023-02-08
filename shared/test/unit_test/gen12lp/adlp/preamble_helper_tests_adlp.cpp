/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/csr_definitions.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/gen12lp/hw_cmds_adlp.h"
#include "shared/source/gen12lp/hw_info_adlp.h"
#include "shared/source/helpers/pipeline_select_helper.h"
#include "shared/source/helpers/preamble.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/helpers/dispatch_flags_helper.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

using PreambleHelperTestsAdlp = ::testing::Test;

ADLPTEST_F(PreambleHelperTestsAdlp, givenSystolicPipelineSelectModeDisabledWhenProgrammingPipelineSelectThenDisableSystolicMode) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using PIPELINE_SELECT = typename FamilyType::PIPELINE_SELECT;

    constexpr static auto bufferSize = sizeof(PIPE_CONTROL) + sizeof(PIPE_CONTROL);

    char streamBuffer[bufferSize];
    LinearStream stream(streamBuffer, bufferSize);

    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &rootDeviceEnvironment = *mockExecutionEnvironment.rootDeviceEnvironments[0];

    DispatchFlags flags = DispatchFlagsHelper::createDefaultDispatchFlags();
    flags.pipelineSelectArgs.systolicPipelineSelectMode = false;
    flags.pipelineSelectArgs.systolicPipelineSelectSupport = PreambleHelper<FamilyType>::isSystolicModeConfigurable(rootDeviceEnvironment);

    PreambleHelper<FamilyType>::programPipelineSelect(&stream, flags.pipelineSelectArgs, rootDeviceEnvironment);
    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(stream, 0);

    GenCmdList pipeControlList = hwParser.getCommandsList<PIPE_CONTROL>();
    auto itorPipeControl = find<PIPE_CONTROL *>(pipeControlList.begin(), pipeControlList.end());
    const auto &pc = *reinterpret_cast<PIPE_CONTROL *>(*itorPipeControl);
    EXPECT_TRUE(pc.getCommandStreamerStallEnable());

    GenCmdList pipelineSelectList = hwParser.getCommandsList<PIPELINE_SELECT>();
    auto itorPipelineSelect = find<PIPELINE_SELECT *>(pipelineSelectList.begin(), pipelineSelectList.end());
    const auto &ps = *reinterpret_cast<PIPELINE_SELECT *>(*itorPipelineSelect);

    const auto expectedMask = pipelineSelectEnablePipelineSelectMaskBits | pipelineSelectMediaSamplerDopClockGateMaskBits | pipelineSelectSystolicModeEnableMaskBits;
    EXPECT_FALSE(ps.getSpecialModeEnable());
    EXPECT_EQ(expectedMask, ps.getMaskBits());
    EXPECT_EQ(PIPELINE_SELECT::PIPELINE_SELECTION_GPGPU, ps.getPipelineSelection());
}

ADLPTEST_F(PreambleHelperTestsAdlp, givenSystolicPipelineSelectModeEnabledWhenProgrammingPipelineSelectThenEnableSystolicMode) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using PIPELINE_SELECT = typename FamilyType::PIPELINE_SELECT;

    constexpr static auto bufferSize = sizeof(PIPE_CONTROL) + sizeof(PIPE_CONTROL);

    char streamBuffer[bufferSize];
    LinearStream stream(streamBuffer, bufferSize);
    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &rootDeviceEnvironment = *mockExecutionEnvironment.rootDeviceEnvironments[0];

    DispatchFlags flags = DispatchFlagsHelper::createDefaultDispatchFlags();
    flags.pipelineSelectArgs.systolicPipelineSelectMode = true;
    flags.pipelineSelectArgs.systolicPipelineSelectSupport = PreambleHelper<FamilyType>::isSystolicModeConfigurable(rootDeviceEnvironment);

    PreambleHelper<FamilyType>::programPipelineSelect(&stream, flags.pipelineSelectArgs, rootDeviceEnvironment);
    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(stream, 0);

    GenCmdList pipeControlList = hwParser.getCommandsList<PIPE_CONTROL>();
    auto itorPipeControl = find<PIPE_CONTROL *>(pipeControlList.begin(), pipeControlList.end());
    const auto &pc = *reinterpret_cast<PIPE_CONTROL *>(*itorPipeControl);
    EXPECT_TRUE(pc.getCommandStreamerStallEnable());

    GenCmdList pipelineSelectList = hwParser.getCommandsList<PIPELINE_SELECT>();
    auto itorPipelineSelect = find<PIPELINE_SELECT *>(pipelineSelectList.begin(), pipelineSelectList.end());
    const auto &ps = *reinterpret_cast<PIPELINE_SELECT *>(*itorPipelineSelect);

    const auto expectedMask = pipelineSelectEnablePipelineSelectMaskBits | pipelineSelectMediaSamplerDopClockGateMaskBits | pipelineSelectSystolicModeEnableMaskBits;
    EXPECT_TRUE(ps.getSpecialModeEnable());
    EXPECT_EQ(expectedMask, ps.getMaskBits());
    EXPECT_EQ(PIPELINE_SELECT::PIPELINE_SELECTION_GPGPU, ps.getPipelineSelection());
}
