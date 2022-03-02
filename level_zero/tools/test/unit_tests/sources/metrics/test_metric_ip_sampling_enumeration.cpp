/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/tools/source/metrics/metric_ip_sampling_source.h"
#include "level_zero/tools/source/metrics/metric_oa_source.h"
#include "level_zero/tools/source/metrics/os_metric_ip_sampling.h"
#include "level_zero/tools/test/unit_tests/sources/metrics/metric_ip_sampling_fixture.h"
#include "level_zero/tools/test/unit_tests/sources/metrics/mock_metric_ip_sampling.h"
#include <level_zero/zet_api.h>

namespace L0 {
extern _ze_driver_handle_t *GlobalDriverHandle;

namespace ult {

using MetricIpSamplingEnumerationTest = MetricIpSamplingFixture;

TEST_F(MetricIpSamplingEnumerationTest, GivenDependenciesAvailableWhenInititializingThenSuccessIsReturned) {

    EXPECT_EQ(ZE_RESULT_SUCCESS, testDevices[0]->getMetricDeviceContext().enableMetricApi());
    for (auto device : testDevices) {
        auto &metricSource = device->getMetricDeviceContext().getMetricSource<IpSamplingMetricSourceImp>();
        EXPECT_TRUE(metricSource.isAvailable());
    }
}

TEST_F(MetricIpSamplingEnumerationTest, GivenDependenciesUnAvailableForRootDeviceWhenInititializingThenFailureIsReturned) {

    osInterfaceVector[0]->isDependencyAvailableReturn = false;
    EXPECT_EQ(ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE, testDevices[0]->getMetricDeviceContext().enableMetricApi());
}

TEST_F(MetricIpSamplingEnumerationTest, GivenDependenciesUnAvailableForSubDeviceWhenInititializingThenFailureIsReturned) {

    osInterfaceVector[1]->isDependencyAvailableReturn = false;
    EXPECT_EQ(ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE, testDevices[0]->getMetricDeviceContext().enableMetricApi());

    auto &metricSource = testDevices[0]->getMetricDeviceContext().getMetricSource<IpSamplingMetricSourceImp>();
    EXPECT_TRUE(metricSource.isAvailable());

    auto &metricSource0 = testDevices[1]->getMetricDeviceContext().getMetricSource<IpSamplingMetricSourceImp>();
    EXPECT_FALSE(metricSource0.isAvailable());

    auto &metricSource1 = testDevices[2]->getMetricDeviceContext().getMetricSource<IpSamplingMetricSourceImp>();
    EXPECT_TRUE(metricSource1.isAvailable());
}

TEST_F(MetricIpSamplingEnumerationTest, GivenDependenciesAvailableWhenMetricGroupGetIsCalledThenValidMetricGroupIsReturned) {

    EXPECT_EQ(ZE_RESULT_SUCCESS, testDevices[0]->getMetricDeviceContext().enableMetricApi());
    for (auto device : testDevices) {

        uint32_t metricGroupCount = 0;
        EXPECT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, nullptr), ZE_RESULT_SUCCESS);
        EXPECT_EQ(metricGroupCount, 1u);

        std::vector<zet_metric_group_handle_t> metricGroups;
        metricGroups.resize(metricGroupCount);

        EXPECT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, metricGroups.data()), ZE_RESULT_SUCCESS);
        EXPECT_NE(metricGroups[0], nullptr);
    }
}

TEST_F(MetricIpSamplingEnumerationTest, GivenDependenciesAvailableWhenMetricGroupGetIsCalledMultipleTimesThenValidMetricGroupIsReturned) {

    EXPECT_EQ(ZE_RESULT_SUCCESS, testDevices[0]->getMetricDeviceContext().enableMetricApi());
    for (auto device : testDevices) {

        uint32_t metricGroupCount = 0;
        EXPECT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, nullptr), ZE_RESULT_SUCCESS);
        EXPECT_EQ(metricGroupCount, 1u);

        std::vector<zet_metric_group_handle_t> metricGroups;
        metricGroups.resize(metricGroupCount);

        EXPECT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, metricGroups.data()), ZE_RESULT_SUCCESS);
        EXPECT_NE(metricGroups[0], nullptr);
        EXPECT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, metricGroups.data()), ZE_RESULT_SUCCESS);
        EXPECT_NE(metricGroups[0], nullptr);
    }
}

TEST_F(MetricIpSamplingEnumerationTest, GivenDependenciesAvailableWhenMetricGroupGetIsCalledThenMetricGroupWithCorrectPropertiesIsReturned) {

    EXPECT_EQ(ZE_RESULT_SUCCESS, testDevices[0]->getMetricDeviceContext().enableMetricApi());
    for (auto device : testDevices) {

        uint32_t metricGroupCount = 0;
        zetMetricGroupGet(device->toHandle(), &metricGroupCount, nullptr);
        EXPECT_EQ(metricGroupCount, 1u);

        std::vector<zet_metric_group_handle_t> metricGroups;
        metricGroups.resize(metricGroupCount);

        ASSERT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, metricGroups.data()), ZE_RESULT_SUCCESS);
        ASSERT_NE(metricGroups[0], nullptr);

        zet_metric_group_properties_t metricGroupProperties;
        EXPECT_EQ(zetMetricGroupGetProperties(metricGroups[0], &metricGroupProperties), ZE_RESULT_SUCCESS);
        EXPECT_EQ(metricGroupProperties.domain, 100u);
        EXPECT_EQ(metricGroupProperties.samplingType, ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_TIME_BASED);
        EXPECT_EQ(metricGroupProperties.metricCount, 10u);
        EXPECT_EQ(strcmp(metricGroupProperties.description, "EU stall sampling"), 0);
        EXPECT_EQ(strcmp(metricGroupProperties.name, "EuStallSampling"), 0);
    }
}

TEST_F(MetricIpSamplingEnumerationTest, GivenDependenciesAvailableWhenMetricGroupGetIsCalledThenCorrectMetricsAreReturned) {

    struct MetricProperties {
        const char *name;
        const char *description;
        const char *component;
        uint32_t tierNumber;
        zet_metric_type_t metricType;
        zet_value_type_t resultType;
        const char *resultUnits;
    };

    std::vector<struct MetricProperties> expectedProperties = {
        {"IP", "IP address", "XVE", 4, ZET_METRIC_TYPE_IP_EXP, ZET_VALUE_TYPE_UINT64, "Address"},
        {"Active", "Active cycles", "XVE", 4, ZET_METRIC_TYPE_EVENT, ZET_VALUE_TYPE_UINT64, "Events"},
        {"ControlStall", "Stall on control", "XVE", 4, ZET_METRIC_TYPE_EVENT, ZET_VALUE_TYPE_UINT64, "Events"},
        {"PipeStall", "Stall on pipe", "XVE", 4, ZET_METRIC_TYPE_EVENT, ZET_VALUE_TYPE_UINT64, "Events"},
        {"SendStall", "Stall on send", "XVE", 4, ZET_METRIC_TYPE_EVENT, ZET_VALUE_TYPE_UINT64, "Events"},
        {"DistStall", "Stall on distance", "XVE", 4, ZET_METRIC_TYPE_EVENT, ZET_VALUE_TYPE_UINT64, "Events"},
        {"SbidStall", "Stall on scoreboard", "XVE", 4, ZET_METRIC_TYPE_EVENT, ZET_VALUE_TYPE_UINT64, "Events"},
        {"SyncStall", "Stall on sync", "XVE", 4, ZET_METRIC_TYPE_EVENT, ZET_VALUE_TYPE_UINT64, "Events"},
        {"InstrFetchStall", "Stall on instruction fetch", "XVE", 4, ZET_METRIC_TYPE_EVENT, ZET_VALUE_TYPE_UINT64, "Events"},
        {"OtherStall", "Stall on other condition", "XVE", 4, ZET_METRIC_TYPE_EVENT, ZET_VALUE_TYPE_UINT64, "Events"},
    };

    EXPECT_EQ(ZE_RESULT_SUCCESS, testDevices[0]->getMetricDeviceContext().enableMetricApi());
    for (auto device : testDevices) {

        uint32_t metricGroupCount = 0;
        zetMetricGroupGet(device->toHandle(), &metricGroupCount, nullptr);
        EXPECT_EQ(metricGroupCount, 1u);
        std::vector<zet_metric_group_handle_t> metricGroups;
        metricGroups.resize(metricGroupCount);
        zetMetricGroupGet(device->toHandle(), &metricGroupCount, metricGroups.data());
        ASSERT_NE(metricGroups[0], nullptr);

        zet_metric_group_properties_t metricGroupProperties;
        zetMetricGroupGetProperties(metricGroups[0], &metricGroupProperties);

        uint32_t metricCount = 0;
        std::vector<zet_metric_handle_t> metricHandles = {};
        metricHandles.resize(metricGroupProperties.metricCount);
        EXPECT_EQ(zetMetricGet(metricGroups[0], &metricCount, nullptr), ZE_RESULT_SUCCESS);
        EXPECT_EQ(metricCount, metricGroupProperties.metricCount);

        EXPECT_EQ(zetMetricGet(metricGroups[0], &metricCount, metricHandles.data()), ZE_RESULT_SUCCESS);
        std::vector<struct MetricProperties>::iterator propertiesIter = expectedProperties.begin();

        zet_metric_properties_t ipSamplingMetricProperties = {};
        for (auto &metricHandle : metricHandles) {
            EXPECT_EQ(zetMetricGetProperties(metricHandle, &ipSamplingMetricProperties), ZE_RESULT_SUCCESS);
            EXPECT_EQ(strcmp(ipSamplingMetricProperties.name, propertiesIter->name), 0);
            EXPECT_EQ(strcmp(ipSamplingMetricProperties.description, propertiesIter->description), 0);
            EXPECT_EQ(strcmp(ipSamplingMetricProperties.component, propertiesIter->component), 0);
            EXPECT_EQ(ipSamplingMetricProperties.tierNumber, propertiesIter->tierNumber);
            EXPECT_EQ(ipSamplingMetricProperties.metricType, propertiesIter->metricType);
            EXPECT_EQ(ipSamplingMetricProperties.resultType, propertiesIter->resultType);
            EXPECT_EQ(strcmp(ipSamplingMetricProperties.resultUnits, propertiesIter->resultUnits), 0);
            propertiesIter++;
        }
    }
}

TEST_F(MetricIpSamplingEnumerationTest, GivenEnumerationIsSuccessfulThenDummyActivationAndDeActivationHappens) {

    EXPECT_EQ(ZE_RESULT_SUCCESS, testDevices[0]->getMetricDeviceContext().enableMetricApi());
    for (auto device : testDevices) {

        uint32_t metricGroupCount = 0;
        zetMetricGroupGet(device->toHandle(), &metricGroupCount, nullptr);
        std::vector<zet_metric_group_handle_t> metricGroups;
        metricGroups.resize(metricGroupCount);
        ASSERT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, metricGroups.data()), ZE_RESULT_SUCCESS);
        ASSERT_NE(metricGroups[0], nullptr);
        zet_metric_group_properties_t metricGroupProperties;
        EXPECT_EQ(zetMetricGroupGetProperties(metricGroups[0], &metricGroupProperties), ZE_RESULT_SUCCESS);
        EXPECT_EQ(strcmp(metricGroupProperties.name, "EuStallSampling"), 0);

        EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), device->toHandle(), 1, &metricGroups[0]), ZE_RESULT_SUCCESS);
        static_cast<DeviceImp *>(device)->activateMetricGroups();
        EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), device->toHandle(), 0, nullptr), ZE_RESULT_SUCCESS);
    }
}

TEST_F(MetricIpSamplingEnumerationTest, GivenEnumerationIsSuccessfulWhenCalculateMultipleMetricValuesExpIsCalledThenUnsupportedFeatureIsReturned) {

    EXPECT_EQ(ZE_RESULT_SUCCESS, testDevices[0]->getMetricDeviceContext().enableMetricApi());
    for (auto device : testDevices) {

        uint32_t metricGroupCount = 0;
        zetMetricGroupGet(device->toHandle(), &metricGroupCount, nullptr);
        std::vector<zet_metric_group_handle_t> metricGroups;
        metricGroups.resize(metricGroupCount);
        ASSERT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, metricGroups.data()), ZE_RESULT_SUCCESS);
        ASSERT_NE(metricGroups[0], nullptr);
        zet_metric_group_properties_t metricGroupProperties;
        EXPECT_EQ(zetMetricGroupGetProperties(metricGroups[0], &metricGroupProperties), ZE_RESULT_SUCCESS);
        EXPECT_EQ(strcmp(metricGroupProperties.name, "EuStallSampling"), 0);

        EXPECT_EQ(zetMetricGroupCalculateMultipleMetricValuesExp(metricGroups[0],
                                                                 ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, 0x0, nullptr,
                                                                 nullptr, nullptr, nullptr, nullptr),
                  ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
    }
}

TEST_F(MetricIpSamplingEnumerationTest, GivenEnumerationIsSuccessfulWhenCalculateMetricValuesIsCalledThenUnsupportedFeatureIsReturned) {

    EXPECT_EQ(ZE_RESULT_SUCCESS, testDevices[0]->getMetricDeviceContext().enableMetricApi());
    for (auto device : testDevices) {

        uint32_t metricGroupCount = 0;
        zetMetricGroupGet(device->toHandle(), &metricGroupCount, nullptr);
        std::vector<zet_metric_group_handle_t> metricGroups;
        metricGroups.resize(metricGroupCount);
        ASSERT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, metricGroups.data()), ZE_RESULT_SUCCESS);
        ASSERT_NE(metricGroups[0], nullptr);
        zet_metric_group_properties_t metricGroupProperties;
        EXPECT_EQ(zetMetricGroupGetProperties(metricGroups[0], &metricGroupProperties), ZE_RESULT_SUCCESS);
        EXPECT_EQ(strcmp(metricGroupProperties.name, "EuStallSampling"), 0);

        EXPECT_EQ(zetMetricGroupCalculateMetricValues(metricGroups[0], ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES,
                                                      0, nullptr, nullptr, nullptr),
                  ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
    }
}

TEST_F(MetricIpSamplingEnumerationTest, GivenEnumerationIsSuccessfulWhenQueryPoolCreateIsCalledThenUnsupportedFeatureIsReturned) {

    EXPECT_EQ(ZE_RESULT_SUCCESS, testDevices[0]->getMetricDeviceContext().enableMetricApi());
    for (auto device : testDevices) {

        uint32_t metricGroupCount = 0;
        zetMetricGroupGet(device->toHandle(), &metricGroupCount, nullptr);
        std::vector<zet_metric_group_handle_t> metricGroups;
        metricGroups.resize(metricGroupCount);
        ASSERT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, metricGroups.data()), ZE_RESULT_SUCCESS);
        ASSERT_NE(metricGroups[0], nullptr);
        zet_metric_group_properties_t metricGroupProperties;
        EXPECT_EQ(zetMetricGroupGetProperties(metricGroups[0], &metricGroupProperties), ZE_RESULT_SUCCESS);
        EXPECT_EQ(strcmp(metricGroupProperties.name, "EuStallSampling"), 0);

        zet_metric_query_pool_desc_t poolDesc = {};
        poolDesc.stype = ZET_STRUCTURE_TYPE_METRIC_QUERY_POOL_DESC;
        poolDesc.count = 1;
        poolDesc.type = ZET_METRIC_QUERY_POOL_TYPE_PERFORMANCE;
        EXPECT_EQ(zetMetricQueryPoolCreate(context->toHandle(), device, metricGroups[0], &poolDesc, nullptr), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
    }
}

TEST_F(MetricIpSamplingEnumerationTest, GivenEnumerationIsSuccessfulWhenAppendMetricMemoryBarrierIsCalledThenUnsupportedFeatureIsReturned) {

    EXPECT_EQ(ZE_RESULT_SUCCESS, testDevices[0]->getMetricDeviceContext().enableMetricApi());
    auto &device = testDevices[0];
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    zet_command_list_handle_t commandListHandle = commandList->toHandle();
    EXPECT_EQ(zetCommandListAppendMetricMemoryBarrier(commandListHandle), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
}

} // namespace ult
} // namespace L0
