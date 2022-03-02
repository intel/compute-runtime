/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/test/unit_tests/sources/sysman/linux/mock_sysman_fixture.h"

#include "mock_sysfs_performance.h"

extern bool sysmanUltsEnable;

using ::testing::_;
using ::testing::Matcher;

namespace L0 {
namespace ult {

constexpr uint32_t mockHandleCount = 0;
class ZesPerformanceFixture : public SysmanMultiDeviceFixture {
  protected:
    std::vector<ze_device_handle_t> deviceHandles;
    void SetUp() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        SysmanMultiDeviceFixture::SetUp();
        pSysmanDeviceImp->pPerformanceHandleContext->handleList.clear();
        uint32_t subDeviceCount = 0;
        Device::fromHandle(device->toHandle())->getSubDevices(&subDeviceCount, nullptr);
        if (subDeviceCount == 0) {
            deviceHandles.resize(1, device->toHandle());
        } else {
            deviceHandles.resize(subDeviceCount, nullptr);
            Device::fromHandle(device->toHandle())->getSubDevices(&subDeviceCount, deviceHandles.data());
        }
        pSysmanDeviceImp->pPerformanceHandleContext->init(deviceHandles, device);
    }
    void TearDown() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        SysmanMultiDeviceFixture::TearDown();
    }

    std::vector<zes_perf_handle_t> get_perf_handles(uint32_t count) {
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
    for (const auto &handle : deviceHandles) {
        Performance *pPerformance = new PerformanceImp(pOsSysman, handle, ZES_ENGINE_TYPE_FLAG_MEDIA);
        zes_perf_properties_t properties = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, pPerformance->performanceGetProperties(&properties));
        double factor = 0;
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pPerformance->performanceGetConfig(&factor));
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pPerformance->performanceSetConfig(factor));
        EXPECT_FALSE(static_cast<PerformanceImp *>(pPerformance)->pOsPerformance->isPerformanceSupported());
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE,
                  static_cast<PerformanceImp *>(pPerformance)->pOsPerformance->osPerformanceGetProperties(properties));
        zes_perf_handle_t perfHandle = pPerformance->toPerformanceHandle();
        EXPECT_EQ(pPerformance, Performance::fromHandle(perfHandle));
        delete pPerformance;
    }
}

TEST_F(ZesPerformanceFixture, GivenValidOfjectsOfClassPerformanceImpAndPerformanceHandleContextThenDuringObjectReleaseCheckDestructorBranches) {
    // Check destructors of PerformanceImp and PerformanceHandleContext
    std::unique_ptr<PerformanceHandleContext> pPerformanceHandleContext1 = std::make_unique<PerformanceHandleContext>(pOsSysman);
    for (const auto &deviceHandle : deviceHandles) {
        Performance *pPerformance1 = new PerformanceImp(pOsSysman, deviceHandle, ZES_ENGINE_TYPE_FLAG_MEDIA);
        pPerformanceHandleContext1->handleList.push_back(pPerformance1);
        Performance *pPerformance2 = new PerformanceImp(pOsSysman, deviceHandle, ZES_ENGINE_TYPE_FLAG_COMPUTE);
        pPerformanceHandleContext1->handleList.push_back(pPerformance2);
    }

    // Check branches of destructors of PerformanceImp and PerformanceHandleContext
    std::unique_ptr<PerformanceHandleContext> pPerformanceHandleContext2 = std::make_unique<PerformanceHandleContext>(pOsSysman);
    for (const auto &deviceHandle : deviceHandles) {
        Performance *pPerformance1 = new PerformanceImp(pOsSysman, deviceHandle, ZES_ENGINE_TYPE_FLAG_MEDIA);
        pPerformanceHandleContext2->handleList.push_back(pPerformance1);
        Performance *pPerformance2 = new PerformanceImp(pOsSysman, deviceHandle, ZES_ENGINE_TYPE_FLAG_COMPUTE);
        pPerformanceHandleContext2->handleList.push_back(pPerformance2);
    }

    for (auto &handle : pPerformanceHandleContext2->handleList) {
        auto pPerformanceImp = static_cast<PerformanceImp *>(handle);
        delete pPerformanceImp->pOsPerformance;
        pPerformanceImp->pOsPerformance = nullptr;
        delete handle;
        handle = nullptr;
    }
}

} // namespace ult
} // namespace L0
