/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/os_interface/linux/sys_calls_linux_ult.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/sysman/source/device/sysman_device.h"
#include "level_zero/sysman/source/driver/sysman_driver.h"
#include "level_zero/sysman/source/driver/sysman_driver_handle_imp.h"
#include "level_zero/sysman/source/shared/linux/kmd_interface/sysman_kmd_interface.h"
#include "level_zero/sysman/source/shared/linux/sysman_fs_access_interface.h"
#include "level_zero/sysman/source/shared/linux/zes_os_sysman_driver_imp.h"
#include "level_zero/sysman/source/shared/linux/zes_os_sysman_imp.h"
#include "level_zero/sysman/test/unit_tests/sources/firmware_util/mock_fw_util_fixture.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_drm.h"

#include "gtest/gtest.h"

using namespace NEO;

namespace L0 {
namespace Sysman {
namespace ult {

class PublicLinuxSysmanImp : public L0::Sysman::LinuxSysmanImp {
  public:
    using LinuxSysmanImp::pFsAccess;
    using LinuxSysmanImp::pFwUtilInterface;
    using LinuxSysmanImp::pPmuInterface;
    using LinuxSysmanImp::pProcfsAccess;
    using LinuxSysmanImp::pSysfsAccess;
    using LinuxSysmanImp::pSysmanKmdInterface;
    using LinuxSysmanImp::pSysmanProductHelper;
    using LinuxSysmanImp::rootPath;
};

class SysmanDeviceFixture : public ::testing::Test {
  public:
    void SetUp() override {
        debugManager.flags.IgnoreProductSpecificIoctlHelper.set(true);
        VariableBackup<decltype(NEO::SysCalls::sysCallsRealpath)> mockRealPath(&NEO::SysCalls::sysCallsRealpath, [](const char *path, char *buf) -> char * {
            constexpr size_t sizeofPath = sizeof("/sys/devices/pci0000:00/0000:00:02.0");
            strcpy_s(buf, sizeofPath, "/sys/devices/pci0000:00/0000:00:02.0");
            return buf;
        });

        VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, [](const char *path, char *buf, size_t bufsize) -> int {
            std::string str = "../../devices/pci0000:37/0000:37:01.0/0000:38:00.0/0000:39:01.0/0000:3a:00.0/drm/renderD128";
            std::memcpy(buf, str.c_str(), str.size());
            return static_cast<int>(str.size());
        });
        execEnv = new NEO::ExecutionEnvironment();
        execEnv->prepareRootDeviceEnvironments(numRootDevices);
        for (auto i = 0u; i < execEnv->rootDeviceEnvironments.size(); i++) {
            execEnv->rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(NEO::defaultHwInfo.get());
            execEnv->rootDeviceEnvironments[i]->osInterface = std::make_unique<NEO::OSInterface>();
            execEnv->rootDeviceEnvironments[i]->osInterface->setDriverModel(std::make_unique<SysmanMockDrm>(*execEnv->rootDeviceEnvironments[i]));
        }

        driverHandle = std::make_unique<L0::Sysman::SysmanDriverHandleImp>();
        driverHandle->initialize(*execEnv);
        pSysmanDevice = driverHandle->sysmanDevices[0];
        L0::Sysman::globalSysmanDriver = driverHandle.get();

        L0::Sysman::sysmanOnlyInit = true;

        pSysmanDeviceImp = static_cast<L0::Sysman::SysmanDeviceImp *>(pSysmanDevice);
        pOsSysman = pSysmanDeviceImp->pOsSysman;
        pLinuxSysmanImp = static_cast<PublicLinuxSysmanImp *>(pOsSysman);
        pLinuxSysmanImp->pFwUtilInterface = new MockFwUtilInterface();
    }
    void TearDown() override {
        L0::Sysman::globalSysmanDriver = nullptr;
        L0::Sysman::sysmanOnlyInit = false;
    }

    L0::Sysman::SysmanDevice *pSysmanDevice = nullptr;
    L0::Sysman::SysmanDeviceImp *pSysmanDeviceImp = nullptr;
    L0::Sysman::OsSysman *pOsSysman = nullptr;
    PublicLinuxSysmanImp *pLinuxSysmanImp = nullptr;
    NEO::ExecutionEnvironment *execEnv = nullptr;
    std::unique_ptr<L0::Sysman::SysmanDriverHandleImp> driverHandle;
    const uint32_t numRootDevices = 1u;
    DebugManagerStateRestore restore;
};

class SysmanMultiDeviceFixture : public ::testing::Test {
  public:
    void SetUp() override {
        VariableBackup<decltype(NEO::SysCalls::sysCallsRealpath)> mockRealPath(&NEO::SysCalls::sysCallsRealpath, [](const char *path, char *buf) -> char * {
            constexpr size_t sizeofPath = sizeof("/sys/devices/pci0000:00/0000:00:02.0");
            strcpy_s(buf, sizeofPath, "/sys/devices/pci0000:00/0000:00:02.0");
            return buf;
        });

        VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, [](const char *path, char *buf, size_t bufsize) -> int {
            std::string str = "../../devices/pci0000:37/0000:37:01.0/0000:38:00.0/0000:39:01.0/0000:3a:00.0/drm/renderD128";
            std::memcpy(buf, str.c_str(), str.size());
            return static_cast<int>(str.size());
        });

        execEnv = new NEO::ExecutionEnvironment();
        execEnv->prepareRootDeviceEnvironments(numRootDevices);
        debugManager.flags.CreateMultipleSubDevices.set(numSubDevices);
        for (auto i = 0u; i < execEnv->rootDeviceEnvironments.size(); i++) {
            execEnv->rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(NEO::defaultHwInfo.get());
            execEnv->rootDeviceEnvironments[i]->osInterface = std::make_unique<NEO::OSInterface>();
            execEnv->rootDeviceEnvironments[i]->osInterface->setDriverModel(std::make_unique<SysmanMockDrm>(*execEnv->rootDeviceEnvironments[i]));
        }

        driverHandle = std::make_unique<L0::Sysman::SysmanDriverHandleImp>();
        driverHandle->initialize(*execEnv);
        pSysmanDevice = driverHandle->sysmanDevices[0];
        L0::Sysman::globalSysmanDriver = driverHandle.get();

        L0::Sysman::sysmanOnlyInit = true;

        pSysmanDeviceImp = static_cast<L0::Sysman::SysmanDeviceImp *>(pSysmanDevice);
        pOsSysman = pSysmanDeviceImp->pOsSysman;
        pLinuxSysmanImp = static_cast<PublicLinuxSysmanImp *>(pOsSysman);
        pLinuxSysmanImp->pFwUtilInterface = new MockFwUtilInterface();
    }
    void TearDown() override {
        L0::Sysman::globalSysmanDriver = nullptr;
        L0::Sysman::sysmanOnlyInit = false;
    }

    L0::Sysman::SysmanDevice *pSysmanDevice = nullptr;
    L0::Sysman::SysmanDeviceImp *pSysmanDeviceImp = nullptr;
    L0::Sysman::OsSysman *pOsSysman = nullptr;
    PublicLinuxSysmanImp *pLinuxSysmanImp = nullptr;
    NEO::ExecutionEnvironment *execEnv = nullptr;
    std::unique_ptr<L0::Sysman::SysmanDriverHandleImp> driverHandle;
    const uint32_t numRootDevices = 4u;
    const uint32_t numSubDevices = 2u;
    DebugManagerStateRestore restorer;
};

class PublicFsAccess : public L0::Sysman::FsAccessInterface {};

class PublicSysfsAccess : public L0::Sysman::SysFsAccessInterface {};

class PublicLinuxSysmanDriverImp : public L0::Sysman::LinuxSysmanDriverImp {
  public:
    PublicLinuxSysmanDriverImp() : LinuxSysmanDriverImp() {}
    using LinuxSysmanDriverImp::pLinuxEventsUtil;
    using LinuxSysmanDriverImp::pUdevLib;
};

} // namespace ult
} // namespace Sysman
} // namespace L0
