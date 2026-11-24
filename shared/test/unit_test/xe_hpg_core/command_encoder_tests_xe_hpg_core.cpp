/*
 * Copyright (C) 2022-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/helpers/cache_flush_xehp_and_later.inl"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/release_helpers/caps/caps_setup.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/unit_test/fixtures/command_container_fixture.h"

using namespace NEO;

using L3ControlTests = ::testing::Test;

HWTEST2_F(L3ControlTests, givenL3ControlWhenAdjustCalledThenUnTypedDataPortCacheFlushIsSet, IsXeHpgCore) {
    using L3_CONTROL = typename FamilyType::L3_CONTROL;
    auto l3Control = FamilyType::cmdInitL3Control;
    auto l3ControlOnStart = l3Control;

    adjustL3ControlField<FamilyType>(&l3Control);
    EXPECT_NE(0, memcmp(&l3ControlOnStart, &l3Control, sizeof(L3_CONTROL))); // no change

    EXPECT_FALSE(l3ControlOnStart.getUnTypedDataPortCacheFlush());
    EXPECT_TRUE(l3Control.getUnTypedDataPortCacheFlush());
}

using CommandEncodeStatesXeHpgCoreTest = Test<CommandEncodeStatesFixture>;

XE_HPG_CORETEST_F(CommandEncodeStatesXeHpgCoreTest, givenCommandContainerWhenAdjustPipelineSelectCalledThenCommandHasGpgpuType) {
    using PIPELINE_SELECT = typename FamilyType::PIPELINE_SELECT;
    EncodeComputeMode<FamilyType>::adjustPipelineSelect(*cmdContainer.get(), descriptor);
    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    auto itorCmd = find<PIPELINE_SELECT *>(commands.begin(), commands.end());
    ASSERT_NE(itorCmd, commands.end());

    auto cmd = genCmdCast<PIPELINE_SELECT *>(*itorCmd);
    EXPECT_EQ(cmd->getPipelineSelection(), PIPELINE_SELECT::PIPELINE_SELECTION_GPGPU);
}

void testProgrammingStateComputeModeXeLpgWithEnabledWa(ExecutionEnvironment &executionEnvironment) {
    using STATE_COMPUTE_MODE = typename XeHpgCoreFamily::STATE_COMPUTE_MODE;
    uint8_t buffer[64]{};
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];
    StateComputeModeProperties properties;
    auto pLinearStream = std::make_unique<LinearStream>(buffer, sizeof(buffer));
    EncodeComputeMode<XeHpgCoreFamily>::programComputeModeCommand(*pLinearStream, properties, rootDeviceEnvironment);
    auto pScm = reinterpret_cast<STATE_COMPUTE_MODE *>(pLinearStream->getCpuBase());
    auto expectedMask = XeHpgCoreFamily::stateComputeModeLargeGrfModeMask;
    EXPECT_EQ(expectedMask, pScm->getMask());
    EXPECT_EQ(STATE_COMPUTE_MODE::PIXEL_ASYNC_COMPUTE_THREAD_LIMIT_DISABLED, pScm->getPixelAsyncComputeThreadLimit());
    EXPECT_EQ(STATE_COMPUTE_MODE::Z_PASS_ASYNC_COMPUTE_THREAD_LIMIT_MAX_60, pScm->getZPassAsyncComputeThreadLimit());
    EXPECT_FALSE(pScm->getLargeGrfMode());

    properties.isCoherencyRequired.value = 1;
    properties.zPassAsyncComputeThreadLimit.value = 1;
    properties.pixelAsyncComputeThreadLimit.value = 1;
    properties.largeGrfMode.value = 1;
    pLinearStream = std::make_unique<LinearStream>(buffer, sizeof(buffer));
    EncodeComputeMode<XeHpgCoreFamily>::programComputeModeCommand(*pLinearStream, properties, rootDeviceEnvironment);
    pScm = reinterpret_cast<STATE_COMPUTE_MODE *>(pLinearStream->getCpuBase());
    expectedMask |= XeHpgCoreFamily::stateComputeModeZPassAsyncComputeThreadLimitMask |
                    XeHpgCoreFamily::stateComputeModePixelAsyncComputeThreadLimitMask;
    EXPECT_EQ(expectedMask, pScm->getMask());
    EXPECT_EQ(STATE_COMPUTE_MODE::PIXEL_ASYNC_COMPUTE_THREAD_LIMIT_MAX_2, pScm->getPixelAsyncComputeThreadLimit());
    EXPECT_EQ(STATE_COMPUTE_MODE::Z_PASS_ASYNC_COMPUTE_THREAD_LIMIT_MAX_64, pScm->getZPassAsyncComputeThreadLimit());
    EXPECT_TRUE(pScm->getLargeGrfMode());
}

void testProgrammingStateComputeModeXeLpgWithDisabledWa(ExecutionEnvironment &executionEnvironment) {
    using STATE_COMPUTE_MODE = typename XeHpgCoreFamily::STATE_COMPUTE_MODE;
    uint8_t buffer[64]{};
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];
    StateComputeModeProperties properties;
    auto pLinearStream = std::make_unique<LinearStream>(buffer, sizeof(buffer));
    EncodeComputeMode<XeHpgCoreFamily>::programComputeModeCommand(*pLinearStream, properties, rootDeviceEnvironment);
    auto pScm = reinterpret_cast<STATE_COMPUTE_MODE *>(pLinearStream->getCpuBase());
    EXPECT_EQ(0u, pScm->getMask());
    EXPECT_EQ(STATE_COMPUTE_MODE::PIXEL_ASYNC_COMPUTE_THREAD_LIMIT_DISABLED, pScm->getPixelAsyncComputeThreadLimit());
    EXPECT_EQ(STATE_COMPUTE_MODE::Z_PASS_ASYNC_COMPUTE_THREAD_LIMIT_MAX_60, pScm->getZPassAsyncComputeThreadLimit());
    EXPECT_FALSE(pScm->getLargeGrfMode());

    properties.isCoherencyRequired.value = 1;
    properties.zPassAsyncComputeThreadLimit.value = 1;
    properties.pixelAsyncComputeThreadLimit.value = 1;
    properties.largeGrfMode.value = 1;
    pLinearStream = std::make_unique<LinearStream>(buffer, sizeof(buffer));
    EncodeComputeMode<XeHpgCoreFamily>::programComputeModeCommand(*pLinearStream, properties, rootDeviceEnvironment);
    pScm = reinterpret_cast<STATE_COMPUTE_MODE *>(pLinearStream->getCpuBase());
    EXPECT_EQ(0u, pScm->getMask());
    EXPECT_EQ(STATE_COMPUTE_MODE::PIXEL_ASYNC_COMPUTE_THREAD_LIMIT_DISABLED, pScm->getPixelAsyncComputeThreadLimit());
    EXPECT_EQ(STATE_COMPUTE_MODE::Z_PASS_ASYNC_COMPUTE_THREAD_LIMIT_MAX_60, pScm->getZPassAsyncComputeThreadLimit());
    EXPECT_FALSE(pScm->getLargeGrfMode());

    properties.isCoherencyRequired.isDirty = true;
    properties.zPassAsyncComputeThreadLimit.isDirty = true;
    properties.pixelAsyncComputeThreadLimit.isDirty = true;
    properties.largeGrfMode.isDirty = true;
    pLinearStream = std::make_unique<LinearStream>(buffer, sizeof(buffer));
    EncodeComputeMode<XeHpgCoreFamily>::programComputeModeCommand(*pLinearStream, properties, rootDeviceEnvironment);
    pScm = reinterpret_cast<STATE_COMPUTE_MODE *>(pLinearStream->getCpuBase());
    auto expectedMask = XeHpgCoreFamily::stateComputeModeZPassAsyncComputeThreadLimitMask | XeHpgCoreFamily::stateComputeModePixelAsyncComputeThreadLimitMask |
                        XeHpgCoreFamily::stateComputeModeLargeGrfModeMask;
    EXPECT_EQ(expectedMask, pScm->getMask());
    EXPECT_EQ(STATE_COMPUTE_MODE::PIXEL_ASYNC_COMPUTE_THREAD_LIMIT_MAX_2, pScm->getPixelAsyncComputeThreadLimit());
    EXPECT_EQ(STATE_COMPUTE_MODE::Z_PASS_ASYNC_COMPUTE_THREAD_LIMIT_MAX_64, pScm->getZPassAsyncComputeThreadLimit());
    EXPECT_TRUE(pScm->getLargeGrfMode());
}

using CommandEncoderXeHpgTests = ::testing::Test;

HWTEST2_F(CommandEncoderXeHpgTests, whenProgrammingStateComputeModeThenProperFieldsAreSet, IsXeLpg) {
    for (auto stateComputeCommandWARequired : {true, false}) {

        MockExecutionEnvironment executionEnvironment{};
        auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];
        auto hwInfo = rootDeviceEnvironment.getMutableHardwareInfo();
        hwInfo->caps.programAllStateComputeCommandFieldsWARequired = stateComputeCommandWARequired;
        if (hwInfo->caps.programAllStateComputeCommandFieldsWARequired) {
            testProgrammingStateComputeModeXeLpgWithEnabledWa(executionEnvironment);
        } else {
            testProgrammingStateComputeModeXeLpgWithDisabledWa(executionEnvironment);
        }
    }
}
