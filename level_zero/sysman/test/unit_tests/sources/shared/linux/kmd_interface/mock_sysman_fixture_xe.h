/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/test/common/helpers/variable_backup.h"

#include "level_zero/sysman/source/shared/linux/kmd_interface/sysman_kmd_interface.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"
#include "level_zero/sysman/test/unit_tests/sources/shared/linux/mock_pmu_interface.h"

namespace L0 {
namespace Sysman {
namespace ult {

class SysmanFixtureDeviceXe : public SysmanDeviceFixture {
  protected:
    std::unique_ptr<MockPmuInterfaceImp> pPmuInterface;

    void SetUp() override {
        SysmanDeviceFixture::SetUp();
        pPmuInterface = std::make_unique<MockPmuInterfaceImp>(pLinuxSysmanImp);
        pLinuxSysmanImp->pSysmanKmdInterface.reset(new SysmanKmdInterfaceXe(pLinuxSysmanImp->getSysmanProductHelper()));
        mockInitFsAccess();
        pPmuInterface->pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    }

    void TearDown() override {
        SysmanDeviceFixture::TearDown();
    }

    void mockInitFsAccess() {
        VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, [](const char *path, char *buf, size_t bufsize) -> int {
            constexpr size_t sizeofPath = sizeof("/sys/devices/pci0000:00/0000:00:01.0/0000:01:00.0/0000:02:01.0/0000:03:00.0");
            strcpy_s(buf, sizeofPath, "/sys/devices/pci0000:00/0000:00:01.0/0000:01:00.0/0000:02:01.0/0000:03:00.0");
            return sizeofPath;
        });
        pLinuxSysmanImp->pSysmanKmdInterface->initFsAccessInterface(*pLinuxSysmanImp->getDrm());
    }
};

} // namespace ult
} // namespace Sysman
} // namespace L0