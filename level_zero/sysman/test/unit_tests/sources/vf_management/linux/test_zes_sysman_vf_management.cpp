/*
 * Copyright (C) 2024-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"
#include "level_zero/sysman/test/unit_tests/sources/shared/linux/kmd_interface/mock_sysman_kmd_interface_i915.h"
#include "level_zero/sysman/test/unit_tests/sources/vf_management/linux/mock_sysfs_vf_management.h"

namespace L0 {
namespace Sysman {
namespace ult {

constexpr uint32_t mockHandleCount = 1u;
bool mockUuidForVfTest = true;

class ZesVfFixture : public SysmanDeviceFixture {
  protected:
    L0::Sysman::SysmanDevice *device = nullptr;
    MockSysmanKmdInterfaceUpstream *pSysmanKmdInterface = nullptr;
    MockVfSysfsAccessInterface *pSysfsAccess = nullptr;
    L0::Sysman::SysFsAccessInterface *pSysfsAccessOld = nullptr;
    L0::Sysman::SysmanKmdInterface *pSysmanKmdInterfaceOld = nullptr;

    void SetUp() override {
        SysmanDeviceFixture::SetUp();
        device = pSysmanDevice;
        pSysfsAccessOld = pLinuxSysmanImp->pSysfsAccess;
        pSysmanKmdInterfaceOld = pLinuxSysmanImp->pSysmanKmdInterface.release();
        pSysmanKmdInterface = new MockSysmanKmdInterfaceUpstream(pLinuxSysmanImp->pSysmanProductHelper.get());
        pSysfsAccess = new MockVfSysfsAccessInterface();
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

TEST_F(ZesVfFixture, GivenValidDeviceHandleWhenQueryingEnabledVfHandlesThenVfHandlesAreReturnedCorrectly) {
    auto handles = getEnabledVfHandles(mockHandleCount);
    for (auto hSysmanVf : handles) {
        ASSERT_NE(nullptr, hSysmanVf);
    }
}

TEST_F(ZesVfFixture, GivenValidDeviceHandleWhenQueryingEnabledVfHandlesThenSameVfHandlesAreReturned) {
    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumEnabledVFExp(device, &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, (uint32_t)mockHandleCount);
    EXPECT_EQ(zesDeviceEnumEnabledVFExp(device, &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, (uint32_t)mockHandleCount);
    std::vector<zes_vf_handle_t> handles(count);
    count = mockHandleCount + 4;
    EXPECT_EQ(zesDeviceEnumEnabledVFExp(device, &count, handles.data()), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, (uint32_t)mockHandleCount);
}

TEST_F(ZesVfFixture, GivenValidDeviceWithNoSRIOVSupportWhenQueryingEnabledVfHandlesThenZeroVfHandlesAreReturned) {
    uint32_t count = 0;
    pSysfsAccess->mockError = ZE_RESULT_ERROR_UNKNOWN;
    EXPECT_EQ(zesDeviceEnumEnabledVFExp(device, &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, (uint32_t)0);
}

TEST_F(ZesVfFixture, GivenValidVfHandleWhenCallingZesVFManagementGetVFEngineUtilizationExpThenUnSupportedErrorIsReturned) {
    auto handles = getEnabledVfHandles(mockHandleCount);
    for (auto hSysmanVf : handles) {
        ASSERT_NE(nullptr, hSysmanVf);
        uint32_t engineCount = 0;
        EXPECT_EQ(zesVFManagementGetVFEngineUtilizationExp(hSysmanVf, &engineCount, nullptr), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
    }
}

TEST_F(ZesVfFixture, GivenValidVfHandleWhenCallingZesVFManagementGetVFEngineUtilizationExp2ThenUnSupportedErrorIsReturned) {
    auto handles = getEnabledVfHandles(mockHandleCount);
    for (auto hSysmanVf : handles) {
        ASSERT_NE(nullptr, hSysmanVf);
        uint32_t engineCount = 0;
        std::vector<zes_vf_util_engine_exp2_t> engineUtils;
        EXPECT_EQ(zesVFManagementGetVFEngineUtilizationExp2(hSysmanVf, &engineCount, engineUtils.data()), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
    }
}

TEST_F(ZesVfFixture, GivenValidVfHandleWhenBusyAndTotalTicksConfigNotAvailableAndCallingVfEngineDataInitThenErrorIsReturned) {

    auto pDrm = new MockVfNeoDrm(const_cast<NEO::RootDeviceEnvironment &>(pSysmanDeviceImp->getRootDeviceEnvironment()));
    pDrm->setupIoctlHelper(pSysmanDeviceImp->getRootDeviceEnvironment().getHardwareInfo()->platform.eProductFamily);
    auto &osInterface = pSysmanDeviceImp->getRootDeviceEnvironment().osInterface;
    osInterface->setDriverModel(std::unique_ptr<MockVfNeoDrm>(pDrm));

    auto pVfImp = std::make_unique<PublicLinuxVfImp>(pOsSysman, 1);
    EXPECT_EQ(pVfImp->vfEngineDataInit(), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
}

TEST_F(ZesVfFixture, GivenValidVfHandleWhenQueryingVfCapabilitiesThenSuccessAndCorrectPciAddressIsReturned) {
    constexpr uint32_t mockedDomain = 0;
    constexpr uint32_t mockedBus = 0x4d;
    constexpr uint32_t mockedDevice = 0;
    constexpr uint32_t mockedFunction = 1;
    auto handles = getEnabledVfHandles(mockHandleCount);
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

TEST_F(ZesVfFixture, GivenValidVfHandleAndSysfsGetRealPathFailsWhenQueryingVfCapabilitiesThenErrorIsReturned) {
    pSysfsAccess->mockRealPathError = ZE_RESULT_ERROR_UNKNOWN;
    auto handles = getEnabledVfHandles(mockHandleCount);
    for (auto hSysmanVf : handles) {
        ASSERT_NE(nullptr, hSysmanVf);
        zes_vf_exp2_capabilities_t capabilities = {};
        EXPECT_EQ(zesVFManagementGetVFCapabilitiesExp2(hSysmanVf, &capabilities), ZE_RESULT_ERROR_UNKNOWN);
    }
}

TEST_F(ZesVfFixture, GivenValidVfHandleAndInvalidBDFWithImproperTokensWhenQueryingVfCapabilitiesThenErrorIsReturned) {
    pSysfsAccess->mockValidBdfData = false;
    pSysfsAccess->mockInvalidTokens = true;
    auto handles = getEnabledVfHandles(mockHandleCount);
    for (auto hSysmanVf : handles) {
        ASSERT_NE(nullptr, hSysmanVf);
        zes_vf_exp2_capabilities_t capabilities = {};
        EXPECT_EQ(zesVFManagementGetVFCapabilitiesExp2(hSysmanVf, &capabilities), ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE);
    }
}

TEST_F(ZesVfFixture, GivenValidVfHandleAndInvalidBDFWhenQueryingVfCapabilitiesThenErrorIsReturned) {
    pSysfsAccess->mockValidBdfData = false;
    pSysfsAccess->mockInvalidTokens = false;
    auto handles = getEnabledVfHandles(mockHandleCount);
    for (auto hSysmanVf : handles) {
        ASSERT_NE(nullptr, hSysmanVf);
        zes_vf_exp2_capabilities_t capabilities = {};
        EXPECT_EQ(zesVFManagementGetVFCapabilitiesExp2(hSysmanVf, &capabilities), ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE);
    }
}

TEST_F(ZesVfFixture, GivenValidVfHandleWhenQueryingVfCapabilitiesThenParamsReturnedCorrectly) {
    auto handles = getEnabledVfHandles(mockHandleCount);
    for (auto hSysmanVf : handles) {
        ASSERT_NE(nullptr, hSysmanVf);
        zes_vf_exp2_capabilities_t capabilities = {};
        EXPECT_EQ(zesVFManagementGetVFCapabilitiesExp2(hSysmanVf, &capabilities), ZE_RESULT_SUCCESS);
        EXPECT_EQ(capabilities.vfDeviceMemSize, mockLmemQuota);
    }
}

TEST_F(ZesVfFixture, GivenValidVfHandleWhenQueryingVfCapabilitiesAndSysfsReadForMemoryQuotaValueFailsThenErrorIsReturned) {

    pSysfsAccess->mockReadMemoryError = ZE_RESULT_ERROR_UNKNOWN;
    auto handles = getEnabledVfHandles(mockHandleCount);
    for (auto hSysmanVf : handles) {
        ASSERT_NE(nullptr, hSysmanVf);
        zes_vf_exp2_capabilities_t capabilities = {};
        EXPECT_EQ(zesVFManagementGetVFCapabilitiesExp2(hSysmanVf, &capabilities), ZE_RESULT_ERROR_UNKNOWN);
    }
}

} // namespace ult
} // namespace Sysman
} // namespace L0
