/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/tools/source/sysman/sysman.h"
#include "level_zero/tools/test/unit_tests/sources/sysman/mocks/mock_sysman_env_vars.h"

#include "gmock/gmock.h"
#include "sysman/windows/os_sysman_imp.h"

extern bool sysmanUltsEnable;

using ::testing::_;
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
    }

    void TearDown() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
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
