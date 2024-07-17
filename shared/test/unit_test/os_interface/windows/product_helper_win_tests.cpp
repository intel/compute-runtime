/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/os_interface/windows/product_helper_win_tests.h"

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/hw_test.h"

namespace NEO {

ProductHelperTestWindows::ProductHelperTestWindows() {
    this->executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    this->rootDeviceEnvironment = std::make_unique<RootDeviceEnvironment>(*executionEnvironment);
}

ProductHelperTestWindows::~ProductHelperTestWindows() = default;

void ProductHelperTestWindows::SetUp() {
    ProductHelperTest::SetUp();

    osInterface.reset(new OSInterface());

    auto wddm = Wddm::createWddm(nullptr, *rootDeviceEnvironment);
    wddm->init();
    outHwInfo = *rootDeviceEnvironment->getHardwareInfo();
}

void ProductHelperTestWindows::TearDown() {
    ProductHelperTest::TearDown();
}

template <typename HelperType>
HelperType &ProductHelperTestWindows::getHelper() const {
    auto &helper = rootDeviceEnvironment->getHelper<HelperType>();
    return helper;
}

template ProductHelper &ProductHelperTestWindows::getHelper() const;
template GfxCoreHelper &ProductHelperTestWindows::getHelper() const;

using ProductHelperTestWindows = ProductHelperTestWindows;

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
