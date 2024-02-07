/*
 * Copyright (C) 2019-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen11/hw_cmds.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/helpers/gfx_core_helper_tests.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"

using GfxCoreHelperTestGen11 = GfxCoreHelperTest;

GEN11TEST_F(GfxCoreHelperTestGen11, WhenGettingMaxBarriersPerSliceThenCorrectSizeIsReturned) {
    auto &helper = getHelper<GfxCoreHelper>();
    EXPECT_EQ(32u, helper.getMaxBarrierRegisterPerSlice());
}

GEN11TEST_F(GfxCoreHelperTestGen11, WhenGettingPitchAlignmentForImageThenCorrectValueIsReturned) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    EXPECT_EQ(4u, gfxCoreHelper.getPitchAlignmentForImage(pDevice->getRootDeviceEnvironment()));
}

GEN11TEST_F(GfxCoreHelperTestGen11, WhenAdjustingDefaultEngineTypeThenEngineTypeIsSet) {
    auto engineType = hardwareInfo.capabilityTable.defaultEngineType;
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    auto &productHelper = getHelper<ProductHelper>();

    gfxCoreHelper.adjustDefaultEngineType(&hardwareInfo, productHelper, nullptr);
    EXPECT_EQ(engineType, hardwareInfo.capabilityTable.defaultEngineType);
}

GEN11TEST_F(GfxCoreHelperTestGen11, whenGetGpgpuEnginesThenReturnThreeRcsEngines) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    auto gpgpuEngines = gfxCoreHelper.getGpgpuEngineInstances(pDevice->getRootDeviceEnvironment());
    EXPECT_EQ(3u, gpgpuEngines.size());
    EXPECT_EQ(aub_stream::ENGINE_RCS, gpgpuEngines[0].first);
    EXPECT_EQ(aub_stream::ENGINE_RCS, gpgpuEngines[1].first);
    EXPECT_EQ(aub_stream::ENGINE_RCS, gpgpuEngines[2].first);
    EXPECT_EQ(3u, pDevice->allEngines.size());
}

using MemorySynchronizatiopCommandsTestsGen11 = ::testing::Test;
GEN11TEST_F(MemorySynchronizatiopCommandsTestsGen11, WhenProgrammingCacheFlushThenExpectConstantCacheFieldSet) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    std::unique_ptr<uint8_t> buffer(new uint8_t[128]);
    MockExecutionEnvironment mockExecutionEnvironment{};
    LinearStream stream(buffer.get(), 128);
    MemorySynchronizationCommands<FamilyType>::addFullCacheFlush(stream, *mockExecutionEnvironment.rootDeviceEnvironments[0]);
    PIPE_CONTROL *pipeControl = genCmdCast<PIPE_CONTROL *>(buffer.get());
    ASSERT_NE(nullptr, pipeControl);
    EXPECT_TRUE(pipeControl->getConstantCacheInvalidationEnable());
}

GEN11TEST_F(GfxCoreHelperTestGen11, givenGen11WhenCallIsPackedSupportedThenReturnTrue) {
    auto &helper = pDevice->getGfxCoreHelper();
    EXPECT_TRUE(helper.packedFormatsSupported());
}
