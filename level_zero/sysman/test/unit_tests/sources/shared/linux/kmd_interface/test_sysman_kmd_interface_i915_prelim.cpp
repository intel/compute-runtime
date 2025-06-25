/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/string.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_execution_environment.h"

#include "level_zero/sysman/source/shared/linux/kmd_interface/sysman_kmd_interface.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_hw_device_id.h"
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

class SysmanFixtureDeviceI915Prelim : public SysmanDeviceFixture {
  protected:
    L0::Sysman::SysmanDevice *device = nullptr;
    std::unique_ptr<MockPmuInterfaceImp> pPmuInterface;

    void SetUp() override {
        VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSuccess);
        SysmanDeviceFixture::SetUp();
        device = pSysmanDevice;
        pLinuxSysmanImp->pSysmanKmdInterface.reset(new SysmanKmdInterfaceI915Prelim(pLinuxSysmanImp->getSysmanProductHelper()));
        mockInitFsAccess();
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

TEST_F(SysmanFixtureDeviceI915Prelim, GivenI915PrelimVersionWhenSysmanKmdInterfaceInstanceIsCreatedThenValidPtrIsReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->getSysmanKmdInterface();
    EXPECT_NE(nullptr, pSysmanKmdInterface);
}

TEST_F(SysmanFixtureDeviceI915Prelim, GivenSysmanKmdInterfaceInstanceWhenCallingGetHwmonNameThenEmptyNameIsReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->getSysmanKmdInterface();
    EXPECT_STREQ("i915_gt0", pSysmanKmdInterface->getHwmonName(0, true).c_str());
    EXPECT_STREQ("i915", pSysmanKmdInterface->getHwmonName(0, false).c_str());
}

TEST_F(SysmanFixtureDeviceI915Prelim, GivenSysmanKmdInterfaceInstanceWhenCallingGetEngineBasePathThenCorrectPathIsReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->getSysmanKmdInterface();
    EXPECT_STREQ("engine", pSysmanKmdInterface->getEngineBasePath(0).c_str());
}

TEST_F(SysmanFixtureDeviceI915Prelim, GivenSysmanKmdInterfaceWhenCallingGetSysmanDeviceDirNameForDiscreteDeviceThenCorrectNameIsReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    EXPECT_STREQ("i915_0000_03_00.0", pSysmanKmdInterface->getSysmanDeviceDirName().c_str());
}

TEST_F(SysmanFixtureDeviceI915Prelim, GivenSysmanKmdInterfaceAndReadSymLinkFailsWhenCallingGetSysmanDeviceDirNameForDiscreteDeviceThenEmptyStringIsReturned) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkFailure);
    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    bool isIntegratedDevice = false;
    pSysmanKmdInterface->setSysmanDeviceDirName(isIntegratedDevice);
    EXPECT_STREQ("", pSysmanKmdInterface->getSysmanDeviceDirName().c_str());
}

TEST_F(SysmanFixtureDeviceI915Prelim, GivenSysmanKmdInterfaceWhenCallingGetSysmanDeviceDirNameForIntegratedDeviceThenCorrectNameIsReturned) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSuccess);
    auto pSysmanKmdInterface = std::make_unique<SysmanKmdInterfaceI915Prelim>(pLinuxSysmanImp->getSysmanProductHelper());
    mockInitFsAccess();
    bool isIntegratedDevice = true;
    pSysmanKmdInterface->setSysmanDeviceDirName(isIntegratedDevice);
    EXPECT_STREQ("i915", pSysmanKmdInterface->getSysmanDeviceDirName().c_str());
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
    EXPECT_EQ(SysfsValueUnit::milli, pSysmanKmdInterface->getNativeUnit(SysfsName::sysfsNameSchedulerTimeout));
    EXPECT_EQ(SysfsValueUnit::milli, pSysmanKmdInterface->getNativeUnit(SysfsName::sysfsNameSchedulerTimeslice));
    EXPECT_EQ(SysfsValueUnit::milli, pSysmanKmdInterface->getNativeUnit(SysfsName::sysfsNameSchedulerWatchDogTimeout));
    EXPECT_EQ(SysfsValueUnit::micro, pSysmanKmdInterface->getNativeUnit(SysfsName::sysfsNamePackageSustainedPowerLimit));
    EXPECT_EQ(SysfsValueUnit::micro, pSysmanKmdInterface->getNativeUnit(SysfsName::sysfsNamePackageDefaultPowerLimit));
}

TEST_F(SysmanFixtureDeviceI915Prelim, GivenSysmanKmdInterfaceInstanceWhenCallingGetPowerDomainsThenValidPowerDomainsAreReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->getSysmanKmdInterface();

    std::vector<zes_power_domain_t> validPowerDomains = {ZES_POWER_DOMAIN_PACKAGE};
    auto powerDomains = pSysmanKmdInterface->getPowerDomains();

    std::unordered_set<zes_power_domain_t> outputPowerDomainList(powerDomains.begin(), powerDomains.end());
    std::unordered_set<zes_power_domain_t> validPowerDomainList(validPowerDomains.begin(), validPowerDomains.end());

    EXPECT_EQ(validPowerDomainList, outputPowerDomainList);
}

TEST_F(SysmanFixtureDeviceI915Prelim, GivenSysmanKmdInterfaceInstanceWhenCheckingSupportForI915DriverThenProperStatusIsReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->getSysmanKmdInterface();
    EXPECT_FALSE(pSysmanKmdInterface->clientInfoAvailableInFdInfo());
    EXPECT_TRUE(pSysmanKmdInterface->isGroupEngineInterfaceAvailable());
}

TEST_F(SysmanFixtureDeviceI915Prelim, GivenSysmanKmdInterfaceInstanceWhenCheckingSupportForStandbyModeThenProperStatusIsReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->getSysmanKmdInterface();
    EXPECT_TRUE(pSysmanKmdInterface->isStandbyModeControlAvailable());
}

TEST_F(SysmanFixtureDeviceI915Prelim, GivenSysmanKmdInterfaceInstanceAndIsIntegratedDeviceWhenGetEventsIsCalledThenValidEventTypeIsReturned) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, &mockReadSuccess);

    bool isIntegratedDevice = true;
    pLinuxSysmanImp->pSysmanKmdInterface->setSysmanDeviceDirName(isIntegratedDevice);

    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    EXPECT_EQ(mockReadVal, pSysmanKmdInterface->getEventType());
}

TEST_F(SysmanFixtureDeviceI915Prelim, GivenSysmanKmdInterfaceInstanceAndIsNotIntegratedDeviceWhenGetEventsIsCalledThenValidEventTypeIsReturned) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, &mockReadSuccess);

    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    EXPECT_EQ(mockReadVal, pSysmanKmdInterface->getEventType());
}

TEST_F(SysmanFixtureDeviceI915Prelim, GivenSysmanKmdInterfaceInstanceAndIsNotIntegratedDeviceAndReadSymLinkFailsWhenGetEventsIsCalledThenFailureIsReturned) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkFailure);

    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    EXPECT_EQ(0u, pSysmanKmdInterface->getEventType());
}

TEST_F(SysmanFixtureDeviceI915Prelim, GivenSysmanKmdInterfaceInstanceAndIsNotIntegratedDeviceAndFsReadFailsWhenGetEventsIsCalledThenFailureIsReturned) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, &mockReadFailure);

    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    EXPECT_EQ(0u, pSysmanKmdInterface->getEventType());
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

TEST_F(SysmanFixtureDeviceI915Prelim, GivenSysmanKmdInterfaceInstanceWhenCallingGetEngineClassStringForComputeThenValidStringIsReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->getSysmanKmdInterface();
    EXPECT_EQ("ccs", pSysmanKmdInterface->getEngineClassString(EngineClass::ENGINE_CLASS_COMPUTE));
}

TEST_F(SysmanFixtureDeviceI915Prelim, GivenSysmanKmdInterfaceInstanceWhenCallingGetNumEngineTypeAndInstancesThenErrorIsReturned) {
    std::vector<std::string> mockVecString = {"rcs"};
    std::map<zes_engine_type_flag_t, std::vector<std::string>> mockMapofEngine = {{ZES_ENGINE_TYPE_FLAG_RENDER, mockVecString}};
    auto pSysmanKmdInterface = pLinuxSysmanImp->getSysmanKmdInterface();

    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pSysmanKmdInterface->getNumEngineTypeAndInstances(
                                                       mockMapofEngine, pLinuxSysmanImp, nullptr, true, 0));
}

TEST_F(SysmanFixtureDeviceI915Prelim, GivenSysmanKmdInterfaceInstanceWhenCallingGetDeviceWedgedStatusThenVerifyDeviceIsNotWedged) {
    class DrmMock : public Drm {
      public:
        DrmMock(RootDeviceEnvironment &rootDeviceEnvironment) : Drm(std::make_unique<MockSysmanHwDeviceIdDrm>(mockFd, ""), rootDeviceEnvironment) {}
        using Drm::setupIoctlHelper;
        int ioctlRetVal = 0;
        int ioctlErrno = 0;
        int mockFd = 33;
        int ioctl(DrmIoctl request, void *arg) override {
            return ioctlRetVal;
        }
        int getErrno() override { return ioctlErrno; }
    };
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto pDrm = new DrmMock(const_cast<NEO::RootDeviceEnvironment &>(pSysmanDeviceImp->getRootDeviceEnvironment()));
    pDrm->setupIoctlHelper(pSysmanDeviceImp->getRootDeviceEnvironment().getHardwareInfo()->platform.eProductFamily);
    auto &osInterface = pSysmanDeviceImp->getRootDeviceEnvironment().osInterface;
    osInterface->setDriverModel(std::unique_ptr<DrmMock>(pDrm));
    auto pSysmanKmdInterface = pLinuxSysmanImp->getSysmanKmdInterface();
    zes_device_state_t deviceState = {};
    pSysmanKmdInterface->getWedgedStatus(pLinuxSysmanImp, &deviceState);
    EXPECT_EQ(0u, deviceState.reset & ZES_RESET_REASON_FLAG_WEDGED);
}

TEST_F(SysmanFixtureDeviceI915Prelim, GivenSysmanKmdInterfaceInstanceWhenCheckingSupportForSettingSchedulerModesThenTrueValueIsReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    EXPECT_TRUE(pSysmanKmdInterface->isSettingExclusiveModeSupported());
    EXPECT_TRUE(pSysmanKmdInterface->isSettingTimeoutModeSupported());
}

TEST_F(SysmanFixtureDeviceI915Prelim, GivenSysmanKmdInterfaceWhenCheckingWhetherClientInfoAvailableInFdInfoThenFalseValueIsReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    EXPECT_FALSE(pSysmanKmdInterface->clientInfoAvailableInFdInfo());
}

TEST_F(SysmanFixtureDeviceI915Prelim, GivenSysmanKmdInterfaceInstanceWhenCheckingSupportForVfEngineUtilizationThenTrueValueIsReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    EXPECT_TRUE(pSysmanKmdInterface->isVfEngineUtilizationSupported());
}

TEST_F(SysmanFixtureDeviceI915Prelim, GivenSysmanKmdInterfaceInstanceWhenCallingGpuBindAndUnbindEntryThenProperStringIsReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    EXPECT_STREQ("/sys/bus/pci/drivers/i915/bind", pSysmanKmdInterface->getGpuBindEntry().c_str());
    EXPECT_STREQ("/sys/bus/pci/drivers/i915/unbind", pSysmanKmdInterface->getGpuUnBindEntry().c_str());
}

TEST_F(SysmanFixtureDeviceI915Prelim, GivenSysmanKmdInterfaceWhenGetEnergyCounterNodeFileIsCalledForDifferentPowerDomainsThenProperPathIsReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    std::string expectedFilePath = "energy1_input";
    EXPECT_EQ(expectedFilePath, pSysmanKmdInterface->getEnergyCounterNodeFile(ZES_POWER_DOMAIN_PACKAGE));
    expectedFilePath = "";
    EXPECT_EQ(expectedFilePath, pSysmanKmdInterface->getEnergyCounterNodeFile(ZES_POWER_DOMAIN_CARD));
}

TEST_F(SysmanFixtureDeviceI915Prelim, GivenSysmanKmdInterfaceInstanceAndPmuFailsDueToTooManyFilesOpenWhenGetEngineActivityFdListIsCalledThenErrorIsReturned) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, &mockReadSuccess);

    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    std::vector<std::pair<int64_t, int64_t>> fdList = {};
    std::pair<uint64_t, uint64_t> configPair = {};
    pPmuInterface->mockErrorNumber = EMFILE;
    pPmuInterface->mockPerfEventOpenReadFail = true;
    EXPECT_EQ(pSysmanKmdInterface->getEngineActivityFdListAndConfigPair(ZES_ENGINE_GROUP_ALL, 0, 0, pPmuInterface.get(), fdList, configPair), ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE);
}

TEST_F(SysmanFixtureDeviceI915Prelim, GivenSysmanKmdInterfaceInstanceAndPmuOpenFailsDueToFileTableOverFlowWhenGetEngineActivityFdListIsCalledThenErrorIsReturned) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, &mockReadSuccess);

    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    std::vector<std::pair<int64_t, int64_t>> fdList = {};
    std::pair<uint64_t, uint64_t> configPair = {};
    pPmuInterface->mockErrorNumber = ENFILE;
    pPmuInterface->mockPerfEventOpenReadFail = true;
    EXPECT_EQ(pSysmanKmdInterface->getEngineActivityFdListAndConfigPair(ZES_ENGINE_GROUP_ALL, 0, 0, pPmuInterface.get(), fdList, configPair), ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE);
}

} // namespace ult
} // namespace Sysman
} // namespace L0
