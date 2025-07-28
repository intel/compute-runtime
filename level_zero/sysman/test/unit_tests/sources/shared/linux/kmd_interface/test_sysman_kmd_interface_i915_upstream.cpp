/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/string.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/variable_backup.h"

#include "level_zero/sysman/source/api/engine/linux/sysman_os_engine_imp.h"
#include "level_zero/sysman/source/shared/linux/kmd_interface/sysman_kmd_interface.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"
#include "level_zero/sysman/test/unit_tests/sources/shared/linux/mock_pmu_interface.h"

#include "gtest/gtest.h"

#include <unordered_set>

namespace L0 {
namespace Sysman {
namespace ult {

using namespace NEO;

static const uint32_t mockReadVal = 23;

static int mockReadLinkSuccess(const char *path, char *buf, size_t bufsize) {
    constexpr size_t sizeofPath = sizeof("/sys/devices/pci0000:00/0000:00:01.0/0000:01:00.0/0000:02:01.0/0000:03:00.0");
    strcpy_s(buf, sizeofPath, "/sys/devices/pci0000:00/0000:00:01.0/0000:01:00.0/0000:02:01.0/0000:03:00.0");
    return sizeofPath;
}

static int mockReadLinkFailure(const char *path, char *buf, size_t bufsize) {
    errno = ENOENT;
    return -1;
}

static ssize_t mockReadSuccess(int fd, void *buf, size_t count, off_t offset) {
    std::ostringstream oStream;
    oStream << mockReadVal;
    std::string value = oStream.str();
    memcpy(buf, value.data(), count);
    return count;
}

static ssize_t mockReadFailure(int fd, void *buf, size_t count, off_t offset) {
    errno = ENOENT;
    return -1;
}

class SysmanFixtureDeviceI915Upstream : public SysmanDeviceFixture {
  protected:
    L0::Sysman::SysmanDevice *device = nullptr;
    std::unique_ptr<MockPmuInterfaceImp> pPmuInterface;

    void SetUp() override {
        VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSuccess);
        SysmanDeviceFixture::SetUp();
        device = pSysmanDevice;
        pPmuInterface = std::make_unique<MockPmuInterfaceImp>(pLinuxSysmanImp);
        pPmuInterface->pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
        VariableBackup<L0::Sysman::PmuInterface *> pmuBackup(&pLinuxSysmanImp->pPmuInterface);
        pLinuxSysmanImp->pPmuInterface = pPmuInterface.get();
        bool isIntegratedDevice = false;
        pLinuxSysmanImp->pSysmanKmdInterface->setSysmanDeviceDirName(isIntegratedDevice);
    }

    void mockInitFsAccess() {
        pLinuxSysmanImp->pSysmanKmdInterface->initFsAccessInterface(*pLinuxSysmanImp->getDrm());
    }

    void TearDown() override {
        SysmanDeviceFixture::TearDown();
    }
};

TEST_F(SysmanFixtureDeviceI915Upstream, GivenSysmanKmdInterfaceWhenGettingSysfsFileNamesThenProperPathsAreReturned) {
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

TEST_F(SysmanFixtureDeviceI915Upstream, GivenSysmanKmdInterfaceWhenGettingFilenamesAndBaseDirectoryDoesntExistThenProperPathsAreReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->getSysmanKmdInterface();
    bool baseDirectoryExists = false;
    EXPECT_STREQ("gt_min_freq_mhz", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameMinFrequency, 0, baseDirectoryExists).c_str());
    EXPECT_STREQ("gt_max_freq_mhz", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameMaxFrequency, 0, baseDirectoryExists).c_str());
    EXPECT_STREQ("gt_boost_freq_mhz", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameBoostFrequency, 0, baseDirectoryExists).c_str());
    EXPECT_STREQ("gt_cur_freq_mhz", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameCurrentFrequency, 0, baseDirectoryExists).c_str());
    EXPECT_STREQ("rapl_PL1_freq_mhz", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameTdpFrequency, 0, baseDirectoryExists).c_str());
    EXPECT_STREQ("gt_act_freq_mhz", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameActualFrequency, 0, baseDirectoryExists).c_str());
    EXPECT_STREQ("gt_RP1_freq_mhz", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameEfficientFrequency, 0, baseDirectoryExists).c_str());
    EXPECT_STREQ("gt_RP0_freq_mhz", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameMaxValueFrequency, 0, baseDirectoryExists).c_str());
    EXPECT_STREQ("gt_RPn_freq_mhz", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameMinValueFrequency, 0, baseDirectoryExists).c_str());
    EXPECT_STREQ("gt_throttle_reason_status", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameThrottleReasonStatus, 0, baseDirectoryExists).c_str());
    EXPECT_STREQ("gt_throttle_reason_status_pl1", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameThrottleReasonPL1, 0, baseDirectoryExists).c_str());
    EXPECT_STREQ("gt_throttle_reason_status_pl2", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameThrottleReasonPL2, 0, baseDirectoryExists).c_str());
    EXPECT_STREQ("gt_throttle_reason_status_pl4", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameThrottleReasonPL4, 0, baseDirectoryExists).c_str());
    EXPECT_STREQ("gt_throttle_reason_status_thermal", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameThrottleReasonThermal, 0, baseDirectoryExists).c_str());
    EXPECT_STREQ("power/rc6_enable", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameStandbyModeControl, 0, baseDirectoryExists).c_str());
}

TEST_F(SysmanFixtureDeviceI915Upstream, GivenSysmanKmdInterfaceInstanceWhenCallingGetPowerLimitFilePathsThenProperPathsAreReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->getSysmanKmdInterface();
    bool baseDirectoryExists = false;
    EXPECT_STREQ("", pSysmanKmdInterface->getBurstPowerLimitFile(SysfsName::sysfsNamePackageBurstPowerLimit, 0, baseDirectoryExists).c_str());
    EXPECT_STREQ("", pSysmanKmdInterface->getBurstPowerLimitFile(SysfsName::sysfsNamePackageBurstPowerLimitInterval, 0, baseDirectoryExists).c_str());
    EXPECT_STREQ("power1_max", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNamePackageSustainedPowerLimit, 0, baseDirectoryExists).c_str());
    EXPECT_STREQ("power1_max_interval", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNamePackageSustainedPowerLimitInterval, 0, baseDirectoryExists).c_str());
    EXPECT_STREQ("power1_rated_max", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNamePackageDefaultPowerLimit, 0, baseDirectoryExists).c_str());
}

TEST_F(SysmanFixtureDeviceI915Upstream, GivenSysmanKmdInterfaceWhenGettingHwMonNameThenCorrectPathIsReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->getSysmanKmdInterface();
    bool isSubdevice = true;
    EXPECT_STREQ("i915_gt0", pSysmanKmdInterface->getHwmonName(0, isSubdevice).c_str());
    isSubdevice = false;
    EXPECT_STREQ("i915", pSysmanKmdInterface->getHwmonName(0, isSubdevice).c_str());
}

TEST_F(SysmanFixtureDeviceI915Upstream, GivenSysmanKmdInterfaceWhenGettingEngineBasePathThenCorrectPathIsReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->getSysmanKmdInterface();
    EXPECT_STREQ("engine", pSysmanKmdInterface->getEngineBasePath(0).c_str());
}

TEST_F(SysmanFixtureDeviceI915Upstream, GivenSysmanKmdInterfaceWhenCallingGetSysmanDeviceDirNameForDiscreteDeviceThenCorrectNameIsReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    EXPECT_STREQ("i915_0000_03_00.0", pSysmanKmdInterface->getSysmanDeviceDirName().c_str());
}

TEST_F(SysmanFixtureDeviceI915Upstream, GivenSysmanKmdInterfaceAndReadSymLinkFailsWhenCallingGetSysmanDeviceDirNameForDiscreteDeviceThenEmptyStringIsReturned) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkFailure);
    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    bool isIntegratedDevice = false;
    pSysmanKmdInterface->setSysmanDeviceDirName(isIntegratedDevice);
    EXPECT_STREQ("", pSysmanKmdInterface->getSysmanDeviceDirName().c_str());
}

TEST_F(SysmanFixtureDeviceI915Upstream, GivenSysmanKmdInterfaceWhenCallingGetSysmanDeviceDirNameForIntegratedDeviceThenCorrectNameIsReturned) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSuccess);
    auto pSysmanKmdInterface = std::make_unique<SysmanKmdInterfaceI915Upstream>(pLinuxSysmanImp->getSysmanProductHelper());
    mockInitFsAccess();
    bool isIntegratedDevice = true;
    pSysmanKmdInterface->setSysmanDeviceDirName(isIntegratedDevice);
    EXPECT_STREQ("i915", pSysmanKmdInterface->getSysmanDeviceDirName().c_str());
}

TEST_F(SysmanFixtureDeviceI915Upstream, GivenSysmanKmdInterfaceWhenCallingGetEngineClassStringThenCorrectPathIsReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->getSysmanKmdInterface();
    EXPECT_STREQ("rcs", pSysmanKmdInterface->getEngineClassString(EngineClass::ENGINE_CLASS_RENDER).value().c_str());
    EXPECT_STREQ("bcs", pSysmanKmdInterface->getEngineClassString(EngineClass::ENGINE_CLASS_COPY).value().c_str());
    EXPECT_STREQ("vcs", pSysmanKmdInterface->getEngineClassString(EngineClass::ENGINE_CLASS_VIDEO).value().c_str());
    EXPECT_STREQ("ccs", pSysmanKmdInterface->getEngineClassString(EngineClass::ENGINE_CLASS_COMPUTE).value().c_str());
    EXPECT_STREQ("vecs", pSysmanKmdInterface->getEngineClassString(EngineClass::ENGINE_CLASS_VIDEO_ENHANCE).value().c_str());
}

TEST_F(SysmanFixtureDeviceI915Upstream, GivenSysmanKmdInterfaceInstanceWhenIsGroupEngineInterfaceAvailableCalledThenFalseValueIsReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->getSysmanKmdInterface();
    EXPECT_FALSE(pSysmanKmdInterface->isGroupEngineInterfaceAvailable());
}

TEST_F(SysmanFixtureDeviceI915Upstream, GivenSysmanKmdInterfaceInstanceWhenCheckingAvailabilityOfBaseFrequencyFactorAndSystemPowerBalanceThenFalseValueIsReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    EXPECT_FALSE(pSysmanKmdInterface->isBaseFrequencyFactorAvailable());
    EXPECT_FALSE(pSysmanKmdInterface->isSystemPowerBalanceAvailable());
}

TEST_F(SysmanFixtureDeviceI915Upstream, GivenSysmanKmdInterfaceInstanceWhenCheckingAvailabilityOfFrequencyFilesThenTrueValueIsReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    EXPECT_TRUE(pSysmanKmdInterface->isDefaultFrequencyAvailable());
    EXPECT_TRUE(pSysmanKmdInterface->isBoostFrequencyAvailable());
    EXPECT_TRUE(pSysmanKmdInterface->isTdpFrequencyAvailable());
}

TEST_F(SysmanFixtureDeviceI915Upstream, GivenSysmanKmdInterfaceInstanceWhenCheckingPhysicalMemorySizeAvailabilityThenFalseValueIsReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    EXPECT_FALSE(pSysmanKmdInterface->isPhysicalMemorySizeSupported());
}

TEST_F(SysmanFixtureDeviceI915Upstream, GivenSysmanKmdInterfaceInstanceWhenCallingGetNativeUnitWithProperSysfsNameThenValidValuesAreReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->getSysmanKmdInterface();
    EXPECT_EQ(SysfsValueUnit::milli, pSysmanKmdInterface->getNativeUnit(SysfsName::sysfsNameSchedulerTimeout));
    EXPECT_EQ(SysfsValueUnit::milli, pSysmanKmdInterface->getNativeUnit(SysfsName::sysfsNameSchedulerTimeslice));
    EXPECT_EQ(SysfsValueUnit::milli, pSysmanKmdInterface->getNativeUnit(SysfsName::sysfsNameSchedulerWatchDogTimeout));
    EXPECT_EQ(SysfsValueUnit::micro, pSysmanKmdInterface->getNativeUnit(SysfsName::sysfsNamePackageSustainedPowerLimit));
    EXPECT_EQ(SysfsValueUnit::micro, pSysmanKmdInterface->getNativeUnit(SysfsName::sysfsNamePackageDefaultPowerLimit));
}

TEST_F(SysmanFixtureDeviceI915Upstream, GivenSysmanKmdInterfaceInstanceWhenCallingGetPowerDomainsThenValidPowerDomainsAreReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->getSysmanKmdInterface();

    std::vector<zes_power_domain_t> validPowerDomains = {ZES_POWER_DOMAIN_PACKAGE};
    auto powerDomains = pSysmanKmdInterface->getPowerDomains();

    std::unordered_set<zes_power_domain_t> outputPowerDomainList(powerDomains.begin(), powerDomains.end());
    std::unordered_set<zes_power_domain_t> validPowerDomainList(validPowerDomains.begin(), validPowerDomains.end());

    EXPECT_EQ(validPowerDomainList, outputPowerDomainList);
}

TEST_F(SysmanFixtureDeviceI915Upstream, GivenSysmanKmdInterfaceInstanceWhenCallingGetPmuConfigsForGroupEnginesThenErrorIsReturned) {

    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    auto pDrm = pLinuxSysmanImp->getDrm();
    std::vector<uint64_t> pmuConfigs = {};
    const std::string sysmanDeviceDir = "/sys/devices/0000:aa:bb:cc";
    EngineGroupInfo engineInfo = {ZES_ENGINE_GROUP_ALL, 0, 0};
    MapOfEngineInfo mapEngineInfo = {};
    EXPECT_EQ(pSysmanKmdInterface->getPmuConfigsForGroupEngines(mapEngineInfo, sysmanDeviceDir, engineInfo, pPmuInterface.get(), pDrm, pmuConfigs), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
}

TEST_F(SysmanFixtureDeviceI915Upstream, GivenSysmanKmdInterfaceInstanceWhenCallingGetPmuConfigsForSingleEnginesThenSuccessIsReturned) {

    uint64_t mockPmuConfig = 16384;
    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    auto pDrm = pLinuxSysmanImp->getDrm();
    std::vector<uint64_t> pmuConfigs = {};
    const std::string sysmanDeviceDir = "/sys/devices/0000:aa:bb:cc";
    EngineGroupInfo engineInfo = {ZES_ENGINE_GROUP_COMPUTE_SINGLE, 0, 0};
    EXPECT_EQ(pSysmanKmdInterface->getPmuConfigsForSingleEngines(sysmanDeviceDir, engineInfo, pPmuInterface.get(), pDrm, pmuConfigs), ZE_RESULT_SUCCESS);
    EXPECT_EQ(pmuConfigs[0], mockPmuConfig);
}

TEST_F(SysmanFixtureDeviceI915Upstream, GivenSysmanKmdInterfaceAndPmuReadFailsWhenCallingReadBusynessFromGroupFdThenErrorIsReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    std::vector<int64_t> fdList = {1, 2};
    zes_engine_stats_t pStats = {};
    pPmuInterface->mockPmuReadFailureReturnValue = -1;
    EXPECT_EQ(pSysmanKmdInterface->readBusynessFromGroupFd(pPmuInterface.get(), fdList, &pStats), ZE_RESULT_ERROR_UNKNOWN);
}

TEST_F(SysmanFixtureDeviceI915Upstream, GivenSysmanKmdInterfaceInstanceAndIsIntegratedDeviceWhenGetEventsIsCalledThenValidEventTypeIsReturned) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, &mockReadSuccess);

    bool isIntegratedDevice = true;
    pLinuxSysmanImp->pSysmanKmdInterface->setSysmanDeviceDirName(isIntegratedDevice);

    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    EXPECT_EQ(mockReadVal, pSysmanKmdInterface->getEventType());
}

TEST_F(SysmanFixtureDeviceI915Upstream, GivenSysmanKmdInterfaceInstanceAndIsNotIntegratedDeviceWhenGetEventsIsCalledThenValidEventTypeIsReturned) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, &mockReadSuccess);

    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    EXPECT_EQ(mockReadVal, pSysmanKmdInterface->getEventType());
}

TEST_F(SysmanFixtureDeviceI915Upstream, GivenSysmanKmdInterfaceInstanceAndIsNotIntegratedDeviceAndReadSymLinkFailsWhenGetEventsIsCalledThenFailureIsReturned) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkFailure);

    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    EXPECT_EQ(0u, pSysmanKmdInterface->getEventType());
}

TEST_F(SysmanFixtureDeviceI915Upstream, GivenSysmanKmdInterfaceInstanceAndIsNotIntegratedDeviceAndFsReadFailsWhenGetEventsIsCalledThenFailureIsReturned) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, &mockReadFailure);

    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    EXPECT_EQ(0u, pSysmanKmdInterface->getEventType());
}

TEST_F(SysmanFixtureDeviceI915Upstream, GivenSysmanKmdInterfaceInstanceWhenCheckingSupportForSettingSchedulerModesThenTrueValueIsReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    EXPECT_TRUE(pSysmanKmdInterface->isSettingExclusiveModeSupported());
    EXPECT_TRUE(pSysmanKmdInterface->isSettingTimeoutModeSupported());
}

TEST_F(SysmanFixtureDeviceI915Upstream, GivenSysmanKmdInterfaceWhenCheckingWhetherClientInfoAvailableInFdInfoThenFalseValueIsReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    EXPECT_FALSE(pSysmanKmdInterface->clientInfoAvailableInFdInfo());
}

TEST_F(SysmanFixtureDeviceI915Upstream, GivenSysmanKmdInterfaceInstanceWhenCheckingSupportForVfEngineUtilizationThenFalseValueIsReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    EXPECT_FALSE(pSysmanKmdInterface->isVfEngineUtilizationSupported());
}

TEST_F(SysmanFixtureDeviceI915Upstream, GivenSysmanKmdInterfaceInstanceWhenGettingBusyAndTotalTicksConfigsForVfThenErrorIsReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    auto pPmuInterface = pLinuxSysmanImp->getPmuInterface();
    std::pair<uint64_t, uint64_t> configPair;
    EXPECT_EQ(pSysmanKmdInterface->getBusyAndTotalTicksConfigsForVf(pPmuInterface, 0, 0, 1, 0, configPair), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
}

TEST_F(SysmanFixtureDeviceI915Upstream, GivenSysmanKmdInterfaceInstanceWhenCallingGpuBindAndUnbindEntryThenProperStringIsReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    EXPECT_STREQ("/sys/bus/pci/drivers/i915/bind", pSysmanKmdInterface->getGpuBindEntry().c_str());
    EXPECT_STREQ("/sys/bus/pci/drivers/i915/unbind", pSysmanKmdInterface->getGpuUnBindEntry().c_str());
}

TEST_F(SysmanFixtureDeviceI915Upstream, GivenSysmanKmdInterfaceWhenGetEnergyCounterNodeFileIsCalledForDifferentPowerDomainsThenProperPathIsReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    std::string expectedFilePath = "energy1_input";
    EXPECT_EQ(expectedFilePath, pSysmanKmdInterface->getEnergyCounterNodeFile(ZES_POWER_DOMAIN_PACKAGE));
    expectedFilePath = "";
    EXPECT_EQ(expectedFilePath, pSysmanKmdInterface->getEnergyCounterNodeFile(ZES_POWER_DOMAIN_CARD));
}

TEST_F(SysmanFixtureDeviceI915Upstream, GivenSysmanKmdInterfaceWhenCallingIsLateBindingVersionAvailableThenFalseIsReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    std::string val;
    EXPECT_FALSE(pSysmanKmdInterface->isLateBindingVersionAvailable("FanTable", val));
    EXPECT_FALSE(pSysmanKmdInterface->isLateBindingVersionAvailable("VRConfig", val));
}

} // namespace ult
} // namespace Sysman
} // namespace L0