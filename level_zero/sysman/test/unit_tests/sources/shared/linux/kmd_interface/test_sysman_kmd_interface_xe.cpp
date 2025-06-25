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

#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"
#include "level_zero/sysman/test/unit_tests/sources/shared/linux/kmd_interface/mock_sysman_kmd_interface_xe.h"
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
    EXPECT_STREQ("device/tile0/gt0/freq0/min_freq", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameMinFrequency, 0, baseDirectoryExists).c_str());
    EXPECT_STREQ("device/tile0/gt0/freq0/max_freq", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameMaxFrequency, 0, baseDirectoryExists).c_str());
    EXPECT_STREQ("device/tile0/gt0/freq0/cur_freq", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameCurrentFrequency, 0, baseDirectoryExists).c_str());
    EXPECT_STREQ("device/tile0/gt0/freq0/act_freq", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameActualFrequency, 0, baseDirectoryExists).c_str());
    EXPECT_STREQ("device/tile0/gt0/freq0/rpe_freq", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameEfficientFrequency, 0, baseDirectoryExists).c_str());
    EXPECT_STREQ("device/tile0/gt0/freq0/rp0_freq", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameMaxValueFrequency, 0, baseDirectoryExists).c_str());
    EXPECT_STREQ("device/tile0/gt0/freq0/rpn_freq", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameMinValueFrequency, 0, baseDirectoryExists).c_str());
    EXPECT_STREQ("device/tile0/gt0/freq0/throttle/status", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameThrottleReasonStatus, 0, baseDirectoryExists).c_str());
    EXPECT_STREQ("device/tile0/gt0/freq0/throttle/reason_pl1", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameThrottleReasonPL1, 0, baseDirectoryExists).c_str());
    EXPECT_STREQ("device/tile0/gt0/freq0/throttle/reason_pl2", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameThrottleReasonPL2, 0, baseDirectoryExists).c_str());
    EXPECT_STREQ("device/tile0/gt0/freq0/throttle/reason_pl4", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameThrottleReasonPL4, 0, baseDirectoryExists).c_str());
    EXPECT_STREQ("device/tile0/gt0/freq0/throttle/reason_thermal", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameThrottleReasonThermal, 0, baseDirectoryExists).c_str());
    EXPECT_STREQ("device/tile0/physical_vram_size_bytes", pSysmanKmdInterface->getSysfsFilePathForPhysicalMemorySize(0).c_str());
    EXPECT_STREQ("device/tile0/gt0/freq_vram_rp0", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameMaxMemoryFrequency, 0, baseDirectoryExists).c_str());
    EXPECT_STREQ("device/tile0/gt0/freq_vram_rpn", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameMinMemoryFrequency, 0, baseDirectoryExists).c_str());
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

TEST_F(SysmanFixtureDeviceXe, Given3DSingleEngineTypeAndSysmanKmdInterfaceWhenCallingGetEngineActivityFdListThenErrorIsReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    std::vector<std::pair<int64_t, int64_t>> fdList = {};
    std::pair<uint64_t, uint64_t> configPair = {};
    EXPECT_EQ(pSysmanKmdInterface->getEngineActivityFdListAndConfigPair(ZES_ENGINE_GROUP_3D_SINGLE, 0, 0, pPmuInterface.get(), fdList, configPair), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
}

TEST_F(SysmanFixtureDeviceXe, GivenSysmanKmdInterfaceAndGettingConfigFromEventFileFailsForEngineActiveTicksWhenCallingGetEngineActivityFdListThenErrorIsReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();

    std::vector<std::pair<int64_t, int64_t>> fdList = {};
    std::pair<uint64_t, uint64_t> configPair = {};
    pPmuInterface->mockEventConfigReturnValue.push_back(-1);
    EXPECT_EQ(pSysmanKmdInterface->getEngineActivityFdListAndConfigPair(ZES_ENGINE_GROUP_COMPUTE_SINGLE, 0, 0, pPmuInterface.get(), fdList, configPair), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
}

TEST_F(SysmanFixtureDeviceXe, GivenSysmanKmdInterfaceAndGettingConfigFromEventFileFailsForEngineTotalTicksWhenCallingGetEngineActivityFdListThenErrorIsReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();

    std::vector<std::pair<int64_t, int64_t>> fdList = {};
    std::pair<uint64_t, uint64_t> configPair = {};
    pPmuInterface->mockEventConfigReturnValue.push_back(0);
    pPmuInterface->mockEventConfigReturnValue.push_back(-1);
    EXPECT_EQ(pSysmanKmdInterface->getEngineActivityFdListAndConfigPair(ZES_ENGINE_GROUP_COMPUTE_SINGLE, 0, 0, pPmuInterface.get(), fdList, configPair), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
}

TEST_F(SysmanFixtureDeviceXe, GivenSysmanKmdInterfaceAndGettingConfigAfterFormatFailsForEngineActiveTicksWhenCallingGetEngineActivityFdListThenErrorIsReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();

    std::vector<std::pair<int64_t, int64_t>> fdList = {};
    std::pair<uint64_t, uint64_t> configPair = {};
    pPmuInterface->mockFormatConfigReturnValue.push_back(-1);
    EXPECT_EQ(pSysmanKmdInterface->getEngineActivityFdListAndConfigPair(ZES_ENGINE_GROUP_COMPUTE_SINGLE, 0, 0, pPmuInterface.get(), fdList, configPair), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
}

TEST_F(SysmanFixtureDeviceXe, GivenSysmanKmdInterfaceAndGettingConfigAfterFormatFailsForEngineTotalTicksWhenCallingGetEngineActivityFdListThenErrorIsReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();

    std::vector<std::pair<int64_t, int64_t>> fdList = {};
    std::pair<uint64_t, uint64_t> configPair = {};
    pPmuInterface->mockFormatConfigReturnValue.push_back(0);
    pPmuInterface->mockFormatConfigReturnValue.push_back(-1);
    EXPECT_EQ(pSysmanKmdInterface->getEngineActivityFdListAndConfigPair(ZES_ENGINE_GROUP_COMPUTE_SINGLE, 0, 0, pPmuInterface.get(), fdList, configPair), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
}

TEST_F(SysmanFixtureDeviceXe, GivenSysmanKmdInterfaceAndPmuInterfaceOpenFailsForBusyTicksHandleWhenCallingGetEngineActivityFdListThenErrorIsReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();

    std::vector<std::pair<int64_t, int64_t>> fdList = {};
    std::pair<uint64_t, uint64_t> configPair = {};

    pPmuInterface->mockPerfEventOpenReadFail = true;
    EXPECT_EQ(pSysmanKmdInterface->getEngineActivityFdListAndConfigPair(ZES_ENGINE_GROUP_COMPUTE_SINGLE, 0, 0, pPmuInterface.get(), fdList, configPair), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
}

TEST_F(SysmanFixtureDeviceXe, GivenSysmanKmdInterfaceAndPmuInterfaceOpenFailsForTotalTicksHandleWhenCallingGetEngineActivityFdListThenErrorIsReturned) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::ostringstream oStream;
        oStream << 23;
        std::string value = oStream.str();
        memcpy(buf, value.data(), count);
        return count;
    });

    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();

    std::vector<std::pair<int64_t, int64_t>> fdList = {};
    std::pair<uint64_t, uint64_t> configPair = {};

    pPmuInterface->mockPerfEventOpenReadFail = true;
    pPmuInterface->mockPerfEventOpenFailAtCount = 3;
    pPmuInterface->mockPmuFd = 10;
    EXPECT_EQ(pSysmanKmdInterface->getEngineActivityFdListAndConfigPair(ZES_ENGINE_GROUP_COMPUTE_SINGLE, 0, 0, pPmuInterface.get(), fdList, configPair), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
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

} // namespace ult
} // namespace Sysman
} // namespace L0