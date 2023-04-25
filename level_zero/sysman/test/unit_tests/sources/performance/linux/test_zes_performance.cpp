/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"
#include "level_zero/sysman/test/unit_tests/sources/performance/linux/mock_sysfs_performance.h"

namespace L0 {
namespace ult {

constexpr uint32_t mockHandleCount = 5;
class ZesPerformanceFixture : public SysmanMultiDeviceFixture {
  protected:
    L0::Sysman::SysmanDevice *device = nullptr;
    void SetUp() override {
        SysmanMultiDeviceFixture::SetUp();
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
    uint32_t count = 0;
    ze_result_t result = zesDeviceEnumPerformanceFactorDomains(device->toHandle(), &count, NULL);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, mockHandleCount);

    uint32_t testcount = count + 1;
    result = zesDeviceEnumPerformanceFactorDomains(device->toHandle(), &testcount, NULL);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(testcount, mockHandleCount);

    count = 0;
    std::vector<zes_perf_handle_t> handles(count, nullptr);
    EXPECT_EQ(zesDeviceEnumPerformanceFactorDomains(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, mockHandleCount);
}

TEST_F(ZesPerformanceFixture, GivenValidOsSysmanPointerWhenCreatingOsPerformanceThenValidhandleForOsPerformanceIsRetrieved) {

    auto subDeviceCount = pLinuxSysmanImp->getSubDeviceCount();
    uint32_t subdeviceId = 0;

    for (subdeviceId = 0; subdeviceId < subDeviceCount; subdeviceId++) {
        ze_bool_t onSubdevice = (subDeviceCount == 0) ? false : true;
        L0::Sysman::Performance *pPerformance = new L0::Sysman::PerformanceImp(pOsSysman, onSubdevice, subdeviceId, ZES_ENGINE_TYPE_FLAG_MEDIA);
        zes_perf_properties_t properties = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, pPerformance->performanceGetProperties(&properties));
        double factor = 0;
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pPerformance->performanceGetConfig(&factor));
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pPerformance->performanceSetConfig(factor));
        EXPECT_FALSE(static_cast<L0::Sysman::PerformanceImp *>(pPerformance)->pOsPerformance->isPerformanceSupported());
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE,
                  static_cast<L0::Sysman::PerformanceImp *>(pPerformance)->pOsPerformance->osPerformanceGetProperties(properties));
        zes_perf_handle_t perfHandle = pPerformance->toPerformanceHandle();
        EXPECT_EQ(pPerformance, L0::Sysman::Performance::fromHandle(perfHandle));
        delete pPerformance;
    }
}

} // namespace ult
} // namespace L0