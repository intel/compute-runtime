/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/test/unit_tests/sources/sysman/linux/mock_sysman_fixture.h"
#include "level_zero/tools/test/unit_tests/sources/sysman/vf_management/linux/mock_sysfs_vf_management.h"

namespace L0 {
namespace ult {

constexpr uint32_t mockHandleCount = 1u;

class ZesVfFixturePrelim : public SysmanDeviceFixture {
  protected:
    std::unique_ptr<MockVfNeoDrm> pDrm;
    std::unique_ptr<MockVfPmuInterfaceImp> pPmuInterface;
    std::unique_ptr<MockVfFsAccess> pFsAccess;
    std::unique_ptr<MockVfSysfsAccess> pSysfsAccess;
    Drm *pOriginalDrm = nullptr;
    PmuInterface *pOriginalPmuInterface = nullptr;
    FsAccess *pOriginalFsAccess = nullptr;
    SysfsAccess *pOriginalSysfsAccess = nullptr;

    void SetUp() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        SysmanDeviceFixture::SetUp();
        pSysfsAccess = std::make_unique<MockVfSysfsAccess>();
        pOriginalSysfsAccess = pLinuxSysmanImp->pSysfsAccess;
        pLinuxSysmanImp->pSysfsAccess = pSysfsAccess.get();
        pOriginalFsAccess = pLinuxSysmanImp->pFsAccess;
        pFsAccess = std::make_unique<MockVfFsAccess>();
        pLinuxSysmanImp->pFsAccess = pFsAccess.get();
        pLinuxSysmanImp->isUsingPrelimEnabledKmd = true;
        pOriginalDrm = pLinuxSysmanImp->pDrm;
        pDrm = std::make_unique<MockVfNeoDrm>(const_cast<NEO::RootDeviceEnvironment &>(neoDevice->getRootDeviceEnvironment()));
        pDrm->setupIoctlHelper(neoDevice->getRootDeviceEnvironment().getHardwareInfo()->platform.eProductFamily);
        pLinuxSysmanImp->pDrm = pDrm.get();
        pPmuInterface = std::make_unique<MockVfPmuInterfaceImp>(pLinuxSysmanImp);
        pOriginalPmuInterface = pLinuxSysmanImp->pPmuInterface;
        pLinuxSysmanImp->pPmuInterface = pPmuInterface.get();
        pSysmanDeviceImp->pVfManagementHandleContext->handleList.clear();
    }

    void TearDown() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        pLinuxSysmanImp->pSysfsAccess = pOriginalSysfsAccess;
        pLinuxSysmanImp->pDrm = pOriginalDrm;
        pLinuxSysmanImp->pPmuInterface = pOriginalPmuInterface;
        pLinuxSysmanImp->pFsAccess = pOriginalFsAccess;
        SysmanDeviceFixture::TearDown();
    }

    std::vector<zes_vf_handle_t> getEnabledVfHandles(uint32_t count) {
        std::vector<zes_vf_handle_t> handles(count, nullptr);
        EXPECT_EQ(zesDeviceEnumEnabledVFExp(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
        return handles;
    }
};

TEST_F(ZesVfFixturePrelim, GivenValidDeviceHandleWhenQueryingEnabledVfHandlesThenCorrectVfHandlesAreReturned) {
    auto handles = getEnabledVfHandles(mockHandleCount);
    for (auto handleVf : handles) {
        ASSERT_NE(nullptr, handleVf);
    }
}

TEST_F(ZesVfFixturePrelim, GivenValidVfHandleWhenCallingZesVFManagementGetVFEngineUtilizationExp2AndSysmanQueryEngineInfoFailsThenErrorIsReturned) {

    pDrm->mockReadSysmanQueryEngineInfo = false;
    auto handles = getEnabledVfHandles(mockHandleCount);
    for (auto handleVf : handles) {
        ASSERT_NE(nullptr, handleVf);
        uint32_t count = 0;
        auto result = zesVFManagementGetVFEngineUtilizationExp2(handleVf, &count, nullptr);
        EXPECT_EQ(result, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
    }
}

TEST_F(ZesVfFixturePrelim, GivenValidVfHandleWhenCallingZesVFManagementGetVFEngineUtilizationExp2WithEngineStatsCountThenCorrectEngineStatsCountIsReturned) {

    auto handles = getEnabledVfHandles(mockHandleCount);
    for (auto handleVf : handles) {
        ASSERT_NE(nullptr, handleVf);
        uint32_t count = 0;
        auto result = zesVFManagementGetVFEngineUtilizationExp2(handleVf, &count, nullptr);
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(count, numberMockedEngines);
        count = count + 5;
        result = zesVFManagementGetVFEngineUtilizationExp2(handleVf, &count, nullptr);
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(count, numberMockedEngines);
    }
}

TEST_F(ZesVfFixturePrelim, GivenValidVfHandleWhenCallingZesVFManagementGetVFEngineUtilizationExp2WithEngineStatsCountLessThanActualThenCorrectEngineStatsCountIsReturned) {

    auto handles = getEnabledVfHandles(mockHandleCount);
    for (auto handleVf : handles) {
        ASSERT_NE(nullptr, handleVf);
        uint32_t count = 0;
        auto result = zesVFManagementGetVFEngineUtilizationExp2(handleVf, &count, nullptr);
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(count, numberMockedEngines);
        count = count - 1;
        std::vector<zes_vf_util_engine_exp2_t> engineUtils(count);
        result = zesVFManagementGetVFEngineUtilizationExp2(handleVf, &count, engineUtils.data());
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(count, engineUtils.size());
        EXPECT_EQ(engineUtils[0].vfEngineType, ZES_ENGINE_GROUP_COMPUTE_SINGLE);
        EXPECT_EQ(engineUtils[0].activeCounterValue, mockActiveTime);
        EXPECT_EQ(engineUtils[0].samplingCounterValue, mockTimestamp);
    }
}

TEST_F(ZesVfFixturePrelim, GivenValidVfHandleWhenQueryingEngineUtilizationMultipleTimesThenCorrectEngineStatsAreReturned) {

    auto handles = getEnabledVfHandles(mockHandleCount);
    for (auto handleVf : handles) {
        ASSERT_NE(nullptr, handleVf);
        uint32_t count = 0;
        auto result = zesVFManagementGetVFEngineUtilizationExp2(handleVf, &count, nullptr);
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        std::vector<zes_vf_util_engine_exp2_t> engineUtils(count);
        result = zesVFManagementGetVFEngineUtilizationExp2(handleVf, &count, engineUtils.data());
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(engineUtils[0].vfEngineType, ZES_ENGINE_GROUP_COMPUTE_SINGLE);
        EXPECT_EQ(engineUtils[0].activeCounterValue, mockActiveTime);
        EXPECT_EQ(engineUtils[0].samplingCounterValue, mockTimestamp);
        EXPECT_EQ(engineUtils[1].vfEngineType, ZES_ENGINE_GROUP_COPY_SINGLE);
        EXPECT_EQ(engineUtils[1].activeCounterValue, mockActiveTime);
        EXPECT_EQ(engineUtils[1].samplingCounterValue, mockTimestamp);
        engineUtils.clear();
        engineUtils.resize(count);
        result = zesVFManagementGetVFEngineUtilizationExp2(handleVf, &count, engineUtils.data());
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(engineUtils[0].vfEngineType, ZES_ENGINE_GROUP_COMPUTE_SINGLE);
        EXPECT_EQ(engineUtils[0].activeCounterValue, mockActiveTime);
        EXPECT_EQ(engineUtils[0].samplingCounterValue, mockTimestamp);
        EXPECT_EQ(engineUtils[1].vfEngineType, ZES_ENGINE_GROUP_COPY_SINGLE);
        EXPECT_EQ(engineUtils[1].activeCounterValue, mockActiveTime);
        EXPECT_EQ(engineUtils[1].samplingCounterValue, mockTimestamp);
    }
}

TEST_F(ZesVfFixturePrelim, GivenValidVfHandleWhenPmuInterfaceOpenFailsForBusyTicksConfigThenErrorIsReturned) {

    pFsAccess->mockReadFail = true;
    auto handles = getEnabledVfHandles(mockHandleCount);
    for (auto handleVf : handles) {
        ASSERT_NE(nullptr, handleVf);
        uint32_t count = 0;
        auto result = zesVFManagementGetVFEngineUtilizationExp2(handleVf, &count, nullptr);
        EXPECT_EQ(result, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
        result = zesVFManagementGetVFEngineUtilizationExp2(handleVf, &count, nullptr);
        EXPECT_EQ(result, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
    }
}

TEST_F(ZesVfFixturePrelim, GivenValidVfHandleWhenPmuInterfaceOpenFailsForTotalTicksConfigThenErrorIsReturned) {

    pPmuInterface->mockPerfEventOpenFailAtCount = 3;
    pPmuInterface->mockPerfEventOpenReadFail = true;
    auto handles = getEnabledVfHandles(mockHandleCount);
    for (auto handleVf : handles) {
        ASSERT_NE(nullptr, handleVf);
        uint32_t count = 0;
        auto result = zesVFManagementGetVFEngineUtilizationExp2(handleVf, &count, nullptr);
        EXPECT_EQ(result, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
        result = zesVFManagementGetVFEngineUtilizationExp2(handleVf, &count, nullptr);
        EXPECT_EQ(result, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
    }
}

TEST_F(ZesVfFixturePrelim, GivenValidVfHandleWhenPmuReadFailsThenErrorIsReturned) {

    pPmuInterface->mockPmuReadFail = true;
    auto handles = getEnabledVfHandles(mockHandleCount);
    for (auto handleVf : handles) {
        ASSERT_NE(nullptr, handleVf);
        uint32_t count = 0;
        auto result = zesVFManagementGetVFEngineUtilizationExp2(handleVf, &count, nullptr);
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        std::vector<zes_vf_util_engine_exp2_t> engineUtils(count);
        result = zesVFManagementGetVFEngineUtilizationExp2(handleVf, &count, engineUtils.data());
        EXPECT_EQ(result, ZE_RESULT_ERROR_UNKNOWN);
    }
}

} // namespace ult
} // namespace L0
