/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/os_interface/windows/hw_info_config_win_tests.h"

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/test.h"

namespace NEO {

template <>
int HwInfoConfigHw<IGFX_UNKNOWN>::configureHardwareCustom(HardwareInfo *hwInfo, OSInterface *osIface) {
    return 0;
}

HwInfoConfigTestWindows::HwInfoConfigTestWindows() {
    this->executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    this->rootDeviceEnvironment = std::make_unique<RootDeviceEnvironment>(*executionEnvironment);
}

HwInfoConfigTestWindows::~HwInfoConfigTestWindows() {
}

void HwInfoConfigTestWindows::SetUp() {
    HwInfoConfigTest::SetUp();

    osInterface.reset(new OSInterface());

    auto wddm = Wddm::createWddm(nullptr, *rootDeviceEnvironment);
    wddm->init();
    outHwInfo = *rootDeviceEnvironment->getHardwareInfo();
}

void HwInfoConfigTestWindows::TearDown() {
    HwInfoConfigTest::TearDown();
}

TEST_F(HwInfoConfigTestWindows, givenCorrectParametersWhenConfiguringHwInfoThenReturnSuccess) {
    int ret = hwConfig.configureHwInfoWddm(&pInHwInfo, &outHwInfo, osInterface.get());
    EXPECT_EQ(0, ret);
}

TEST_F(HwInfoConfigTestWindows, givenCorrectParametersWhenConfiguringHwInfoThenSetFtrSvmCorrectly) {
    auto ftrSvm = outHwInfo.featureTable.flags.ftrSVM;

    int ret = hwConfig.configureHwInfoWddm(&pInHwInfo, &outHwInfo, osInterface.get());
    ASSERT_EQ(0, ret);

    EXPECT_EQ(outHwInfo.capabilityTable.ftrSvm, ftrSvm);
}

TEST_F(HwInfoConfigTestWindows, givenInstrumentationForHardwareIsEnabledOrDisabledWhenConfiguringHwInfoThenOverrideItUsingHaveInstrumentation) {
    int ret;

    outHwInfo.capabilityTable.instrumentationEnabled = false;
    ret = hwConfig.configureHwInfoWddm(&pInHwInfo, &outHwInfo, osInterface.get());
    ASSERT_EQ(0, ret);
    EXPECT_FALSE(outHwInfo.capabilityTable.instrumentationEnabled);

    outHwInfo.capabilityTable.instrumentationEnabled = true;
    ret = hwConfig.configureHwInfoWddm(&pInHwInfo, &outHwInfo, osInterface.get());
    ASSERT_EQ(0, ret);
    EXPECT_TRUE(outHwInfo.capabilityTable.instrumentationEnabled);
}

HWTEST_F(HwInfoConfigTestWindows, givenFtrIaCoherencyFlagWhenConfiguringHwInfoThenSetCoherencySupportCorrectly) {
    HardwareInfo initialHwInfo = *defaultHwInfo;
    auto hwInfoConfig = HwInfoConfig::get(initialHwInfo.platform.eProductFamily);

    bool initialCoherencyStatus = false;
    hwInfoConfig->setCapabilityCoherencyFlag(outHwInfo, initialCoherencyStatus);

    initialHwInfo.featureTable.flags.ftrL3IACoherency = false;
    hwInfoConfig->configureHwInfoWddm(&initialHwInfo, &outHwInfo, osInterface.get());
    EXPECT_FALSE(outHwInfo.capabilityTable.ftrSupportsCoherency);

    initialHwInfo.featureTable.flags.ftrL3IACoherency = true;
    hwInfoConfig->configureHwInfoWddm(&initialHwInfo, &outHwInfo, osInterface.get());
    EXPECT_EQ(initialCoherencyStatus, outHwInfo.capabilityTable.ftrSupportsCoherency);
}

} // namespace NEO
