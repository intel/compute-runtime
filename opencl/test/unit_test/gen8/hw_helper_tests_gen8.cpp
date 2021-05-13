/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/constants.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/unit_test/helpers/get_gpgpu_engines_tests.inl"

#include "opencl/source/helpers/cl_hw_helper.h"
#include "opencl/source/helpers/hardware_commands_helper.h"
#include "opencl/test/unit_test/helpers/hw_helper_tests.h"
#include "opencl/test/unit_test/mocks/mock_cl_hw_helper.h"

using HwHelperTestGen8 = HwHelperTest;

GEN8TEST_F(HwHelperTestGen8, WhenGettingMaxBarriersPerSliceThenCorrectSizeIsReturned) {
    auto &helper = HwHelper::get(renderCoreFamily);

    EXPECT_EQ(16u, helper.getMaxBarrierRegisterPerSlice());
}

GEN8TEST_F(HwHelperTestGen8, WhenSettingCapabilityCoherencyFlagThenFlagIsSet) {
    auto &helper = HwHelper::get(renderCoreFamily);

    bool coherency = false;
    helper.setCapabilityCoherencyFlag(&hardwareInfo, coherency);
    EXPECT_TRUE(coherency);
}

GEN8TEST_F(HwHelperTestGen8, WhenGettingPitchAlignmentForImageThenCorrectValueIsReturned) {
    auto &helper = HwHelper::get(renderCoreFamily);
    EXPECT_EQ(4u, helper.getPitchAlignmentForImage(&hardwareInfo));
}

GEN8TEST_F(HwHelperTestGen8, WhenAdjustingDefaultEngineTypeThenEngineTypeIsSet) {
    auto engineType = hardwareInfo.capabilityTable.defaultEngineType;
    auto &helper = HwHelper::get(renderCoreFamily);
    helper.adjustDefaultEngineType(&hardwareInfo);
    EXPECT_EQ(engineType, hardwareInfo.capabilityTable.defaultEngineType);
}

GEN8TEST_F(HwHelperTestGen8, givenGen8PlatformWhenSetupHardwareCapabilitiesIsCalledThenSpecificImplementationIsUsed) {
    auto &helper = HwHelper::get(renderCoreFamily);
    HardwareCapabilities hwCaps = {0};
    helper.setupHardwareCapabilities(&hwCaps, hardwareInfo);

    EXPECT_EQ(2048u, hwCaps.image3DMaxHeight);
    EXPECT_EQ(2048u, hwCaps.image3DMaxWidth);
    EXPECT_EQ(2 * MemoryConstants::gigaByte - 8 * MemoryConstants::megaByte, hwCaps.maxMemAllocSize);
    EXPECT_FALSE(hwCaps.isStatelesToStatefullWithOffsetSupported);
}

GEN8TEST_F(HwHelperTestGen8, whenGetGpgpuEnginesThenReturnThreeEngines) {
    whenGetGpgpuEnginesThenReturnTwoRcsEngines<FamilyType>(pDevice->getHardwareInfo());
    EXPECT_EQ(3u, pDevice->engines.size());
}

GEN8TEST_F(HwHelperTestGen8, WhenGettingDeviceIpVersionThenMakeCorrectDeviceIpVersion) {
    EXPECT_EQ(ClHwHelperMock::makeDeviceIpVersion(8, 0, 0), ClHwHelper::get(renderCoreFamily).getDeviceIpVersion(*defaultHwInfo));
}

GEN8TEST_F(HwHelperTestGen8, WhenGettingSupportedDeviceFeatureCapabilitiesThenReturnCorrectValue) {
    EXPECT_EQ(0u, ClHwHelper::get(renderCoreFamily).getSupportedDeviceFeatureCapabilities());
}

using MemorySynchronizatiopCommandsTestsGen8 = ::testing::Test;
GEN8TEST_F(MemorySynchronizatiopCommandsTestsGen8, WhenProgrammingCacheFlushThenExpectConstantCacheFieldSet) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    std::unique_ptr<uint8_t> buffer(new uint8_t[128]);

    LinearStream stream(buffer.get(), 128);
    MemorySynchronizationCommands<FamilyType>::addFullCacheFlush(stream);
    PIPE_CONTROL *pipeControl = genCmdCast<PIPE_CONTROL *>(buffer.get());
    ASSERT_NE(nullptr, pipeControl);
    EXPECT_TRUE(pipeControl->getConstantCacheInvalidationEnable());
}
