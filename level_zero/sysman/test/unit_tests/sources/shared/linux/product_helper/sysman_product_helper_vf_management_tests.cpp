/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"
#include "level_zero/sysman/test/unit_tests/sources/shared/linux/kmd_interface/mock_sysman_kmd_interface_i915.h"
#include "level_zero/sysman/test/unit_tests/sources/vf_management/linux/mock_sysfs_vf_management.h"

namespace L0 {
namespace Sysman {
namespace ult {

constexpr uint32_t mockHandleCount = 1u;

class SysmanProductHelperVfManagementTest : public SysmanDeviceFixture {
  protected:
    L0::Sysman::SysmanDevice *device = nullptr;
    std::unique_ptr<MockVfSysfsAccessInterface> pSysfsAccess;
    L0::Sysman::SysFsAccessInterface *pSysfsAccessOld = nullptr;

    void SetUp() override {
        SysmanDeviceFixture::SetUp();
        device = pSysmanDevice;
        pSysfsAccessOld = pLinuxSysmanImp->pSysfsAccess;
        pSysfsAccess = std::make_unique<MockVfSysfsAccessInterface>();
        pLinuxSysmanImp->pSysfsAccess = pSysfsAccess.get();
        pSysmanDeviceImp->pVfManagementHandleContext->handleList.clear();
    }

    void TearDown() override {
        pLinuxSysmanImp->pSysfsAccess = pSysfsAccessOld;
        SysmanDeviceFixture::TearDown();
    }

    std::vector<zes_vf_handle_t> getEnabledVfHandles(uint32_t count) {
        std::vector<zes_vf_handle_t> handles(count, nullptr);
        EXPECT_EQ(zesDeviceEnumEnabledVFExp(device, &count, handles.data()), ZE_RESULT_SUCCESS);
        return handles;
    }
};

HWTEST2_F(SysmanProductHelperVfManagementTest, GivenSysmanProductHelperWhenCheckingIsVfMemoryUtilizationSupportedThenTrueValueIsReturned, IsDG2) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    EXPECT_TRUE(pSysmanProductHelper->isVfMemoryUtilizationSupported());
}

HWTEST2_F(SysmanProductHelperVfManagementTest, GivenSysmanProductHelperWhenCheckingIsVfMemoryUtilizationSupportedThenFalseValueIsReturned, IsNotDG2) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    EXPECT_FALSE(pSysmanProductHelper->isVfMemoryUtilizationSupported());
}

HWTEST2_F(SysmanProductHelperVfManagementTest, GivenValidVfHandleWhenQueryingMemoryUtilizationThenMemoryParamsAreReturnedCorrectly, IsDG2) {
    auto handles = getEnabledVfHandles(mockHandleCount);
    for (auto hSysmanVf : handles) {
        ASSERT_NE(nullptr, hSysmanVf);
        uint32_t count = 0;
        EXPECT_EQ(zesVFManagementGetVFMemoryUtilizationExp2(hSysmanVf, &count, nullptr), ZE_RESULT_SUCCESS);
        EXPECT_GT(count, 0u);
        std::vector<zes_vf_util_mem_exp2_t> memUtils(count);
        EXPECT_EQ(zesVFManagementGetVFMemoryUtilizationExp2(hSysmanVf, &count, memUtils.data()), ZE_RESULT_SUCCESS);
        for (uint32_t it = 0; it < count; it++) {
            EXPECT_EQ(memUtils[it].vfMemUtilized, mockLmemUsed);
            EXPECT_EQ(memUtils[it].vfMemLocation, ZES_MEM_LOC_DEVICE);
        }
    }
}

HWTEST2_F(SysmanProductHelperVfManagementTest, GivenValidVfHandleWhenQueryingMemoryUtilizationWithoutMemUtilOutputParamTwiceThenApiReturnedCorrectly, IsDG2) {
    auto handles = getEnabledVfHandles(mockHandleCount);
    for (auto hSysmanVf : handles) {
        ASSERT_NE(nullptr, hSysmanVf);
        uint32_t count = 0;
        EXPECT_EQ(zesVFManagementGetVFMemoryUtilizationExp2(hSysmanVf, &count, nullptr), ZE_RESULT_SUCCESS);
        EXPECT_GT(count, 0u);
        uint32_t count2 = count;
        EXPECT_EQ(zesVFManagementGetVFMemoryUtilizationExp2(hSysmanVf, &count2, nullptr), ZE_RESULT_SUCCESS);
        EXPECT_EQ(count2, count);
        count2 = count2 + 2;
        EXPECT_EQ(zesVFManagementGetVFMemoryUtilizationExp2(hSysmanVf, &count2, nullptr), ZE_RESULT_SUCCESS);
        EXPECT_EQ(count2, count);
    }
}

HWTEST2_F(SysmanProductHelperVfManagementTest, GivenValidVfHandleWhenQueryingMemoryUtilizationWithSysfsAbsentThenUnSupportedErrorIsReturned, IsDG2) {
    auto handles = getEnabledVfHandles(mockHandleCount);
    for (auto hSysmanVf : handles) {
        ASSERT_NE(nullptr, hSysmanVf);
        uint32_t mockCount = 1;
        pSysfsAccess->mockError = ZE_RESULT_ERROR_UNKNOWN;
        std::vector<zes_vf_util_mem_exp2_t> memUtils(mockCount);
        EXPECT_EQ(zesVFManagementGetVFMemoryUtilizationExp2(hSysmanVf, &mockCount, memUtils.data()), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
    }
}

HWTEST2_F(SysmanProductHelperVfManagementTest, GivenVfHandleWhenCallingVfOsGetMemoryUtilizationThenErrorIsReturned, IsNotDG2) {
    std::vector<zes_vf_util_mem_exp2_t> memUtils = {};
    uint32_t count = 0;
    auto pVfImp = std::make_unique<PublicLinuxVfImp>(pOsSysman, 1);
    EXPECT_EQ(pVfImp->vfOsGetMemoryUtilization(&count, memUtils.data()), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
}

} // namespace ult
} // namespace Sysman
} // namespace L0