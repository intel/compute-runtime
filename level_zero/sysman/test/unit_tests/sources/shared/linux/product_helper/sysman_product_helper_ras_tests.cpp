/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/ras/linux/ras_util/sysman_ras_util.h"
#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"
#include "level_zero/sysman/test/unit_tests/sources/ras/linux/mock_sysman_ras.h"

namespace L0 {
namespace Sysman {
namespace ult {

using SysmanProductHelperRasTest = SysmanDeviceFixture;

HWTEST2_F(SysmanProductHelperRasTest, GivenSysmanProductHelperInstanceWhenQueryingRasInterfaceThenVerifyProperInterfacesAreReturned, IsPVC) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(NEO::defaultHwInfo->platform.eProductFamily);
    EXPECT_EQ(RasInterfaceType::pmu, pSysmanProductHelper->getGtRasUtilInterface());
    EXPECT_EQ(RasInterfaceType::gsc, pSysmanProductHelper->getHbmRasUtilInterface());
}

HWTEST2_F(SysmanProductHelperRasTest, GivenSysmanProductHelperInstanceWhenQueryingRasInterfaceThenVerifyProperInterfacesAreReturned, IsDG2) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(NEO::defaultHwInfo->platform.eProductFamily);
    EXPECT_EQ(RasInterfaceType::pmu, pSysmanProductHelper->getGtRasUtilInterface());
    EXPECT_EQ(RasInterfaceType::none, pSysmanProductHelper->getHbmRasUtilInterface());
}

HWTEST2_F(SysmanProductHelperRasTest, GivenValidRasHandleWhenRequestingCountWithRasGetStateExpThenCountIsNotZero, IsPVC) {
    bool isSubDevice = true;
    uint32_t subDeviceId = 0u;

    auto pLinuxRasImp = std::make_unique<PublicLinuxRasImp>(pOsSysman, ZES_RAS_ERROR_TYPE_CORRECTABLE, isSubDevice, subDeviceId);
    pLinuxRasImp->rasSources.clear();
    pLinuxRasImp->rasSources.push_back(std::make_unique<L0::Sysman::LinuxRasSourceGt>(pLinuxSysmanImp, ZES_RAS_ERROR_TYPE_CORRECTABLE, isSubDevice, subDeviceId));
    pLinuxRasImp->rasSources.push_back(std::make_unique<L0::Sysman::LinuxRasSourceHbm>(pLinuxSysmanImp, ZES_RAS_ERROR_TYPE_CORRECTABLE, isSubDevice, subDeviceId));

    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxRasImp->osRasGetStateExp(&count, nullptr));
    EXPECT_EQ(4u, count);
}

HWTEST2_F(SysmanProductHelperRasTest, GivenValidRasHandleWhenRequestingCountWithRasGetStateExpThenCountIsNotZero, IsDG1) {
    bool isSubDevice = true;
    uint32_t subDeviceId = 0u;

    auto pLinuxRasImp = std::make_unique<PublicLinuxRasImp>(pOsSysman, ZES_RAS_ERROR_TYPE_CORRECTABLE, isSubDevice, subDeviceId);
    pLinuxRasImp->rasSources.clear();
    pLinuxRasImp->rasSources.push_back(std::make_unique<L0::Sysman::LinuxRasSourceGt>(pLinuxSysmanImp, ZES_RAS_ERROR_TYPE_CORRECTABLE, isSubDevice, subDeviceId));
    pLinuxRasImp->rasSources.push_back(std::make_unique<L0::Sysman::LinuxRasSourceHbm>(pLinuxSysmanImp, ZES_RAS_ERROR_TYPE_CORRECTABLE, isSubDevice, subDeviceId));

    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxRasImp->osRasGetStateExp(&count, nullptr));
    EXPECT_EQ(3u, count);
}

HWTEST2_F(SysmanProductHelperRasTest, GivenValidRasHandleAndRasSourcesWhenCallingRasGetStateThenSuccessIsReturned, IsPVC) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, [](const char *path, char *buf, size_t bufsize) -> int {
        constexpr size_t sizeofPath = sizeof("/sys/devices/pci0000:00/0000:00:01.0/0000:01:00.0/0000:02:01.0/0000:03:00.0");
        strcpy_s(buf, sizeofPath, "/sys/devices/pci0000:00/0000:00:01.0/0000:01:00.0/0000:02:01.0/0000:03:00.0");
        return sizeofPath;
    });

    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::ostringstream oStream;
        oStream << pmuDriverType;
        std::string value = oStream.str();
        memcpy(buf, value.data(), count);
        return count;
    });

    VariableBackup<L0::Sysman::FsAccessInterface *> fsBackup(&pLinuxSysmanImp->pFsAccess);
    auto pFsAccess = std::make_unique<MockRasFsAccess>();
    pLinuxSysmanImp->pFsAccess = pFsAccess.get();

    auto pPmuInterface = std::make_unique<MockRasPmuInterfaceImp>(pLinuxSysmanImp);
    pPmuInterface->mockPmuReadAfterClear = true;
    VariableBackup<L0::Sysman::PmuInterface *> pmuBackup(&pLinuxSysmanImp->pPmuInterface);
    pLinuxSysmanImp->pPmuInterface = pPmuInterface.get();

    VariableBackup<L0::Sysman::SysFsAccessInterface *> sysfsBackup(&pLinuxSysmanImp->pSysfsAccess);
    auto pSysfsAccess = std::make_unique<MockRasSysfsAccess>();
    pLinuxSysmanImp->pSysfsAccess = pSysfsAccess.get();

    bool isSubDevice = false;
    uint32_t subDeviceId = 0u;

    auto pLinuxRasImp = std::make_unique<PublicLinuxRasImp>(pOsSysman, ZES_RAS_ERROR_TYPE_CORRECTABLE, isSubDevice, subDeviceId);
    pLinuxRasImp->rasSources.clear();
    pLinuxRasImp->rasSources.push_back(std::make_unique<L0::Sysman::LinuxRasSourceGt>(pLinuxSysmanImp, ZES_RAS_ERROR_TYPE_CORRECTABLE, isSubDevice, subDeviceId));
    pLinuxRasImp->rasSources.push_back(std::make_unique<L0::Sysman::LinuxRasSourceHbm>(pLinuxSysmanImp, ZES_RAS_ERROR_TYPE_CORRECTABLE, isSubDevice, subDeviceId));

    zes_ras_state_t state = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxRasImp->osRasGetState(state, 0));
    EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_RESET], 0u);
    EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_PROGRAMMING_ERRORS], 0u);
    EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_DRIVER_ERRORS], 0u);
    EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_COMPUTE_ERRORS], correctableGrfErrorCount + correctableEuErrorCount + initialCorrectableComputeErrors);
    EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_NON_COMPUTE_ERRORS], 0u);
    EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_CACHE_ERRORS], 0u);
    EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_DISPLAY_ERRORS], 0u);
}

HWTEST2_F(SysmanProductHelperRasTest, GivenValidRasHandleAndRasSourcesWhenCallingRasGetStateStateExpThenSuccessIsReturned, IsPVC) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, [](const char *path, char *buf, size_t bufsize) -> int {
        constexpr size_t sizeofPath = sizeof("/sys/devices/pci0000:00/0000:00:01.0/0000:01:00.0/0000:02:01.0/0000:03:00.0");
        strcpy_s(buf, sizeofPath, "/sys/devices/pci0000:00/0000:00:01.0/0000:01:00.0/0000:02:01.0/0000:03:00.0");
        return sizeofPath;
    });

    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::ostringstream oStream;
        oStream << pmuDriverType;
        std::string value = oStream.str();
        memcpy(buf, value.data(), count);
        return count;
    });

    VariableBackup<L0::Sysman::FsAccessInterface *> fsBackup(&pLinuxSysmanImp->pFsAccess);
    auto pFsAccess = std::make_unique<MockRasFsAccess>();
    pLinuxSysmanImp->pFsAccess = pFsAccess.get();

    auto pPmuInterface = std::make_unique<MockRasPmuInterfaceImp>(pLinuxSysmanImp);
    pPmuInterface->mockPmuReadAfterClear = true;
    VariableBackup<L0::Sysman::PmuInterface *> pmuBackup(&pLinuxSysmanImp->pPmuInterface);
    pLinuxSysmanImp->pPmuInterface = pPmuInterface.get();

    VariableBackup<L0::Sysman::SysFsAccessInterface *> sysfsBackup(&pLinuxSysmanImp->pSysfsAccess);
    auto pSysfsAccess = std::make_unique<MockRasSysfsAccess>();
    pLinuxSysmanImp->pSysfsAccess = pSysfsAccess.get();

    bool isSubDevice = false;
    uint32_t subDeviceId = 0u;

    auto pLinuxRasImp = std::make_unique<PublicLinuxRasImp>(pOsSysman, ZES_RAS_ERROR_TYPE_CORRECTABLE, isSubDevice, subDeviceId);
    pLinuxRasImp->rasSources.clear();
    pLinuxRasImp->rasSources.push_back(std::make_unique<L0::Sysman::LinuxRasSourceGt>(pLinuxSysmanImp, ZES_RAS_ERROR_TYPE_CORRECTABLE, isSubDevice, subDeviceId));
    pLinuxRasImp->rasSources.push_back(std::make_unique<L0::Sysman::LinuxRasSourceHbm>(pLinuxSysmanImp, ZES_RAS_ERROR_TYPE_CORRECTABLE, isSubDevice, subDeviceId));

    uint32_t count = 2;
    std::vector<zes_ras_state_exp_t> rasStates(count);
    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxRasImp->osRasGetStateExp(&count, rasStates.data()));
    for (uint32_t i = 0; i < count; i++) {
        if (rasStates[i].category == ZES_RAS_ERROR_CATEGORY_EXP_COMPUTE_ERRORS) {
            uint32_t expectedErrCount = correctableGrfErrorCount + correctableEuErrorCount + initialCorrectableComputeErrors;
            EXPECT_EQ(rasStates[i].errorCounter, expectedErrCount);
            break;
        } else if (rasStates[i].category == ZES_RAS_ERROR_CATEGORY_EXP_NON_COMPUTE_ERRORS) {
            uint32_t expectedErrCount = 0u;
            EXPECT_EQ(rasStates[i].errorCounter, expectedErrCount);
            break;
        }
    }
}

} // namespace ult
} // namespace Sysman
} // namespace L0
