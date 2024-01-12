/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/string.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/variable_backup.h"

#include "level_zero/sysman/source/shared/linux/sysman_kmd_interface.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"
#include "level_zero/sysman/test/unit_tests/sources/shared/linux/sysman_kmd_interface_tests.h"

#include "gtest/gtest.h"

namespace L0 {
namespace Sysman {
namespace ult {

using namespace NEO;

class SysmanFixtureDeviceI915Prelim : public SysmanDeviceFixture {
  protected:
    L0::Sysman::SysmanDevice *device = nullptr;

    void SetUp() override {
        SysmanDeviceFixture::SetUp();
        device = pSysmanDevice;
        pLinuxSysmanImp->pSysmanKmdInterface.reset(new SysmanKmdInterfaceI915Prelim(pLinuxSysmanImp->getProductFamily()));
    }

    void TearDown() override {
        SysmanDeviceFixture::TearDown();
    }
};

TEST_F(SysmanFixtureDeviceI915Prelim, GivenI915PrelimVersionWhenSysmanKmdInterfaceInstanceIsCreatedThenValidPtrIsReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->getSysmanKmdInterface();
    EXPECT_NE(nullptr, pSysmanKmdInterface);
}

TEST_F(SysmanFixtureDeviceI915Prelim, GivenSysmanKmdInterfaceInstanceWhenCallingGetHwmonNameThenEmptyNameIsReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->getSysmanKmdInterface();
    EXPECT_STREQ("i915_gt0", pSysmanKmdInterface->getHwmonName(0, true).c_str());
    EXPECT_STREQ("i915", pSysmanKmdInterface->getHwmonName(0, false).c_str());
}

TEST_F(SysmanFixtureDeviceI915Prelim, GivenSysmanKmdInterfaceInstanceWhenCallingGetEngineBasePathThenEmptyPathIsReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->getSysmanKmdInterface();
    EXPECT_STREQ("", pSysmanKmdInterface->getEngineBasePath(0).c_str());
}

TEST_F(SysmanFixtureDeviceI915Prelim, GivenSysmanKmdInterfaceWhenGettingSysfsFileNamesThenProperPathsAreReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->getSysmanKmdInterface();
    bool baseDirectoryExists = true;
    EXPECT_STREQ("gt/gt0/rps_min_freq_mhz", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameMinFrequency, 0, baseDirectoryExists).c_str());
    EXPECT_STREQ("gt/gt0/rps_max_freq_mhz", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameMaxFrequency, 0, baseDirectoryExists).c_str());
    EXPECT_STREQ("gt/gt0/.defaults/rps_min_freq_mhz", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameMinDefaultFrequency, 0, baseDirectoryExists).c_str());
    EXPECT_STREQ("gt/gt0/.defaults/rps_max_freq_mhz", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameMaxDefaultFrequency, 0, baseDirectoryExists).c_str());
    EXPECT_STREQ("gt/gt0/rps_boost_freq_mhz", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameBoostFrequency, 0, baseDirectoryExists).c_str());
    EXPECT_STREQ("gt/gt0/punit_req_freq_mhz", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameCurrentFrequency, 0, baseDirectoryExists).c_str());
    EXPECT_STREQ("gt/gt0/rapl_PL1_freq_mhz", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameTdpFrequency, 0, baseDirectoryExists).c_str());
    EXPECT_STREQ("gt/gt0/rps_act_freq_mhz", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameActualFrequency, 0, baseDirectoryExists).c_str());
    EXPECT_STREQ("gt/gt0/rps_RP1_freq_mhz", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameEfficientFrequency, 0, baseDirectoryExists).c_str());
    EXPECT_STREQ("gt/gt0/rps_RP0_freq_mhz", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameMaxValueFrequency, 0, baseDirectoryExists).c_str());
    EXPECT_STREQ("gt/gt0/rps_RPn_freq_mhz", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameMinValueFrequency, 0, baseDirectoryExists).c_str());
    EXPECT_STREQ("gt/gt0/throttle_reason_status", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameThrottleReasonStatus, 0, baseDirectoryExists).c_str());
    EXPECT_STREQ("gt/gt0/throttle_reason_pl1", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameThrottleReasonPL1, 0, baseDirectoryExists).c_str());
    EXPECT_STREQ("gt/gt0/throttle_reason_pl2", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameThrottleReasonPL2, 0, baseDirectoryExists).c_str());
    EXPECT_STREQ("gt/gt0/throttle_reason_pl4", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameThrottleReasonPL4, 0, baseDirectoryExists).c_str());
    EXPECT_STREQ("gt/gt0/throttle_reason_thermal", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameThrottleReasonThermal, 0, baseDirectoryExists).c_str());
    EXPECT_STREQ("gt/gt0/addr_range", pSysmanKmdInterface->getSysfsFilePathForPhysicalMemorySize(0).c_str());
    EXPECT_STREQ("gt/gt0/mem_RP0_freq_mhz", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameMaxMemoryFrequency, 0, baseDirectoryExists).c_str());
    EXPECT_STREQ("gt/gt0/mem_RPn_freq_mhz", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameMinMemoryFrequency, 0, baseDirectoryExists).c_str());
    EXPECT_STREQ("gt/gt0/rc6_enable", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameStandbyModeControl, 0, baseDirectoryExists).c_str());
}

TEST_F(SysmanFixtureDeviceI915Prelim, GivenSysmanKmdInterfaceInstanceWhenCallingGetNativeUnitWithProperSysfsNameThenValidValuesAreReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->getSysmanKmdInterface();
    EXPECT_EQ(SysmanKmdInterface::SysfsValueUnit::milliSecond, pSysmanKmdInterface->getNativeUnit(SysfsName::sysfsNameSchedulerTimeout));
    EXPECT_EQ(SysmanKmdInterface::SysfsValueUnit::milliSecond, pSysmanKmdInterface->getNativeUnit(SysfsName::sysfsNameSchedulerTimeslice));
    EXPECT_EQ(SysmanKmdInterface::SysfsValueUnit::milliSecond, pSysmanKmdInterface->getNativeUnit(SysfsName::sysfsNameSchedulerWatchDogTimeout));
}

TEST_F(SysmanFixtureDeviceI915Prelim, GivenSysmanKmdInterfaceInstanceWhenCallingGetEngineActivityFdThenInvalidFdisReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->getSysmanKmdInterface();
    auto pPmuInterface = std::make_unique<MockPmuInterfaceImp>(pLinuxSysmanImp);
    EXPECT_EQ(-1, pSysmanKmdInterface->getEngineActivityFd(ZES_ENGINE_GROUP_COMPUTE_SINGLE, 0, 0, pPmuInterface.get()));
}

TEST_F(SysmanFixtureDeviceI915Prelim, GivenSysmanKmdInterfaceInstanceWhenCheckingSupportForI915DriverThenProperStatusIsReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->getSysmanKmdInterface();
    EXPECT_FALSE(pSysmanKmdInterface->clientInfoAvailableInFdInfo());
    EXPECT_FALSE(pSysmanKmdInterface->isGroupEngineInterfaceAvailable());
    EXPECT_FALSE(pSysmanKmdInterface->useDefaultMaximumWatchdogTimeoutForExclusiveMode());
}

TEST_F(SysmanFixtureDeviceI915Prelim, GivenSysmanKmdInterfaceInstanceWhenCheckingSupportForStandbyModeThenProperStatusIsReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->getSysmanKmdInterface();
    EXPECT_TRUE(pSysmanKmdInterface->isStandbyModeControlAvailable());
}

TEST_F(SysmanFixtureDeviceI915Prelim, GivenSysmanKmdInterfaceInstanceWhenCallingGetEventTypeThenInvalidValueIsReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->getSysmanKmdInterface();
    EXPECT_EQ(0u, pSysmanKmdInterface->getEventType(true));
}

TEST_F(SysmanFixtureDeviceI915Prelim, GivenSysmanKmdInterfaceInstanceWhenCheckingAvailabilityOfFrequencyFilesThenTrueValueIsReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->getSysmanKmdInterface();
    EXPECT_TRUE(pSysmanKmdInterface->isDefaultFrequencyAvailable());
    EXPECT_TRUE(pSysmanKmdInterface->isBoostFrequencyAvailable());
    EXPECT_TRUE(pSysmanKmdInterface->isTdpFrequencyAvailable());
}

TEST_F(SysmanFixtureDeviceI915Prelim, GivenSysmanKmdInterfaceInstanceWhenCheckingPhysicalMemorySizeAvailabilityThenTrueValueIsReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->getSysmanKmdInterface();
    EXPECT_TRUE(pSysmanKmdInterface->isPhysicalMemorySizeSupported());
}

TEST_F(SysmanFixtureDeviceI915Prelim, GivenSysmanKmdInterfaceInstanceWhenCallingGetEngineClassStringThenInvalidValueIsReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->getSysmanKmdInterface();
    EXPECT_EQ(std::nullopt, pSysmanKmdInterface->getEngineClassString(EngineClass::ENGINE_CLASS_COMPUTE));
}

TEST_F(SysmanFixtureDeviceI915Prelim, GivenSysmanKmdInterfaceInstanceWhenCallingGetNumEngineTypeAndInstancesTenErrorIsReturned) {
    std::vector<std::string> mockVecString = {"rcs"};
    std::map<zes_engine_type_flag_t, std::vector<std::string>> mockMapofEngine = {{ZES_ENGINE_TYPE_FLAG_RENDER, mockVecString}};
    auto pSysmanKmdInterface = pLinuxSysmanImp->getSysmanKmdInterface();

    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pSysmanKmdInterface->getNumEngineTypeAndInstances(
                                                       mockMapofEngine, pLinuxSysmanImp, nullptr, true, 0));
}

} // namespace ult
} // namespace Sysman
} // namespace L0