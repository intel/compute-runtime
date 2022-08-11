/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/os_interface.h"

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/tools/source/sysman/sysman.h"
#include "level_zero/tools/test/unit_tests/sources/sysman/firmware_util/mock_fw_util_fixture.h"
#include "level_zero/tools/test/unit_tests/sources/sysman/linux/mock_procfs_access_fixture.h"
#include "level_zero/tools/test/unit_tests/sources/sysman/linux/mock_sysfs_access_fixture.h"

#include "sysman/linux/os_sysman_imp.h"

extern bool sysmanUltsEnable;

using ::testing::_;
using ::testing::Matcher;
using ::testing::NiceMock;
using namespace NEO;

namespace L0 {
namespace ult {
constexpr int mockFd = 0;
class SysmanMockDrm : public Drm {
  public:
    SysmanMockDrm(RootDeviceEnvironment &rootDeviceEnvironment) : Drm(std::make_unique<HwDeviceIdDrm>(mockFd, ""), rootDeviceEnvironment) {
        setupIoctlHelper(rootDeviceEnvironment.getHardwareInfo()->platform.eProductFamily);
    }
};

class PublicLinuxSysmanImp : public L0::LinuxSysmanImp {
  public:
    using LinuxSysmanImp::mapOfSubDeviceIdToPmtObject;
    using LinuxSysmanImp::pDrm;
    using LinuxSysmanImp::pFsAccess;
    using LinuxSysmanImp::pFwUtilInterface;
    using LinuxSysmanImp::pPmuInterface;
    using LinuxSysmanImp::pProcfsAccess;
    using LinuxSysmanImp::pSysfsAccess;
};

class SysmanDeviceFixture : public DeviceFixture, public ::testing::Test {
  public:
    Mock<LinuxSysfsAccess> *pSysfsAccess = nullptr;
    Mock<LinuxProcfsAccess> *pProcfsAccess = nullptr;
    MockFwUtilInterface *pFwUtilInterface = nullptr;
    void SetUp() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        DeviceFixture::setUp();
        neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[device->getRootDeviceIndex()]->osInterface = std::make_unique<NEO::OSInterface>();
        auto &osInterface = device->getOsInterface();
        osInterface.setDriverModel(std::make_unique<SysmanMockDrm>(const_cast<NEO::RootDeviceEnvironment &>(neoDevice->getRootDeviceEnvironment())));
        setenv("ZES_ENABLE_SYSMAN", "1", 1);
        device->setSysmanHandle(new SysmanDeviceImp(device->toHandle()));
        pSysmanDevice = device->getSysmanHandle();
        pSysmanDeviceImp = static_cast<SysmanDeviceImp *>(pSysmanDevice);
        pOsSysman = pSysmanDeviceImp->pOsSysman;
        pLinuxSysmanImp = static_cast<PublicLinuxSysmanImp *>(pOsSysman);

        pFwUtilInterface = new MockFwUtilInterface();
        pSysfsAccess = new NiceMock<Mock<LinuxSysfsAccess>>;
        pProcfsAccess = new NiceMock<Mock<LinuxProcfsAccess>>;
        pLinuxSysmanImp->pFwUtilInterface = pFwUtilInterface;
        pLinuxSysmanImp->pSysfsAccess = pSysfsAccess;
        pLinuxSysmanImp->pProcfsAccess = pProcfsAccess;

        ON_CALL(*pSysfsAccess, getRealPath(_, Matcher<std::string &>(_)))
            .WillByDefault(::testing::Invoke(pSysfsAccess, &Mock<LinuxSysfsAccess>::getRealPathVal));
        ON_CALL(*pProcfsAccess, getFileName(_, _, Matcher<std::string &>(_)))
            .WillByDefault(::testing::Invoke(pProcfsAccess, &Mock<LinuxProcfsAccess>::getMockFileName));

        pSysmanDeviceImp->init();
    }
    void TearDown() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }

        DeviceFixture::tearDown();
        unsetenv("ZES_ENABLE_SYSMAN");
    }

    SysmanDevice *pSysmanDevice = nullptr;
    SysmanDeviceImp *pSysmanDeviceImp = nullptr;
    OsSysman *pOsSysman = nullptr;
    PublicLinuxSysmanImp *pLinuxSysmanImp = nullptr;
};

class SysmanMultiDeviceFixture : public MultiDeviceFixture, public ::testing::Test {
  public:
    Mock<LinuxSysfsAccess> *pSysfsAccess = nullptr;
    Mock<LinuxProcfsAccess> *pProcfsAccess = nullptr;
    MockFwUtilInterface *pFwUtilInterface = nullptr;
    void SetUp() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        MultiDeviceFixture::setUp();
        device = driverHandle->devices[0];
        neoDevice = device->getNEODevice();
        neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[device->getRootDeviceIndex()]->osInterface = std::make_unique<NEO::OSInterface>();
        auto &osInterface = device->getOsInterface();
        osInterface.setDriverModel(std::make_unique<SysmanMockDrm>(const_cast<NEO::RootDeviceEnvironment &>(neoDevice->getRootDeviceEnvironment())));
        setenv("ZES_ENABLE_SYSMAN", "1", 1);
        device->setSysmanHandle(new SysmanDeviceImp(device->toHandle()));
        pSysmanDevice = device->getSysmanHandle();
        for (auto &subDevice : static_cast<DeviceImp *>(device)->subDevices) {
            static_cast<DeviceImp *>(subDevice)->setSysmanHandle(pSysmanDevice);
        }

        pSysmanDeviceImp = static_cast<SysmanDeviceImp *>(pSysmanDevice);
        pOsSysman = pSysmanDeviceImp->pOsSysman;
        pLinuxSysmanImp = static_cast<PublicLinuxSysmanImp *>(pOsSysman);

        pFwUtilInterface = new MockFwUtilInterface();
        pSysfsAccess = new NiceMock<Mock<LinuxSysfsAccess>>;
        pProcfsAccess = new NiceMock<Mock<LinuxProcfsAccess>>;
        pLinuxSysmanImp->pFwUtilInterface = pFwUtilInterface;
        pLinuxSysmanImp->pSysfsAccess = pSysfsAccess;
        pLinuxSysmanImp->pProcfsAccess = pProcfsAccess;

        ON_CALL(*pSysfsAccess, getRealPath(_, Matcher<std::string &>(_)))
            .WillByDefault(::testing::Invoke(pSysfsAccess, &Mock<LinuxSysfsAccess>::getRealPathVal));
        ON_CALL(*pProcfsAccess, getFileName(_, _, Matcher<std::string &>(_)))
            .WillByDefault(::testing::Invoke(pProcfsAccess, &Mock<LinuxProcfsAccess>::getMockFileName));

        pSysmanDeviceImp->init();
        subDeviceCount = numSubDevices;
    }
    void TearDown() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        unsetenv("ZES_ENABLE_SYSMAN");
        MultiDeviceFixture::tearDown();
    }

    SysmanDevice *pSysmanDevice = nullptr;
    SysmanDeviceImp *pSysmanDeviceImp = nullptr;
    OsSysman *pOsSysman = nullptr;
    PublicLinuxSysmanImp *pLinuxSysmanImp = nullptr;
    NEO::Device *neoDevice = nullptr;
    L0::Device *device = nullptr;
    uint32_t subDeviceCount = 0u;
};

class PublicFsAccess : public L0::FsAccess {
  public:
    using FsAccess::accessSyscall;
    using FsAccess::statSyscall;
};

class PublicSysfsAccess : public L0::SysfsAccess {
  public:
    using SysfsAccess::accessSyscall;
};

} // namespace ult
} // namespace L0
