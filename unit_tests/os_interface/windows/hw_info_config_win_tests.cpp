/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/os_interface/windows/hw_info_config_win_tests.h"

#include "core/execution_environment/root_device_environment.h"
#include "core/helpers/hw_helper.h"
#include "core/os_interface/windows/os_interface.h"
#include "core/os_interface/windows/wddm/wddm.h"
#include "core/unit_tests/helpers/debug_manager_state_restore.h"
#include "test.h"
#include "unit_tests/mocks/mock_execution_environment.h"

#include "instrumentation.h"

namespace NEO {

template <>
uint64_t HwInfoConfigHw<IGFX_UNKNOWN>::getHostMemCapabilities() {
    return 0;
}

template <>
uint64_t HwInfoConfigHw<IGFX_UNKNOWN>::getDeviceMemCapabilities() {
    return 0;
}

template <>
uint64_t HwInfoConfigHw<IGFX_UNKNOWN>::getSingleDeviceSharedMemCapabilities() {
    return 0;
}

template <>
uint64_t HwInfoConfigHw<IGFX_UNKNOWN>::getCrossDeviceSharedMemCapabilities() {
    return 0;
}

template <>
uint64_t HwInfoConfigHw<IGFX_UNKNOWN>::getSharedSystemMemCapabilities() {
    return 0;
}

template <>
int HwInfoConfigHw<IGFX_UNKNOWN>::configureHardwareCustom(HardwareInfo *hwInfo, OSInterface *osIface) {
    return 0;
}

template <>
void HwInfoConfigHw<IGFX_UNKNOWN>::adjustPlatformForProductFamily(HardwareInfo *hwInfo) {
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

    std::unique_ptr<Wddm> wddm(Wddm::createWddm(nullptr, *rootDeviceEnvironment));
    wddm->init();
    outHwInfo = *executionEnvironment->getHardwareInfo();
}

void HwInfoConfigTestWindows::TearDown() {
    HwInfoConfigTest::TearDown();
}

TEST_F(HwInfoConfigTestWindows, givenCorrectParametersWhenConfiguringHwInfoThenReturnSuccess) {
    int ret = hwConfig.configureHwInfo(&pInHwInfo, &outHwInfo, osInterface.get());
    EXPECT_EQ(0, ret);
}

TEST_F(HwInfoConfigTestWindows, givenCorrectParametersWhenConfiguringHwInfoThenSetFtrSvmCorrectly) {
    auto ftrSvm = outHwInfo.featureTable.ftrSVM;

    int ret = hwConfig.configureHwInfo(&pInHwInfo, &outHwInfo, osInterface.get());
    ASSERT_EQ(0, ret);

    EXPECT_EQ(outHwInfo.capabilityTable.ftrSvm, ftrSvm);
}

TEST_F(HwInfoConfigTestWindows, givenInstrumentationForHardwareIsEnabledOrDisabledWhenConfiguringHwInfoThenOverrideItUsingHaveInstrumentation) {
    int ret;

    outHwInfo.capabilityTable.instrumentationEnabled = false;
    ret = hwConfig.configureHwInfo(&pInHwInfo, &outHwInfo, osInterface.get());
    ASSERT_EQ(0, ret);
    EXPECT_FALSE(outHwInfo.capabilityTable.instrumentationEnabled);

    outHwInfo.capabilityTable.instrumentationEnabled = true;
    ret = hwConfig.configureHwInfo(&pInHwInfo, &outHwInfo, osInterface.get());
    ASSERT_EQ(0, ret);
    EXPECT_TRUE(outHwInfo.capabilityTable.instrumentationEnabled == haveInstrumentation);
}

HWTEST_F(HwInfoConfigTestWindows, givenFtrIaCoherencyFlagWhenConfiguringHwInfoThenSetCoherencySupportCorrectly) {
    HardwareInfo initialHwInfo = **platformDevices;
    auto &hwHelper = HwHelper::get(initialHwInfo.platform.eRenderCoreFamily);
    auto hwInfoConfig = HwInfoConfig::get(initialHwInfo.platform.eProductFamily);

    bool initialCoherencyStatus = false;
    hwHelper.setCapabilityCoherencyFlag(&outHwInfo, initialCoherencyStatus);

    initialHwInfo.featureTable.ftrL3IACoherency = false;
    hwInfoConfig->configureHwInfo(&initialHwInfo, &outHwInfo, osInterface.get());
    EXPECT_FALSE(outHwInfo.capabilityTable.ftrSupportsCoherency);

    initialHwInfo.featureTable.ftrL3IACoherency = true;
    hwInfoConfig->configureHwInfo(&initialHwInfo, &outHwInfo, osInterface.get());
    EXPECT_EQ(initialCoherencyStatus, outHwInfo.capabilityTable.ftrSupportsCoherency);
}

} // namespace NEO
