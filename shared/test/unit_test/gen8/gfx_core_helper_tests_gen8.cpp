/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen8/hw_cmds.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/helpers/gfx_core_helper_tests.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"

using GfxCoreHelperTestGen8 = GfxCoreHelperTest;

GEN8TEST_F(GfxCoreHelperTestGen8, WhenGettingMaxBarriersPerSliceThenCorrectSizeIsReturned) {
    auto &helper = getHelper<GfxCoreHelper>();

    EXPECT_EQ(16u, helper.getMaxBarrierRegisterPerSlice());
}

GEN8TEST_F(GfxCoreHelperTestGen8, WhenGettingPitchAlignmentForImageThenCorrectValueIsReturned) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    EXPECT_EQ(4u, gfxCoreHelper.getPitchAlignmentForImage(pDevice->getRootDeviceEnvironment()));
}

GEN8TEST_F(GfxCoreHelperTestGen8, WhenAdjustingDefaultEngineTypeThenEngineTypeIsSet) {
    auto engineType = hardwareInfo.capabilityTable.defaultEngineType;
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    auto &productHelper = getHelper<ProductHelper>();
    gfxCoreHelper.adjustDefaultEngineType(&hardwareInfo, productHelper, nullptr);
    EXPECT_EQ(engineType, hardwareInfo.capabilityTable.defaultEngineType);
}

GEN8TEST_F(GfxCoreHelperTestGen8, whenGetGpgpuEnginesThenReturnThreeEngines) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    auto gpgpuEngines = gfxCoreHelper.getGpgpuEngineInstances(pDevice->getRootDeviceEnvironment());
    EXPECT_EQ(3u, gpgpuEngines.size());
    EXPECT_EQ(aub_stream::ENGINE_RCS, gpgpuEngines[0].first);
    EXPECT_EQ(aub_stream::ENGINE_RCS, gpgpuEngines[1].first);
    EXPECT_EQ(aub_stream::ENGINE_RCS, gpgpuEngines[2].first);
    EXPECT_EQ(3u, pDevice->allEngines.size());
}

using MemorySynchronizatiopCommandsTestsGen8 = ::testing::Test;
GEN8TEST_F(MemorySynchronizatiopCommandsTestsGen8, WhenProgrammingCacheFlushThenExpectConstantCacheFieldSet) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    std::unique_ptr<uint8_t> buffer(new uint8_t[128]);
    MockExecutionEnvironment mockExecutionEnvironment{};
    LinearStream stream(buffer.get(), 128);
    MemorySynchronizationCommands<FamilyType>::addFullCacheFlush(stream, *mockExecutionEnvironment.rootDeviceEnvironments[0]);
    PIPE_CONTROL *pipeControl = genCmdCast<PIPE_CONTROL *>(buffer.get());
    ASSERT_NE(nullptr, pipeControl);
    EXPECT_TRUE(pipeControl->getConstantCacheInvalidationEnable());
}

using CompilerProductHelperTestGen8 = Test<DeviceFixture>;
GEN8TEST_F(CompilerProductHelperTestGen8, givenHwInfosWhenIsMatrixMultiplyAccumulateSupportedThenReturnFalse) {
    auto &compilerProductHelper = getHelper<CompilerProductHelper>();
    auto releaseHelper = this->pDevice->getReleaseHelper();
    EXPECT_FALSE(compilerProductHelper.isMatrixMultiplyAccumulateSupported(releaseHelper));
}
