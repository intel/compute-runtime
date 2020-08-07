/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/source/os_interface/windows/os_interface.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"

#include "test.h"

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/tools/source/sysman/sysman.h"

#include "sysman/windows/os_sysman_imp.h"

using ::testing::_;
using namespace NEO;

namespace L0 {
namespace ult {

class PublicWddmSysmanImp : public L0::WddmSysmanImp {
  public:
    using WddmSysmanImp::pKmdSysManager;
};

class SysmanDeviceFixture : public DeviceFixture, public ::testing::Test {
  public:
    void SetUp() override {
        DeviceFixture::SetUp();
        neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[device->getRootDeviceIndex()]->osInterface = std::make_unique<NEO::OSInterface>();

        _putenv_s("ZES_ENABLE_SYSMAN", "1");
        device->setSysmanHandle(L0::SysmanDeviceHandleContext::init(device->toHandle()));
        pSysmanDevice = device->getSysmanHandle();
        pSysmanDeviceImp = static_cast<SysmanDeviceImp *>(pSysmanDevice);
        pOsSysman = pSysmanDeviceImp->pOsSysman;
        pWddmSysmanImp = static_cast<PublicWddmSysmanImp *>(pOsSysman);
    }

    void TearDown() override {
        DeviceFixture::TearDown();
        _putenv_s("ZES_ENABLE_SYSMAN", "0");
    }

    SysmanDevice *pSysmanDevice = nullptr;
    SysmanDeviceImp *pSysmanDeviceImp = nullptr;
    OsSysman *pOsSysman = nullptr;
    PublicWddmSysmanImp *pWddmSysmanImp = nullptr;
};

} // namespace ult
} // namespace L0