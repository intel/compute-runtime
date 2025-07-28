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
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"
#include "level_zero/sysman/test/unit_tests/sources/shared/linux/kmd_interface/mock_sysman_kmd_interface_xe.h"
#include "level_zero/sysman/test/unit_tests/sources/shared/linux/mock_pmu_interface.h"

#include "gtest/gtest.h"

#include <unordered_set>

namespace L0 {
namespace Sysman {
namespace ult {

using namespace NEO;

static const MapOfEngineInfo mockMapEngineInfo = {
    {ZES_ENGINE_GROUP_ALL, {{0, 0}}},
    {ZES_ENGINE_GROUP_COMPUTE_ALL, {{0, 0}}},
    {ZES_ENGINE_GROUP_MEDIA_ALL, {{0, 0}}},
    {ZES_ENGINE_GROUP_COPY_ALL, {{0, 0}}},
    {ZES_ENGINE_GROUP_RENDER_ALL, {{0, 0}}},
    {ZES_ENGINE_GROUP_MEDIA_DECODE_SINGLE, {{1, 0}}},
    {ZES_ENGINE_GROUP_MEDIA_ENCODE_SINGLE, {{1, 0}}},
    {ZES_ENGINE_GROUP_MEDIA_ENHANCEMENT_SINGLE, {{0, 0}}},
    {ZES_ENGINE_GROUP_RENDER_SINGLE, {{1, 0}}},
    {ZES_ENGINE_GROUP_COPY_SINGLE, {{0, 0}}},
    {ZES_ENGINE_GROUP_COMPUTE_SINGLE, {{1, 0}}}};

static const uint32_t mockReadVal = 23;

static int mockReadLinkSuccess(const char *path, char *buf, size_t bufsize) {
    constexpr size_t sizeofPath = sizeof("/sys/devices/pci0000:00/0000:00:01.0/0000:01:00.0/0000:02:01.0/0000:03:00.0");
    strcpy_s(buf, sizeofPath, "/sys/devices/pci0000:00/0000:00:01.0/0000:01:00.0/0000:02:01.0/0000:03:00.0");
    return sizeofPath;
}

static ssize_t mockReadSuccess(int fd, void *buf, size_t count, off_t offset) {
    std::ostringstream oStream;
    oStream << mockReadVal;
    std::string value = oStream.str();
    memcpy(buf, value.data(), count);
    return count;
}

static int mockReadLinkFailure(const char *path, char *buf, size_t bufsize) {
    errno = ENOENT;
    return -1;
}

class SysmanFixtureDeviceXe : public SysmanDeviceFixture {
  protected:
    std::unique_ptr<MockPmuInterfaceImp> pPmuInterface;

    void SetUp() override {
        VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSuccess);
        SysmanDeviceFixture::SetUp();
        pPmuInterface = std::make_unique<MockPmuInterfaceImp>(pLinuxSysmanImp);
        pLinuxSysmanImp->pSysmanKmdInterface.reset(new SysmanKmdInterfaceXe(pLinuxSysmanImp->getSysmanProductHelper()));
        mockInitFsAccess();
        pPmuInterface->pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
        bool isIntegratedDevice = true;
        pLinuxSysmanImp->pSysmanKmdInterface->setSysmanDeviceDirName(isIntegratedDevice);
    }

    void mockInitFsAccess() {
        pLinuxSysmanImp->pSysmanKmdInterface->initFsAccessInterface(*pLinuxSysmanImp->getDrm());
    }

    void TearDown() override {
        SysmanDeviceFixture::TearDown();
    }
};

TEST_F(SysmanFixtureDeviceXe, GivenSysmanKmdInterfaceWhenCallingGetSysmanDeviceDirNameThenCorrectNameIsReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    EXPECT_STREQ("xe_0000_03_00.0", pSysmanKmdInterface->getSysmanDeviceDirName().c_str());
}

TEST_F(SysmanFixtureDeviceXe, GivenSysmanKmdInterfaceAndReadSymLinkFailsWhenCallingGetSysmanDeviceDirNameThenEmptyStringIsReturned) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkFailure);
    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    bool isIntegratedDevice = false;
    pSysmanKmdInterface->setSysmanDeviceDirName(isIntegratedDevice);
    EXPECT_STREQ("", pSysmanKmdInterface->getSysmanDeviceDirName().c_str());
}

TEST_F(SysmanFixtureDeviceXe, GivenSysmanKmdInterfaceWhenGettingSysfsFileNamesThenProperPathsAreReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    bool baseDirectoryExists = true;
    zes_freq_domain_t frequencyDomainNumber = ZES_FREQ_DOMAIN_GPU;
    for (int index = 0; index < 2; index++) {
        EXPECT_STREQ(("device/tile0/gt" + std::to_string(index) + "/freq0/min_freq").c_str(), pSysmanKmdInterface->getSysfsPathForFreqDomain(SysfsName::sysfsNameMinFrequency, 0, baseDirectoryExists, frequencyDomainNumber).c_str());
        EXPECT_STREQ(("device/tile0/gt" + std::to_string(index) + "/freq0/max_freq").c_str(), pSysmanKmdInterface->getSysfsPathForFreqDomain(SysfsName::sysfsNameMaxFrequency, 0, baseDirectoryExists, frequencyDomainNumber).c_str());
        EXPECT_STREQ(("device/tile0/gt" + std::to_string(index) + "/freq0/cur_freq").c_str(), pSysmanKmdInterface->getSysfsPathForFreqDomain(SysfsName::sysfsNameCurrentFrequency, 0, baseDirectoryExists, frequencyDomainNumber).c_str());
        EXPECT_STREQ(("device/tile0/gt" + std::to_string(index) + "/freq0/act_freq").c_str(), pSysmanKmdInterface->getSysfsPathForFreqDomain(SysfsName::sysfsNameActualFrequency, 0, baseDirectoryExists, frequencyDomainNumber).c_str());
        EXPECT_STREQ(("device/tile0/gt" + std::to_string(index) + "/freq0/rpe_freq").c_str(), pSysmanKmdInterface->getSysfsPathForFreqDomain(SysfsName::sysfsNameEfficientFrequency, 0, baseDirectoryExists, frequencyDomainNumber).c_str());
        EXPECT_STREQ(("device/tile0/gt" + std::to_string(index) + "/freq0/rp0_freq").c_str(), pSysmanKmdInterface->getSysfsPathForFreqDomain(SysfsName::sysfsNameMaxValueFrequency, 0, baseDirectoryExists, frequencyDomainNumber).c_str());
        EXPECT_STREQ(("device/tile0/gt" + std::to_string(index) + "/freq0/rpn_freq").c_str(), pSysmanKmdInterface->getSysfsPathForFreqDomain(SysfsName::sysfsNameMinValueFrequency, 0, baseDirectoryExists, frequencyDomainNumber).c_str());
        EXPECT_STREQ(("device/tile0/gt" + std::to_string(index) + "/freq0/throttle/status").c_str(), pSysmanKmdInterface->getSysfsPathForFreqDomain(SysfsName::sysfsNameThrottleReasonStatus, 0, baseDirectoryExists, frequencyDomainNumber).c_str());
        EXPECT_STREQ(("device/tile0/gt" + std::to_string(index) + "/freq0/throttle/reason_pl1").c_str(), pSysmanKmdInterface->getSysfsPathForFreqDomain(SysfsName::sysfsNameThrottleReasonPL1, 0, baseDirectoryExists, frequencyDomainNumber).c_str());
        EXPECT_STREQ(("device/tile0/gt" + std::to_string(index) + "/freq0/throttle/reason_pl2").c_str(), pSysmanKmdInterface->getSysfsPathForFreqDomain(SysfsName::sysfsNameThrottleReasonPL2, 0, baseDirectoryExists, frequencyDomainNumber).c_str());
        EXPECT_STREQ(("device/tile0/gt" + std::to_string(index) + "/freq0/throttle/reason_pl4").c_str(), pSysmanKmdInterface->getSysfsPathForFreqDomain(SysfsName::sysfsNameThrottleReasonPL4, 0, baseDirectoryExists, frequencyDomainNumber).c_str());
        EXPECT_STREQ(("device/tile0/gt" + std::to_string(index) + "/freq0/throttle/reason_thermal").c_str(), pSysmanKmdInterface->getSysfsPathForFreqDomain(SysfsName::sysfsNameThrottleReasonThermal, 0, baseDirectoryExists, frequencyDomainNumber).c_str());
        EXPECT_STREQ(("device/tile0/gt" + std::to_string(index) + "/freq_vram_rp0").c_str(), pSysmanKmdInterface->getSysfsPathForFreqDomain(SysfsName::sysfsNameMaxMemoryFrequency, 0, baseDirectoryExists, frequencyDomainNumber).c_str());
        EXPECT_STREQ(("device/tile0/gt" + std::to_string(index) + "/freq_vram_rpn").c_str(), pSysmanKmdInterface->getSysfsPathForFreqDomain(SysfsName::sysfsNameMinMemoryFrequency, 0, baseDirectoryExists, frequencyDomainNumber).c_str());
        frequencyDomainNumber = ZES_FREQ_DOMAIN_MEDIA;
    }
    EXPECT_STREQ("", pSysmanKmdInterface->getSysfsPathForFreqDomain(SysfsName::sysfsNameMinDefaultFrequency, 0, baseDirectoryExists, frequencyDomainNumber).c_str());
    EXPECT_STREQ("device/tile0/physical_vram_size_bytes", pSysmanKmdInterface->getSysfsFilePathForPhysicalMemorySize(0).c_str());
}

TEST_F(SysmanFixtureDeviceXe, GivenSysmanKmdInterfaceWhenGettingSysfsFileNameIfBaseDirectoryDoesNotExistThenEmptyPathIsReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    bool baseDirectoryExists = false;
    zes_freq_domain_t frequencyDomainNumber = ZES_FREQ_DOMAIN_MEDIA;
    EXPECT_STREQ("", pSysmanKmdInterface->getSysfsPathForFreqDomain(SysfsName::sysfsNameMaxFrequency, 0, baseDirectoryExists, frequencyDomainNumber).c_str());
}

TEST_F(SysmanFixtureDeviceXe, GivenSysmanKmdInterfaceInstanceWhenCallingGetPowerLimitFilePathsThenProperPathsAreReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->getSysmanKmdInterface();
    bool baseDirectoryExists = false;
    EXPECT_STREQ("power1_cap", pSysmanKmdInterface->getBurstPowerLimitFile(SysfsName::sysfsNameCardBurstPowerLimit, 0, baseDirectoryExists).c_str());
    EXPECT_STREQ("power1_cap_interval", pSysmanKmdInterface->getBurstPowerLimitFile(SysfsName::sysfsNameCardBurstPowerLimitInterval, 0, baseDirectoryExists).c_str());
    EXPECT_STREQ("power1_max", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameCardSustainedPowerLimit, 0, baseDirectoryExists).c_str());
    EXPECT_STREQ("power1_max_interval", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameCardSustainedPowerLimitInterval, 0, baseDirectoryExists).c_str());
    EXPECT_STREQ("power1_rated_max", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameCardDefaultPowerLimit, 0, baseDirectoryExists).c_str());
    EXPECT_STREQ("power1_crit", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameCardCriticalPowerLimit, 0, baseDirectoryExists).c_str());
    EXPECT_STREQ("power2_cap", pSysmanKmdInterface->getBurstPowerLimitFile(SysfsName::sysfsNamePackageBurstPowerLimit, 0, baseDirectoryExists).c_str());
    EXPECT_STREQ("power2_cap_interval", pSysmanKmdInterface->getBurstPowerLimitFile(SysfsName::sysfsNamePackageBurstPowerLimitInterval, 0, baseDirectoryExists).c_str());
    EXPECT_STREQ("power2_max", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNamePackageSustainedPowerLimit, 0, baseDirectoryExists).c_str());
    EXPECT_STREQ("power2_max_interval", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNamePackageSustainedPowerLimitInterval, 0, baseDirectoryExists).c_str());
    EXPECT_STREQ("power2_rated_max", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNamePackageDefaultPowerLimit, 0, baseDirectoryExists).c_str());
    EXPECT_STREQ("power2_crit", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNamePackageCriticalPowerLimit, 0, baseDirectoryExists).c_str());
}

TEST_F(SysmanFixtureDeviceXe, GivenSysmanKmdInterfaceWhenGettingHwMonNameThenCorrectPathIsReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    bool isSubdevice = true;
    EXPECT_STREQ("xe", pSysmanKmdInterface->getHwmonName(0, isSubdevice).c_str());
    isSubdevice = false;
    EXPECT_STREQ("xe", pSysmanKmdInterface->getHwmonName(0, isSubdevice).c_str());
}

TEST_F(SysmanFixtureDeviceXe, GivenSysmanKmdInterfaceWhenGettingEngineBasePathThenCorrectPathIsReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    EXPECT_STREQ("device/tile0/gt0/engines", pSysmanKmdInterface->getEngineBasePath(0).c_str());
}

TEST_F(SysmanFixtureDeviceXe, GivenSysmanKmdInterfaceWhenCallingGetEngineClassStringThenCorrectPathIsReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    EXPECT_STREQ("rcs", pSysmanKmdInterface->getEngineClassString(EngineClass::ENGINE_CLASS_RENDER).value().c_str());
    EXPECT_STREQ("bcs", pSysmanKmdInterface->getEngineClassString(EngineClass::ENGINE_CLASS_COPY).value().c_str());
    EXPECT_STREQ("vcs", pSysmanKmdInterface->getEngineClassString(EngineClass::ENGINE_CLASS_VIDEO).value().c_str());
    EXPECT_STREQ("ccs", pSysmanKmdInterface->getEngineClassString(EngineClass::ENGINE_CLASS_COMPUTE).value().c_str());
    EXPECT_STREQ("vecs", pSysmanKmdInterface->getEngineClassString(EngineClass::ENGINE_CLASS_VIDEO_ENHANCE).value().c_str());
}

TEST_F(SysmanFixtureDeviceXe, GivenSysmanKmdInterfaceInstanceWhenIsGroupEngineInterfaceAvailableCalledThenFalseValueIsReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    EXPECT_FALSE(pSysmanKmdInterface->isGroupEngineInterfaceAvailable());
}

TEST_F(SysmanFixtureDeviceXe, GivenSysmanKmdInterfaceInstanceWhenCheckingAvailabilityOfBaseFrequencyFactorAndSystemPowerBalanceThenTrueValueIsReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    EXPECT_FALSE(pSysmanKmdInterface->isBaseFrequencyFactorAvailable());
    EXPECT_FALSE(pSysmanKmdInterface->isSystemPowerBalanceAvailable());
    EXPECT_FALSE(pSysmanKmdInterface->isMediaFrequencyFactorAvailable());
}

TEST_F(SysmanFixtureDeviceXe, GivenSysmanKmdInterfaceInstanceWhenCheckingAvailabilityOfFrequencyFilesThenFalseValueIsReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    EXPECT_FALSE(pSysmanKmdInterface->isDefaultFrequencyAvailable());
    EXPECT_FALSE(pSysmanKmdInterface->isBoostFrequencyAvailable());
    EXPECT_FALSE(pSysmanKmdInterface->isTdpFrequencyAvailable());
}

TEST_F(SysmanFixtureDeviceXe, GivenSysmanKmdInterfaceInstanceWhenCheckingPhysicalMemorySizeAvailabilityThenTrueValueIsReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    EXPECT_TRUE(pSysmanKmdInterface->isPhysicalMemorySizeSupported());
}

TEST_F(SysmanFixtureDeviceXe, GivenSysmanKmdInterfaceInstanceWhenCallingGetNativeUnitWithProperSysfsNameThenValidValuesAreReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->getSysmanKmdInterface();
    EXPECT_EQ(SysfsValueUnit::micro, pSysmanKmdInterface->getNativeUnit(SysfsName::sysfsNameSchedulerTimeout));
    EXPECT_EQ(SysfsValueUnit::micro, pSysmanKmdInterface->getNativeUnit(SysfsName::sysfsNameSchedulerTimeslice));
    EXPECT_EQ(SysfsValueUnit::milli, pSysmanKmdInterface->getNativeUnit(SysfsName::sysfsNameSchedulerWatchDogTimeout));
    EXPECT_EQ(SysfsValueUnit::milli, pSysmanKmdInterface->getNativeUnit(SysfsName::sysfsNameSchedulerWatchDogTimeoutMaximum));
    EXPECT_EQ(SysfsValueUnit::micro, pSysmanKmdInterface->getNativeUnit(SysfsName::sysfsNamePackageSustainedPowerLimit));
    EXPECT_EQ(SysfsValueUnit::micro, pSysmanKmdInterface->getNativeUnit(SysfsName::sysfsNamePackageDefaultPowerLimit));
}

TEST_F(SysmanFixtureDeviceXe, GivenSysmanKmdInterfaceInstanceWhenCallingGetPowerDomainsThenValidPowerDomainsAreReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->getSysmanKmdInterface();

    std::vector<zes_power_domain_t> validPowerDomains = {ZES_POWER_DOMAIN_PACKAGE, ZES_POWER_DOMAIN_CARD, ZES_POWER_DOMAIN_MEMORY, ZES_POWER_DOMAIN_GPU};
    auto powerDomains = pSysmanKmdInterface->getPowerDomains();

    std::unordered_set<zes_power_domain_t> outputPowerDomainList(powerDomains.begin(), powerDomains.end());
    std::unordered_set<zes_power_domain_t> validPowerDomainList(validPowerDomains.begin(), validPowerDomains.end());

    EXPECT_EQ(validPowerDomainList, outputPowerDomainList);
}

TEST_F(SysmanFixtureDeviceXe, GivenSysmanKmdInterfaceAndIsIntegratedDeviceInstanceWhenGetEventsIsCalledThenValidEventTypeIsReturned) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, &mockReadSuccess);

    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    EXPECT_EQ(mockReadVal, pSysmanKmdInterface->getEventType());
}

TEST_F(SysmanFixtureDeviceXe, GivenSysmanKmdInterfaceInstanceWhenCheckingSupportForSettingSchedulerModesThenFalseValueIsReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    EXPECT_FALSE(pSysmanKmdInterface->isSettingExclusiveModeSupported());
    EXPECT_FALSE(pSysmanKmdInterface->isSettingTimeoutModeSupported());
}

TEST_F(SysmanFixtureDeviceXe, GivenSysmanKmdInterfaceWhenCheckingWhetherClientInfoAvailableInFdInfoThenTrueValueIsReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    EXPECT_TRUE(pSysmanKmdInterface->clientInfoAvailableInFdInfo());
}

TEST_F(SysmanFixtureDeviceXe, GivenSysmanKmdInterfaceInstanceWhenCheckingSupportForVfEngineUtilizationThenTrueValueIsReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    EXPECT_TRUE(pSysmanKmdInterface->isVfEngineUtilizationSupported());
}

TEST_F(SysmanFixtureDeviceXe, GivenSysmanKmdInterfaceInstanceWhenCallingGpuBindAndUnbindEntryThenProperStringIsReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    EXPECT_STREQ("/sys/bus/pci/drivers/xe/bind", pSysmanKmdInterface->getGpuBindEntry().c_str());
    EXPECT_STREQ("/sys/bus/pci/drivers/xe/unbind", pSysmanKmdInterface->getGpuUnBindEntry().c_str());
}

TEST_F(SysmanFixtureDeviceXe, GivenSysmanKmdInterfaceWhenGetEnergyCounterNodeFileIsCalledForDifferentPowerDomainsThenProperFileNameIsReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    std::string expectedFilePath = "energy1_input";
    EXPECT_EQ(expectedFilePath, pSysmanKmdInterface->getEnergyCounterNodeFile(ZES_POWER_DOMAIN_CARD));
    expectedFilePath = "energy2_input";
    EXPECT_EQ(expectedFilePath, pSysmanKmdInterface->getEnergyCounterNodeFile(ZES_POWER_DOMAIN_PACKAGE));
    expectedFilePath = "";
    EXPECT_EQ(expectedFilePath, pSysmanKmdInterface->getEnergyCounterNodeFile(ZES_POWER_DOMAIN_UNKNOWN));
}

TEST_F(SysmanFixtureDeviceXe, GivenInvalidConfigFromEventFileForEngineActiveTicksWhenCallingGetPmuConfigsForSingleEnginesThenErrorIsReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    auto pDrm = pLinuxSysmanImp->getDrm();
    std::vector<uint64_t> pmuConfigs = {};
    const std::string sysmanDeviceDir = "/sys/devices/0000:aa:bb:cc";
    pPmuInterface->mockEventConfigReturnValue.push_back(-1);
    EngineGroupInfo engineGroupInfo = {ZES_ENGINE_GROUP_COMPUTE_SINGLE, 0, 0};
    EXPECT_EQ(pSysmanKmdInterface->getPmuConfigsForSingleEngines(sysmanDeviceDir, engineGroupInfo, pPmuInterface.get(), pDrm, pmuConfigs), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
}

TEST_F(SysmanFixtureDeviceXe, GivenInvalidConfigFromEventFileForEngineActiveTicksWhenCallingGetPmuConfigsForGroupEnginesThenErrorIsReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    auto pDrm = pLinuxSysmanImp->getDrm();
    std::vector<uint64_t> pmuConfigs = {};
    const std::string sysmanDeviceDir = "/sys/devices/0000:aa:bb:cc";
    pPmuInterface->mockEventConfigReturnValue.push_back(-1);
    EngineGroupInfo engineGroupInfo = {ZES_ENGINE_GROUP_MEDIA_ALL, 0, 0};
    const MapOfEngineInfo mapEngineInfo = {
        {ZES_ENGINE_GROUP_ALL, {{0, 0}}},
        {ZES_ENGINE_GROUP_MEDIA_ALL, {{0, 0}}},
        {ZES_ENGINE_GROUP_MEDIA_ENCODE_SINGLE, {{0, 0}}}};
    EXPECT_EQ(pSysmanKmdInterface->getPmuConfigsForGroupEngines(mapEngineInfo, sysmanDeviceDir, engineGroupInfo, pPmuInterface.get(), pDrm, pmuConfigs), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
}

TEST_F(SysmanFixtureDeviceXe, GivenInvalidConfigFromEventFileForEngineTotalTicksWhenCallingGetPmuConfigsForGroupEnginesThenErrorIsReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    auto pDrm = pLinuxSysmanImp->getDrm();
    std::vector<uint64_t> pmuConfigs = {};
    const std::string sysmanDeviceDir = "/sys/devices/0000:aa:bb:cc";
    pPmuInterface->mockEventConfigReturnValue.push_back(0);
    pPmuInterface->mockEventConfigReturnValue.push_back(-1);
    EngineGroupInfo engineGroupInfo = {ZES_ENGINE_GROUP_MEDIA_ALL, 0, 0};
    EXPECT_EQ(pSysmanKmdInterface->getPmuConfigsForGroupEngines(mockMapEngineInfo, sysmanDeviceDir, engineGroupInfo, pPmuInterface.get(), pDrm, pmuConfigs), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
}

TEST_F(SysmanFixtureDeviceXe, GivenInvalidConfigAfterFormatForEngineActiveTicksWhenCallingGetPmuConfigsForGroupEnginesThenErrorIsReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    auto pDrm = pLinuxSysmanImp->getDrm();
    std::vector<uint64_t> pmuConfigs = {};
    const std::string sysmanDeviceDir = "/sys/devices/0000:aa:bb:cc";
    pPmuInterface->mockFormatConfigReturnValue.push_back(-1);
    EngineGroupInfo engineGroupInfo = {ZES_ENGINE_GROUP_MEDIA_ALL, 0, 0};
    const MapOfEngineInfo mapEngineInfo = {
        {ZES_ENGINE_GROUP_ALL, {{0, 0}}},
        {ZES_ENGINE_GROUP_MEDIA_ALL, {{0, 0}}},
        {ZES_ENGINE_GROUP_MEDIA_ENHANCEMENT_SINGLE, {{0, 0}}}};
    EXPECT_EQ(pSysmanKmdInterface->getPmuConfigsForGroupEngines(mapEngineInfo, sysmanDeviceDir, engineGroupInfo, pPmuInterface.get(), pDrm, pmuConfigs), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
}

TEST_F(SysmanFixtureDeviceXe, GivenMediaGroupEngineAndNoMediaSingleEnginesAvailableWhenCallingGetPmuConfigsForGroupEnginesThenNoPmuConfigsAreReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    auto pDrm = pLinuxSysmanImp->getDrm();
    std::vector<uint64_t> pmuConfigs = {};
    const std::string sysmanDeviceDir = "/sys/devices/0000:aa:bb:cc";
    EngineGroupInfo engineGroupInfo = {ZES_ENGINE_GROUP_MEDIA_ALL, 0, 0};
    const MapOfEngineInfo mapEngineInfo = {{ZES_ENGINE_GROUP_MEDIA_ALL, {{0, 0}}}};
    EXPECT_EQ(pSysmanKmdInterface->getPmuConfigsForGroupEngines(mapEngineInfo, sysmanDeviceDir, engineGroupInfo, pPmuInterface.get(), pDrm, pmuConfigs), ZE_RESULT_SUCCESS);
    EXPECT_TRUE(pmuConfigs.empty());
}

TEST_F(SysmanFixtureDeviceXe, GivenComputeGroupEngineAndInvalidConfigAfterFormatForEngineActiveTicksWhenCallingGetPmuConfigsForGroupEnginesThenErrorIsReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    auto pDrm = pLinuxSysmanImp->getDrm();
    std::vector<uint64_t> pmuConfigs = {};
    const std::string sysmanDeviceDir = "/sys/devices/0000:aa:bb:cc";
    pPmuInterface->mockFormatConfigReturnValue.push_back(-1);
    EngineGroupInfo engineGroupInfo = {ZES_ENGINE_GROUP_COMPUTE_ALL, 0, 0};
    EXPECT_EQ(pSysmanKmdInterface->getPmuConfigsForGroupEngines(mockMapEngineInfo, sysmanDeviceDir, engineGroupInfo, pPmuInterface.get(), pDrm, pmuConfigs), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
}

TEST_F(SysmanFixtureDeviceXe, GivenCopyGroupEngineAndInvalidConfigAfterFormatForEngineActiveTicksWhenCallingGetPmuConfigsForGroupEnginesThenErrorIsReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    auto pDrm = pLinuxSysmanImp->getDrm();
    std::vector<uint64_t> pmuConfigs = {};
    const std::string sysmanDeviceDir = "/sys/devices/0000:aa:bb:cc";
    pPmuInterface->mockFormatConfigReturnValue.push_back(-1);
    EngineGroupInfo engineGroupInfo = {ZES_ENGINE_GROUP_COPY_ALL, 0, 0};
    EXPECT_EQ(pSysmanKmdInterface->getPmuConfigsForGroupEngines(mockMapEngineInfo, sysmanDeviceDir, engineGroupInfo, pPmuInterface.get(), pDrm, pmuConfigs), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
}

TEST_F(SysmanFixtureDeviceXe, GivenRenderGroupEngineAndInvalidConfigAfterFormatForEngineActiveTicksWhenCallingGetPmuConfigsForGroupEnginesThenErrorIsReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    auto pDrm = pLinuxSysmanImp->getDrm();
    std::vector<uint64_t> pmuConfigs = {};
    const std::string sysmanDeviceDir = "/sys/devices/0000:aa:bb:cc";
    pPmuInterface->mockFormatConfigReturnValue.push_back(-1);
    EngineGroupInfo engineGroupInfo = {ZES_ENGINE_GROUP_RENDER_ALL, 0, 0};
    EXPECT_EQ(pSysmanKmdInterface->getPmuConfigsForGroupEngines(mockMapEngineInfo, sysmanDeviceDir, engineGroupInfo, pPmuInterface.get(), pDrm, pmuConfigs), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
}

TEST_F(SysmanFixtureDeviceXe, GivenGroupAllEngineAndInvalidConfigAfterFormatForEngineActiveTicksWhenCallingGetPmuConfigsForGroupEnginesThenErrorIsReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    auto pDrm = pLinuxSysmanImp->getDrm();
    std::vector<uint64_t> pmuConfigs = {};
    const std::string sysmanDeviceDir = "/sys/devices/0000:aa:bb:cc";
    pPmuInterface->mockFormatConfigReturnValue.push_back(-1);
    EngineGroupInfo engineGroupInfo = {ZES_ENGINE_GROUP_ALL, 0, 0};
    EXPECT_EQ(pSysmanKmdInterface->getPmuConfigsForGroupEngines(mockMapEngineInfo, sysmanDeviceDir, engineGroupInfo, pPmuInterface.get(), pDrm, pmuConfigs), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
}

TEST_F(SysmanFixtureDeviceXe, GivenInvalidConfigAfterFormatForEngineTotalTicksWhenCallingGetPmuConfigsForSingleEnginesThenErrorIsReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    auto pDrm = pLinuxSysmanImp->getDrm();
    std::vector<uint64_t> pmuConfigs = {};
    const std::string sysmanDeviceDir = "/sys/devices/0000:aa:bb:cc";
    pPmuInterface->mockFormatConfigReturnValue.push_back(0);
    pPmuInterface->mockFormatConfigReturnValue.push_back(-1);
    EngineGroupInfo engineGroupInfo = {ZES_ENGINE_GROUP_COMPUTE_SINGLE, 0, 0};
    EXPECT_EQ(pSysmanKmdInterface->getPmuConfigsForSingleEngines(sysmanDeviceDir, engineGroupInfo, pPmuInterface.get(), pDrm, pmuConfigs), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
}

TEST_F(SysmanFixtureDeviceXe, GivenMediaGroupEngineWhenCallingGetPmuConfigsForGroupEnginesThenSuccessIsReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    auto pDrm = pLinuxSysmanImp->getDrm();
    std::vector<uint64_t> mockPmuConfigs = {1, 2, 1, 2, 1, 2};
    std::vector<uint64_t> pmuConfigs = {};
    const std::string sysmanDeviceDir = "/sys/devices/0000:aa:bb:cc";
    EngineGroupInfo engineGroupInfo = {ZES_ENGINE_GROUP_MEDIA_ALL, 0, 0};
    EXPECT_EQ(pSysmanKmdInterface->getPmuConfigsForGroupEngines(mockMapEngineInfo, sysmanDeviceDir, engineGroupInfo, pPmuInterface.get(), pDrm, pmuConfigs), ZE_RESULT_SUCCESS);
    for (uint32_t i = 0; i < pmuConfigs.size(); i++) {
        EXPECT_EQ(pmuConfigs[i], mockPmuConfigs[i]);
    }
}

TEST_F(SysmanFixtureDeviceXe, GivenSysmanKmdInterfaceAndPmuReadFailsWhenCallingReadBusynessFromGroupFdThenErrorIsReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    std::vector<int64_t> fdList = {1, 2};
    zes_engine_stats_t pStats = {};
    pPmuInterface->mockPmuReadFailureReturnValue = -1;
    EXPECT_EQ(pSysmanKmdInterface->readBusynessFromGroupFd(pPmuInterface.get(), fdList, &pStats), ZE_RESULT_ERROR_UNKNOWN);
}

TEST_F(SysmanFixtureDeviceXe, GivenSysmanKmdInterfaceAndGetPmuConfigsFailsWhenCallingGetBusyAndTotalTicksConfigsForVfThenErrorIsReturned) {

    pPmuInterface->mockEventConfigReturnValue.push_back(-1);
    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    std::pair<uint64_t, uint64_t> configPair;
    EXPECT_EQ(pSysmanKmdInterface->getBusyAndTotalTicksConfigsForVf(pPmuInterface.get(), 0, 0, 1, 0, configPair), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
}

TEST_F(SysmanFixtureDeviceXe, GivenSysmanKmdInterfaceAndGetPmuConfigsForVfFailsWhenCallingGetBusyAndTotalTicksConfigsForVfThenErrorIsReturned) {
    pPmuInterface->mockVfConfigReturnValue.push_back(-1);
    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    std::pair<uint64_t, uint64_t> configPair;
    EXPECT_EQ(pSysmanKmdInterface->getBusyAndTotalTicksConfigsForVf(pPmuInterface.get(), 0, 0, 1, 0, configPair), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
}

TEST_F(SysmanFixtureDeviceXe, GivenSysmanKmdInterfaceWhenCallingGetBusyAndTotalTicksConfigsForVfThenValidConfigsAndSuccessIsReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    std::pair<uint64_t, uint64_t> configPair;
    EXPECT_EQ(pSysmanKmdInterface->getBusyAndTotalTicksConfigsForVf(pPmuInterface.get(), 0, 0, 1, 0, configPair), ZE_RESULT_SUCCESS);
    EXPECT_EQ(pPmuInterface->mockActiveTicksConfig, configPair.first);
    EXPECT_EQ(pPmuInterface->mockTotalTicksConfig, configPair.second);
}

TEST_F(SysmanFixtureDeviceXe, GivenSysmanKmdInterfaceWhenCallingReadPcieDowngradeAttributeWithInvalidSysfsNodeThenUnsupportedIsReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    uint32_t val;
    EXPECT_EQ(pSysmanKmdInterface->readPcieDowngradeAttribute("unknown", val), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
}

TEST_F(SysmanFixtureDeviceXe, GivenSysmanKmdInterfaceWhenCallingIsLateBindingVersionAvailableWithInvalidSysfsNodeThenUnsupportedIsReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    std::string val;
    EXPECT_FALSE(pSysmanKmdInterface->isLateBindingVersionAvailable("unknown", val));
}

} // namespace ult
} // namespace Sysman
} // namespace L0
