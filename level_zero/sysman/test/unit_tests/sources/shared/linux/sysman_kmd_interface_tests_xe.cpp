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

class SysmanFixtureDeviceXe : public SysmanDeviceFixture {
  protected:
    L0::Sysman::SysmanDevice *device = nullptr;
    std::unique_ptr<MockPmuInterfaceImp> pPmuInterface;

    void SetUp() override {
        SysmanDeviceFixture::SetUp();
        device = pSysmanDevice;
        pPmuInterface = std::make_unique<MockPmuInterfaceImp>(pLinuxSysmanImp);
        pLinuxSysmanImp->pSysmanKmdInterface.reset(new SysmanKmdInterfaceXe(pLinuxSysmanImp->getProductFamily()));
        mockInitFsAccess();
        pPmuInterface->pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
        VariableBackup<L0::Sysman::PmuInterface *> pmuBackup(&pLinuxSysmanImp->pPmuInterface);
        pLinuxSysmanImp->pPmuInterface = pPmuInterface.get();
    }

    void TearDown() override {
        SysmanDeviceFixture::TearDown();
    }

    void mockInitFsAccess() {
        VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSuccess);
        pLinuxSysmanImp->pSysmanKmdInterface->initFsAccessInterface(*pLinuxSysmanImp->getDrm());
    }
};

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

TEST_F(SysmanFixtureDeviceXe, GivenSysmanKmdInterfaceInstanceWhenIsGroupEngineInterfaceAvailableCalledThenTrueValueIsReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    EXPECT_TRUE(pSysmanKmdInterface->isGroupEngineInterfaceAvailable());
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
    EXPECT_EQ(SysmanKmdInterface::SysfsValueUnit::micro, pSysmanKmdInterface->getNativeUnit(SysfsName::sysfsNameSchedulerTimeout));
    EXPECT_EQ(SysmanKmdInterface::SysfsValueUnit::micro, pSysmanKmdInterface->getNativeUnit(SysfsName::sysfsNameSchedulerTimeslice));
    EXPECT_EQ(SysmanKmdInterface::SysfsValueUnit::milli, pSysmanKmdInterface->getNativeUnit(SysfsName::sysfsNameSchedulerWatchDogTimeout));
    EXPECT_EQ(SysmanKmdInterface::SysfsValueUnit::milli, pSysmanKmdInterface->getNativeUnit(SysfsName::sysfsNameSchedulerWatchDogTimeoutMaximum));
    EXPECT_EQ(SysmanKmdInterface::SysfsValueUnit::micro, pSysmanKmdInterface->getNativeUnit(SysfsName::sysfsNameSustainedPowerLimit));
    EXPECT_EQ(SysmanKmdInterface::SysfsValueUnit::micro, pSysmanKmdInterface->getNativeUnit(SysfsName::sysfsNameCriticalPowerLimit));
    EXPECT_EQ(SysmanKmdInterface::SysfsValueUnit::micro, pSysmanKmdInterface->getNativeUnit(SysfsName::sysfsNameDefaultPowerLimit));
}

TEST_F(SysmanFixtureDeviceXe, GivenGroupEngineTypeAndSysmanKmdInterfaceInstanceWhenGetEngineActivityFdIsCalledThenUnsupportedFeatureIsReturned) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, &mockReadSuccess);

    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();

    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pSysmanKmdInterface->getEngineActivityFd(ZES_ENGINE_GROUP_ALL, 0, 0, pPmuInterface.get()));
}

TEST_F(SysmanFixtureDeviceXe, GivenSingleEngineTypeAndSysmanKmdInterfaceInstanceWhenGetEngineActivityFdIsCalledThenUnsupportedFeatureIsReturned) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, &mockReadSuccess);

    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pSysmanKmdInterface->getEngineActivityFd(ZES_ENGINE_GROUP_COMPUTE_SINGLE, 0, 0, pPmuInterface.get()));
}

TEST_F(SysmanFixtureDeviceXe, GivenSysmanKmdInterfaceAndIsIntegratedDeviceInstanceWhenGetEventsIsCalledThenValidEventTypeIsReturned) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, &mockReadSuccess);

    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    bool isIntegratedDevice = true;
    EXPECT_EQ(mockReadVal, pSysmanKmdInterface->getEventType(isIntegratedDevice));
}

TEST_F(SysmanFixtureDeviceXe, GivenSysmanKmdInterfaceAndIsNotIntegratedDeviceInstanceWhenGetEventsIsCalledThenValidEventTypeIsReturned) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, &mockReadSuccess);

    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    bool isIntegratedDevice = false;
    EXPECT_EQ(mockReadVal, pSysmanKmdInterface->getEventType(isIntegratedDevice));
}

TEST_F(SysmanFixtureDeviceXe, GivenSysmanKmdInterfaceInstanceAndIsNotIntegratedDeviceAndReadSymLinkFailsWhenGetEventsIsCalledThenFailureIsReturned) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkFailure);

    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    bool isIntegratedDevice = false;
    EXPECT_EQ(0u, pSysmanKmdInterface->getEventType(isIntegratedDevice));
}

TEST_F(SysmanFixtureDeviceXe, GivenSysmanKmdInterfaceInstanceAndIsNotIntegratedDeviceAndFsReadFailsWhenGetEventsIsCalledThenFailureIsReturned) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, &mockReadFailure);

    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    bool isIntegratedDevice = false;
    EXPECT_EQ(0u, pSysmanKmdInterface->getEventType(isIntegratedDevice));
}

} // namespace ult
} // namespace Sysman
} // namespace L0