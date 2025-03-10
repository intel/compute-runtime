/*
 * Copyright (C) 2024-2025 Intel Corporation
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

class ZesVfFixturePrelim : public SysmanDeviceFixture {
  protected:
    L0::Sysman::SysmanDevice *device = nullptr;
    MockVfSysfsAccessInterface *pSysfsAccess = nullptr;
    MockVfFsAccessInterface *pFsAccess = nullptr;
    std::unique_ptr<MockVfPmuInterfaceImp> pPmuInterface;
    PmuInterface *pOriginalPmuInterface = nullptr;
    MockVfNeoDrm *pDrm = nullptr;
    MockSysmanKmdInterfacePrelim *pSysmanKmdInterface = nullptr;

    void SetUp() override {
        SysmanDeviceFixture::SetUp();
        VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, [](const char *path, char *buf, size_t bufsize) -> int {
            std::string str = "../../devices/pci0000:37/0000:37:01.0/0000:38:00.0/0000:39:01.0/0000:3a:00.0/drm/renderD128";
            std::memcpy(buf, str.c_str(), str.size());
            return static_cast<int>(str.size());
        });

        pFsAccess = new MockVfFsAccessInterface();
        pSysfsAccess = new MockVfSysfsAccessInterface();

        pSysmanKmdInterface = new MockSysmanKmdInterfacePrelim(pLinuxSysmanImp->getSysmanProductHelper());
        pSysmanKmdInterface->pFsAccess.reset(pFsAccess);
        pSysmanKmdInterface->pSysfsAccess.reset(pSysfsAccess);
        pLinuxSysmanImp->pSysmanKmdInterface.reset(pSysmanKmdInterface);

        pLinuxSysmanImp->pFsAccess = pLinuxSysmanImp->getSysmanKmdInterface()->getFsAccess();
        pLinuxSysmanImp->pSysfsAccess = pLinuxSysmanImp->getSysmanKmdInterface()->getSysFsAccess();

        pDrm = new MockVfNeoDrm(const_cast<NEO::RootDeviceEnvironment &>(pSysmanDeviceImp->getRootDeviceEnvironment()));
        pDrm->setupIoctlHelper(pSysmanDeviceImp->getRootDeviceEnvironment().getHardwareInfo()->platform.eProductFamily);
        auto &osInterface = pSysmanDeviceImp->getRootDeviceEnvironment().osInterface;
        osInterface->setDriverModel(std::unique_ptr<MockVfNeoDrm>(pDrm));

        pSysmanDeviceImp->pVfManagementHandleContext->handleList.clear();
        pPmuInterface = std::make_unique<MockVfPmuInterfaceImp>(pLinuxSysmanImp);
        pOriginalPmuInterface = pLinuxSysmanImp->pPmuInterface;
        pPmuInterface->pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
        pLinuxSysmanImp->pPmuInterface = pPmuInterface.get();
        bool isIntegratedDevice = true;
        pLinuxSysmanImp->pSysmanKmdInterface->setSysmanDeviceDirName(isIntegratedDevice);
        device = pSysmanDevice;
    }

    void TearDown() override {
        pLinuxSysmanImp->pPmuInterface = pOriginalPmuInterface;
        SysmanDeviceFixture::TearDown();
    }

    std::vector<zes_vf_handle_t> getEnabledVfHandles(uint32_t count) {
        std::vector<zes_vf_handle_t> handles(count, nullptr);
        EXPECT_EQ(zesDeviceEnumEnabledVFExp(device, &count, handles.data()), ZE_RESULT_SUCCESS);
        return handles;
    }
};

TEST_F(ZesVfFixturePrelim, GivenValidDeviceHandleWhenQueryingEnabledVfHandlesThenCorrectVfHandlesAreReturned) {
    auto handles = getEnabledVfHandles(mockHandleCount);
    for (auto handleVf : handles) {
        ASSERT_NE(nullptr, handleVf);
    }
}

TEST_F(ZesVfFixturePrelim, GivenValidVfHandleWhenCallingZesVFManagementGetVFEngineUtilizationExp2AndOpenCallFailsThenErrorIsReturned) {

    MockVfNeoDrm *pDrm = nullptr;
    int mockFd = -1;
    pDrm = new MockVfNeoDrm(const_cast<NEO::RootDeviceEnvironment &>(pSysmanDeviceImp->getRootDeviceEnvironment()), mockFd);
    pDrm->setupIoctlHelper(pSysmanDeviceImp->getRootDeviceEnvironment().getHardwareInfo()->platform.eProductFamily);
    auto &osInterface = pSysmanDeviceImp->getRootDeviceEnvironment().osInterface;
    osInterface->setDriverModel(std::unique_ptr<MockVfNeoDrm>(pDrm));

    auto handles = getEnabledVfHandles(mockHandleCount);
    for (auto handleVf : handles) {
        ASSERT_NE(nullptr, handleVf);
        uint32_t count = 0;
        auto result = zesVFManagementGetVFEngineUtilizationExp2(handleVf, &count, nullptr);
        EXPECT_EQ(result, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
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
} // namespace Sysman
} // namespace L0