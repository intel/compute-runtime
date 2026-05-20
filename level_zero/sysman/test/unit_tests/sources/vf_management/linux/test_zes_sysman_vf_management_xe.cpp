/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"
#include "level_zero/sysman/test/unit_tests/sources/shared/linux/kmd_interface/mock_sysman_kmd_interface_xe.h"
#include "level_zero/sysman/test/unit_tests/sources/vf_management/linux/mock_sysfs_vf_management_xe.h"

namespace L0 {
namespace Sysman {
namespace ult {

constexpr uint32_t mockHandleCountXe = 1u;

class ZesVfFixtureXe : public SysmanDeviceFixture {
  protected:
    L0::Sysman::SysmanDevice *device = nullptr;
    MockSysmanKmdInterfaceXe *pSysmanKmdInterface = nullptr;
    MockVfXeSysfsAccessInterface *pSysfsAccess = nullptr;
    L0::Sysman::SysFsAccessInterface *pSysfsAccessOld = nullptr;
    L0::Sysman::SysmanKmdInterface *pSysmanKmdInterfaceOld = nullptr;

    void SetUp() override {
        SysmanDeviceFixture::SetUp();
        device = pSysmanDevice;
        pSysfsAccessOld = pLinuxSysmanImp->pSysfsAccess;
        pSysmanKmdInterfaceOld = pLinuxSysmanImp->pSysmanKmdInterface.release();
        pSysmanKmdInterface = new MockSysmanKmdInterfaceXe(pLinuxSysmanImp->pSysmanProductHelper.get());
        pSysfsAccess = new MockVfXeSysfsAccessInterface();
        pLinuxSysmanImp->pSysfsAccess = pSysfsAccess;
        pSysmanKmdInterface->pSysfsAccess.reset(pSysfsAccess);
        pLinuxSysmanImp->pSysmanKmdInterface.reset(pSysmanKmdInterface);
        pSysmanDeviceImp->pVfManagementHandleContext->handleList.clear();
    }

    void TearDown() override {
        pLinuxSysmanImp->pSysfsAccess = pSysfsAccessOld;
        pLinuxSysmanImp->pSysmanKmdInterface.reset(pSysmanKmdInterfaceOld);
        SysmanDeviceFixture::TearDown();
    }

    std::vector<zes_vf_handle_t> getEnabledVfHandles(uint32_t count) {
        std::vector<zes_vf_handle_t> handles(count, nullptr);
        EXPECT_EQ(zesDeviceEnumEnabledVFExp(device, &count, handles.data()), ZE_RESULT_SUCCESS);
        return handles;
    }
};

TEST_F(ZesVfFixtureXe, GivenValidVfHandleWhenQueryingVfCapabilitiesThenSuccessAndCorrectPciAddressIsReturned) {
    constexpr uint32_t mockedDomain = 0;
    constexpr uint32_t mockedBus = 0x4d;
    constexpr uint32_t mockedDevice = 0;
    constexpr uint32_t mockedFunction = 1;
    auto handles = getEnabledVfHandles(mockHandleCountXe);
    for (auto hSysmanVf : handles) {
        ASSERT_NE(nullptr, hSysmanVf);
        zes_vf_exp2_capabilities_t capabilities = {};
        EXPECT_EQ(zesVFManagementGetVFCapabilitiesExp2(hSysmanVf, &capabilities), ZE_RESULT_SUCCESS);
        EXPECT_EQ(capabilities.address.domain, mockedDomain);
        EXPECT_EQ(capabilities.address.bus, mockedBus);
        EXPECT_EQ(capabilities.address.device, mockedDevice);
        EXPECT_EQ(capabilities.address.function, mockedFunction);
    }
}

TEST_F(ZesVfFixtureXe, GivenValidVfHandleAndSysfsGetRealPathFailsWhenQueryingVfCapabilitiesThenErrorIsReturned) {
    pSysfsAccess->mockRealPathError = ZE_RESULT_ERROR_UNKNOWN;
    auto handles = getEnabledVfHandles(mockHandleCountXe);
    for (auto hSysmanVf : handles) {
        ASSERT_NE(nullptr, hSysmanVf);
        zes_vf_exp2_capabilities_t capabilities = {};
        EXPECT_EQ(zesVFManagementGetVFCapabilitiesExp2(hSysmanVf, &capabilities), ZE_RESULT_ERROR_UNKNOWN);
    }
}

TEST_F(ZesVfFixtureXe, GivenValidVfHandleAndInvalidBDFWithImproperTokensWhenQueryingVfCapabilitiesThenErrorIsReturned) {
    pSysfsAccess->mockValidBdfData = false;
    pSysfsAccess->mockInvalidTokens = true;
    auto handles = getEnabledVfHandles(mockHandleCountXe);
    for (auto hSysmanVf : handles) {
        ASSERT_NE(nullptr, hSysmanVf);
        zes_vf_exp2_capabilities_t capabilities = {};
        EXPECT_EQ(zesVFManagementGetVFCapabilitiesExp2(hSysmanVf, &capabilities), ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE);
    }
}

TEST_F(ZesVfFixtureXe, GivenValidVfHandleAndInvalidBDFWhenQueryingVfCapabilitiesThenErrorIsReturned) {
    pSysfsAccess->mockValidBdfData = false;
    pSysfsAccess->mockInvalidTokens = false;
    auto handles = getEnabledVfHandles(mockHandleCountXe);
    for (auto hSysmanVf : handles) {
        ASSERT_NE(nullptr, hSysmanVf);
        zes_vf_exp2_capabilities_t capabilities = {};
        EXPECT_EQ(zesVFManagementGetVFCapabilitiesExp2(hSysmanVf, &capabilities), ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE);
    }
}

TEST_F(ZesVfFixtureXe, GivenValidVfHandleWhenQueryingVfCapabilitiesThenMemSizeParamIsReturnedCorrectly) {
    auto handles = getEnabledVfHandles(mockHandleCountXe);
    for (auto hSysmanVf : handles) {
        ASSERT_NE(nullptr, hSysmanVf);
        zes_vf_exp2_capabilities_t capabilities = {};
        EXPECT_EQ(zesVFManagementGetVFCapabilitiesExp2(hSysmanVf, &capabilities), ZE_RESULT_SUCCESS);
        EXPECT_EQ(capabilities.vfDeviceMemSize, mockLmemQuota);
    }
}

TEST_F(ZesVfFixtureXe, GivenValidVfHandleWhenSysfsReadForXeMemoryQuotaFailsWhenQueryingVfCapabilitiesThenErrorIsReturned) {
    pSysfsAccess->mockReadMemoryError = ZE_RESULT_ERROR_UNKNOWN;
    auto handles = getEnabledVfHandles(mockHandleCountXe);
    for (auto hSysmanVf : handles) {
        ASSERT_NE(nullptr, hSysmanVf);
        zes_vf_exp2_capabilities_t capabilities = {};
        EXPECT_EQ(zesVFManagementGetVFCapabilitiesExp2(hSysmanVf, &capabilities), ZE_RESULT_ERROR_UNKNOWN);
    }
}

} // namespace ult
} // namespace Sysman
} // namespace L0
