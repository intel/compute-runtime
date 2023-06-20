/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/performance/linux/sysman_os_performance_imp.h"
#include "level_zero/sysman/source/performance/sysman_performance.h"
#include "level_zero/sysman/source/performance/sysman_performance_imp.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"

namespace L0 {
namespace Sysman {
namespace ult {

constexpr uint32_t mockHandleCount = 5;
class ZesPerformanceFixture : public SysmanMultiDeviceFixture {
  protected:
    L0::Sysman::SysmanDevice *device = nullptr;
    void SetUp() override {
        SysmanMultiDeviceFixture::SetUp();
        device = pSysmanDevice;
        pSysmanDeviceImp->pPerformanceHandleContext->handleList.clear();
        getPerfHandles(0);
    }
    void TearDown() override {
        SysmanMultiDeviceFixture::TearDown();
    }
    std::vector<zes_perf_handle_t> getPerfHandles(uint32_t count) {
        std::vector<zes_perf_handle_t> handles(count, nullptr);
        EXPECT_EQ(zesDeviceEnumPerformanceFactorDomains(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
        return handles;
    }
};

TEST_F(ZesPerformanceFixture, GivenValidSysmanHandleWhenRetrievingPerfThenZeroHandlesInReturn) {
    uint32_t handleCount = 0;
    uint32_t count = 0;
    ze_result_t result = zesDeviceEnumPerformanceFactorDomains(device->toHandle(), &count, NULL);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, handleCount);

    uint32_t testcount = count + 1;
    result = zesDeviceEnumPerformanceFactorDomains(device->toHandle(), &testcount, NULL);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(testcount, handleCount);

    count = 0;
    std::vector<zes_perf_handle_t> handles(count, nullptr);
    EXPECT_EQ(zesDeviceEnumPerformanceFactorDomains(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, handleCount);
}

TEST_F(ZesPerformanceFixture, GivenValidOsSysmanPointerWhenCreatingOsPerformanceAndCallingPerformancePropertiesThenErrorIsReturned) {
    uint32_t handleId = 0;
    for (handleId = 0; handleId < mockHandleCount; handleId++) {
        auto pPerformance = std::make_unique<L0::Sysman::PerformanceImp>(pOsSysman, true, handleId, ZES_ENGINE_TYPE_FLAG_MEDIA);
        zes_perf_properties_t properties = {};
        EXPECT_FALSE(pPerformance->pOsPerformance->isPerformanceSupported());
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pPerformance->pOsPerformance->osPerformanceGetProperties(properties));
        zes_perf_handle_t perfHandle = pPerformance->toPerformanceHandle();
        EXPECT_EQ(pPerformance.get(), L0::Sysman::Performance::fromHandle(perfHandle));
    }
}

TEST_F(ZesPerformanceFixture, GivenValidOsSysmanPointerWhenCreatingOsPerformanceAndCallingPerformanceGetAndSetConfigThenErrorIsReturned) {
    uint32_t handleId = 0;
    for (handleId = 0; handleId < mockHandleCount; handleId++) {
        auto pPerformance = std::make_unique<L0::Sysman::PerformanceImp>(pOsSysman, true, handleId, ZES_ENGINE_TYPE_FLAG_MEDIA);
        double factor = 0;
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pPerformance->performanceGetConfig(&factor));
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pPerformance->performanceSetConfig(factor));
        zes_perf_handle_t perfHandle = pPerformance->toPerformanceHandle();
        EXPECT_EQ(pPerformance.get(), L0::Sysman::Performance::fromHandle(perfHandle));
    }
}

} // namespace ult
} // namespace Sysman
} // namespace L0