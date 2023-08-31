/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/test/unit_tests/sources/shared/linux/sysman_kmd_interface_tests.h"

#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/string.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/variable_backup.h"

#include "level_zero/sysman/source/shared/linux/sysman_kmd_interface.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"

#include "gtest/gtest.h"

namespace L0 {
namespace Sysman {
namespace ult {

using namespace NEO;

const uint32_t mockReadVal = 23;

class SysmanFixtureDeviceI915 : public SysmanDeviceFixture {
  protected:
    L0::Sysman::SysmanDevice *device = nullptr;
    std::unique_ptr<MockPmuInterfaceImp> pPmuInterface;

    void SetUp() override {
        SysmanDeviceFixture::SetUp();
        device = pSysmanDevice;
        pPmuInterface = std::make_unique<MockPmuInterfaceImp>(pLinuxSysmanImp);
        pPmuInterface->pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
        VariableBackup<L0::Sysman::PmuInterface *> pmuBackup(&pLinuxSysmanImp->pPmuInterface);
        pLinuxSysmanImp->pPmuInterface = pPmuInterface.get();
    }

    void TearDown() override {
        SysmanDeviceFixture::TearDown();
    }
};

TEST_F(SysmanFixtureDeviceI915, GivenSysmanKmdInterfaceInstanceWhenIsGroupEngineInterfaceAvailableCalledThenFalseValueIsReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    EXPECT_FALSE(pSysmanKmdInterface->isGroupEngineInterfaceAvailable());
}

TEST_F(SysmanFixtureDeviceI915, GivenSysmanKmdInterfaceInstanceWhenCheckingAvailabilityOfBaseFrequencyFactorAndSystemPowerBalanceThenFalseValueIsReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    EXPECT_FALSE(pSysmanKmdInterface->isBaseFrequencyFactorAvailable());
    EXPECT_FALSE(pSysmanKmdInterface->isSystemPowerBalanceAvailable());
}

TEST_F(SysmanFixtureDeviceI915, GivenGroupEngineTypeAndSysmanKmdInterfaceInstanceWhenGetEngineActivityFdIsCalledThenInvalidFdIsReturned) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, [](const char *path, char *buf, size_t bufsize) -> int {
        constexpr size_t sizeofPath = sizeof("/sys/devices/pci0000:00/0000:00:01.0/0000:01:00.0/0000:02:01.0/0000:03:00.0");
        strcpy_s(buf, sizeofPath, "/sys/devices/pci0000:00/0000:00:01.0/0000:01:00.0/0000:02:01.0/0000:03:00.0");
        return sizeofPath;
    });

    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::ostringstream oStream;
        oStream << mockReadVal;
        std::string value = oStream.str();
        memcpy(buf, value.data(), count);
        return count;
    });

    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    EXPECT_EQ(-1, pSysmanKmdInterface->getEngineActivityFd(ZES_ENGINE_GROUP_ALL, 0, 0, pPmuInterface.get()));
    EXPECT_EQ(-1, pSysmanKmdInterface->getEngineActivityFd(ZES_ENGINE_GROUP_COMPUTE_ALL, 0, 0, pPmuInterface.get()));
    EXPECT_EQ(-1, pSysmanKmdInterface->getEngineActivityFd(ZES_ENGINE_GROUP_COPY_ALL, 0, 0, pPmuInterface.get()));
    EXPECT_EQ(-1, pSysmanKmdInterface->getEngineActivityFd(ZES_ENGINE_GROUP_RENDER_ALL, 0, 0, pPmuInterface.get()));
    EXPECT_EQ(-1, pSysmanKmdInterface->getEngineActivityFd(ZES_ENGINE_GROUP_MEDIA_ALL, 0, 0, pPmuInterface.get()));
}

TEST_F(SysmanFixtureDeviceI915, GivenSingleEngineTypeAndSysmanKmdInterfaceInstanceWhenGetEngineActivityFdIsCalledThenValidFdIsReturned) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, [](const char *path, char *buf, size_t bufsize) -> int {
        constexpr size_t sizeofPath = sizeof("/sys/devices/pci0000:00/0000:00:01.0/0000:01:00.0/0000:02:01.0/0000:03:00.0");
        strcpy_s(buf, sizeofPath, "/sys/devices/pci0000:00/0000:00:01.0/0000:01:00.0/0000:02:01.0/0000:03:00.0");
        return sizeofPath;
    });

    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::ostringstream oStream;
        oStream << mockReadVal;
        std::string value = oStream.str();
        memcpy(buf, value.data(), count);
        return count;
    });

    pPmuInterface->mockPmuFd = 10;
    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    EXPECT_EQ(pPmuInterface->mockPmuFd, pSysmanKmdInterface->getEngineActivityFd(ZES_ENGINE_GROUP_COMPUTE_SINGLE, 0, 0, pPmuInterface.get()));
}

TEST_F(SysmanFixtureDeviceI915, GivenSysmanKmdInterfaceInstanceAndIsIntegratedDeviceWhenGetEventsIsCalledThenValidEventTypeIsReturned) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::ostringstream oStream;
        oStream << mockReadVal;
        std::string value = oStream.str();
        memcpy(buf, value.data(), count);
        return count;
    });

    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    bool isIntegratedDevice = true;
    EXPECT_EQ(mockReadVal, pSysmanKmdInterface->getEventType(isIntegratedDevice));
}

TEST_F(SysmanFixtureDeviceI915, GivenSysmanKmdInterfaceInstanceAndIsNotIntegratedDeviceWhenGetEventsIsCalledThenValidEventTypeIsReturned) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, [](const char *path, char *buf, size_t bufsize) -> int {
        constexpr size_t sizeofPath = sizeof("/sys/devices/pci0000:00/0000:00:01.0/0000:01:00.0/0000:02:01.0/0000:03:00.0");
        strcpy_s(buf, sizeofPath, "/sys/devices/pci0000:00/0000:00:01.0/0000:01:00.0/0000:02:01.0/0000:03:00.0");
        return sizeofPath;
    });

    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::ostringstream oStream;
        oStream << mockReadVal;
        std::string value = oStream.str();
        memcpy(buf, value.data(), count);
        return count;
    });

    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    bool isIntegratedDevice = false;
    EXPECT_EQ(mockReadVal, pSysmanKmdInterface->getEventType(isIntegratedDevice));
}

TEST_F(SysmanFixtureDeviceI915, GivenSysmanKmdInterfaceInstanceAndIsNotIntegratedDeviceAndReadSymLinkFailsWhenGetEventsIsCalledThenFailureIsReturned) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, [](const char *path, char *buf, size_t bufsize) -> int {
        errno = ENOENT;
        return -1;
    });

    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    bool isIntegratedDevice = false;
    EXPECT_EQ(0u, pSysmanKmdInterface->getEventType(isIntegratedDevice));
}

TEST_F(SysmanFixtureDeviceI915, GivenSysmanKmdInterfaceInstanceAndIsNotIntegratedDeviceAndFsReadFailsWhenGetEventsIsCalledThenFailureIsReturned) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        errno = ENOENT;
        return -1;
    });

    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    bool isIntegratedDevice = false;
    EXPECT_EQ(0u, pSysmanKmdInterface->getEventType(isIntegratedDevice));
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
        pPmuInterface->pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();

        VariableBackup<L0::Sysman::PmuInterface *> pmuBackup(&pLinuxSysmanImp->pPmuInterface);
        pLinuxSysmanImp->pPmuInterface = pPmuInterface.get();
    }

    void TearDown() override {
        SysmanDeviceFixture::TearDown();
    }
};

TEST_F(SysmanFixtureDeviceXe, GivenSysmanKmdInterfaceInstanceWhenIsGroupEngineInterfaceAvailableCalledThenTrueValueIsReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    EXPECT_TRUE(pSysmanKmdInterface->isGroupEngineInterfaceAvailable());
}

TEST_F(SysmanFixtureDeviceXe, GivenSysmanKmdInterfaceInstanceWhenCheckingAvailabilityOfBaseFrequencyFactorAndSystemPowerBalanceThenTrueValueIsReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    EXPECT_TRUE(pSysmanKmdInterface->isBaseFrequencyFactorAvailable());
    EXPECT_TRUE(pSysmanKmdInterface->isSystemPowerBalanceAvailable());
}

TEST_F(SysmanFixtureDeviceXe, GivenGroupEngineTypeAndSysmanKmdInterfaceInstanceWhenGetEngineActivityFdIsCalledThenValidFdIsReturned) {

    auto pSysmanKmdInterface = std::make_unique<MockSysmanKmdInterfaceXe>(pLinuxSysmanImp->getProductFamily());
    pSysmanKmdInterface->pFsAccess = std::make_unique<MockFsAccessInterface>();
    pSysmanKmdInterface->pSysfsAccess = std::make_unique<MockSysFsAccessInterface>();
    pPmuInterface->mockPmuFd = 10;
    pPmuInterface->pSysmanKmdInterface = pSysmanKmdInterface.get();

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, [](const char *path, char *buf, size_t bufsize) -> int {
        constexpr size_t sizeofPath = sizeof("/sys/devices/pci0000:00/0000:00:01.0/0000:01:00.0/0000:02:01.0/0000:03:00.0");
        strcpy_s(buf, sizeofPath, "/sys/devices/pci0000:00/0000:00:01.0/0000:01:00.0/0000:02:01.0/0000:03:00.0");
        return sizeofPath;
    });

    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::ostringstream oStream;
        oStream << mockReadVal;
        std::string value = oStream.str();
        memcpy(buf, value.data(), count);
        return count;
    });

    EXPECT_EQ(pPmuInterface->mockPmuFd, pSysmanKmdInterface->getEngineActivityFd(ZES_ENGINE_GROUP_ALL, 0, 0, pPmuInterface.get()));
    EXPECT_EQ(pPmuInterface->mockPmuFd, pSysmanKmdInterface->getEngineActivityFd(ZES_ENGINE_GROUP_COMPUTE_ALL, 0, 0, pPmuInterface.get()));
    EXPECT_EQ(pPmuInterface->mockPmuFd, pSysmanKmdInterface->getEngineActivityFd(ZES_ENGINE_GROUP_COPY_ALL, 0, 0, pPmuInterface.get()));
    EXPECT_EQ(pPmuInterface->mockPmuFd, pSysmanKmdInterface->getEngineActivityFd(ZES_ENGINE_GROUP_RENDER_ALL, 0, 0, pPmuInterface.get()));
    EXPECT_EQ(pPmuInterface->mockPmuFd, pSysmanKmdInterface->getEngineActivityFd(ZES_ENGINE_GROUP_MEDIA_ALL, 0, 0, pPmuInterface.get()));
}

TEST_F(SysmanFixtureDeviceXe, GivenSingleEngineTypeAndSysmanKmdInterfaceInstanceWhenGetEngineActivityFdIsCalledThenInvalidFdIsReturned) {

    auto pSysmanKmdInterface = std::make_unique<MockSysmanKmdInterfaceXe>(pLinuxSysmanImp->getProductFamily());
    pSysmanKmdInterface->pFsAccess = std::make_unique<MockFsAccessInterface>();
    pSysmanKmdInterface->pSysfsAccess = std::make_unique<MockSysFsAccessInterface>();
    pPmuInterface->pSysmanKmdInterface = pSysmanKmdInterface.get();

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, [](const char *path, char *buf, size_t bufsize) -> int {
        constexpr size_t sizeofPath = sizeof("/sys/devices/pci0000:00/0000:00:01.0/0000:01:00.0/0000:02:01.0/0000:03:00.0");
        strcpy_s(buf, sizeofPath, "/sys/devices/pci0000:00/0000:00:01.0/0000:01:00.0/0000:02:01.0/0000:03:00.0");
        return sizeofPath;
    });

    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::ostringstream oStream;
        oStream << mockReadVal;
        std::string value = oStream.str();
        memcpy(buf, value.data(), count);
        return count;
    });

    EXPECT_EQ(-1, pSysmanKmdInterface->getEngineActivityFd(ZES_ENGINE_GROUP_COMPUTE_SINGLE, 0, 0, pPmuInterface.get()));
}

TEST_F(SysmanFixtureDeviceXe, GivenSysmanKmdInterfaceAndIsIntegratedDeviceInstanceWhenGetEventsIsCalledThenValidEventTypeIsReturned) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::ostringstream oStream;
        oStream << mockReadVal;
        std::string value = oStream.str();
        memcpy(buf, value.data(), count);
        return count;
    });

    auto pSysmanKmdInterface = std::make_unique<MockSysmanKmdInterfaceXe>(pLinuxSysmanImp->getProductFamily());
    bool isIntegratedDevice = true;
    pSysmanKmdInterface->pFsAccess = std::make_unique<MockFsAccessInterface>();
    pSysmanKmdInterface->pSysfsAccess = std::make_unique<MockSysFsAccessInterface>();
    EXPECT_EQ(mockReadVal, pSysmanKmdInterface->getEventType(isIntegratedDevice));
}

TEST_F(SysmanFixtureDeviceXe, GivenSysmanKmdInterfaceAndIsNotIntegratedDeviceInstanceWhenGetEventsIsCalledThenValidEventTypeIsReturned) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, [](const char *path, char *buf, size_t bufsize) -> int {
        constexpr size_t sizeofPath = sizeof("/sys/devices/pci0000:00/0000:00:01.0/0000:01:00.0/0000:02:01.0/0000:03:00.0");
        strcpy_s(buf, sizeofPath, "/sys/devices/pci0000:00/0000:00:01.0/0000:01:00.0/0000:02:01.0/0000:03:00.0");
        return sizeofPath;
    });

    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::ostringstream oStream;
        oStream << mockReadVal;
        std::string value = oStream.str();
        memcpy(buf, value.data(), count);
        return count;
    });

    auto pSysmanKmdInterface = std::make_unique<MockSysmanKmdInterfaceXe>(pLinuxSysmanImp->getProductFamily());
    bool isIntegratedDevice = false;
    pSysmanKmdInterface->pFsAccess = std::make_unique<MockFsAccessInterface>();
    pSysmanKmdInterface->pSysfsAccess = std::make_unique<MockSysFsAccessInterface>();
    EXPECT_EQ(mockReadVal, pSysmanKmdInterface->getEventType(isIntegratedDevice));
}

TEST_F(SysmanFixtureDeviceXe, GivenSysmanKmdInterfaceInstanceAndIsNotIntegratedDeviceAndReadSymLinkFailsWhenGetEventsIsCalledThenFailureIsReturned) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, [](const char *path, char *buf, size_t bufsize) -> int {
        errno = ENOENT;
        return -1;
    });

    auto pSysmanKmdInterface = std::make_unique<MockSysmanKmdInterfaceXe>(pLinuxSysmanImp->getProductFamily());
    bool isIntegratedDevice = false;
    pSysmanKmdInterface->pFsAccess = std::make_unique<MockFsAccessInterface>();
    pSysmanKmdInterface->pSysfsAccess = std::make_unique<MockSysFsAccessInterface>();
    EXPECT_EQ(0u, pSysmanKmdInterface->getEventType(isIntegratedDevice));
}

TEST_F(SysmanFixtureDeviceXe, GivenSysmanKmdInterfaceInstanceAndIsNotIntegratedDeviceAndFsReadFailsWhenGetEventsIsCalledThenFailureIsReturned) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        errno = ENOENT;
        return -1;
    });

    auto pSysmanKmdInterface = std::make_unique<MockSysmanKmdInterfaceXe>(pLinuxSysmanImp->getProductFamily());
    bool isIntegratedDevice = false;
    pSysmanKmdInterface->pFsAccess = std::make_unique<MockFsAccessInterface>();
    pSysmanKmdInterface->pSysfsAccess = std::make_unique<MockSysFsAccessInterface>();
    EXPECT_EQ(0u, pSysmanKmdInterface->getEventType(isIntegratedDevice));
}

} // namespace ult
} // namespace Sysman
} // namespace L0