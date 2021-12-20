/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/helpers/hw_helper_tests.h"
#include "shared/test/unit_test/helpers/get_gpgpu_engines_tests.inl"

#include "opencl/source/helpers/cl_hw_helper.h"
#include "opencl/test/unit_test/mocks/mock_cl_hw_helper.h"

using HwHelperTestGen11 = HwHelperTest;

GEN11TEST_F(HwHelperTestGen11, WhenGettingMaxBarriersPerSliceThenCorrectSizeIsReturned) {
    auto &helper = HwHelper::get(renderCoreFamily);
    EXPECT_EQ(32u, helper.getMaxBarrierRegisterPerSlice());
}

GEN11TEST_F(HwHelperTestGen11, WhenGettingPitchAlignmentForImageThenCorrectValueIsReturned) {
    auto &helper = HwHelper::get(renderCoreFamily);
    EXPECT_EQ(4u, helper.getPitchAlignmentForImage(&hardwareInfo));
}

GEN11TEST_F(HwHelperTestGen11, WhenAdjustingDefaultEngineTypeThenEngineTypeIsSet) {
    auto engineType = hardwareInfo.capabilityTable.defaultEngineType;
    auto &helper = HwHelper::get(renderCoreFamily);
    helper.adjustDefaultEngineType(&hardwareInfo);
    EXPECT_EQ(engineType, hardwareInfo.capabilityTable.defaultEngineType);
}

GEN11TEST_F(HwHelperTestGen11, whenGetGpgpuEnginesThenReturnThreeRcsEngines) {
    whenGetGpgpuEnginesThenReturnTwoRcsEngines<FamilyType>(pDevice->getHardwareInfo());
    EXPECT_EQ(3u, pDevice->allEngines.size());
}

GEN11TEST_F(HwHelperTestGen11, WhenGettingDeviceIpVersionThenMakeCorrectDeviceIpVersion) {
    EXPECT_EQ(ClHwHelperMock::makeDeviceIpVersion(11, 0, 0), ClHwHelper::get(renderCoreFamily).getDeviceIpVersion(*defaultHwInfo));
}

GEN11TEST_F(HwHelperTestGen11, WhenGettingSupportedDeviceFeatureCapabilitiesThenReturnCorrectValue) {
    EXPECT_EQ(0u, ClHwHelper::get(renderCoreFamily).getSupportedDeviceFeatureCapabilities());
}

using MemorySynchronizatiopCommandsTestsGen11 = ::testing::Test;
GEN11TEST_F(MemorySynchronizatiopCommandsTestsGen11, WhenProgrammingCacheFlushThenExpectConstantCacheFieldSet) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    std::unique_ptr<uint8_t> buffer(new uint8_t[128]);

    LinearStream stream(buffer.get(), 128);
    MemorySynchronizationCommands<FamilyType>::addFullCacheFlush(stream, *defaultHwInfo);
    PIPE_CONTROL *pipeControl = genCmdCast<PIPE_CONTROL *>(buffer.get());
    ASSERT_NE(nullptr, pipeControl);
    EXPECT_TRUE(pipeControl->getConstantCacheInvalidationEnable());
}

GEN11TEST_F(MemorySynchronizatiopCommandsTestsGen11, givenGen11WhenCallIsPackedSupportedThenReturnTrue) {
    auto &helper = HwHelper::get(renderCoreFamily);
    EXPECT_TRUE(helper.packedFormatsSupported());
}
