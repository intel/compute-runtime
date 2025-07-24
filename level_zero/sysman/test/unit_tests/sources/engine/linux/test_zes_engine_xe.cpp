/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/xe/ioctl_helper_xe.h"
#include "shared/test/common/helpers/variable_backup.h"

#include "level_zero/sysman/source/api/engine/linux/sysman_os_engine_imp.h"
#include "level_zero/sysman/test/unit_tests/sources/engine/linux/mock_engine_xe.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"
#include "level_zero/sysman/test/unit_tests/sources/shared/linux/kmd_interface/mock_sysman_kmd_interface_xe.h"
#include "level_zero/sysman/test/unit_tests/sources/shared/linux/mock_pmu_interface.h"

namespace L0 {
namespace Sysman {
namespace ult {

static MapOfEngineInfo mockMapEngineInfo = {
    {ZES_ENGINE_GROUP_ALL, {{0, 0}}},
    {ZES_ENGINE_GROUP_COMPUTE_ALL, {{0, 0}}},
    {ZES_ENGINE_GROUP_MEDIA_ALL, {{0, 0}}},
    {ZES_ENGINE_GROUP_COPY_ALL, {{0, 0}}},
    {ZES_ENGINE_GROUP_RENDER_ALL, {{0, 0}}},
    {ZES_ENGINE_GROUP_RENDER_SINGLE, {{1, 0}}},
    {ZES_ENGINE_GROUP_COPY_SINGLE, {{0, 0}}},
    {ZES_ENGINE_GROUP_COMPUTE_SINGLE, {{1, 0}}}};

class ZesEngineFixtureXe : public SysmanDeviceFixture {
  protected:
    L0::Sysman::SysmanDevice *device = nullptr;
    std::unique_ptr<MockPmuInterfaceImp> pPmuInterface;
    L0::Sysman::PmuInterface *pOriginalPmuInterface = nullptr;
    MockSysmanKmdInterfaceXe *pSysmanKmdInterface = nullptr;

    void SetUp() override {
        SysmanDeviceFixture::SetUp();
        VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, [](const char *path, char *buf, size_t bufsize) -> int {
            std::string str = "../../devices/pci0000:37/0000:37:01.0/0000:38:00.0/0000:39:01.0/0000:3a:00.0/drm/renderD128";
            std::memcpy(buf, str.c_str(), str.size());
            return static_cast<int>(str.size());
        });
        device = pSysmanDevice;
        MockNeoDrm *pDrm = new MockNeoDrm(const_cast<NEO::RootDeviceEnvironment &>(pSysmanDeviceImp->getRootDeviceEnvironment()));
        pDrm->ioctlHelper = std::make_unique<NEO::IoctlHelperXe>(*pDrm);
        auto &osInterface = pSysmanDeviceImp->getRootDeviceEnvironment().osInterface;
        osInterface->setDriverModel(std::unique_ptr<MockNeoDrm>(pDrm));

        pSysmanKmdInterface = new MockSysmanKmdInterfaceXe(pLinuxSysmanImp->getSysmanProductHelper());
        pLinuxSysmanImp->pSysmanKmdInterface.reset(pSysmanKmdInterface);
        pLinuxSysmanImp->pSysmanKmdInterface->initFsAccessInterface(*pDrm);

        pPmuInterface = std::make_unique<MockPmuInterfaceImp>(pLinuxSysmanImp);
        pPmuInterface->pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
        pOriginalPmuInterface = pLinuxSysmanImp->pPmuInterface;
        pLinuxSysmanImp->pPmuInterface = pPmuInterface.get();

        pSysmanDeviceImp->pEngineHandleContext->handleList.clear();
        bool isIntegratedDevice = true;
        pLinuxSysmanImp->pSysmanKmdInterface->setSysmanDeviceDirName(isIntegratedDevice);
    }

    void TearDown() override {
        pLinuxSysmanImp->pPmuInterface = pOriginalPmuInterface;
        SysmanDeviceFixture::TearDown();
    }
};

TEST_F(ZesEngineFixtureXe, GivenEngineImpInstanceForSingleComputeEngineAndGetPmuConfigsForSingleEnginesFailsWhenCallingIsEngineModuleSupportedThenFalseValueIsReturned) {

    pPmuInterface->mockEventConfigReturnValue.push_back(-1);
    auto pLinuxEngineImp = std::make_unique<L0::Sysman::LinuxEngineImp>(pOsSysman, mockMapEngineInfo, ZES_ENGINE_GROUP_COMPUTE_SINGLE, 0, 0, 0);
    EXPECT_FALSE(pLinuxEngineImp->isEngineModuleSupported());
}

TEST_F(ZesEngineFixtureXe, GivenEngineImpInstanceForMediaGroupEngineAndSingleMediaEngineNotAvailableWhenCallingIsEngineModuleSupportedThenFalseValueIsReturned) {
    auto pLinuxEngineImp = std::make_unique<L0::Sysman::LinuxEngineImp>(pOsSysman, mockMapEngineInfo, ZES_ENGINE_GROUP_MEDIA_ALL, 0, 0, 0);
    EXPECT_FALSE(pLinuxEngineImp->isEngineModuleSupported());
}

} // namespace ult
} // namespace Sysman
} // namespace L0