/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/test_base.h"

#include "level_zero/core/source/cmdlist/cmdlist.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/include/zet_intel_gpu_metric.h"
#include "level_zero/tools/source/metrics/metric_ip_sampling_source.h"
#include "level_zero/tools/source/metrics/metric_oa_source.h"
#include "level_zero/tools/source/metrics/os_interface_metric.h"
#include "level_zero/tools/test/unit_tests/sources/metrics/metric_ip_sampling_fixture.h"
#include "level_zero/tools/test/unit_tests/sources/metrics/mock_metric_ip_sampling.h"
#include <level_zero/zet_api.h>

namespace L0 {
extern _ze_driver_handle_t *globalDriverHandle;

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

        zet_metric_group_properties_t metricGroupProperties = {ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES, nullptr};
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
        {"IP", "IP address", "XVE", 4, ZET_METRIC_TYPE_IP, ZET_VALUE_TYPE_UINT64, "Address"},
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

        zet_metric_group_properties_t metricGroupProperties = {ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES, nullptr};
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
        zet_metric_group_properties_t metricGroupProperties = {ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES, nullptr};
        EXPECT_EQ(zetMetricGroupGetProperties(metricGroups[0], &metricGroupProperties), ZE_RESULT_SUCCESS);
        EXPECT_EQ(strcmp(metricGroupProperties.name, "EuStallSampling"), 0);

        EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), device->toHandle(), 1, &metricGroups[0]), ZE_RESULT_SUCCESS);
        static_cast<DeviceImp *>(device)->activateMetricGroups();
        EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), device->toHandle(), 0, nullptr), ZE_RESULT_SUCCESS);
    }
}

TEST_F(MetricIpSamplingEnumerationTest, GivenEnumerationIsSuccessfulWhenReadingMetricsFrequencyAndValidBitsThenConfirmAreTheSameAsDevice) {

    EXPECT_EQ(ZE_RESULT_SUCCESS, testDevices[0]->getMetricDeviceContext().enableMetricApi());

    for (auto device : testDevices) {

        ze_device_properties_t deviceProps = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES_1_2, nullptr};
        device->getProperties(&deviceProps);

        uint32_t metricGroupCount = 0;
        zetMetricGroupGet(device->toHandle(), &metricGroupCount, nullptr);
        EXPECT_EQ(metricGroupCount, 1u);

        std::vector<zet_metric_group_handle_t> metricGroups;
        metricGroups.resize(metricGroupCount);

        ASSERT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, metricGroups.data()), ZE_RESULT_SUCCESS);
        ASSERT_NE(metricGroups[0], nullptr);

        zet_metric_global_timestamps_resolution_exp_t metricTimestampProperties = {ZET_STRUCTURE_TYPE_METRIC_GLOBAL_TIMESTAMPS_RESOLUTION_EXP, nullptr};

        zet_metric_group_properties_t metricGroupProperties = {ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES, &metricTimestampProperties};
        EXPECT_EQ(zetMetricGroupGetProperties(metricGroups[0], &metricGroupProperties), ZE_RESULT_SUCCESS);
        EXPECT_EQ(strcmp(metricGroupProperties.description, "EU stall sampling"), 0);
        EXPECT_EQ(strcmp(metricGroupProperties.name, "EuStallSampling"), 0);
        EXPECT_EQ(metricTimestampProperties.timerResolution, deviceProps.timerResolution);
        EXPECT_EQ(metricTimestampProperties.timestampValidBits, deviceProps.timestampValidBits);
    }
}

TEST_F(MetricIpSamplingEnumerationTest, GivenEnumerationIsSuccessfulOnMulitDeviceWhenReadingMetricsTimestampThenResultIsSuccess) {

    EXPECT_EQ(ZE_RESULT_SUCCESS, testDevices[0]->getMetricDeviceContext().enableMetricApi());

    for (auto device : testDevices) {

        ze_device_properties_t deviceProps = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES_1_2, nullptr};
        device->getProperties(&deviceProps);

        uint32_t metricGroupCount = 0;
        zetMetricGroupGet(device->toHandle(), &metricGroupCount, nullptr);
        EXPECT_EQ(metricGroupCount, 1u);

        std::vector<zet_metric_group_handle_t> metricGroups;
        metricGroups.resize(metricGroupCount);

        ASSERT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, metricGroups.data()), ZE_RESULT_SUCCESS);
        ASSERT_NE(metricGroups[0], nullptr);
        ze_bool_t synchronizedWithHost = true;
        uint64_t globalTimestamp = 0;
        uint64_t metricTimestamp = 0;

        EXPECT_EQ(L0::zetMetricGroupGetGlobalTimestampsExp(metricGroups[0], synchronizedWithHost, &globalTimestamp, &metricTimestamp), ZE_RESULT_SUCCESS);
    }
}

using MetricIpSamplingTimestampTest = MetricIpSamplingTimestampFixture;

TEST_F(MetricIpSamplingTimestampTest, GivenEnumerationIsSuccessfulWhenReadingMetricsFrequencyThenValuesAreUpdated) {

    EXPECT_EQ(ZE_RESULT_SUCCESS, device->getMetricDeviceContext().enableMetricApi());

    ze_device_properties_t deviceProps = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES_1_2, nullptr};
    device->getProperties(&deviceProps);

    uint32_t metricGroupCount = 0;
    zetMetricGroupGet(device->toHandle(), &metricGroupCount, nullptr);
    EXPECT_EQ(metricGroupCount, 1u);

    std::vector<zet_metric_group_handle_t> metricGroups;
    metricGroups.resize(metricGroupCount);

    ASSERT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, metricGroups.data()), ZE_RESULT_SUCCESS);
    ASSERT_NE(metricGroups[0], nullptr);

    ze_bool_t synchronizedWithHost = true;
    uint64_t globalTimestamp = 0;
    uint64_t metricTimestamp = 0;

    EXPECT_EQ(L0::zetMetricGroupGetGlobalTimestampsExp(metricGroups[0], synchronizedWithHost, &globalTimestamp, &metricTimestamp), ZE_RESULT_SUCCESS);
    EXPECT_NE(globalTimestamp, 0UL);
    EXPECT_NE(metricTimestamp, 0UL);

    synchronizedWithHost = false;
    globalTimestamp = 0;
    metricTimestamp = 0;

    EXPECT_EQ(L0::zetMetricGroupGetGlobalTimestampsExp(metricGroups[0], synchronizedWithHost, &globalTimestamp, &metricTimestamp), ZE_RESULT_SUCCESS);
    EXPECT_NE(globalTimestamp, 0UL);
    EXPECT_NE(metricTimestamp, 0UL);

    debugManager.flags.EnableImplicitScaling.set(1);
    globalTimestamp = 0;
    metricTimestamp = 0;

    EXPECT_EQ(L0::zetMetricGroupGetGlobalTimestampsExp(metricGroups[0], synchronizedWithHost, &globalTimestamp, &metricTimestamp), ZE_RESULT_SUCCESS);
    EXPECT_NE(globalTimestamp, 0UL);
    EXPECT_NE(metricTimestamp, 0UL);
}

TEST_F(MetricIpSamplingTimestampTest, GivenGetGpuCpuTimeIsFalseWhenReadingMetricsFrequencyThenValuesAreZero) {

    EXPECT_EQ(ZE_RESULT_SUCCESS, device->getMetricDeviceContext().enableMetricApi());

    neoDevice->setOSTime(new FalseGpuCpuTime());

    ze_device_properties_t deviceProps = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES_1_2, nullptr};
    device->getProperties(&deviceProps);

    uint32_t metricGroupCount = 0;
    zetMetricGroupGet(device->toHandle(), &metricGroupCount, nullptr);
    EXPECT_EQ(metricGroupCount, 1u);

    std::vector<zet_metric_group_handle_t> metricGroups;
    metricGroups.resize(metricGroupCount);

    ASSERT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, metricGroups.data()), ZE_RESULT_SUCCESS);
    ASSERT_NE(metricGroups[0], nullptr);

    ze_bool_t synchronizedWithHost = true;
    uint64_t globalTimestamp = 1;
    uint64_t metricTimestamp = 1;

    EXPECT_EQ(L0::zetMetricGroupGetGlobalTimestampsExp(metricGroups[0], synchronizedWithHost, &globalTimestamp, &metricTimestamp), ZE_RESULT_ERROR_DEVICE_LOST);
    EXPECT_EQ(globalTimestamp, 0UL);
    EXPECT_EQ(metricTimestamp, 0UL);
}

using MetricIpSamplingCalculateMetricsTest = MetricIpSamplingCalculateMetricsFixture;

TEST_F(MetricIpSamplingCalculateMetricsTest, GivenEnumerationIsSuccessfulWhenCalculateMultipleMetricValuesExpIsCalledThenValidDataIsReturned) {
    EXPECT_EQ(ZE_RESULT_SUCCESS, testDevices[0]->getMetricDeviceContext().enableMetricApi());

    std::vector<zet_typed_value_t> metricValues(30);

    for (auto device : testDevices) {

        auto expectedSetCount = 2u;
        ze_device_properties_t props = {};
        device->getProperties(&props);

        if (props.flags & ZE_DEVICE_PROPERTY_FLAG_SUBDEVICE) {
            expectedSetCount = 1u;
        }

        uint32_t metricGroupCount = 0;
        zetMetricGroupGet(device->toHandle(), &metricGroupCount, nullptr);
        std::vector<zet_metric_group_handle_t> metricGroups;
        metricGroups.resize(metricGroupCount);
        ASSERT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, metricGroups.data()), ZE_RESULT_SUCCESS);
        ASSERT_NE(metricGroups[0], nullptr);
        zet_metric_group_properties_t metricGroupProperties = {ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES, nullptr};
        EXPECT_EQ(zetMetricGroupGetProperties(metricGroups[0], &metricGroupProperties), ZE_RESULT_SUCCESS);
        EXPECT_EQ(strcmp(metricGroupProperties.name, "EuStallSampling"), 0);

        std::vector<uint8_t> rawDataWithHeader(rawDataVectorSize + sizeof(IpSamplingMetricDataHeader));
        addHeader(rawDataWithHeader.data(), rawDataWithHeader.size(), reinterpret_cast<uint8_t *>(rawDataVector.data()), rawDataVectorSize, 0);

        uint32_t setCount = 0;
        // Use random initializer
        uint32_t totalMetricValueCount = std::numeric_limits<uint32_t>::max();
        std::vector<uint32_t> metricCounts(2);
        EXPECT_EQ(L0::zetMetricGroupCalculateMultipleMetricValuesExp(metricGroups[0],
                                                                     ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, rawDataWithHeader.size(), reinterpret_cast<uint8_t *>(rawDataWithHeader.data()),
                                                                     &setCount, &totalMetricValueCount, metricCounts.data(), nullptr),
                  ZE_RESULT_SUCCESS);
        EXPECT_EQ(setCount, expectedSetCount);
        EXPECT_EQ(totalMetricValueCount, 40u);
        EXPECT_EQ(L0::zetMetricGroupCalculateMultipleMetricValuesExp(metricGroups[0],
                                                                     ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, rawDataWithHeader.size(), reinterpret_cast<uint8_t *>(rawDataWithHeader.data()),
                                                                     &setCount, &totalMetricValueCount, metricCounts.data(), metricValues.data()),
                  ZE_RESULT_SUCCESS);
        EXPECT_EQ(setCount, expectedSetCount);
        EXPECT_EQ(totalMetricValueCount, 20u);
        EXPECT_EQ(metricCounts[0], 20u);
        for (uint32_t i = 0; i < totalMetricValueCount; i++) {
            EXPECT_TRUE(expectedMetricValues[i].type == metricValues[i].type);
            EXPECT_TRUE(expectedMetricValues[i].value.ui64 == metricValues[i].value.ui64);
        }
    }
}

TEST_F(MetricIpSamplingCalculateMetricsTest, GivenEnumerationIsSuccessfulWhenCalculateMultipleMetricValuesExpIsCalledWithInvalidHeaderThenErrorIsReturned) {
    EXPECT_EQ(ZE_RESULT_SUCCESS, testDevices[0]->getMetricDeviceContext().enableMetricApi());

    std::vector<zet_typed_value_t> metricValues(30);

    for (auto device : testDevices) {

        ze_device_properties_t props = {};
        device->getProperties(&props);

        if ((props.flags & ZE_DEVICE_PROPERTY_FLAG_SUBDEVICE) == 0) {

            uint32_t metricGroupCount = 0;
            zetMetricGroupGet(device->toHandle(), &metricGroupCount, nullptr);
            std::vector<zet_metric_group_handle_t> metricGroups;
            metricGroups.resize(metricGroupCount);
            ASSERT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, metricGroups.data()), ZE_RESULT_SUCCESS);
            ASSERT_NE(metricGroups[0], nullptr);
            zet_metric_group_properties_t metricGroupProperties = {ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES, nullptr};
            EXPECT_EQ(zetMetricGroupGetProperties(metricGroups[0], &metricGroupProperties), ZE_RESULT_SUCCESS);
            EXPECT_EQ(strcmp(metricGroupProperties.name, "EuStallSampling"), 0);

            std::vector<uint8_t> rawDataWithHeader(rawDataVectorSize + sizeof(IpSamplingMetricDataHeader));
            addHeader(rawDataWithHeader.data(), rawDataWithHeader.size(), reinterpret_cast<uint8_t *>(rawDataVector.data()), rawDataVectorSize, 0);
            auto header = reinterpret_cast<IpSamplingMetricDataHeader *>(rawDataWithHeader.data());
            header->magic = IpSamplingMetricDataHeader::magicValue - 1;

            uint32_t setCount = 0;
            // Use random initializer
            uint32_t totalMetricValueCount = std::numeric_limits<uint32_t>::max();
            std::vector<uint32_t> metricCounts(2);
            EXPECT_EQ(L0::zetMetricGroupCalculateMultipleMetricValuesExp(metricGroups[0],
                                                                         ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, rawDataWithHeader.size(), reinterpret_cast<uint8_t *>(rawDataWithHeader.data()),
                                                                         &setCount, &totalMetricValueCount, metricCounts.data(), nullptr),
                      ZE_RESULT_ERROR_INVALID_SIZE);
        }
    }
}

TEST_F(MetricIpSamplingCalculateMetricsTest, GivenEnumerationIsSuccessfulWhenCalculateMultipleMetricValuesExpIsCalledWithDataFromSingleDeviceThenValidDataIsReturned) {
    EXPECT_EQ(ZE_RESULT_SUCCESS, testDevices[0]->getMetricDeviceContext().enableMetricApi());

    std::vector<zet_typed_value_t> metricValues(30);

    for (auto device : testDevices) {

        auto expectedSetCount = 2u;
        ze_device_properties_t props = {};
        device->getProperties(&props);

        if (props.flags & ZE_DEVICE_PROPERTY_FLAG_SUBDEVICE) {
            expectedSetCount = 1u;

            uint32_t metricGroupCount = 0;
            zetMetricGroupGet(device->toHandle(), &metricGroupCount, nullptr);
            std::vector<zet_metric_group_handle_t> metricGroups;
            metricGroups.resize(metricGroupCount);
            ASSERT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, metricGroups.data()), ZE_RESULT_SUCCESS);
            ASSERT_NE(metricGroups[0], nullptr);
            zet_metric_group_properties_t metricGroupProperties = {ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES, nullptr};
            EXPECT_EQ(zetMetricGroupGetProperties(metricGroups[0], &metricGroupProperties), ZE_RESULT_SUCCESS);
            EXPECT_EQ(strcmp(metricGroupProperties.name, "EuStallSampling"), 0);

            uint32_t setCount = 0;
            // Use random initializer
            uint32_t totalMetricValueCount = std::numeric_limits<uint32_t>::max();
            std::vector<uint32_t> metricCounts(1);
            EXPECT_EQ(L0::zetMetricGroupCalculateMultipleMetricValuesExp(metricGroups[0],
                                                                         ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, rawDataVectorSize, reinterpret_cast<uint8_t *>(rawDataVector.data()),
                                                                         &setCount, &totalMetricValueCount, metricCounts.data(), nullptr),
                      ZE_RESULT_SUCCESS);
            EXPECT_EQ(setCount, expectedSetCount);
            EXPECT_EQ(totalMetricValueCount, 40u);
            EXPECT_EQ(L0::zetMetricGroupCalculateMultipleMetricValuesExp(metricGroups[0],
                                                                         ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, rawDataVectorSize, reinterpret_cast<uint8_t *>(rawDataVector.data()),
                                                                         &setCount, &totalMetricValueCount, metricCounts.data(), metricValues.data()),
                      ZE_RESULT_SUCCESS);
            EXPECT_EQ(setCount, expectedSetCount);
            EXPECT_EQ(totalMetricValueCount, 20u);
            EXPECT_EQ(metricCounts[0], 20u);
            for (uint32_t i = 0; i < totalMetricValueCount; i++) {
                EXPECT_TRUE(expectedMetricValues[i].type == metricValues[i].type);
                EXPECT_TRUE(expectedMetricValues[i].value.ui64 == metricValues[i].value.ui64);
            }
        }
    }
}

TEST_F(MetricIpSamplingCalculateMetricsTest, GivenEnumerationIsSuccessfulWhenCalculateMultipleMetricValuesExpIsCalledWithDataFromSingleDeviceAndInvalidRawDataThenErrorIsReturned) {
    EXPECT_EQ(ZE_RESULT_SUCCESS, testDevices[0]->getMetricDeviceContext().enableMetricApi());

    std::vector<zet_typed_value_t> metricValues(30);

    for (auto device : testDevices) {
        ze_device_properties_t props = {};
        device->getProperties(&props);
        if (props.flags & ZE_DEVICE_PROPERTY_FLAG_SUBDEVICE) {
            uint32_t metricGroupCount = 0;
            zetMetricGroupGet(device->toHandle(), &metricGroupCount, nullptr);
            std::vector<zet_metric_group_handle_t> metricGroups;
            metricGroups.resize(metricGroupCount);
            ASSERT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, metricGroups.data()), ZE_RESULT_SUCCESS);
            ASSERT_NE(metricGroups[0], nullptr);
            zet_metric_group_properties_t metricGroupProperties = {ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES, nullptr};
            EXPECT_EQ(zetMetricGroupGetProperties(metricGroups[0], &metricGroupProperties), ZE_RESULT_SUCCESS);
            EXPECT_EQ(strcmp(metricGroupProperties.name, "EuStallSampling"), 0);

            uint32_t setCount = 0;
            uint32_t totalMetricValueCount = std::numeric_limits<uint32_t>::max();
            std::vector<uint32_t> metricCounts(1);
            EXPECT_EQ(L0::zetMetricGroupCalculateMultipleMetricValuesExp(metricGroups[0],
                                                                         ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, 1,
                                                                         reinterpret_cast<uint8_t *>(rawDataVector.data()),
                                                                         &setCount, &totalMetricValueCount, metricCounts.data(), nullptr),
                      ZE_RESULT_ERROR_INVALID_SIZE);
        }
    }
}

TEST_F(MetricIpSamplingCalculateMetricsTest, GivenEnumerationIsSuccessfulWhenCalculateMultipleMetricValuesExpIsCalledWithLessThanRequiredMetricCountThenValidDataIsReturned) {
    EXPECT_EQ(ZE_RESULT_SUCCESS, testDevices[0]->getMetricDeviceContext().enableMetricApi());

    std::vector<zet_typed_value_t> metricValues(30);

    for (auto device : testDevices) {

        auto expectedSetCount = 2u;
        ze_device_properties_t props = {};
        device->getProperties(&props);

        if (props.flags & ZE_DEVICE_PROPERTY_FLAG_SUBDEVICE) {
            expectedSetCount = 1u;
        }

        uint32_t metricGroupCount = 0;
        zetMetricGroupGet(device->toHandle(), &metricGroupCount, nullptr);
        std::vector<zet_metric_group_handle_t> metricGroups;
        metricGroups.resize(metricGroupCount);
        ASSERT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, metricGroups.data()), ZE_RESULT_SUCCESS);
        ASSERT_NE(metricGroups[0], nullptr);
        zet_metric_group_properties_t metricGroupProperties = {ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES, nullptr};
        EXPECT_EQ(zetMetricGroupGetProperties(metricGroups[0], &metricGroupProperties), ZE_RESULT_SUCCESS);
        EXPECT_EQ(strcmp(metricGroupProperties.name, "EuStallSampling"), 0);

        // Allocate for 2 reads
        std::vector<uint8_t> rawDataWithHeader((rawDataVectorSize + sizeof(IpSamplingMetricDataHeader)) * 2);
        addHeader(rawDataWithHeader.data(), rawDataWithHeader.size(), reinterpret_cast<uint8_t *>(rawDataVector.data()), rawDataVectorSize, 0);
        addHeader(rawDataWithHeader.data() + rawDataVectorSize + sizeof(IpSamplingMetricDataHeader),
                  rawDataWithHeader.size(), reinterpret_cast<uint8_t *>(rawDataVector.data()), rawDataVectorSize, 0);

        // Use random initializer
        uint32_t setCount = std::numeric_limits<uint32_t>::max();
        uint32_t totalMetricValueCount = 0;
        std::vector<uint32_t> metricCounts(2);
        EXPECT_EQ(L0::zetMetricGroupCalculateMultipleMetricValuesExp(metricGroups[0],
                                                                     ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, rawDataWithHeader.size(),
                                                                     reinterpret_cast<uint8_t *>(rawDataWithHeader.data()),
                                                                     &setCount, &totalMetricValueCount, metricCounts.data(), nullptr),
                  ZE_RESULT_SUCCESS);
        EXPECT_EQ(setCount, expectedSetCount);
        EXPECT_EQ(totalMetricValueCount, 80u);
        totalMetricValueCount = 10;
        EXPECT_EQ(L0::zetMetricGroupCalculateMultipleMetricValuesExp(metricGroups[0],
                                                                     ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, rawDataWithHeader.size(),
                                                                     reinterpret_cast<uint8_t *>(rawDataWithHeader.data()),
                                                                     &setCount, &totalMetricValueCount, metricCounts.data(), metricValues.data()),
                  ZE_RESULT_SUCCESS);
        EXPECT_EQ(setCount, expectedSetCount);
        EXPECT_EQ(totalMetricValueCount, 10u);
        EXPECT_EQ(metricCounts[0], 10u);
        for (uint32_t i = 0; i < totalMetricValueCount; i++) {
            EXPECT_TRUE(expectedMetricValues[i].type == metricValues[i].type);
            EXPECT_TRUE(expectedMetricValues[i].value.ui64 == metricValues[i].value.ui64);
        }
    }
}

TEST_F(MetricIpSamplingCalculateMetricsTest, GivenEnumerationIsSuccessfulWhenCalculateMultipleMetricValuesExpIsCalledWithInvalidRawDataSizeThenErrorIsReturned) {

    EXPECT_EQ(ZE_RESULT_SUCCESS, testDevices[0]->getMetricDeviceContext().enableMetricApi());

    for (auto device : testDevices) {

        uint32_t metricGroupCount = 0;
        zetMetricGroupGet(device->toHandle(), &metricGroupCount, nullptr);
        std::vector<zet_metric_group_handle_t> metricGroups;
        metricGroups.resize(metricGroupCount);
        ASSERT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, metricGroups.data()), ZE_RESULT_SUCCESS);
        ASSERT_NE(metricGroups[0], nullptr);
        zet_metric_group_properties_t metricGroupProperties = {ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES, nullptr};
        EXPECT_EQ(zetMetricGroupGetProperties(metricGroups[0], &metricGroupProperties), ZE_RESULT_SUCCESS);
        EXPECT_EQ(strcmp(metricGroupProperties.name, "EuStallSampling"), 0);

        std::vector<uint8_t> rawDataWithHeader(rawDataVectorSize + sizeof(IpSamplingMetricDataHeader) + 1);
        addHeader(rawDataWithHeader.data(), rawDataWithHeader.size(), reinterpret_cast<uint8_t *>(rawDataVector.data()), rawDataVectorSize, 0);

        uint32_t setCount = 0;
        uint32_t totalMetricValueCount = 0;
        std::vector<uint32_t> metricCounts(2);
        EXPECT_EQ(L0::zetMetricGroupCalculateMultipleMetricValuesExp(metricGroups[0],
                                                                     ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, rawDataWithHeader.size() + 1, reinterpret_cast<uint8_t *>(rawDataWithHeader.data()),
                                                                     &setCount, &totalMetricValueCount, metricCounts.data(), nullptr),
                  ZE_RESULT_ERROR_INVALID_SIZE);
    }
}

TEST_F(MetricIpSamplingCalculateMetricsTest, WhenCalculateMultipleMetricValuesExpIsCalledWithInvalidRawDataSizeDuringValueCalculationPhaseThenErrorIsReturned) {

    EXPECT_EQ(ZE_RESULT_SUCCESS, testDevices[0]->getMetricDeviceContext().enableMetricApi());
    std::vector<zet_typed_value_t> metricValues(30);

    for (auto device : testDevices) {

        uint32_t metricGroupCount = 0;
        zetMetricGroupGet(device->toHandle(), &metricGroupCount, nullptr);
        std::vector<zet_metric_group_handle_t> metricGroups;
        metricGroups.resize(metricGroupCount);
        ASSERT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, metricGroups.data()), ZE_RESULT_SUCCESS);
        ASSERT_NE(metricGroups[0], nullptr);
        zet_metric_group_properties_t metricGroupProperties = {ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES, nullptr};
        EXPECT_EQ(zetMetricGroupGetProperties(metricGroups[0], &metricGroupProperties), ZE_RESULT_SUCCESS);
        EXPECT_EQ(strcmp(metricGroupProperties.name, "EuStallSampling"), 0);

        std::vector<uint8_t> rawDataWithHeader(rawDataVectorSize + sizeof(IpSamplingMetricDataHeader));
        addHeader(rawDataWithHeader.data(), rawDataWithHeader.size(), reinterpret_cast<uint8_t *>(rawDataVector.data()), rawDataVectorSize, 0);

        uint32_t setCount = 0;
        uint32_t totalMetricValueCount = 0;
        std::vector<uint32_t> metricCounts(2);
        EXPECT_EQ(L0::zetMetricGroupCalculateMultipleMetricValuesExp(metricGroups[0],
                                                                     ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, rawDataWithHeader.size(), reinterpret_cast<uint8_t *>(rawDataWithHeader.data()),
                                                                     &setCount, &totalMetricValueCount, metricCounts.data(), nullptr),
                  ZE_RESULT_SUCCESS);
        // Incorrect raw data size
        auto header = reinterpret_cast<IpSamplingMetricDataHeader *>(rawDataWithHeader.data());
        header->rawDataSize = static_cast<uint32_t>(rawDataVectorSize - 1);
        EXPECT_EQ(L0::zetMetricGroupCalculateMultipleMetricValuesExp(metricGroups[0],
                                                                     ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, rawDataWithHeader.size(), reinterpret_cast<uint8_t *>(rawDataWithHeader.data()),
                                                                     &setCount, &totalMetricValueCount, metricCounts.data(), metricValues.data()),
                  ZE_RESULT_ERROR_INVALID_SIZE);
    }
}

TEST_F(MetricIpSamplingCalculateMetricsTest, WhenCalculateMultipleMetricValuesExpCalculateSizeIsCalledWithInvalidRawDataSizeInHeaderDuringSizeCalculationThenErrorIsReturned) {

    EXPECT_EQ(ZE_RESULT_SUCCESS, testDevices[0]->getMetricDeviceContext().enableMetricApi());

    for (auto device : testDevices) {

        uint32_t metricGroupCount = 0;
        zetMetricGroupGet(device->toHandle(), &metricGroupCount, nullptr);
        std::vector<zet_metric_group_handle_t> metricGroups;
        metricGroups.resize(metricGroupCount);
        ASSERT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, metricGroups.data()), ZE_RESULT_SUCCESS);
        ASSERT_NE(metricGroups[0], nullptr);
        zet_metric_group_properties_t metricGroupProperties = {ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES, nullptr};
        EXPECT_EQ(zetMetricGroupGetProperties(metricGroups[0], &metricGroupProperties), ZE_RESULT_SUCCESS);
        EXPECT_EQ(strcmp(metricGroupProperties.name, "EuStallSampling"), 0);

        std::vector<uint8_t> rawDataWithHeader(rawDataVectorSize + sizeof(IpSamplingMetricDataHeader) + 1);
        addHeader(rawDataWithHeader.data(), rawDataWithHeader.size(), reinterpret_cast<uint8_t *>(rawDataVector.data()), rawDataVectorSize - 1, 0);

        uint32_t setCount = 0;
        uint32_t totalMetricValueCount = 0;
        std::vector<uint32_t> metricCounts(2);
        EXPECT_EQ(L0::zetMetricGroupCalculateMultipleMetricValuesExp(metricGroups[0],
                                                                     ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, rawDataWithHeader.size(), reinterpret_cast<uint8_t *>(rawDataWithHeader.data()),
                                                                     &setCount, &totalMetricValueCount, metricCounts.data(), nullptr),
                  ZE_RESULT_ERROR_INVALID_SIZE);
    }
}

TEST_F(MetricIpSamplingCalculateMetricsTest, WhenCalculateMultipleMetricValuesExpCalculateSizeIsCalledWithInvalidRawDataSizeInHeaderThenErrorIsReturned) {

    EXPECT_EQ(ZE_RESULT_SUCCESS, testDevices[0]->getMetricDeviceContext().enableMetricApi());

    for (auto device : testDevices) {

        uint32_t metricGroupCount = 0;
        zetMetricGroupGet(device->toHandle(), &metricGroupCount, nullptr);
        std::vector<zet_metric_group_handle_t> metricGroups;
        metricGroups.resize(metricGroupCount);
        ASSERT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, metricGroups.data()), ZE_RESULT_SUCCESS);
        ASSERT_NE(metricGroups[0], nullptr);
        zet_metric_group_properties_t metricGroupProperties = {ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES, nullptr};
        EXPECT_EQ(zetMetricGroupGetProperties(metricGroups[0], &metricGroupProperties), ZE_RESULT_SUCCESS);
        EXPECT_EQ(strcmp(metricGroupProperties.name, "EuStallSampling"), 0);

        std::vector<uint8_t> rawDataWithHeader(rawDataVectorSize + sizeof(IpSamplingMetricDataHeader) + 1);
        addHeader(rawDataWithHeader.data(), rawDataWithHeader.size(), reinterpret_cast<uint8_t *>(rawDataVector.data()), rawDataVectorSize - 1, 0);

        uint32_t setCount = 0;
        uint32_t totalMetricValueCount = 10;
        std::vector<uint32_t> metricCounts(2);
        std::vector<zet_typed_value_t> metricValues(30);
        EXPECT_EQ(L0::zetMetricGroupCalculateMultipleMetricValuesExp(metricGroups[0],
                                                                     ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, rawDataWithHeader.size(), reinterpret_cast<uint8_t *>(rawDataWithHeader.data()),
                                                                     &setCount, &totalMetricValueCount, metricCounts.data(), metricValues.data()),
                  ZE_RESULT_ERROR_INVALID_SIZE);
    }
}

TEST_F(MetricIpSamplingCalculateMetricsTest, GivenEnumerationIsSuccessfulWhenCalculateMultipleMetricValuesExpCalculateDataWithBadRawDataSizeIsCalledThenErrorUnknownIsReturned) {

    EXPECT_EQ(ZE_RESULT_SUCCESS, testDevices[0]->getMetricDeviceContext().enableMetricApi());

    std::vector<zet_typed_value_t> metricValues(30);

    for (auto device : testDevices) {

        auto expectedSetCount = 2u;
        ze_device_properties_t props = {};
        device->getProperties(&props);

        if (props.flags & ZE_DEVICE_PROPERTY_FLAG_SUBDEVICE) {
            expectedSetCount = 1u;
        }

        uint32_t metricGroupCount = 0;
        zetMetricGroupGet(device->toHandle(), &metricGroupCount, nullptr);
        std::vector<zet_metric_group_handle_t> metricGroups;
        metricGroups.resize(metricGroupCount);
        ASSERT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, metricGroups.data()), ZE_RESULT_SUCCESS);
        ASSERT_NE(metricGroups[0], nullptr);
        zet_metric_group_properties_t metricGroupProperties = {ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES, nullptr};
        EXPECT_EQ(zetMetricGroupGetProperties(metricGroups[0], &metricGroupProperties), ZE_RESULT_SUCCESS);
        EXPECT_EQ(strcmp(metricGroupProperties.name, "EuStallSampling"), 0);

        std::vector<uint8_t> rawDataWithHeader(rawDataVectorSize + sizeof(IpSamplingMetricDataHeader));
        addHeader(rawDataWithHeader.data(), rawDataWithHeader.size(), reinterpret_cast<uint8_t *>(rawDataVector.data()), rawDataVectorSize, 0);

        uint32_t setCount = 0;
        uint32_t totalMetricValueCount = 0;
        std::vector<uint32_t> metricCounts(2);
        EXPECT_EQ(L0::zetMetricGroupCalculateMultipleMetricValuesExp(metricGroups[0],
                                                                     ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, rawDataWithHeader.size(), reinterpret_cast<uint8_t *>(rawDataWithHeader.data()),
                                                                     &setCount, &totalMetricValueCount, metricCounts.data(), nullptr),
                  ZE_RESULT_SUCCESS);
        EXPECT_EQ(setCount, expectedSetCount);
        EXPECT_EQ(totalMetricValueCount, 40u);
        EXPECT_EQ(L0::zetMetricGroupCalculateMultipleMetricValuesExp(metricGroups[0],
                                                                     ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, rawDataWithHeader.size() + 1, reinterpret_cast<uint8_t *>(rawDataWithHeader.data()),
                                                                     &setCount, &totalMetricValueCount, metricCounts.data(), metricValues.data()),
                  ZE_RESULT_ERROR_INVALID_SIZE);
    }
}

TEST_F(MetricIpSamplingCalculateMetricsTest, GivenEnumerationIsSuccessfulWhenCalculateMetricValuesIsCalledThenValidDataIsReturned) {

    EXPECT_EQ(ZE_RESULT_SUCCESS, testDevices[0]->getMetricDeviceContext().enableMetricApi());

    std::vector<zet_typed_value_t> metricValues(30);

    for (auto device : testDevices) {

        uint32_t metricGroupCount = 0;
        zetMetricGroupGet(device->toHandle(), &metricGroupCount, nullptr);
        std::vector<zet_metric_group_handle_t> metricGroups;
        metricGroups.resize(metricGroupCount);
        ASSERT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, metricGroups.data()), ZE_RESULT_SUCCESS);
        ASSERT_NE(metricGroups[0], nullptr);
        zet_metric_group_properties_t metricGroupProperties = {ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES, nullptr};
        EXPECT_EQ(zetMetricGroupGetProperties(metricGroups[0], &metricGroupProperties), ZE_RESULT_SUCCESS);
        EXPECT_EQ(strcmp(metricGroupProperties.name, "EuStallSampling"), 0);

        uint32_t metricValueCount = 0;
        EXPECT_EQ(zetMetricGroupCalculateMetricValues(metricGroups[0], ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES,
                                                      rawDataVectorSize, reinterpret_cast<uint8_t *>(rawDataVector.data()), &metricValueCount, nullptr),
                  ZE_RESULT_SUCCESS);
        EXPECT_TRUE(metricValueCount == 40);
        EXPECT_EQ(zetMetricGroupCalculateMetricValues(metricGroups[0], ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES,
                                                      rawDataVectorSize, reinterpret_cast<uint8_t *>(rawDataVector.data()), &metricValueCount, metricValues.data()),
                  ZE_RESULT_SUCCESS);
        EXPECT_TRUE(metricValueCount == 20);
        for (uint32_t i = 0; i < metricValueCount; i++) {
            EXPECT_TRUE(expectedMetricValues[i].type == metricValues[i].type);
            EXPECT_TRUE(expectedMetricValues[i].value.ui64 == metricValues[i].value.ui64);
        }
    }
}

TEST_F(MetricIpSamplingCalculateMetricsTest, GivenEnumerationIsSuccessfulWhenCalculateMetricValuesIsCalledWithDataFromMultipleSubdevicesThenReturnError) {

    EXPECT_EQ(ZE_RESULT_SUCCESS, testDevices[0]->getMetricDeviceContext().enableMetricApi());

    std::vector<zet_typed_value_t> metricValues(30);

    for (auto device : testDevices) {

        uint32_t metricGroupCount = 0;
        zetMetricGroupGet(device->toHandle(), &metricGroupCount, nullptr);
        std::vector<zet_metric_group_handle_t> metricGroups;
        metricGroups.resize(metricGroupCount);
        ASSERT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, metricGroups.data()), ZE_RESULT_SUCCESS);
        ASSERT_NE(metricGroups[0], nullptr);
        zet_metric_group_properties_t metricGroupProperties = {ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES, nullptr};
        EXPECT_EQ(zetMetricGroupGetProperties(metricGroups[0], &metricGroupProperties), ZE_RESULT_SUCCESS);
        EXPECT_EQ(strcmp(metricGroupProperties.name, "EuStallSampling"), 0);

        std::vector<uint8_t> rawDataWithHeader(rawDataVectorSize + sizeof(IpSamplingMetricDataHeader));
        addHeader(rawDataWithHeader.data(), rawDataWithHeader.size(), reinterpret_cast<uint8_t *>(rawDataVector.data()), rawDataVectorSize, 0);
        uint32_t metricValueCount = 0;
        EXPECT_EQ(zetMetricGroupCalculateMetricValues(metricGroups[0], ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES,
                                                      rawDataWithHeader.size(), reinterpret_cast<uint8_t *>(rawDataWithHeader.data()),
                                                      &metricValueCount, metricValues.data()),
                  ZE_RESULT_ERROR_INVALID_ARGUMENT);
    }
}

TEST_F(MetricIpSamplingCalculateMetricsTest, GivenEnumerationIsSuccessfulWhenCalculateMetricValuesIsCalledWithSmallValueCountThenValidDataIsReturned) {

    EXPECT_EQ(ZE_RESULT_SUCCESS, testDevices[0]->getMetricDeviceContext().enableMetricApi());

    std::vector<zet_typed_value_t> metricValues(30);

    for (auto device : testDevices) {

        uint32_t metricGroupCount = 0;
        zetMetricGroupGet(device->toHandle(), &metricGroupCount, nullptr);
        std::vector<zet_metric_group_handle_t> metricGroups;
        metricGroups.resize(metricGroupCount);
        ASSERT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, metricGroups.data()), ZE_RESULT_SUCCESS);
        ASSERT_NE(metricGroups[0], nullptr);
        zet_metric_group_properties_t metricGroupProperties = {ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES, nullptr};
        EXPECT_EQ(zetMetricGroupGetProperties(metricGroups[0], &metricGroupProperties), ZE_RESULT_SUCCESS);
        EXPECT_EQ(strcmp(metricGroupProperties.name, "EuStallSampling"), 0);

        uint32_t metricValueCount = 0;
        EXPECT_EQ(zetMetricGroupCalculateMetricValues(metricGroups[0], ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES,
                                                      rawDataVectorSize, reinterpret_cast<uint8_t *>(rawDataVector.data()), &metricValueCount, nullptr),
                  ZE_RESULT_SUCCESS);
        EXPECT_TRUE(metricValueCount == 40);
        metricValueCount = 15;
        EXPECT_EQ(zetMetricGroupCalculateMetricValues(metricGroups[0], ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES,
                                                      rawDataVectorSize, reinterpret_cast<uint8_t *>(rawDataVector.data()), &metricValueCount, metricValues.data()),
                  ZE_RESULT_SUCCESS);
        EXPECT_TRUE(metricValueCount == 15);
        for (uint32_t i = 0; i < metricValueCount; i++) {
            EXPECT_TRUE(expectedMetricValues[i].type == metricValues[i].type);
            EXPECT_TRUE(expectedMetricValues[i].value.ui64 == metricValues[i].value.ui64);
        }
    }
}

TEST_F(MetricIpSamplingCalculateMetricsTest, GivenEnumerationIsSuccessfulWithBadRawDataSizeWhenCalculateMetricValuesCalculateSizeIsCalledThenErrorUnknownIsReturned) {

    EXPECT_EQ(ZE_RESULT_SUCCESS, testDevices[0]->getMetricDeviceContext().enableMetricApi());

    for (auto device : testDevices) {

        uint32_t metricGroupCount = 0;
        zetMetricGroupGet(device->toHandle(), &metricGroupCount, nullptr);
        std::vector<zet_metric_group_handle_t> metricGroups;
        metricGroups.resize(metricGroupCount);
        ASSERT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, metricGroups.data()), ZE_RESULT_SUCCESS);
        ASSERT_NE(metricGroups[0], nullptr);
        zet_metric_group_properties_t metricGroupProperties = {ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES, nullptr};
        EXPECT_EQ(zetMetricGroupGetProperties(metricGroups[0], &metricGroupProperties), ZE_RESULT_SUCCESS);
        EXPECT_EQ(strcmp(metricGroupProperties.name, "EuStallSampling"), 0);

        uint32_t metricValueCount = 0;
        EXPECT_EQ(zetMetricGroupCalculateMetricValues(metricGroups[0], ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES,
                                                      rawDataVectorSize - 1, reinterpret_cast<uint8_t *>(rawDataVector.data()), &metricValueCount, nullptr),
                  ZE_RESULT_ERROR_INVALID_SIZE);
    }
}

TEST_F(MetricIpSamplingCalculateMetricsTest, GivenEnumerationIsSuccessfulWhenCalculateMetricValuesWithBadRawDataSizeCalculateDataIsCalledThenUnsupportedFeatureIsReturned) {

    EXPECT_EQ(ZE_RESULT_SUCCESS, testDevices[0]->getMetricDeviceContext().enableMetricApi());

    std::vector<zet_typed_value_t> metricValues(30);

    for (auto device : testDevices) {

        uint32_t metricGroupCount = 0;
        zetMetricGroupGet(device->toHandle(), &metricGroupCount, nullptr);
        std::vector<zet_metric_group_handle_t> metricGroups;
        metricGroups.resize(metricGroupCount);
        ASSERT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, metricGroups.data()), ZE_RESULT_SUCCESS);
        ASSERT_NE(metricGroups[0], nullptr);
        zet_metric_group_properties_t metricGroupProperties = {ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES, nullptr};
        EXPECT_EQ(zetMetricGroupGetProperties(metricGroups[0], &metricGroupProperties), ZE_RESULT_SUCCESS);
        EXPECT_EQ(strcmp(metricGroupProperties.name, "EuStallSampling"), 0);

        uint32_t metricValueCount = 0;
        EXPECT_EQ(zetMetricGroupCalculateMetricValues(metricGroups[0], ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES,
                                                      rawDataVectorSize, reinterpret_cast<uint8_t *>(rawDataVector.data()), &metricValueCount, nullptr),
                  ZE_RESULT_SUCCESS);
        EXPECT_TRUE(metricValueCount == 40);
        EXPECT_EQ(zetMetricGroupCalculateMetricValues(metricGroups[0], ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES,
                                                      rawDataVectorSize - 1, reinterpret_cast<uint8_t *>(rawDataVector.data()), &metricValueCount, metricValues.data()),
                  ZE_RESULT_ERROR_INVALID_SIZE);
    }
}

TEST_F(MetricIpSamplingCalculateMetricsTest, GivenDataOverflowOccurredWhenStreamerReadDataIscalledThenCalculateMultipleMetricsValulesExpReturnsOverflowWarning) {
    EXPECT_EQ(ZE_RESULT_SUCCESS, testDevices[0]->getMetricDeviceContext().enableMetricApi());

    std::vector<zet_typed_value_t> metricValues(30);

    for (auto device : testDevices) {

        auto expectedSetCount = 2u;
        ze_device_properties_t props = {};
        device->getProperties(&props);

        if (props.flags & ZE_DEVICE_PROPERTY_FLAG_SUBDEVICE) {
            expectedSetCount = 1u;
        }
        uint32_t metricGroupCount = 0;
        zetMetricGroupGet(device->toHandle(), &metricGroupCount, nullptr);
        std::vector<zet_metric_group_handle_t> metricGroups;
        metricGroups.resize(metricGroupCount);
        ASSERT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, metricGroups.data()), ZE_RESULT_SUCCESS);
        ASSERT_NE(metricGroups[0], nullptr);
        zet_metric_group_properties_t metricGroupProperties = {ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES, nullptr};
        EXPECT_EQ(zetMetricGroupGetProperties(metricGroups[0], &metricGroupProperties), ZE_RESULT_SUCCESS);
        EXPECT_EQ(strcmp(metricGroupProperties.name, "EuStallSampling"), 0);

        std::vector<uint8_t> rawDataWithHeader(rawDataVectorOverflowSize + sizeof(IpSamplingMetricDataHeader));
        addHeader(rawDataWithHeader.data(), rawDataWithHeader.size(), reinterpret_cast<uint8_t *>(rawDataVectorOverflow.data()), rawDataVectorOverflowSize, 0);

        uint32_t setCount = 0;
        uint32_t totalMetricValueCount = 0;
        std::vector<uint32_t> metricCounts(2);
        EXPECT_EQ(L0::zetMetricGroupCalculateMultipleMetricValuesExp(metricGroups[0],
                                                                     ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, rawDataWithHeader.size(), reinterpret_cast<uint8_t *>(rawDataWithHeader.data()),
                                                                     &setCount, &totalMetricValueCount, metricCounts.data(), nullptr),
                  ZE_RESULT_SUCCESS);
        EXPECT_EQ(setCount, expectedSetCount);
        EXPECT_EQ(totalMetricValueCount, 40u);
        EXPECT_EQ(L0::zetMetricGroupCalculateMultipleMetricValuesExp(metricGroups[0],
                                                                     ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, rawDataWithHeader.size(), reinterpret_cast<uint8_t *>(rawDataWithHeader.data()),
                                                                     &setCount, &totalMetricValueCount, metricCounts.data(), metricValues.data()),
                  ZE_RESULT_WARNING_DROPPED_DATA);
        EXPECT_EQ(setCount, expectedSetCount);
        EXPECT_EQ(totalMetricValueCount, 20u);
        EXPECT_TRUE(metricCounts[0] == 20);
        for (uint32_t i = 0; i < totalMetricValueCount; i++) {
            EXPECT_TRUE(expectedMetricOverflowValues[i].type == metricValues[i].type);
            EXPECT_TRUE(expectedMetricOverflowValues[i].value.ui64 == metricValues[i].value.ui64);
        }
    }
}

TEST_F(MetricIpSamplingCalculateMetricsTest, GivenEnumerationIsSuccessfulWithCALCULATIONTYPEMAXWhenCalculateMetricValuesIsCalledThenErrorUnknownIsReturned) {

    EXPECT_EQ(ZE_RESULT_SUCCESS, testDevices[0]->getMetricDeviceContext().enableMetricApi());

    std::vector<zet_typed_value_t> metricValues(30);

    for (auto device : testDevices) {

        uint32_t metricGroupCount = 0;
        zetMetricGroupGet(device->toHandle(), &metricGroupCount, nullptr);
        std::vector<zet_metric_group_handle_t> metricGroups;
        metricGroups.resize(metricGroupCount);
        ASSERT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, metricGroups.data()), ZE_RESULT_SUCCESS);
        ASSERT_NE(metricGroups[0], nullptr);
        zet_metric_group_properties_t metricGroupProperties = {ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES, nullptr};
        EXPECT_EQ(zetMetricGroupGetProperties(metricGroups[0], &metricGroupProperties), ZE_RESULT_SUCCESS);
        EXPECT_EQ(strcmp(metricGroupProperties.name, "EuStallSampling"), 0);

        uint32_t metricValueCount = 0;
        EXPECT_EQ(zetMetricGroupCalculateMetricValues(metricGroups[0], ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES,
                                                      rawDataVectorSize, reinterpret_cast<uint8_t *>(rawDataVector.data()), &metricValueCount, nullptr),
                  ZE_RESULT_SUCCESS);
        EXPECT_TRUE(metricValueCount == 40);
        EXPECT_EQ(zetMetricGroupCalculateMetricValues(metricGroups[0], ZET_METRIC_GROUP_CALCULATION_TYPE_MAX_METRIC_VALUES,
                                                      rawDataVectorSize, reinterpret_cast<uint8_t *>(rawDataVector.data()), &metricValueCount, metricValues.data()),
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
        zet_metric_group_properties_t metricGroupProperties = {ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES, nullptr};
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

using MetricExportDataIpSamplingTest = MetricIpSamplingEnumerationTest;

TEST_F(MetricExportDataIpSamplingTest, WhenMetricGroupGetExportDataIsCalledThenReturnSuccess) {

    EXPECT_EQ(ZE_RESULT_SUCCESS, testDevices[0]->getMetricDeviceContext().enableMetricApi());
    for (auto device : testDevices) {

        uint32_t metricGroupCount = 0;
        zetMetricGroupGet(device->toHandle(), &metricGroupCount, nullptr);
        EXPECT_EQ(metricGroupCount, 1u);

        std::vector<zet_metric_group_handle_t> metricGroups;
        metricGroups.resize(metricGroupCount);

        ASSERT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, metricGroups.data()), ZE_RESULT_SUCCESS);
        ASSERT_NE(metricGroups[0], nullptr);

        uint8_t dummyRawData = 8;
        size_t exportDataSize = 0;
        const auto dummyRawDataSize = 1u;
        EXPECT_EQ(zetMetricGroupGetExportDataExp(metricGroups[0],
                                                 &dummyRawData, dummyRawDataSize, &exportDataSize, nullptr),
                  ZE_RESULT_SUCCESS);
        EXPECT_GE(exportDataSize, 0u);
        std::vector<uint8_t> exportDataMem(exportDataSize);
        EXPECT_EQ(zetMetricGroupGetExportDataExp(metricGroups[0],
                                                 &dummyRawData, dummyRawDataSize, &exportDataSize, exportDataMem.data()),
                  ZE_RESULT_SUCCESS);
        zet_intel_metric_df_gpu_export_data_format_t *exportData = reinterpret_cast<zet_intel_metric_df_gpu_export_data_format_t *>(exportDataMem.data());
        EXPECT_EQ(exportData->header.type, ZET_INTEL_METRIC_DF_SOURCE_TYPE_IPSAMPLING);
        EXPECT_EQ(dummyRawData, exportDataMem[exportData->header.rawDataOffset]);
    }
}

TEST_F(MetricExportDataIpSamplingTest, GivenIncorrectExportDataSizeWhenMetricGroupGetExportDataIsCalledThenErrorIsReturned) {

    EXPECT_EQ(ZE_RESULT_SUCCESS, testDevices[0]->getMetricDeviceContext().enableMetricApi());
    for (auto device : testDevices) {

        uint32_t metricGroupCount = 0;
        zetMetricGroupGet(device->toHandle(), &metricGroupCount, nullptr);
        EXPECT_EQ(metricGroupCount, 1u);

        std::vector<zet_metric_group_handle_t> metricGroups;
        metricGroups.resize(metricGroupCount);

        ASSERT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, metricGroups.data()), ZE_RESULT_SUCCESS);
        ASSERT_NE(metricGroups[0], nullptr);

        uint8_t dummyRawData = 0;
        size_t exportDataSize = 0;
        const auto dummyRawDataSize = 1u;
        EXPECT_EQ(zetMetricGroupGetExportDataExp(metricGroups[0],
                                                 &dummyRawData, dummyRawDataSize, &exportDataSize, nullptr),
                  ZE_RESULT_SUCCESS);
        EXPECT_GE(exportDataSize, 0u);
        exportDataSize -= 1;
        std::vector<uint8_t> exportDataMem(exportDataSize);
        EXPECT_EQ(zetMetricGroupGetExportDataExp(metricGroups[0],
                                                 &dummyRawData, dummyRawDataSize, &exportDataSize, exportDataMem.data()),
                  ZE_RESULT_ERROR_INVALID_SIZE);
    }
}

} // namespace ult
} // namespace L0
