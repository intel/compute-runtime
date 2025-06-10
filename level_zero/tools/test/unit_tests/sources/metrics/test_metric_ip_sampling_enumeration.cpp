/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/common/test_macros/test_base.h"

#include "level_zero/core/source/cmdlist/cmdlist.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/tools/source/metrics/metric_ip_sampling_source.h"
#include "level_zero/tools/source/metrics/metric_ip_sampling_streamer.h"
#include "level_zero/tools/source/metrics/metric_oa_source.h"
#include "level_zero/tools/source/metrics/os_interface_metric.h"
#include "level_zero/tools/test/unit_tests/sources/metrics/mock_metric_ip_sampling.h"
#include "level_zero/tools/test/unit_tests/sources/metrics/mock_metric_source.h"
#include "level_zero/zet_intel_gpu_metric.h"
#include "level_zero/zet_intel_gpu_metric_export.h"
#include <level_zero/zet_api.h>

#include "metric_ip_sampling_fixture.h"

namespace L0 {
extern _ze_driver_handle_t *globalDriverHandle;

namespace ult {

using MetricIpSamplingEnumerationTest = MetricIpSamplingMultiDevFixture;

HWTEST2_F(MetricIpSamplingEnumerationTest, GivenDependenciesAvailableWhenInititializingThenSuccessIsReturned, EustallSupportedPlatforms) {

    EXPECT_EQ(ZE_RESULT_SUCCESS, testDevices[0]->getMetricDeviceContext().enableMetricApi());
    for (auto device : testDevices) {
        auto &metricSource = device->getMetricDeviceContext().getMetricSource<IpSamplingMetricSourceImp>();
        EXPECT_TRUE(metricSource.isAvailable());
    }
}

HWTEST2_F(MetricIpSamplingEnumerationTest, GivenDependenciesUnAvailableForRootDeviceWhenInititializingThenFailureIsReturned, EustallSupportedPlatforms) {

    osInterfaceVector[0]->isDependencyAvailableReturn = false;
    EXPECT_EQ(ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE, testDevices[0]->getMetricDeviceContext().enableMetricApi());
}

HWTEST2_F(MetricIpSamplingEnumerationTest, GivenDependenciesUnAvailableForSubDeviceWhenInititializingThenFailureIsReturned, EustallSupportedPlatforms) {

    osInterfaceVector[1]->isDependencyAvailableReturn = false;
    EXPECT_EQ(ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE, testDevices[0]->getMetricDeviceContext().enableMetricApi());

    auto &metricSource = testDevices[0]->getMetricDeviceContext().getMetricSource<IpSamplingMetricSourceImp>();
    EXPECT_TRUE(metricSource.isAvailable());

    auto &metricSource0 = testDevices[1]->getMetricDeviceContext().getMetricSource<IpSamplingMetricSourceImp>();
    EXPECT_FALSE(metricSource0.isAvailable());

    auto &metricSource1 = testDevices[2]->getMetricDeviceContext().getMetricSource<IpSamplingMetricSourceImp>();
    EXPECT_TRUE(metricSource1.isAvailable());
}

HWTEST2_F(MetricIpSamplingEnumerationTest, GivenIpSamplingAvailableWhenCreateMetricGroupsFromMetricsIsCalledThenErrorIsReturned, EustallSupportedPlatforms) {

    EXPECT_EQ(ZE_RESULT_SUCCESS, testDevices[0]->getMetricDeviceContext().enableMetricApi());
    auto &metricSource = testDevices[0]->getMetricDeviceContext().getMetricSource<IpSamplingMetricSourceImp>();

    std::vector<zet_metric_handle_t> metricList{};
    const char metricGroupNamePrefix[ZET_INTEL_MAX_METRIC_GROUP_NAME_PREFIX_EXP] = {};
    const char description[ZET_MAX_METRIC_GROUP_DESCRIPTION] = {};
    uint32_t maxMetricGroupCount = 0;
    std::vector<zet_metric_group_handle_t> metricGroupList = {};
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, metricSource.createMetricGroupsFromMetrics(metricList, metricGroupNamePrefix, description, &maxMetricGroupCount, metricGroupList));
}

HWTEST2_F(MetricIpSamplingEnumerationTest, GivenDependenciesAvailableWhenMetricGroupGetIsCalledThenValidMetricGroupIsReturned, EustallSupportedPlatforms) {

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

HWTEST2_F(MetricIpSamplingEnumerationTest, GivenDependenciesNotAvailableWhenMetricGroupGetIsCalledThenValidMetricGroupIsReturned, EustallSupportedPlatforms) {

    EXPECT_EQ(ZE_RESULT_SUCCESS, testDevices[0]->getMetricDeviceContext().enableMetricApi());

    for (auto &osInterface : osInterfaceVector) {
        osInterface->startMeasurementReturn = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    for (auto device : testDevices) {

        uint32_t metricGroupCount = 0;
        EXPECT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, nullptr), ZE_RESULT_SUCCESS);
        EXPECT_EQ(metricGroupCount, 0u);
    }
}

HWTEST2_F(MetricIpSamplingEnumerationTest, GivenDependenciesAvailableWhenMetricGroupGetIsCalledMultipleTimesThenValidMetricGroupIsReturned, EustallSupportedPlatforms) {

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

HWTEST2_F(MetricIpSamplingEnumerationTest, GivenDependenciesAvailableWhenMetricGroupGetIsCalledThenMetricGroupWithCorrectPropertiesIsReturned, EustallSupportedPlatforms) {

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

HWTEST2_F(MetricIpSamplingEnumerationTest, GivenDependenciesAvailableWhenMetricGroupSourceIdIsRequestedThenCorrectSourceIdIsReturned, EustallSupportedPlatforms) {

    EXPECT_EQ(ZE_RESULT_SUCCESS, testDevices[0]->getMetricDeviceContext().enableMetricApi());
    for (auto device : testDevices) {

        uint32_t metricGroupCount = 0;
        zetMetricGroupGet(device->toHandle(), &metricGroupCount, nullptr);
        EXPECT_EQ(metricGroupCount, 1u);

        std::vector<zet_metric_group_handle_t> metricGroups;
        metricGroups.resize(metricGroupCount);

        ASSERT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, metricGroups.data()), ZE_RESULT_SUCCESS);
        ASSERT_NE(metricGroups[0], nullptr);

        zet_intel_metric_source_id_exp_t metricGroupSourceId{};
        metricGroupSourceId.sourceId = 0xFFFFFFFF;
        metricGroupSourceId.pNext = nullptr;
        metricGroupSourceId.stype = ZET_INTEL_STRUCTURE_TYPE_METRIC_SOURCE_ID_EXP;
        zet_metric_group_properties_t metricGroupProperties = {ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES, &metricGroupSourceId};
        EXPECT_EQ(zetMetricGroupGetProperties(metricGroups[0], &metricGroupProperties), ZE_RESULT_SUCCESS);
        EXPECT_EQ(metricGroupSourceId.sourceId, MetricSource::metricSourceTypeIpSampling);
    }
}

using DriverExtensionsTest = Test<ExtensionFixture>;

TEST_F(DriverExtensionsTest, givenDriverHandleWhenAskingForExtensionsThenReturnCorrectVersions) {
    verifyExtensionDefinition(ZET_INTEL_METRIC_SOURCE_ID_EXP_NAME, ZET_INTEL_METRIC_SOURCE_ID_EXP_VERSION_CURRENT);
    verifyExtensionDefinition(ZET_INTEL_METRIC_CALCULATE_EXP_NAME, ZET_INTEL_METRIC_CALCULATE_EXP_VERSION_CURRENT);
}

struct TestMetricProperties {
    const char *name;
    const char *description;
    const char *component;
    uint32_t tierNumber;
    zet_metric_type_t metricType;
    zet_value_type_t resultType;
    const char *resultUnits;
};

HWTEST2_F(MetricIpSamplingEnumerationTest, GivenDependenciesAvailableWhenMetricGroupGetIsCalledThenCorrectMetricsAreReturned, EustallSupportedPlatforms) {

    std::vector<struct TestMetricProperties> expectedProperties = {
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
        std::vector<struct TestMetricProperties>::iterator propertiesIter = expectedProperties.begin();

        zet_metric_properties_t ipSamplingMetricProperties = {};
        for (auto &metricHandle : metricHandles) {
            EXPECT_EQ(zetMetricGetProperties(metricHandle, &ipSamplingMetricProperties), ZE_RESULT_SUCCESS);
            EXPECT_EQ(zetMetricDestroyExp(metricHandle), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
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
using IsNotGen9ThruPVC = IsNotWithinProducts<IGFX_SKYLAKE, IGFX_PVC>;
HWTEST2_F(MetricIpSamplingEnumerationTest, GivenEnableMetricAPIOnUnsupportedPlatformsThenFailureIsReturned, IsNotGen9ThruPVC) {
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

        EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), device->toHandle(), 1, &metricGroups[0]), ZE_RESULT_SUCCESS);
        static_cast<DeviceImp *>(device)->activateMetricGroups();
        EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), device->toHandle(), 0, nullptr), ZE_RESULT_SUCCESS);
    }
}

HWTEST2_F(MetricIpSamplingEnumerationTest, GivenEnumerationIsSuccessfulThenDummyActivationAndDeActivationHappens, EustallSupportedPlatforms) {
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

HWTEST2_F(MetricIpSamplingEnumerationTest, GivenEnumerationIsSuccessfulWhenMetricsDisableIsCalledActivationReturnsFailure, EustallSupportedPlatforms) {
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

        // Disable Metrics
        EXPECT_EQ(zetIntelDeviceDisableMetricsExp(device->toHandle()), ZE_RESULT_ERROR_HANDLE_OBJECT_IN_USE);
        // De-Activate all metric groups.
        EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), device->toHandle(), 0, nullptr), ZE_RESULT_SUCCESS);
        // Disable Metrics with all groups deactivated should return success
        EXPECT_EQ(zetIntelDeviceDisableMetricsExp(device->toHandle()), ZE_RESULT_SUCCESS);
        // Activate metric group on a disabled device should be failure.
        EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), device->toHandle(), 1, &metricGroups[0]), ZE_RESULT_ERROR_UNINITIALIZED);
        EXPECT_EQ(zetIntelDeviceEnableMetricsExp(device->toHandle()), ZE_RESULT_SUCCESS);
    }
}

HWTEST2_F(MetricIpSamplingEnumerationTest, GivenEnumerationIsSuccessfulThenUnsupportedApisForMetricGroupReturnsFailure, EustallSupportedPlatforms) {
    EXPECT_EQ(ZE_RESULT_SUCCESS, testDevices[0]->getMetricDeviceContext().enableMetricApi());

    for (auto device : testDevices) {

        uint32_t metricGroupCount = 0;
        zetMetricGroupGet(device->toHandle(), &metricGroupCount, nullptr);
        std::vector<zet_metric_group_handle_t> metricGroups;
        metricGroups.resize(metricGroupCount);
        ASSERT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, metricGroups.data()), ZE_RESULT_SUCCESS);
        ASSERT_NE(metricGroups[0], nullptr);

        zet_metric_handle_t hMetric{};
        EXPECT_EQ(zetMetricGroupAddMetricExp(metricGroups[0], hMetric, nullptr, nullptr), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
        EXPECT_EQ(zetMetricGroupRemoveMetricExp(metricGroups[0], hMetric), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
        EXPECT_EQ(zetMetricGroupCloseExp(metricGroups[0]), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
        EXPECT_EQ(zetMetricGroupDestroyExp(metricGroups[0]), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);

        zet_metric_group_properties_t metricGroupProperties = {ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES, nullptr};
        EXPECT_EQ(zetMetricGroupGetProperties(metricGroups[0], &metricGroupProperties), ZE_RESULT_SUCCESS);
        EXPECT_EQ(strcmp(metricGroupProperties.name, "EuStallSampling"), 0);

        EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), device->toHandle(), 1, &metricGroups[0]), ZE_RESULT_SUCCESS);
        static_cast<DeviceImp *>(device)->activateMetricGroups();
        EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), device->toHandle(), 0, nullptr), ZE_RESULT_SUCCESS);

        EXPECT_EQ(zetIntelCommandListAppendMarkerExp(nullptr, metricGroups[0], 0), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
    }
}

HWTEST2_F(MetricIpSamplingEnumerationTest, GivenEnumerationIsSuccessfulWhenReadingMetricsFrequencyAndValidBitsThenConfirmAreTheSameAsDevice, EustallSupportedPlatforms) {

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

HWTEST2_F(MetricIpSamplingEnumerationTest, GivenEnumerationIsSuccessfulWhenQueryingMetricGroupTypeThenAppropriateGroupTypeIsReturned, EustallSupportedPlatforms) {

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

        zet_metric_group_type_exp_t metricGroupType{};
        metricGroupType.stype = ZET_STRUCTURE_TYPE_METRIC_GROUP_TYPE_EXP;
        metricGroupType.pNext = nullptr;
        metricGroupType.type = ZET_METRIC_GROUP_TYPE_EXP_FLAG_FORCE_UINT32;

        zet_metric_group_properties_t metricGroupProperties = {ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES, &metricGroupType};
        EXPECT_EQ(zetMetricGroupGetProperties(metricGroups[0], &metricGroupProperties), ZE_RESULT_SUCCESS);
        EXPECT_EQ(strcmp(metricGroupProperties.description, "EU stall sampling"), 0);
        EXPECT_EQ(strcmp(metricGroupProperties.name, "EuStallSampling"), 0);
        EXPECT_EQ(metricGroupType.type, ZET_METRIC_GROUP_TYPE_EXP_FLAG_OTHER);
    }
}

HWTEST2_F(MetricIpSamplingEnumerationTest, GivenEnumerationIsSuccessfulWhenQueryingUnsupportedPropertyThenErrorIsReturned, EustallSupportedPlatforms) {

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

        zet_metric_group_type_exp_t metricGroupType{};
        metricGroupType.stype = ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES;
        metricGroupType.pNext = nullptr;
        metricGroupType.type = ZET_METRIC_GROUP_TYPE_EXP_FLAG_FORCE_UINT32;

        zet_metric_group_properties_t metricGroupProperties = {ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES, &metricGroupType};
        EXPECT_EQ(zetMetricGroupGetProperties(metricGroups[0], &metricGroupProperties), ZE_RESULT_ERROR_INVALID_ARGUMENT);
    }
}

HWTEST2_F(MetricIpSamplingEnumerationTest, GivenEnumerationIsSuccessfulOnMulitDeviceWhenReadingMetricsTimestampThenResultIsSuccess, EustallSupportedPlatforms) {

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

using MetricIpSamplingTimestampTest = MetricIpSamplingFixture;

HWTEST2_F(MetricIpSamplingTimestampTest, GivenEnumerationIsSuccessfulWhenReadingMetricsFrequencyThenValuesAreUpdated, EustallSupportedPlatforms) {

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

HWTEST2_F(MetricIpSamplingTimestampTest, GivenGetGpuCpuTimeIsFalseWhenReadingMetricsFrequencyThenValuesAreZero, EustallSupportedPlatforms) {

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

using MetricIpSamplingCalculateMetricsTest = MetricIpSamplingCalculateMultiDevFixture;

HWTEST2_F(MetricIpSamplingCalculateMetricsTest, GivenEnumerationIsSuccessfulWhenCalculateMultipleMetricValuesExpIsCalledThenValidDataIsReturned, EustallSupportedPlatforms) {
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
        EXPECT_EQ(totalMetricValueCount, 30u); // Three IPs plus nine metrics per IP
        EXPECT_EQ(L0::zetMetricGroupCalculateMultipleMetricValuesExp(metricGroups[0],
                                                                     ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, rawDataWithHeader.size(), reinterpret_cast<uint8_t *>(rawDataWithHeader.data()),
                                                                     &setCount, &totalMetricValueCount, metricCounts.data(), metricValues.data()),
                  ZE_RESULT_SUCCESS);
        EXPECT_EQ(setCount, expectedSetCount);
        EXPECT_EQ(totalMetricValueCount, 30u);
        EXPECT_EQ(metricCounts[0], 30u);
        for (uint32_t i = 0; i < totalMetricValueCount; i++) {
            EXPECT_TRUE(expectedMetricValues[i].type == metricValues[i].type);
            EXPECT_TRUE(expectedMetricValues[i].value.ui64 == metricValues[i].value.ui64);
        }
    }
}

HWTEST2_F(MetricIpSamplingCalculateMetricsTest, GivenEnumerationIsSuccessfulWhenCalculateMultipleMetricValuesExpIsCalledWithInvalidHeaderThenErrorIsReturned, EustallSupportedPlatforms) {
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

HWTEST2_F(MetricIpSamplingCalculateMetricsTest, GivenEnumerationIsSuccessfulWhenCalculateMultipleMetricValuesExpIsCalledWithDataFromSingleDeviceThenValidDataIsReturned, EustallSupportedPlatforms) {
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
            EXPECT_EQ(totalMetricValueCount, 30u);
            EXPECT_EQ(L0::zetMetricGroupCalculateMultipleMetricValuesExp(metricGroups[0],
                                                                         ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, rawDataVectorSize, reinterpret_cast<uint8_t *>(rawDataVector.data()),
                                                                         &setCount, &totalMetricValueCount, metricCounts.data(), metricValues.data()),
                      ZE_RESULT_SUCCESS);
            EXPECT_EQ(setCount, expectedSetCount);
            EXPECT_EQ(totalMetricValueCount, 30u);
            EXPECT_EQ(metricCounts[0], 30u);
            for (uint32_t i = 0; i < totalMetricValueCount; i++) {
                EXPECT_TRUE(expectedMetricValues[i].type == metricValues[i].type);
                EXPECT_TRUE(expectedMetricValues[i].value.ui64 == metricValues[i].value.ui64);
            }
        }
    }
}

HWTEST2_F(MetricIpSamplingCalculateMetricsTest, GivenEnumerationIsSuccessfulWhenCalculateMultipleMetricValuesExpIsCalledWithDataFromSingleDeviceAndInvalidRawDataThenErrorIsReturned, EustallSupportedPlatforms) {
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

HWTEST2_F(MetricIpSamplingCalculateMetricsTest, GivenEnumerationIsSuccessfulWhenCalculateMultipleMetricValuesExpIsCalledWithLessThanRequiredMetricCountThenValidDataIsReturned, EustallSupportedPlatforms) {
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
        EXPECT_EQ(totalMetricValueCount, 60u);
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

HWTEST2_F(MetricIpSamplingCalculateMetricsTest, GivenEnumerationIsSuccessfulWhenCalculateMultipleMetricValuesExpIsCalledWithInvalidRawDataSizeThenErrorIsReturned, EustallSupportedPlatforms) {

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

HWTEST2_F(MetricIpSamplingCalculateMetricsTest, WhenCalculateMultipleMetricValuesExpIsCalledWithInvalidRawDataSizeDuringValueCalculationPhaseThenErrorIsReturned, EustallSupportedPlatforms) {

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

HWTEST2_F(MetricIpSamplingCalculateMetricsTest, WhenCalculateMultipleMetricValuesExpCalculateSizeIsCalledWithInvalidRawDataSizeInHeaderDuringSizeCalculationThenErrorIsReturned, EustallSupportedPlatforms) {

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

HWTEST2_F(MetricIpSamplingCalculateMetricsTest, WhenCalculateMultipleMetricValuesExpCalculateSizeIsCalledWithInvalidRawDataSizeInHeaderThenErrorIsReturned, EustallSupportedPlatforms) {

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

HWTEST2_F(MetricIpSamplingCalculateMetricsTest, GivenEnumerationIsSuccessfulWhenCalculateMultipleMetricValuesExpCalculateDataWithBadRawDataSizeIsCalledThenErrorUnknownIsReturned, EustallSupportedPlatforms) {

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
        EXPECT_EQ(totalMetricValueCount, 30u);
        totalMetricValueCount += 1;
        EXPECT_EQ(L0::zetMetricGroupCalculateMultipleMetricValuesExp(metricGroups[0],
                                                                     ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, rawDataWithHeader.size() + 1, reinterpret_cast<uint8_t *>(rawDataWithHeader.data()),
                                                                     &setCount, &totalMetricValueCount, metricCounts.data(), metricValues.data()),
                  ZE_RESULT_ERROR_INVALID_SIZE);
    }
}

HWTEST2_F(MetricIpSamplingCalculateMetricsTest, GivenEnumerationIsSuccessfulWhenCalculateMetricValuesIsCalledThenValidDataIsReturned, EustallSupportedPlatforms) {

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
        EXPECT_TRUE(metricValueCount == 30);
        EXPECT_EQ(zetMetricGroupCalculateMetricValues(metricGroups[0], ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES,
                                                      rawDataVectorSize, reinterpret_cast<uint8_t *>(rawDataVector.data()), &metricValueCount, metricValues.data()),
                  ZE_RESULT_SUCCESS);
        EXPECT_TRUE(metricValueCount == 30);
        for (uint32_t i = 0; i < metricValueCount; i++) {
            EXPECT_TRUE(expectedMetricValues[i].type == metricValues[i].type);
            EXPECT_TRUE(expectedMetricValues[i].value.ui64 == metricValues[i].value.ui64);
        }
    }
}

HWTEST2_F(MetricIpSamplingCalculateMetricsTest, GivenEnumerationIsSuccessfulWhenCalculateMetricValuesIsCalledWithDataFromMultipleSubdevicesThenReturnError, EustallSupportedPlatforms) {

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

HWTEST2_F(MetricIpSamplingCalculateMetricsTest, GivenEnumerationIsSuccessfulWhenCalculateMetricValuesIsCalledWithSmallValueCountThenValidDataIsReturned, EustallSupportedPlatforms) {

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
        EXPECT_TRUE(metricValueCount == 30);
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

HWTEST2_F(MetricIpSamplingCalculateMetricsTest, GivenEnumerationIsSuccessfulWithBadRawDataSizeWhenCalculateMetricValuesCalculateSizeIsCalledThenErrorUnknownIsReturned, EustallSupportedPlatforms) {

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

HWTEST2_F(MetricIpSamplingCalculateMetricsTest, GivenEnumerationIsSuccessfulWhenCalculateMetricValuesWithBadRawDataSizeCalculateDataIsCalledThenUnsupportedFeatureIsReturned, EustallSupportedPlatforms) {

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
        EXPECT_TRUE(metricValueCount == 30);
        EXPECT_EQ(zetMetricGroupCalculateMetricValues(metricGroups[0], ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES,
                                                      rawDataVectorSize - 1, reinterpret_cast<uint8_t *>(rawDataVector.data()), &metricValueCount, metricValues.data()),
                  ZE_RESULT_ERROR_INVALID_SIZE);
    }
}

HWTEST2_F(MetricIpSamplingCalculateMetricsTest, GivenDataOverflowOccurredWhenStreamerReadDataIscalledThenCalculateMultipleMetricsValulesExpReturnsOverflowWarning, EustallSupportedPlatforms) {
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
        EXPECT_EQ(totalMetricValueCount, 20u);
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

HWTEST2_F(MetricIpSamplingCalculateMetricsTest, GivenEnumerationIsSuccessfulWithCALCULATIONTYPEMAXWhenCalculateMetricValuesIsCalledThenErrorUnknownIsReturned, EustallSupportedPlatforms) {

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
        EXPECT_TRUE(metricValueCount == 30);
        EXPECT_EQ(zetMetricGroupCalculateMetricValues(metricGroups[0], ZET_METRIC_GROUP_CALCULATION_TYPE_MAX_METRIC_VALUES,
                                                      rawDataVectorSize, reinterpret_cast<uint8_t *>(rawDataVector.data()), &metricValueCount, metricValues.data()),
                  ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
    }
}

HWTEST2_F(MetricIpSamplingEnumerationTest, GivenEnumerationIsSuccessfulWhenQueryPoolCreateIsCalledThenUnsupportedFeatureIsReturned, EustallSupportedPlatforms) {

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

HWTEST2_F(MetricIpSamplingEnumerationTest, GivenEnumerationIsSuccessfulWhenAppendMetricMemoryBarrierIsCalledThenUnsupportedFeatureIsReturned, EustallSupportedPlatforms) {

    EXPECT_EQ(ZE_RESULT_SUCCESS, testDevices[0]->getMetricDeviceContext().enableMetricApi());
    auto &device = testDevices[0];
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false));
    zet_command_list_handle_t commandListHandle = commandList->toHandle();
    EXPECT_EQ(zetCommandListAppendMetricMemoryBarrier(commandListHandle), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
}

using MetricExportDataIpSamplingTest = MetricIpSamplingMultiDevFixture;

HWTEST2_F(MetricExportDataIpSamplingTest, WhenMetricGroupGetExportDataIsCalledThenReturnSuccess, EustallSupportedPlatforms) {

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

HWTEST2_F(MetricExportDataIpSamplingTest, GivenIncorrectExportDataSizeWhenMetricGroupGetExportDataIsCalledThenErrorIsReturned, EustallSupportedPlatforms) {

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

class MockMultiDomainDeferredActivationTracker : public MultiDomainDeferredActivationTracker {

  public:
    ~MockMultiDomainDeferredActivationTracker() override = default;
    MockMultiDomainDeferredActivationTracker(uint32_t subdeviceIndex) : MultiDomainDeferredActivationTracker(subdeviceIndex) {}

    bool activateMetricGroupsDeferred(uint32_t count, zet_metric_group_handle_t *phMetricGroups) override {
        return false;
    }
};

HWTEST2_F(MetricIpSamplingEnumerationTest, GivenEnumerationIsSuccessfulAndActivationFailsThenErrorIsReturned, EustallSupportedPlatforms) {
    EXPECT_EQ(ZE_RESULT_SUCCESS, testDevices[0]->getMetricDeviceContext().enableMetricApi());

    for (auto device : testDevices) {
        auto &metricSource = (static_cast<DeviceImp *>(device))->getMetricDeviceContext().getMetricSource<IpSamplingMetricSourceImp>();
        MockMultiDomainDeferredActivationTracker *mockTracker = new MockMultiDomainDeferredActivationTracker(0);
        metricSource.setActivationTracker(mockTracker);
    }

    for (auto device : testDevices) {

        uint32_t metricGroupCount = 0;
        zetMetricGroupGet(device->toHandle(), &metricGroupCount, nullptr);
        std::vector<zet_metric_group_handle_t> metricGroups;
        metricGroups.resize(metricGroupCount);
        ASSERT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, metricGroups.data()), ZE_RESULT_SUCCESS);
        ASSERT_NE(metricGroups[0], nullptr);

        EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), device->toHandle(), 1, &metricGroups[0]), ZE_RESULT_ERROR_UNKNOWN);
    }
}

HWTEST2_F(MetricIpSamplingEnumerationTest, GivenEnumerationIsSuccessfulWhenUnsupportedFunctionsAreCalledErrorIsReturned, EustallSupportedPlatforms) {
    EXPECT_EQ(ZE_RESULT_SUCCESS, testDevices[0]->getMetricDeviceContext().enableMetricApi());

    for (auto device : testDevices) {
        auto &metricSource = (static_cast<DeviceImp *>(device))->getMetricDeviceContext().getMetricSource<IpSamplingMetricSourceImp>();

        char name[ZET_MAX_METRIC_GROUP_NAME] = {};
        char description[ZET_MAX_METRIC_GROUP_DESCRIPTION] = {};
        zet_metric_group_sampling_type_flag_t samplingType = ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_EVENT_BASED;
        zet_metric_group_handle_t *pMetricGroupHandle = nullptr;
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, metricSource.metricGroupCreate(
                                                           name, description, samplingType, pMetricGroupHandle));
        uint32_t count = 0;
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, metricSource.metricProgrammableGet(
                                                           &count, nullptr));
    }
}

HWTEST2_F(MetricIpSamplingEnumerationTest, GivenEnumerationIsSuccessfulWhenUnsupportedFunctionsForDeviceContextAreCalledErrorIsReturned, EustallSupportedPlatforms) {
    EXPECT_EQ(ZE_RESULT_SUCCESS, testDevices[0]->getMetricDeviceContext().enableMetricApi());

    for (auto device : testDevices) {
        auto &deviceContext = (static_cast<DeviceImp *>(device))->getMetricDeviceContext();

        char name[ZET_MAX_METRIC_GROUP_NAME] = {};
        char description[ZET_MAX_METRIC_GROUP_DESCRIPTION] = {};
        zet_metric_group_sampling_type_flag_t samplingType = ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_EVENT_BASED;
        zet_metric_group_handle_t *pMetricGroupHandle = nullptr;
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, deviceContext.metricGroupCreate(
                                                           name, description, samplingType, pMetricGroupHandle));
        uint32_t count = 0;
        EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext.metricProgrammableGet(
                                         &count, nullptr));
        EXPECT_EQ(count, 0u);
    }
}

} // namespace ult
} // namespace L0
