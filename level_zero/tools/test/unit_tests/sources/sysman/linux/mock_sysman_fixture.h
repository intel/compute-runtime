/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/os_interface.h"

#include "test.h"

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/tools/source/sysman/sysman.h"

#include "sysman/linux/os_sysman_imp.h"

using ::testing::_;
using ::testing::NiceMock;
using namespace NEO;

namespace L0 {
namespace ult {
constexpr int mockFd = 0;
class SysmanMockDrm : public Drm {
  public:
    SysmanMockDrm(RootDeviceEnvironment &rootDeviceEnvironment) : Drm(std::make_unique<HwDeviceId>(mockFd, ""), rootDeviceEnvironment) {}
};

class PublicLinuxSysmanImp : public L0::LinuxSysmanImp {
  public:
    using LinuxSysmanImp::pDrm;
    using LinuxSysmanImp::pFsAccess;
    using LinuxSysmanImp::pPmt;
    using LinuxSysmanImp::pPmuInterface;
    using LinuxSysmanImp::pProcfsAccess;
    using LinuxSysmanImp::pSysfsAccess;
};

class SysmanDeviceFixture : public DeviceFixture, public ::testing::Test {
  public:
    void SetUp() override {
        DeviceFixture::SetUp();
        neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[device->getRootDeviceIndex()]->osInterface = std::make_unique<NEO::OSInterface>();
        auto osInterface = device->getOsInterface().get();
        osInterface->setDrm(new SysmanMockDrm(const_cast<NEO::RootDeviceEnvironment &>(neoDevice->getRootDeviceEnvironment())));
        setenv("ZES_ENABLE_SYSMAN", "1", 1);
        device->setSysmanHandle(L0::SysmanDeviceHandleContext::init(device->toHandle()));
        pSysmanDevice = device->getSysmanHandle();
        pSysmanDeviceImp = static_cast<SysmanDeviceImp *>(pSysmanDevice);
        pOsSysman = pSysmanDeviceImp->pOsSysman;
        pLinuxSysmanImp = static_cast<PublicLinuxSysmanImp *>(pOsSysman);
    }
    void TearDown() override {
        DeviceFixture::TearDown();
        unsetenv("ZES_ENABLE_SYSMAN");
    }

    SysmanDevice *pSysmanDevice = nullptr;
    SysmanDeviceImp *pSysmanDeviceImp = nullptr;
    OsSysman *pOsSysman = nullptr;
    PublicLinuxSysmanImp *pLinuxSysmanImp = nullptr;
};

} // namespace ult
} // namespace L0