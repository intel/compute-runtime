/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_wddm.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/tools/test/unit_tests/sources/metrics/mock_metric_oa.h"

#include "gtest/gtest.h"

namespace L0 {
namespace ult {

using MetricEnumerationTestWddm = Test<MetricContextFixture>;

TEST_F(MetricEnumerationTestWddm, givenCorrectWddmAdapterWhenGetMetricsAdapterThenReturnSuccess) {

    auto &rootDevice = neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[device->getRootDeviceIndex()];
    auto &osInterface = rootDevice->osInterface;
    auto wddm = new WddmMock(*rootDevice);
    auto adapterGroupParams = TAdapterGroupParams_1_6{};
    auto adapterParams = TAdapterParams_1_9{};

    osInterface = std::make_unique<NEO::OSInterface>();
    osInterface->setDriverModel(std::unique_ptr<DriverModel>(wddm));

    adapterGroupParams.AdapterCount = 1;
    adapterParams.SystemId.Type = MetricsDiscovery::TAdapterIdType::ADAPTER_ID_TYPE_LUID;
    adapterParams.SystemId.Luid.HighPart = 0;
    adapterParams.SystemId.Luid.LowPart = 0;

    openMetricsAdapterGroup();

    setupDefaultMocksForMetricDevice(metricsDevice);

    adapterGroup.GetParamsResult = &adapterGroupParams;
    adapterGroup.GetAdapterResult = &adapter;
    adapter.GetParamsResult = &adapterParams;

    mockMetricEnumeration->getAdapterIdOutMajor = adapterParams.SystemId.Luid.HighPart;
    mockMetricEnumeration->getAdapterIdOutMinor = adapterParams.SystemId.Luid.LowPart;

    mockMetricEnumeration->getMetricsAdapterResult = &adapter;

    EXPECT_EQ(mockMetricEnumeration->openMetricsDiscovery(), ZE_RESULT_SUCCESS);
}
} // namespace ult
} // namespace L0
