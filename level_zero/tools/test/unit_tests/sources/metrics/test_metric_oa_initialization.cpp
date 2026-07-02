/*
 * Copyright (C) 2022-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/os_library.h"

#include "level_zero/core/test/unit_tests/mocks/mock_driver.h"
#include "level_zero/tools/source/metrics/metric_oa_source.h"
#include "level_zero/tools/test/unit_tests/sources/metrics/mock_metric_oa.h"

#include "gtest/gtest.h"

namespace L0 {

namespace ult {

class MockOsLibrary : public NEO::OsLibrary {
  public:
    MockOsLibrary() {
    }

    void *getProcAddress(const std::string &procName) override {
        return nullptr;
    }

    bool isLoaded() override {
        return false;
    }

    std::string getFullPath() override {
        return std::string();
    }

    static OsLibrary *load(const OsLibraryCreateProperties &properties) {
        auto ptr = new (std::nothrow) MockOsLibrary();
        if (ptr == nullptr) {
            return nullptr;
        }
        return ptr;
    }
};

using MetricInitializationTest = Test<MetricContextFixture>;

TEST_F(MetricInitializationTest, GivenOaDependenciesAreAvailableThenMetricInitializationIsSuccess) {

    globalDriverHandles->push_back(driverHandle.get());
    VariableBackup<decltype(NEO::OsLibrary::loadFunc)> funcBackup{&NEO::OsLibrary::loadFunc, MockOsLibrary::load};

    openMetricsAdapter();
    setupDefaultMocksForMetricDevice(metricsDevice);

    EXPECT_EQ(device->getMetricDeviceContext().enableMetricApi(), ZE_RESULT_SUCCESS);
    globalDriverHandles->clear();
}

TEST_F(MetricInitializationTest, GivenMdapiAdapterCannotBeOpenedWhenLoadDependenciesIsCalledThenSourceIsNotAvailable) {

    mockMetricsLibrary->initializationState = ZE_RESULT_SUCCESS;

    auto &metricSource = device->getMetricDeviceContext().getMetricSource<OaMetricSourceImp>();
    EXPECT_EQ(metricSource.loadDependencies(), false);
    EXPECT_FALSE(metricSource.isAvailable());
}

TEST_F(MetricInitializationTest, GivenMdapiAdapterOpensSuccessfullyWhenLoadDependenciesIsCalledThenSourceIsAvailableAndAdapterIsClosed) {

    mockMetricsLibrary->initializationState = ZE_RESULT_SUCCESS;

    openMetricsAdapter();
    setupDefaultMocksForMetricDevice(metricsDevice);

    auto &metricSource = device->getMetricDeviceContext().getMetricSource<OaMetricSourceImp>();
    EXPECT_EQ(metricSource.loadDependencies(), true);
    EXPECT_TRUE(metricSource.isAvailable());

    EXPECT_NE(nullptr, mockMetricEnumeration->hMetricsDiscovery.get());
    EXPECT_NE(nullptr, mockMetricEnumeration->openAdapterGroup);
    EXPECT_EQ(nullptr, mockMetricEnumeration->getMdapiAdapterGroup());
    EXPECT_EQ(nullptr, mockMetricEnumeration->getMdapiAdapter());
    EXPECT_EQ(nullptr, mockMetricEnumeration->getMdapiDevice());
}

TEST_F(MetricInitializationTest, GivenSuccessfulAdapterProbeWhenRealMetricsDiscoveryOpenIsCalledLaterThenItStillSucceeds) {

    mockMetricsLibrary->initializationState = ZE_RESULT_SUCCESS;

    openMetricsAdapter();
    setupDefaultMocksForMetricDevice(metricsDevice);

    auto &metricSource = device->getMetricDeviceContext().getMetricSource<OaMetricSourceImp>();
    EXPECT_EQ(metricSource.loadDependencies(), true);

    EXPECT_EQ(mockMetricEnumeration->openMetricsDiscovery(), ZE_RESULT_SUCCESS);
    EXPECT_EQ(mockMetricEnumeration->cleanupMetricsDiscovery(), ZE_RESULT_SUCCESS);
}

} // namespace ult
} // namespace L0
