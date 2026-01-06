/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/test/unit_tests/sources/sysman/windows/mock_sysman_fixture.h"

#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/mocks/mock_device.h"

#include "level_zero/tools/source/sysman/os_sysman_driver.h"
#include "level_zero/tools/source/sysman/sysman_imp.h"

namespace L0 {
namespace ult {

void SysmanDeviceFixture::SetUp() {
    if (!sysmanUltsEnable) {
        GTEST_SKIP();
    }
    DeviceFixture::setUp();
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[device->getRootDeviceIndex()]->osInterface = std::make_unique<NEO::OSInterface>();

    SysmanEnabledFixture::SetUp();
    device->setSysmanHandle(L0::SysmanDeviceHandleContext::init(device->toHandle()));
    pSysmanDevice = device->getSysmanHandle();
    pSysmanDeviceImp = static_cast<SysmanDeviceImp *>(pSysmanDevice);
    pOsSysman = pSysmanDeviceImp->pOsSysman;
    pWddmSysmanImp = static_cast<PublicWddmSysmanImp *>(pOsSysman);

    if (globalOsSysmanDriver == nullptr) {
        globalOsSysmanDriver = L0::OsSysmanDriver::create();
    }
}

void SysmanDeviceFixture::TearDown() {
    if (!sysmanUltsEnable) {
        GTEST_SKIP();
    }

    if (globalOsSysmanDriver != nullptr) {
        delete globalOsSysmanDriver;
        globalOsSysmanDriver = nullptr;
    }

    SysmanEnabledFixture::TearDown();
    DeviceFixture::tearDown();
}

} // namespace ult
} // namespace L0
