/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_wddm.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/tools/source/metrics/os_interface_metric.h"
#include "level_zero/tools/test/unit_tests/sources/metrics/mock_metric_oa.h"

#include "gtest/gtest.h"

namespace L0 {
namespace ult {

class MetricQueryPoolWindowsTest : public MetricContextFixture,
                                   public ::testing::Test {
  public:
    void SetUp() override {
        MetricContextFixture::setUp();
        neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[device->getRootDeviceIndex()]->osInterface = std::make_unique<NEO::OSInterface>();
        auto &osInterface = device->getOsInterface();
        wddm = new NEO::WddmMock(const_cast<NEO::RootDeviceEnvironment &>(neoDevice->getRootDeviceEnvironment()));
        wddm->init();
        osInterface.setDriverModel(std::unique_ptr<DriverModel>(wddm));
    }

    void TearDown() override {
        MetricContextFixture::tearDown();
    }

    NEO::WddmMock *wddm;
};

TEST_F(MetricQueryPoolWindowsTest, givenCorrectArgumentsWhenGetContextDataIsCalledThenReturnsSuccess) {

    ClientData_1_0 clientData = {};
    ContextCreateData_1_0 contextData = {};

    contextData.ClientData = &clientData;

    EXPECT_EQ(mockMetricsLibrary->metricsLibraryGetContextData(*device, contextData), true);

    auto &osInterface = device->getOsInterface();
    auto wddm = osInterface.getDriverModel()->as<NEO::Wddm>();

    EXPECT_EQ(contextData.ClientData->Windows.KmdInstrumentationEnabled, true);
    EXPECT_EQ(contextData.ClientData->Windows.Device, reinterpret_cast<void *>(static_cast<UINT_PTR>(wddm->getDeviceHandle())));
    EXPECT_EQ(contextData.ClientData->Windows.Escape, reinterpret_cast<void *>(wddm->getEscapeHandle()));
    EXPECT_EQ(contextData.ClientData->Windows.Adapter, reinterpret_cast<void *>(static_cast<UINT_PTR>(wddm->getAdapter())));
}

TEST_F(MetricQueryPoolWindowsTest, givenValidDeviceHandleWhenFlushCommandBufferCallbackIsCalledThenReturnsSuccess) {

    ClientData_1_0 clientData = {};
    ContextCreateData_1_0 contextData = {};

    contextData.ClientData = &clientData;

    EXPECT_EQ(mockMetricsLibrary->metricsLibraryGetContextData(*device, contextData), true);

    auto &callback = mockMetricsLibrary->metricsLibraryGetCallbacks().CommandBufferFlush;

    EXPECT_NE(callback, nullptr);
    EXPECT_EQ(callback(clientData.Handle), StatusCode::Success);
}

TEST_F(MetricQueryPoolWindowsTest, givenInvalidDeviceHandleWhenFlushCommandBufferCallbackIsCalledThenReturnsFailed) {

    ClientData_1_0 clientData = {};
    ContextCreateData_1_0 contextData = {};

    contextData.ClientData = &clientData;

    EXPECT_EQ(mockMetricsLibrary->metricsLibraryGetContextData(*device, contextData), true);

    auto &callback = mockMetricsLibrary->metricsLibraryGetCallbacks().CommandBufferFlush;
    clientData.Handle.data = nullptr;

    EXPECT_NE(callback, nullptr);
    EXPECT_EQ(callback(clientData.Handle), StatusCode::Failed);
}

} // namespace ult
} // namespace L0
