/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_driver_model.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/driver/driver.h"
#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"

namespace L0 {
namespace Sysman {
namespace ult {

using HasZesInitSupport = IsAnyProducts<IGFX_LUNARLAKE,
                                        IGFX_BMG>;

using SysmanProductHelperSysmanInitTest = SysmanDeviceFixture;
HWTEST2_F(SysmanProductHelperSysmanInitTest, GivenValidProductHelperHandleWhenQueryingForZesInitSupportThenTrueIsReturned, IsLNL) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    EXPECT_TRUE(pSysmanProductHelper->isZesInitSupported());
}

HWTEST2_F(SysmanProductHelperSysmanInitTest, GivenValidProductHelperHandleWhenQueryingForZesInitSupportThenTrueIsReturned, IsBMG) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    EXPECT_TRUE(pSysmanProductHelper->isZesInitSupported());
}

HWTEST2_F(SysmanProductHelperSysmanInitTest, GivenValidProductHelperHandleWhenQueryingForZesInitSupportThenFalseIsReturned, IsPVC) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    EXPECT_FALSE(pSysmanProductHelper->isZesInitSupported());
}

class SysmanNewPlatformInitTest : public ::testing::Test {
  public:
    void SetUp() override{};
    void TearDown() override{};
};

HWTEST2_F(SysmanNewPlatformInitTest, GivenLegacySysmanVariableSetWhenSysmanInitOnLinuxIsCalledThenUnSupportedErrorIsReturned, IsPVC) {
    L0::sysmanInitFromCore = true;
    auto execEnv = new NEO::ExecutionEnvironment();
    execEnv->prepareRootDeviceEnvironments(1);
    execEnv->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(NEO::defaultHwInfo.get());
    execEnv->rootDeviceEnvironments[0]->osInterface = std::make_unique<NEO::OSInterface>();
    execEnv->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelDRM>());
    auto pSysmanDeviceImp = std::make_unique<L0::Sysman::SysmanDeviceImp>(execEnv, 0);
    auto pLinuxSysmanImp = static_cast<PublicLinuxSysmanImp *>(pSysmanDeviceImp->pOsSysman);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pLinuxSysmanImp->init());
    L0::sysmanInitFromCore = false;
}

HWTEST2_F(SysmanNewPlatformInitTest, GivenLegacySysmanVariableSetWhenSysmanInitOnLinuxIsCalledThenSuccessIsReturned, IsBMG) {
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

    L0::sysmanInitFromCore = true;
    auto execEnv = new NEO::ExecutionEnvironment();
    execEnv->prepareRootDeviceEnvironments(1);
    execEnv->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(NEO::defaultHwInfo.get());
    execEnv->rootDeviceEnvironments[0]->osInterface = std::make_unique<NEO::OSInterface>();
    execEnv->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<SysmanMockDrm>(*execEnv->rootDeviceEnvironments[0]));
    auto pSysmanDeviceImp = std::make_unique<L0::Sysman::SysmanDeviceImp>(execEnv, 0);
    auto pLinuxSysmanImp = static_cast<PublicLinuxSysmanImp *>(pSysmanDeviceImp->pOsSysman);
    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxSysmanImp->init());
    EXPECT_FALSE(L0::sysmanInitFromCore);
}

} // namespace ult
} // namespace Sysman
} // namespace L0
