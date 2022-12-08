/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/os_interface/windows/hw_info_config_win_tests.h"

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/hw_test.h"

namespace NEO {

HwInfoConfigTestWindows::HwInfoConfigTestWindows() {
    this->executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    this->rootDeviceEnvironment = std::make_unique<RootDeviceEnvironment>(*executionEnvironment);
}

HwInfoConfigTestWindows::~HwInfoConfigTestWindows() = default;

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

template <typename HelperType>
HelperType &HwInfoConfigTestWindows::getHelper() const {
    auto &helper = rootDeviceEnvironment->getHelper<HelperType>();
    return helper;
}

template ProductHelper &HwInfoConfigTestWindows::getHelper() const;
template GfxCoreHelper &HwInfoConfigTestWindows::getHelper() const;

using ProductHelperTestWindows = HwInfoConfigTestWindows;

TEST_F(ProductHelperTestWindows, givenCorrectParametersWhenConfiguringHwInfoThenReturnSuccess) {

    int ret = productHelper->configureHwInfoWddm(&pInHwInfo, &outHwInfo, *rootDeviceEnvironment.get());
    EXPECT_EQ(0, ret);
}

TEST_F(ProductHelperTestWindows, givenCorrectParametersWhenConfiguringHwInfoThenSetFtrSvmCorrectly) {
    auto ftrSvm = outHwInfo.featureTable.flags.ftrSVM;

    int ret = productHelper->configureHwInfoWddm(&pInHwInfo, &outHwInfo, *rootDeviceEnvironment.get());
    ASSERT_EQ(0, ret);

    EXPECT_EQ(outHwInfo.capabilityTable.ftrSvm, ftrSvm);
}

TEST_F(ProductHelperTestWindows, givenInstrumentationForHardwareIsEnabledOrDisabledWhenConfiguringHwInfoThenOverrideItUsingHaveInstrumentation) {
    int ret;

    outHwInfo.capabilityTable.instrumentationEnabled = false;
    ret = productHelper->configureHwInfoWddm(&pInHwInfo, &outHwInfo, *rootDeviceEnvironment.get());
    ASSERT_EQ(0, ret);
    EXPECT_FALSE(outHwInfo.capabilityTable.instrumentationEnabled);

    outHwInfo.capabilityTable.instrumentationEnabled = true;
    ret = productHelper->configureHwInfoWddm(&pInHwInfo, &outHwInfo, *rootDeviceEnvironment.get());
    ASSERT_EQ(0, ret);
    EXPECT_TRUE(outHwInfo.capabilityTable.instrumentationEnabled);
}

HWTEST_F(ProductHelperTestWindows, givenFtrIaCoherencyFlagWhenConfiguringHwInfoThenSetCoherencySupportCorrectly) {
    HardwareInfo initialHwInfo = *defaultHwInfo;

    bool initialCoherencyStatus = false;
    productHelper->setCapabilityCoherencyFlag(outHwInfo, initialCoherencyStatus);

    initialHwInfo.featureTable.flags.ftrL3IACoherency = false;
    productHelper->configureHwInfoWddm(&initialHwInfo, &outHwInfo, *rootDeviceEnvironment.get());
    EXPECT_FALSE(outHwInfo.capabilityTable.ftrSupportsCoherency);

    initialHwInfo.featureTable.flags.ftrL3IACoherency = true;
    productHelper->configureHwInfoWddm(&initialHwInfo, &outHwInfo, *rootDeviceEnvironment.get());
    EXPECT_EQ(initialCoherencyStatus, outHwInfo.capabilityTable.ftrSupportsCoherency);
}

} // namespace NEO
