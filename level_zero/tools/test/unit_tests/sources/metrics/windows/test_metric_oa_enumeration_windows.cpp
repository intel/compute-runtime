/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/mocks/mock_wddm.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/tools/test/unit_tests/sources/metrics/mock_metric_oa.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

using ::testing::_;
using ::testing::Return;

namespace L0 {
namespace ult {

using MetricEnumerationTestWindows = Test<MetricContextFixture>;

TEST_F(MetricEnumerationTestWindows, givenCorrectWindowsAdapterWhenGetMetricsAdapterThenReturnSuccess) {

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

    EXPECT_CALL(adapterGroup, GetParams())
        .Times(1)
        .WillOnce(Return(&adapterGroupParams));

    EXPECT_CALL(adapterGroup, GetAdapter(_))
        .WillRepeatedly(Return(&adapter));

    EXPECT_CALL(adapter, GetParams())
        .WillRepeatedly(Return(&adapterParams));

    EXPECT_CALL(*mockMetricEnumeration, getAdapterId(_, _))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgReferee<0>(adapterParams.SystemId.Luid.HighPart), ::testing::SetArgReferee<1>(adapterParams.SystemId.Luid.LowPart), Return(true)));

    EXPECT_CALL(*mockMetricEnumeration, getMetricsAdapter())
        .Times(1)
        .WillOnce([&]() { return mockMetricEnumeration->baseGetMetricsAdapter(); });

    EXPECT_EQ(mockMetricEnumeration->openMetricsDiscovery(), ZE_RESULT_SUCCESS);
}
} // namespace ult
} // namespace L0
