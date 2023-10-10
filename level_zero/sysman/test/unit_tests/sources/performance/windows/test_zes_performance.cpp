/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/performance/windows/sysman_os_performance_imp.h"
#include "level_zero/sysman/test/unit_tests/sources/performance/windows/mock_performance.h"
#include "level_zero/sysman/test/unit_tests/sources/windows/mock_sysman_fixture.h"

namespace L0 {
namespace Sysman {
namespace ult {

constexpr uint32_t mockHandleCount = 1u;
constexpr double maxPerformanceFactor = 100;
constexpr double halfOfMaxPerformanceFactor = 50;
constexpr double minPerformanceFactor = 0;

class SysmanDevicePerformanceFixture : public SysmanDeviceFixture {
  protected:
    std::unique_ptr<PerformanceKmdSysManager> pKmdSysManager;
    L0::Sysman::KmdSysManager *pOriginalKmdSysManager = nullptr;
    void SetUp() override {
        SysmanDeviceFixture::SetUp();
        pKmdSysManager.reset(new PerformanceKmdSysManager);

        pKmdSysManager->allowSetCalls = true;

        pOriginalKmdSysManager = pWddmSysmanImp->pKmdSysManager;
        pWddmSysmanImp->pKmdSysManager = pKmdSysManager.get();

        pSysmanDeviceImp->pPerformanceHandleContext->handleList.clear();
    }

    void TearDown() override {
        pWddmSysmanImp->pKmdSysManager = pOriginalKmdSysManager;
        SysmanDeviceFixture::TearDown();
    }

    std::vector<zes_perf_handle_t> getPerfHandles(uint32_t count) {
        std::vector<zes_perf_handle_t> handles(count, nullptr);
        EXPECT_EQ(zesDeviceEnumPerformanceFactorDomains(pSysmanDevice->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
        return handles;
    }
};

TEST_F(SysmanDevicePerformanceFixture, GivenValidSysmanHandleWhenRetrievingPerfThenValidHandlesReturned) {
    uint32_t count = 0;
    ze_result_t result = zesDeviceEnumPerformanceFactorDomains(pSysmanDevice->toHandle(), &count, NULL);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, mockHandleCount);

    uint32_t testcount = count + 1;
    result = zesDeviceEnumPerformanceFactorDomains(pSysmanDevice->toHandle(), &testcount, NULL);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(testcount, mockHandleCount);

    count = 0;
    std::vector<zes_perf_handle_t> handles(count, nullptr);
    EXPECT_EQ(zesDeviceEnumPerformanceFactorDomains(pSysmanDevice->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, mockHandleCount);
}

TEST_F(SysmanDevicePerformanceFixture, GivenValidSysmanHandleWhenRetrievingPerfAndRequestSingleFailsThenFailuresAreReturned) {
    pKmdSysManager->mockRequestSingle = true;
    pKmdSysManager->mockRequestSingleResult = ZE_RESULT_ERROR_UNKNOWN;
    uint32_t count = 0;
    ze_result_t result = zesDeviceEnumPerformanceFactorDomains(pSysmanDevice->toHandle(), &count, NULL);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, 0u);
}

TEST_F(SysmanDevicePerformanceFixture, GivenValidPerfHandleWhenGettingPerformancePropertiesThenValidPropertiesReturned) {
    auto handles = getPerfHandles(mockHandleCount);
    for (const auto &handle : handles) {
        zes_perf_properties_t properties = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPerformanceFactorGetProperties(handle, &properties));
        EXPECT_FALSE(properties.onSubdevice);
        EXPECT_EQ(properties.engines, ZES_ENGINE_TYPE_FLAG_MEDIA);
        EXPECT_EQ(properties.subdeviceId, 0u);
    }
}

TEST_F(SysmanDevicePerformanceFixture, GivenValidPerfHandleWhenSettingMediaConfigAndGettingMediaConfigThenSuccessIsReturned) {
    pKmdSysManager->allowSetCalls = true;
    auto handles = getPerfHandles(mockHandleCount);
    for (const auto &handle : handles) {
        zes_perf_properties_t properties = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPerformanceFactorGetProperties(handle, &properties));
        if (properties.engines == ZES_ENGINE_TYPE_FLAG_MEDIA) {
            double setFactor = 49;
            double getFactor = 0;
            EXPECT_EQ(ZE_RESULT_SUCCESS, zesPerformanceFactorSetConfig(handle, setFactor));
            EXPECT_EQ(ZE_RESULT_SUCCESS, zesPerformanceFactorGetConfig(handle, &getFactor));
            EXPECT_DOUBLE_EQ(std::round(getFactor), halfOfMaxPerformanceFactor);

            setFactor = 50;
            getFactor = 0;
            EXPECT_EQ(ZE_RESULT_SUCCESS, zesPerformanceFactorSetConfig(handle, setFactor));
            EXPECT_EQ(ZE_RESULT_SUCCESS, zesPerformanceFactorGetConfig(handle, &getFactor));
            EXPECT_DOUBLE_EQ(std::round(getFactor), halfOfMaxPerformanceFactor);

            setFactor = 60;
            EXPECT_EQ(ZE_RESULT_SUCCESS, zesPerformanceFactorSetConfig(handle, setFactor));
            EXPECT_EQ(ZE_RESULT_SUCCESS, zesPerformanceFactorGetConfig(handle, &getFactor));
            EXPECT_DOUBLE_EQ(std::round(getFactor), maxPerformanceFactor);

            setFactor = 100;
            EXPECT_EQ(ZE_RESULT_SUCCESS, zesPerformanceFactorSetConfig(handle, setFactor));
            EXPECT_EQ(ZE_RESULT_SUCCESS, zesPerformanceFactorGetConfig(handle, &getFactor));
            EXPECT_DOUBLE_EQ(std::round(getFactor), maxPerformanceFactor);

            setFactor = 0;
            EXPECT_EQ(ZE_RESULT_SUCCESS, zesPerformanceFactorSetConfig(handle, setFactor));
            EXPECT_EQ(ZE_RESULT_SUCCESS, zesPerformanceFactorGetConfig(handle, &getFactor));
            EXPECT_DOUBLE_EQ(std::round(getFactor), minPerformanceFactor);
        }
    }
}

TEST_F(SysmanDevicePerformanceFixture, GivenValidPerfHandlesWhenRetrievingPerformanceFactorAndKmdReturnsInvalidValueThenUnknownErrorIsReturned) {
    pKmdSysManager->mockPerformanceFactor = 512;
    auto handles = getPerfHandles(mockHandleCount);
    for (const auto &handle : handles) {
        double factor = 0;
        EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zesPerformanceFactorGetConfig(handle, &factor));
    }
}

TEST_F(SysmanDevicePerformanceFixture, GivenValidPerfHandleWhenRetrievingPerformanceFactorAndOsPerformanceGetConfigFailsThenFailureIsReturned) {
    pKmdSysManager->mockRequestSingle = true;
    pKmdSysManager->mockRequestSingleResult = ZE_RESULT_ERROR_UNKNOWN;
    PublicWddmPerformanceImp *pWddmPerformanceImp = new PublicWddmPerformanceImp(pOsSysman, 0, 0u, ZES_ENGINE_TYPE_FLAG_MEDIA);
    double pFactor = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, pWddmPerformanceImp->osPerformanceGetConfig(&pFactor));
    delete pWddmPerformanceImp;
}

TEST_F(SysmanDevicePerformanceFixture, GivenValidPerfHandleWhenRetrievingPerformanceFactorAndOsPerformanceSetConfigFailsThenFailureIsReturned) {
    pKmdSysManager->allowSetCalls = true;
    pKmdSysManager->mockRequestSingle = true;
    pKmdSysManager->mockRequestSingleResult = ZE_RESULT_ERROR_UNKNOWN;
    PublicWddmPerformanceImp *pWddmPerformanceImp = new PublicWddmPerformanceImp(pOsSysman, 0, 0u, ZES_ENGINE_TYPE_FLAG_MEDIA);
    double pFactor = halfOfMaxPerformanceFactor;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, pWddmPerformanceImp->osPerformanceSetConfig(pFactor));
    delete pWddmPerformanceImp;
}

TEST_F(SysmanDevicePerformanceFixture, GivenValidPerfHandleWhenSettingPerformanceFactorWithInvalidValuesThenInvalidArgumentErrorIsReturned) {
    pKmdSysManager->allowSetCalls = true;
    auto handles = getPerfHandles(mockHandleCount);
    for (const auto &handle : handles) {
        zes_perf_properties_t properties = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPerformanceFactorGetProperties(handle, &properties));
        if (properties.engines == ZES_ENGINE_TYPE_FLAG_MEDIA) {
            double setFactor = -1;
            EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zesPerformanceFactorSetConfig(handle, setFactor));

            setFactor = 110;
            EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zesPerformanceFactorSetConfig(handle, setFactor));
        }
    }
}

} // namespace ult
} // namespace Sysman
} // namespace L0