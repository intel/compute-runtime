/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/os_interface/windows/hw_info_config_win_tests.h"

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_execution_environment.h"

#include "test.h"

namespace NEO {

template <>
uint64_t HwInfoConfigHw<IGFX_UNKNOWN>::getHostMemCapabilities(const HardwareInfo * /*hwInfo*/) {
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
void HwInfoConfigHw<IGFX_UNKNOWN>::getKernelExtendedProperties(uint32_t *fp16, uint32_t *fp32, uint32_t *fp64) {
}

template <>
uint32_t HwInfoConfigHw<IGFX_UNKNOWN>::getDeviceMemoryMaxClkRate(const HardwareInfo *hwInfo) {
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

template <>
void HwInfoConfigHw<IGFX_UNKNOWN>::convertTimestampsFromOaToCsDomain(uint64_t &timestampData){};

template <>
void HwInfoConfigHw<IGFX_UNKNOWN>::adjustSamplerState(void *sampler, const HardwareInfo &hwInfo){};

template <>
bool HwInfoConfigHw<IGFX_UNKNOWN>::isAdditionalStateBaseAddressWARequired(const HardwareInfo &hwInfo) const {
    return false;
}

template <>
bool HwInfoConfigHw<IGFX_UNKNOWN>::isMaxThreadsForWorkgroupWARequired(const HardwareInfo &hwInfo) const {
    return false;
}

template <>
uint32_t HwInfoConfigHw<IGFX_UNKNOWN>::getMaxThreadsForWorkgroupInDSSOrSS(const HardwareInfo &hwInfo, uint32_t maxNumEUsPerSubSlice, uint32_t maxNumEUsPerDualSubSlice) const {
    return 0;
}

template <>
uint32_t HwInfoConfigHw<IGFX_UNKNOWN>::getMaxThreadsForWorkgroup(const HardwareInfo &hwInfo, uint32_t maxNumEUsPerSubSlice) const {
    return 0;
}

template <>
void HwInfoConfigHw<IGFX_UNKNOWN>::setForceNonCoherent(void *const commandPtr, const StateComputeModeProperties &properties) {}

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
    auto ftrSvm = outHwInfo.featureTable.ftrSVM;

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

HWTEST_F(HwInfoConfigTestWindows, givenHardwareInfoWhenCallingIsAdditionalStateBaseAddressWARequiredThenFalseIsReturned) {
    bool ret = hwConfig.isAdditionalStateBaseAddressWARequired(outHwInfo);
    EXPECT_FALSE(ret);
}

HWTEST_F(HwInfoConfigTestWindows, givenHardwareInfoWhenCallingIsMaxThreadsForWorkgroupWARequiredThenFalseIsReturned) {
    bool ret = hwConfig.isMaxThreadsForWorkgroupWARequired(outHwInfo);
    EXPECT_FALSE(ret);
}

HWTEST_F(HwInfoConfigTestWindows, givenFtrIaCoherencyFlagWhenConfiguringHwInfoThenSetCoherencySupportCorrectly) {
    HardwareInfo initialHwInfo = *defaultHwInfo;
    auto &hwHelper = HwHelper::get(initialHwInfo.platform.eRenderCoreFamily);
    auto hwInfoConfig = HwInfoConfig::get(initialHwInfo.platform.eProductFamily);

    bool initialCoherencyStatus = false;
    hwHelper.setCapabilityCoherencyFlag(&outHwInfo, initialCoherencyStatus);

    initialHwInfo.featureTable.ftrL3IACoherency = false;
    hwInfoConfig->configureHwInfoWddm(&initialHwInfo, &outHwInfo, osInterface.get());
    EXPECT_FALSE(outHwInfo.capabilityTable.ftrSupportsCoherency);

    initialHwInfo.featureTable.ftrL3IACoherency = true;
    hwInfoConfig->configureHwInfoWddm(&initialHwInfo, &outHwInfo, osInterface.get());
    EXPECT_EQ(initialCoherencyStatus, outHwInfo.capabilityTable.ftrSupportsCoherency);
}

} // namespace NEO
