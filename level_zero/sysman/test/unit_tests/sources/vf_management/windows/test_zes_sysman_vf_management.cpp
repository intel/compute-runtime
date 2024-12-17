/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/vf_management/windows/sysman_os_vf_imp.h"
#include "level_zero/sysman/test/unit_tests/sources/windows/mock_sysman_fixture.h"

namespace L0 {
namespace Sysman {
namespace ult {

class ZesVfFixture : public SysmanDeviceFixture {
  protected:
    void SetUp() override {
        SysmanDeviceFixture::SetUp();
    }

    void TearDown() override {
        SysmanDeviceFixture::TearDown();
    }
};

TEST_F(ZesVfFixture, GivenValidDeviceHandleAndOneVfIsEnabledWhenRetrievingVfHandlesThenCorrectCountIsReturned) {

    WddmVfImp::numEnabledVfs = 1;
    uint32_t count = 0;
    uint32_t mockHandleCount = 1u;
    ze_result_t result = zesDeviceEnumEnabledVFExp(pSysmanDevice->toHandle(), &count, nullptr);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, mockHandleCount);
    result = zesDeviceEnumEnabledVFExp(pSysmanDevice->toHandle(), &count, nullptr);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, mockHandleCount);
    count = count + 1;
    result = zesDeviceEnumEnabledVFExp(pSysmanDevice->toHandle(), &count, nullptr);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, mockHandleCount);
    std::vector<zes_vf_handle_t> handles(count);
    result = zesDeviceEnumEnabledVFExp(pSysmanDevice->toHandle(), &count, handles.data());
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    EXPECT_EQ(handles.size(), mockHandleCount);
    WddmVfImp::numEnabledVfs = 0;
}

TEST_F(ZesVfFixture, GivenValidVfHandleWhenQueryingVfCapabilitiesThenZeroPciAddressIsReturned) {

    uint32_t vfId = 1;
    std::unique_ptr<VfManagement> pVfManagement = std::make_unique<VfImp>(pOsSysman, vfId);
    zes_vf_handle_t vfHandle = pVfManagement->toVfManagementHandle();
    zes_vf_exp_capabilities_t capabilities = {};
    ze_result_t result = zesVFManagementGetVFCapabilitiesExp(vfHandle, &capabilities);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    EXPECT_EQ(capabilities.address.domain, (uint32_t)0);
    EXPECT_EQ(capabilities.address.bus, (uint32_t)0);
    EXPECT_EQ(capabilities.address.device, (uint32_t)0);
    EXPECT_EQ(capabilities.address.function, (uint32_t)0);
}

TEST_F(ZesVfFixture, GivenValidVfHandleWhenQueryingOsVfCapabilitiesThenErrorIsReturned) {

    auto pWddmVfImp = std::make_unique<WddmVfImp>();
    zes_vf_exp_capabilities_t capabilities = {};
    ze_result_t result = pWddmVfImp->vfOsGetCapabilities(&capabilities);
    EXPECT_EQ(result, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
}

TEST_F(ZesVfFixture, GivenValidVfHandleWhenQueryingMemoryUtilizationThenErrorIsReturned) {

    uint32_t vfId = 1;
    uint32_t count = 0;
    uint32_t mockHandleCount = 1;
    std::unique_ptr<VfManagement> pVfManagement = std::make_unique<VfImp>(pOsSysman, vfId);
    pSysmanDeviceImp->pVfManagementHandleContext->handleList.push_back(std::move(pVfManagement));

    ze_result_t result = zesDeviceEnumEnabledVFExp(pSysmanDevice->toHandle(), &count, nullptr);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, mockHandleCount);

    std::vector<zes_vf_handle_t> vfHandleList(count);
    result = zesDeviceEnumEnabledVFExp(pSysmanDevice->toHandle(), &count, vfHandleList.data());
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    for (auto vfHandle : vfHandleList) {
        EXPECT_NE(vfHandle, nullptr);
        result = zesVFManagementGetVFMemoryUtilizationExp2(vfHandle, &count, nullptr);
        EXPECT_EQ(result, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
    }
}

TEST_F(ZesVfFixture, GivenValidVfHandleWhenQueryingEngineUtilizationThenErrorIsReturned) {

    uint32_t vfId = 1;
    uint32_t count = 0;
    std::unique_ptr<VfManagement> pVfManagement = std::make_unique<VfImp>(pOsSysman, vfId);
    zes_vf_handle_t vfHandle = pVfManagement->toVfManagementHandle();
    ze_result_t result = zesVFManagementGetVFEngineUtilizationExp2(vfHandle, &count, nullptr);
    EXPECT_EQ(result, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
}

TEST_F(ZesVfFixture, GivenValidVfHandleWhenQueryingOsGetLocalMemoryUsedThenErrorIsReturned) {
    auto pWddmVfImp = std::make_unique<WddmVfImp>();
    uint64_t lMemUsed = 0;
    bool result = pWddmVfImp->vfOsGetLocalMemoryUsed(lMemUsed);
    EXPECT_FALSE(result);
}

TEST_F(ZesVfFixture, GivenValidVfHandleWhenQueryingOsGetLocalMemoryQuotaThenErrorIsReturned) {
    auto pWddmVfImp = std::make_unique<WddmVfImp>();
    uint64_t lMemQuota = 0;
    bool result = pWddmVfImp->vfOsGetLocalMemoryQuota(lMemQuota);
    EXPECT_FALSE(result);
}

} // namespace ult
} // namespace Sysman
} // namespace L0