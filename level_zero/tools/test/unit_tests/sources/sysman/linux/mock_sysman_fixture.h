/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/mocks/mock_device.h"

#include "level_zero/core/source/driver/driver.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/tools/source/sysman/linux/os_sysman_driver_imp.h"
#include "level_zero/tools/source/sysman/linux/os_sysman_imp.h"
#include "level_zero/tools/source/sysman/sysman.h"
#include "level_zero/tools/test/unit_tests/sources/sysman/firmware_util/mock_fw_util_fixture.h"
#include "level_zero/tools/test/unit_tests/sources/sysman/linux/mock_procfs_access_fixture.h"
#include "level_zero/tools/test/unit_tests/sources/sysman/linux/mock_sysfs_access_fixture.h"

extern bool sysmanUltsEnable;

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
    using LinuxSysmanImp::memType;
    using LinuxSysmanImp::pDrm;
    using LinuxSysmanImp::pFsAccess;
    using LinuxSysmanImp::pFwUtilInterface;
    using LinuxSysmanImp::pPmuInterface;
    using LinuxSysmanImp::pProcfsAccess;
    using LinuxSysmanImp::pSysfsAccess;
};

class SysmanDeviceFixture : public DeviceFixture, public ::testing::Test {
  public:
    MockLinuxSysfsAccess *pSysfsAccess = nullptr;
    MockLinuxProcfsAccess *pProcfsAccess = nullptr;
    MockFwUtilInterface *pFwUtilInterface = nullptr;
    void SetUp() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        debugManager.flags.EnableHostUsmAllocationPool.set(0);
        debugManager.flags.EnableDeviceUsmAllocationPool.set(0);
        DeviceFixture::setUp();
        neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[device->getRootDeviceIndex()]->osInterface = std::make_unique<NEO::OSInterface>();
        auto &osInterface = *device->getOsInterface();
        osInterface.setDriverModel(std::make_unique<SysmanMockDrm>(const_cast<NEO::RootDeviceEnvironment &>(neoDevice->getRootDeviceEnvironment())));
        setenv("ZES_ENABLE_SYSMAN", "1", 1);
        device->setSysmanHandle(new SysmanDeviceImp(device->toHandle()));
        pSysmanDevice = device->getSysmanHandle();
        pSysmanDeviceImp = static_cast<SysmanDeviceImp *>(pSysmanDevice);
        pOsSysman = pSysmanDeviceImp->pOsSysman;
        pLinuxSysmanImp = static_cast<PublicLinuxSysmanImp *>(pOsSysman);

        pFwUtilInterface = new MockFwUtilInterface();
        pSysfsAccess = new MockLinuxSysfsAccess;
        pProcfsAccess = new MockLinuxProcfsAccess;
        pLinuxSysmanImp->pFwUtilInterface = pFwUtilInterface;
        pLinuxSysmanImp->pSysfsAccess = pSysfsAccess;
        pLinuxSysmanImp->pProcfsAccess = pProcfsAccess;

        if (globalOsSysmanDriver == nullptr) {
            globalOsSysmanDriver = L0::OsSysmanDriver::create();
        }

        pSysmanDeviceImp->init();
        L0::sysmanInitFromCore = true;
    }
    void TearDown() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }

        if (globalOsSysmanDriver != nullptr) {
            delete globalOsSysmanDriver;
            globalOsSysmanDriver = nullptr;
        }

        DeviceFixture::tearDown();
        unsetenv("ZES_ENABLE_SYSMAN");
    }
    DebugManagerStateRestore restorer;
    SysmanDevice *pSysmanDevice = nullptr;
    SysmanDeviceImp *pSysmanDeviceImp = nullptr;
    OsSysman *pOsSysman = nullptr;
    PublicLinuxSysmanImp *pLinuxSysmanImp = nullptr;
};

class SysmanMultiDeviceFixture : public MultiDeviceFixture, public ::testing::Test {
  public:
    MockLinuxSysfsAccess *pSysfsAccess = nullptr;
    MockLinuxProcfsAccess *pProcfsAccess = nullptr;
    MockFwUtilInterface *pFwUtilInterface = nullptr;
    void SetUp() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        MultiDeviceFixture::setUp();
        device = driverHandle->devices[0];
        neoDevice = device->getNEODevice();
        neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[device->getRootDeviceIndex()]->osInterface = std::make_unique<NEO::OSInterface>();
        auto &osInterface = *device->getOsInterface();
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
        pSysfsAccess = new MockLinuxSysfsAccess;
        pProcfsAccess = new MockLinuxProcfsAccess;
        pLinuxSysmanImp->pFwUtilInterface = pFwUtilInterface;
        pLinuxSysmanImp->pSysfsAccess = pSysfsAccess;
        pLinuxSysmanImp->pProcfsAccess = pProcfsAccess;

        if (globalOsSysmanDriver == nullptr) {
            globalOsSysmanDriver = L0::OsSysmanDriver::create();
        }

        pSysmanDeviceImp->init();
        subDeviceCount = numSubDevices;
        L0::sysmanInitFromCore = true;
    }
    void TearDown() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        if (globalOsSysmanDriver != nullptr) {
            delete globalOsSysmanDriver;
            globalOsSysmanDriver = nullptr;
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
    using SysfsAccess::statSyscall;
};

class UdevLibMock : public UdevLib {
  public:
    UdevLibMock() = default;

    ADDMETHOD_NOBASE(registerEventsFromSubsystemAndGetFd, int, mockFd, (std::vector<std::string> & subsystemList));
    ADDMETHOD_NOBASE(getEventGenerationSourceDevice, dev_t, 0, (void *dev));
    ADDMETHOD_NOBASE(getEventType, const char *, "change", (void *dev));
    ADDMETHOD_NOBASE(getEventPropertyValue, const char *, "MOCK", (void *dev, const char *key));
    ADDMETHOD_NOBASE(allocateDeviceToReceiveData, void *, (void *)(0x12345678), ());
    ADDMETHOD_NOBASE_VOIDRETURN(dropDeviceReference, (void *dev));
};

class PublicLinuxSysmanDriverImp : public L0::LinuxSysmanDriverImp {
  public:
    PublicLinuxSysmanDriverImp() : LinuxSysmanDriverImp() {}
    using LinuxSysmanDriverImp::pLinuxEventsUtil;
    using LinuxSysmanDriverImp::pUdevLib;
};

} // namespace ult
} // namespace L0
