/*
 * Copyright (C) 2023-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/ras/linux/ras_util/sysman_ras_util.h"
#include "level_zero/sysman/source/shared/linux/nl_api/sysman_drm_ras_types.h"
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

HWTEST2_F(SysmanProductHelperRasTest, GivenSysmanProductHelperInstanceWhenQueryingRasInterfaceThenVerifyProperInterfacesAreReturned, IsCRI) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(NEO::defaultHwInfo->platform.eProductFamily);
    EXPECT_EQ(RasInterfaceType::netlink, pSysmanProductHelper->getGtRasUtilInterface());
    EXPECT_EQ(RasInterfaceType::netlink, pSysmanProductHelper->getHbmRasUtilInterface());
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

HWTEST2_F(SysmanProductHelperRasTest, GivenPmuRasUtilForCorrectableErrorTypeWhenCallingGetSupportedErrorCategoriesExpThenCorrectCategoriesAreReturned, IsGtRasSupportedProduct) {
    auto pRasUtil = std::make_unique<PmuRasUtil>(ZES_RAS_ERROR_TYPE_CORRECTABLE, pLinuxSysmanImp, false, 0u);
    auto categories = pRasUtil->getSupportedErrorCategoriesExp();
    EXPECT_EQ(3u, categories.size());
    EXPECT_NE(categories.end(), std::find(categories.begin(), categories.end(), ZES_RAS_ERROR_CATEGORY_EXP_CACHE_ERRORS));
    EXPECT_NE(categories.end(), std::find(categories.begin(), categories.end(), ZES_RAS_ERROR_CATEGORY_EXP_NON_COMPUTE_ERRORS));
    EXPECT_NE(categories.end(), std::find(categories.begin(), categories.end(), ZES_RAS_ERROR_CATEGORY_EXP_COMPUTE_ERRORS));
}

HWTEST2_F(SysmanProductHelperRasTest, GivenPmuRasUtilForUncorrectableErrorTypeWhenCallingGetSupportedErrorCategoriesExpThenCorrectCategoriesAreReturned, IsGtRasSupportedProduct) {
    auto pRasUtil = std::make_unique<PmuRasUtil>(ZES_RAS_ERROR_TYPE_UNCORRECTABLE, pLinuxSysmanImp, false, 0u);
    auto categories = pRasUtil->getSupportedErrorCategoriesExp();
    EXPECT_EQ(7u, categories.size());
    EXPECT_NE(categories.end(), std::find(categories.begin(), categories.end(), ZES_RAS_ERROR_CATEGORY_EXP_CACHE_ERRORS));
    EXPECT_NE(categories.end(), std::find(categories.begin(), categories.end(), ZES_RAS_ERROR_CATEGORY_EXP_RESET));
    EXPECT_NE(categories.end(), std::find(categories.begin(), categories.end(), ZES_RAS_ERROR_CATEGORY_EXP_PROGRAMMING_ERRORS));
    EXPECT_NE(categories.end(), std::find(categories.begin(), categories.end(), ZES_RAS_ERROR_CATEGORY_EXP_NON_COMPUTE_ERRORS));
    EXPECT_NE(categories.end(), std::find(categories.begin(), categories.end(), ZES_RAS_ERROR_CATEGORY_EXP_COMPUTE_ERRORS));
    EXPECT_NE(categories.end(), std::find(categories.begin(), categories.end(), ZES_RAS_ERROR_CATEGORY_EXP_DRIVER_ERRORS));
    EXPECT_NE(categories.end(), std::find(categories.begin(), categories.end(), ZES_RAS_ERROR_CATEGORY_EXP_L3FABRIC_ERRORS));
}

HWTEST2_F(SysmanProductHelperRasTest, GivenGscRasUtilWhenCallingGetSupportedErrorCategoriesExpThenMemoryErrorCategoryIsReturned, IsPVC) {
    auto pRasUtil = std::make_unique<GscRasUtil>(ZES_RAS_ERROR_TYPE_CORRECTABLE, pLinuxSysmanImp, 0u);
    auto categories = pRasUtil->getSupportedErrorCategoriesExp();
    EXPECT_EQ(1u, categories.size());
    EXPECT_EQ(ZES_RAS_ERROR_CATEGORY_EXP_MEMORY_ERRORS, categories[0]);
}

HWTEST2_F(SysmanProductHelperRasTest, GivenNetlinkRasUtilWhenCallingGetSupportedErrorCategoriesExpThenAllSixCategoriesAreReturned, IsCRI) {
    auto pRasUtil = std::make_unique<MockRasNetlinkUtil>(ZES_RAS_ERROR_TYPE_CORRECTABLE, pLinuxSysmanImp, 0u);
    MockRasNetlinkUtil::rasErrorList[pRasUtil->rasNodeId] = {
        {0, "core-compute", 10, 0}, {0, "device-memory", 20, 0}, {0, "fabric", 30, 0}, {0, "scale", 40, 0}, {0, "pcie", 50, 0}, {0, "soc-internal", 60, 0}};
    auto categories = pRasUtil->getSupportedErrorCategoriesExp();
    EXPECT_EQ(6u, categories.size());
    EXPECT_NE(categories.end(), std::find(categories.begin(), categories.end(), ZES_RAS_ERROR_CATEGORY_EXP_COMPUTE_ERRORS));
    EXPECT_NE(categories.end(), std::find(categories.begin(), categories.end(), ZES_RAS_ERROR_CATEGORY_EXP_MEMORY_ERRORS));
    EXPECT_NE(categories.end(), std::find(categories.begin(), categories.end(), static_cast<zes_ras_error_category_exp_t>(ZES_INTEL_RAS_ERROR_CATEGORY_EXP_PCIE_ERRORS)));
    EXPECT_NE(categories.end(), std::find(categories.begin(), categories.end(), static_cast<zes_ras_error_category_exp_t>(ZES_INTEL_RAS_ERROR_CATEGORY_EXP_FABRIC_ERRORS)));
    EXPECT_NE(categories.end(), std::find(categories.begin(), categories.end(), ZES_RAS_ERROR_CATEGORY_EXP_SCALE_ERRORS));
    EXPECT_NE(categories.end(), std::find(categories.begin(), categories.end(), static_cast<zes_ras_error_category_exp_t>(ZES_INTEL_RAS_ERROR_CATEGORY_EXP_SOC_INTERNAL_ERRORS)));
    MockRasNetlinkUtil::rasErrorList.erase(pRasUtil->rasNodeId);
}

HWTEST2_F(SysmanProductHelperRasTest, GivenLinuxRasImpWhenCallingOsRasGetSupportedCategoriesExpWithZeroCountThenCorrectCountIsReturned, IsGtRasSupportedProduct) {
    bool isSubDevice = false;
    uint32_t subDeviceId = 0u;

    auto pLinuxRasImpCorr = std::make_unique<PublicLinuxRasImp>(pOsSysman, ZES_RAS_ERROR_TYPE_CORRECTABLE, isSubDevice, subDeviceId);
    uint32_t count = 0u;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxRasImpCorr->osRasGetSupportedCategoriesExp(&count, nullptr));
    EXPECT_EQ(3u, count);

    auto pLinuxRasImpUncorr = std::make_unique<PublicLinuxRasImp>(pOsSysman, ZES_RAS_ERROR_TYPE_UNCORRECTABLE, isSubDevice, subDeviceId);
    count = 0u;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxRasImpUncorr->osRasGetSupportedCategoriesExp(&count, nullptr));
    EXPECT_EQ(7u, count);
}

HWTEST2_F(SysmanProductHelperRasTest, GivenLinuxRasImpWhenCallingOsRasGetSupportedCategoriesExpWithNonZeroCountThenCategoriesArePopulated, IsGtRasSupportedProduct) {
    bool isSubDevice = false;
    uint32_t subDeviceId = 0u;

    auto pLinuxRasImp = std::make_unique<PublicLinuxRasImp>(pOsSysman, ZES_RAS_ERROR_TYPE_CORRECTABLE, isSubDevice, subDeviceId);
    uint32_t count = 3u;
    std::vector<zes_ras_error_category_exp_t> categories(count);
    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxRasImp->osRasGetSupportedCategoriesExp(&count, categories.data()));
    EXPECT_EQ(3u, count);
    EXPECT_NE(categories.end(), std::find(categories.begin(), categories.end(), ZES_RAS_ERROR_CATEGORY_EXP_CACHE_ERRORS));
    EXPECT_NE(categories.end(), std::find(categories.begin(), categories.end(), ZES_RAS_ERROR_CATEGORY_EXP_NON_COMPUTE_ERRORS));
    EXPECT_NE(categories.end(), std::find(categories.begin(), categories.end(), ZES_RAS_ERROR_CATEGORY_EXP_COMPUTE_ERRORS));
}

HWTEST2_F(SysmanProductHelperRasTest, GivenLinuxRasImpWhenCallingOsRasGetSupportedCategoriesExpWithLowerCountThenRequestedCountIsPopulated, IsGtRasSupportedProduct) {
    bool isSubDevice = false;
    uint32_t subDeviceId = 0u;

    auto pLinuxRasImp = std::make_unique<PublicLinuxRasImp>(pOsSysman, ZES_RAS_ERROR_TYPE_UNCORRECTABLE, isSubDevice, subDeviceId);
    uint32_t fullCount = 0u;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxRasImp->osRasGetSupportedCategoriesExp(&fullCount, nullptr));
    EXPECT_EQ(7u, fullCount);

    uint32_t partialCount = fullCount - 1u;
    std::vector<zes_ras_error_category_exp_t> categories(fullCount, ZES_RAS_ERROR_CATEGORY_EXP_FORCE_UINT32);
    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxRasImp->osRasGetSupportedCategoriesExp(&partialCount, categories.data()));
    EXPECT_EQ(fullCount - 1u, partialCount);
}

HWTEST2_F(SysmanProductHelperRasTest, GivenLinuxRasImpWithHbmSourceWhenCallingOsRasGetSupportedCategoriesExpThenMemoryErrorCategoryIsIncluded, IsPVC) {
    bool isSubDevice = false;
    uint32_t subDeviceId = 0u;

    auto pLinuxRasImp = std::make_unique<PublicLinuxRasImp>(pOsSysman, ZES_RAS_ERROR_TYPE_CORRECTABLE, isSubDevice, subDeviceId);
    pLinuxRasImp->rasSources.clear();
    pLinuxRasImp->supportedErrorCategoriesExp.clear();
    pLinuxRasImp->rasSources.push_back(std::make_unique<L0::Sysman::LinuxRasSourceGt>(pLinuxSysmanImp, ZES_RAS_ERROR_TYPE_CORRECTABLE, isSubDevice, subDeviceId));
    pLinuxRasImp->rasSources.push_back(std::make_unique<L0::Sysman::LinuxRasSourceHbm>(pLinuxSysmanImp, ZES_RAS_ERROR_TYPE_CORRECTABLE, isSubDevice, subDeviceId));
    for (const auto &rasSource : pLinuxRasImp->rasSources) {
        auto cats = rasSource->getSupportedErrorCategoriesExp();
        pLinuxRasImp->supportedErrorCategoriesExp.insert(pLinuxRasImp->supportedErrorCategoriesExp.end(), cats.begin(), cats.end());
    }

    uint32_t count = 0u;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxRasImp->osRasGetSupportedCategoriesExp(&count, nullptr));
    EXPECT_EQ(4u, count);

    std::vector<zes_ras_error_category_exp_t> categories(count);
    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxRasImp->osRasGetSupportedCategoriesExp(&count, categories.data()));
    EXPECT_NE(categories.end(), std::find(categories.begin(), categories.end(), ZES_RAS_ERROR_CATEGORY_EXP_MEMORY_ERRORS));
}

} // namespace ult
} // namespace Sysman
} // namespace L0
