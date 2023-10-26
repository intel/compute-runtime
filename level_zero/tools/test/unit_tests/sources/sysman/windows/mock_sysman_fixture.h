/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/source/os_interface/os_interface.h"

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/tools/source/sysman/sysman.h"
#include "level_zero/tools/source/sysman/sysman_imp.h"
#include "level_zero/tools/source/sysman/windows/os_sysman_driver_imp.h"
#include "level_zero/tools/source/sysman/windows/os_sysman_imp.h"
#include "level_zero/tools/test/unit_tests/sources/sysman/mocks/mock_sysman_env_vars.h"

extern bool sysmanUltsEnable;

using namespace NEO;

namespace L0 {
namespace ult {

class PublicWddmSysmanImp : public L0::WddmSysmanImp {
  public:
    using WddmSysmanImp::pFwUtilInterface;
    using WddmSysmanImp::pKmdSysManager;
};

class SysmanDeviceFixture : public DeviceFixture, public SysmanEnabledFixture {
  public:
    void SetUp() override {
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

        if (GlobalOsSysmanDriver == nullptr) {
            GlobalOsSysmanDriver = L0::OsSysmanDriver::create();
        }
    }

    void TearDown() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }

        if (GlobalOsSysmanDriver != nullptr) {
            delete GlobalOsSysmanDriver;
            GlobalOsSysmanDriver = nullptr;
        }

        SysmanEnabledFixture::TearDown();
        DeviceFixture::tearDown();
    }

    SysmanDevice *pSysmanDevice = nullptr;
    SysmanDeviceImp *pSysmanDeviceImp = nullptr;
    OsSysman *pOsSysman = nullptr;
    PublicWddmSysmanImp *pWddmSysmanImp = nullptr;
};

} // namespace ult
} // namespace L0
