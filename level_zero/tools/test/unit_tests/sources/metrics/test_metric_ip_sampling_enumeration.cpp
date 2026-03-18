/*
 * Copyright (C) 2022-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hpc_core/hw_cmds_pvc.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/api/extensions/public/ze_exp_ext.h"
#include "level_zero/core/source/cmdlist/cmdlist.h"
#include "level_zero/core/source/context/context_imp.h"
#include "level_zero/core/source/device/device.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/tools/source/metrics/metric_ip_sampling_source.h"
#include "level_zero/tools/test/unit_tests/sources/metrics/mock_metric_ip_sampling.h"
#include "level_zero/zet_intel_gpu_metric.h"
#include "level_zero/zet_intel_gpu_metric_export.h"
#include <level_zero/zet_api.h>

#include "metric_ip_sampling_fixture.h"

namespace L0 {
extern _ze_driver_handle_t *globalDriverHandle;

namespace ult {

using MetricIpSamplingEnumerationTest = MetricIpSamplingMultiDevFixture;

HWTEST2_F(MetricIpSamplingEnumerationTest, GivenDependenciesUnAvailableForSubDeviceWhenInitializingThenFailureIsReturned, HasIPSamplingSupport) {

    // Set dependencies unavailable for sub-device 1
    MockMetricDeviceContext *mockRootDeviceContext = new MockMetricDeviceContext(*testDevices[1]);
    testDevices[1]->metricContext.reset(mockRootDeviceContext);
    auto mockMetricIpSamplingOsInterface = new MockMetricIpSamplingOsInterface();
    std::unique_ptr<MetricIpSamplingOsInterface> metricIpSamplingOsInterface = std::unique_ptr<MetricIpSamplingOsInterface>(mockMetricIpSamplingOsInterface);
    auto &metricSource1 = testDevices[1]->getMetricDeviceContext().getMetricSource<IpSamplingMetricSourceImp>();
    metricSource1.setMetricOsInterface(metricIpSamplingOsInterface);
    mockMetricIpSamplingOsInterface->isDependencyAvailableReturn = false;

    EXPECT_EQ(ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE, testDevices[1]->getMetricDeviceContext().enableMetricApi());
    EXPECT_FALSE(metricSource1.isAvailable());

    auto &metricSource0 = testDevices[0]->getMetricDeviceContext().getMetricSource<IpSamplingMetricSourceImp>();
    EXPECT_TRUE(metricSource0.isAvailable());

    auto &metricSource2 = testDevices[2]->getMetricDeviceContext().getMetricSource<IpSamplingMetricSourceImp>();
    EXPECT_TRUE(metricSource2.isAvailable());
}

HWTEST2_F(MetricIpSamplingEnumerationTest, GivenIpSamplingAvailableWhenCreateMetricGroupsFromMetricsIsCalledThenErrorIsReturned, HasIPSamplingSupport) {

    auto &metricSource = testDevices[0]->getMetricDeviceContext().getMetricSource<IpSamplingMetricSourceImp>();

    std::vector<zet_metric_handle_t> metricList{};
    const char metricGroupNamePrefix[ZET_INTEL_MAX_METRIC_GROUP_NAME_PREFIX_EXP] = {};
    const char description[ZET_MAX_METRIC_GROUP_DESCRIPTION] = {};
    uint32_t maxMetricGroupCount = 0;
    std::vector<zet_metric_group_handle_t> metricGroupList = {};
    MetricGroupDescription metricGroupDesc(metricGroupNamePrefix, description);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, metricSource.createMetricGroupsFromMetrics(metricList, &metricGroupDesc, &maxMetricGroupCount, metricGroupList));
}

HWTEST2_F(MetricIpSamplingEnumerationTest, GivenDependenciesAvailableWhenMetricGroupGetIsCalledThenValidMetricGroupIsReturned, HasIPSamplingSupport) {

    for (auto device : rootOneSubDev) {
        uint32_t metricGroupCount = 0;
        EXPECT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, nullptr), ZE_RESULT_SUCCESS);
        EXPECT_EQ(metricGroupCount, 1u);

        std::vector<zet_metric_group_handle_t> metricGroups;
        metricGroups.resize(metricGroupCount);

        EXPECT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, metricGroups.data()), ZE_RESULT_SUCCESS);
        EXPECT_NE(metricGroups[0], nullptr);
    }
}

HWTEST2_F(MetricIpSamplingEnumerationTest, GivenDependenciesNotAvailableWhenMetricGroupGetIsCalledThenNoMetricGroupIsReturned, HasIPSamplingSupport) {

    for (auto &osInterface : osInterfaceVector) {
        osInterface->startMeasurementReturn = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    for (auto device : rootOneSubDev) {
        uint32_t metricGroupCount = 0;
        EXPECT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, nullptr), ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE);
        EXPECT_EQ(metricGroupCount, 0u);
    }
}

HWTEST2_F(MetricIpSamplingEnumerationTest, GivenDependenciesAvailableWhenMetricGroupGetIsCalledMultipleTimesThenValidMetricGroupIsReturned, HasIPSamplingSupport) {

    for (auto device : rootOneSubDev) {
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

HWTEST2_F(MetricIpSamplingEnumerationTest, GivenDependenciesAvailableWhenMetricGroupGetIsCalledThenMetricGroupWithCorrectPropertiesIsReturned, HasIPSamplingSupport) {

    for (auto device : rootOneSubDev) {
        zet_metric_group_handle_t hMetricGroup = MetricIpSamplingMultiDevFixture::getMetricGroupForDevice(device);
        zet_metric_group_properties_t metricGroupProperties = {ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES, nullptr};
        EXPECT_EQ(zetMetricGroupGetProperties(hMetricGroup, &metricGroupProperties), ZE_RESULT_SUCCESS);
        EXPECT_EQ(metricGroupProperties.domain, 100u);
        EXPECT_EQ(metricGroupProperties.samplingType, ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_TIME_BASED);
        uint32_t expectedMetricCount = 0;
        ipSamplingTestProductHelper->getExpectedMetricCount(productFamily, expectedMetricCount);
        EXPECT_EQ(metricGroupProperties.metricCount, expectedMetricCount);
        EXPECT_EQ(strcmp(metricGroupProperties.description, "EU stall sampling"), 0);
        EXPECT_EQ(strcmp(metricGroupProperties.name, "EuStallSampling"), 0);
    }
}

HWTEST2_F(MetricIpSamplingEnumerationTest, GivenDependenciesAvailableWhenMetricGroupSourceIdIsRequestedThenCorrectSourceIdIsReturned, HasIPSamplingSupport) {

    for (auto device : rootOneSubDev) {
        zet_metric_group_handle_t hMetricGroup = MetricIpSamplingMultiDevFixture::getMetricGroupForDevice(device);
        zet_intel_metric_source_id_exp_t metricGroupSourceId{};
        metricGroupSourceId.sourceId = 0xFFFFFFFF;
        metricGroupSourceId.pNext = nullptr;
        metricGroupSourceId.stype = ZET_INTEL_STRUCTURE_TYPE_METRIC_SOURCE_ID_EXP;
        zet_metric_group_properties_t metricGroupProperties = {ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES, &metricGroupSourceId};
        EXPECT_EQ(zetMetricGroupGetProperties(hMetricGroup, &metricGroupProperties), ZE_RESULT_SUCCESS);
        EXPECT_EQ(metricGroupSourceId.sourceId, MetricSource::metricSourceTypeIpSampling);
    }
}

using DriverExtensionsTest = Test<ExtensionFixture>;

TEST_F(DriverExtensionsTest, givenDriverHandleWhenAskingForExtensionsThenReturnCorrectVersions) {
    verifyExtensionDefinition(ZET_INTEL_METRIC_SOURCE_ID_EXP_NAME, ZET_INTEL_METRIC_SOURCE_ID_EXP_VERSION_CURRENT);
    verifyExtensionDefinition(ZET_INTEL_METRIC_CALCULATION_EXP_NAME, ZET_INTEL_METRIC_CALCULATION_EXP_VERSION_CURRENT);
}

HWTEST2_F(MetricIpSamplingEnumerationTest, GivenDependenciesAvailableWhenMetricGroupGetIsCalledThenCorrectMetricsAreReturned, HasIPSamplingSupport) {

    std::vector<IpSamplingTestProductHelper::MetricProperties> expectedMetricsProperties = {};
    ipSamplingTestProductHelper->getExpectedMetricsProperties(productFamily, expectedMetricsProperties);
    for (auto device : rootOneSubDev) {
        zet_metric_group_handle_t hMetricGroup = MetricIpSamplingMultiDevFixture::getMetricGroupForDevice(device);
        zet_metric_group_properties_t metricGroupProperties = {ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES, nullptr};
        zetMetricGroupGetProperties(hMetricGroup, &metricGroupProperties);

        uint32_t metricCount = 0;
        std::vector<zet_metric_handle_t> metricHandles = {};
        metricHandles.resize(metricGroupProperties.metricCount);
        EXPECT_EQ(zetMetricGet(hMetricGroup, &metricCount, nullptr), ZE_RESULT_SUCCESS);
        EXPECT_EQ(metricCount, metricGroupProperties.metricCount);
        uint32_t expectedMetricCount = 0;
        ipSamplingTestProductHelper->getExpectedMetricCount(productFamily, expectedMetricCount);
        EXPECT_EQ(metricCount, expectedMetricCount);
        EXPECT_EQ(zetMetricGet(hMetricGroup, &metricCount, metricHandles.data()), ZE_RESULT_SUCCESS);
        std::vector<IpSamplingTestProductHelper::MetricProperties>::iterator propertiesIter = expectedMetricsProperties.begin();

        zet_metric_properties_t metricProperties = {};
        metricProperties.stype = ZET_STRUCTURE_TYPE_METRIC_PROPERTIES;
        metricProperties.pNext = nullptr;
        for (auto &metricHandle : metricHandles) {
            EXPECT_EQ(zetMetricGetProperties(metricHandle, &metricProperties), ZE_RESULT_SUCCESS);
            EXPECT_EQ(zetMetricDestroyExp(metricHandle), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
            EXPECT_EQ(strcmp(metricProperties.name, propertiesIter->name), 0);
            EXPECT_EQ(strcmp(metricProperties.description, propertiesIter->description), 0);
            EXPECT_EQ(strcmp(metricProperties.component, propertiesIter->component), 0);
            EXPECT_EQ(metricProperties.tierNumber, propertiesIter->tierNumber);
            EXPECT_EQ(metricProperties.metricType, propertiesIter->metricType);
            EXPECT_EQ(metricProperties.resultType, propertiesIter->resultType);
            EXPECT_EQ(strcmp(metricProperties.resultUnits, propertiesIter->resultUnits), 0);
            propertiesIter++;
        }
    }
}

HWTEST2_F(MetricIpSamplingEnumerationTest, GivenIpSamplingMetricsThenCalculablePropertyIsAlwaysTrue, HasIPSamplingSupport) {

    for (auto device : rootOneSubDev) {
        zet_metric_group_handle_t hMetricGroup = MetricIpSamplingMultiDevFixture::getMetricGroupForDevice(device);

        uint32_t metricCount = 0;
        EXPECT_EQ(zetMetricGet(hMetricGroup, &metricCount, nullptr), ZE_RESULT_SUCCESS);
        std::vector<zet_metric_handle_t> metricHandles(metricCount);
        EXPECT_EQ(zetMetricGet(hMetricGroup, &metricCount, metricHandles.data()), ZE_RESULT_SUCCESS);

        zet_metric_properties_t metricProperties = {};
        metricProperties.stype = ZET_STRUCTURE_TYPE_METRIC_PROPERTIES;
        metricProperties.pNext = nullptr;

        zet_intel_metric_calculable_properties_exp_t calculableProperties{};
        calculableProperties.stype = ZET_INTEL_STRUCTURE_TYPE_METRIC_CALCULABLE_PROPERTIES_EXP;
        calculableProperties.pNext = nullptr;
        metricProperties.pNext = &calculableProperties;

        for (auto &metricHandle : metricHandles) {
            EXPECT_EQ(zetMetricGetProperties(metricHandle, &metricProperties), ZE_RESULT_SUCCESS);
            EXPECT_TRUE(calculableProperties.isCalculable);
        }

        // Check that invalid structure is handled gracefully
        zet_metric_group_properties_t metricGroupProperties = {ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES, nullptr};
        metricProperties.pNext = &metricGroupProperties;
        EXPECT_EQ(zetMetricGetProperties(metricHandles[0], &metricProperties), ZE_RESULT_SUCCESS);
    }
}

HWTEST2_F(MetricIpSamplingEnumerationTest, GivenEnumerationIsSuccessfulThenDummyActivationAndDeActivationHappens, HasIPSamplingSupport) {

    for (auto device : rootOneSubDev) {
        zet_metric_group_handle_t hMetricGroup = MetricIpSamplingMultiDevFixture::getMetricGroupForDevice(device);

        zet_metric_group_properties_t metricGroupProperties = {ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES, nullptr};
        EXPECT_EQ(zetMetricGroupGetProperties(hMetricGroup, &metricGroupProperties), ZE_RESULT_SUCCESS);
        EXPECT_EQ(strcmp(metricGroupProperties.name, "EuStallSampling"), 0);

        EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), device->toHandle(), 1, &hMetricGroup), ZE_RESULT_SUCCESS);
        device->activateMetricGroups();
        EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), device->toHandle(), 0, nullptr), ZE_RESULT_SUCCESS);
    }
}

HWTEST2_F(MetricIpSamplingEnumerationTest, GivenMetricsDisableIsCalledThenActivationReturnsFailure, HasIPSamplingSupport) {

    for (auto device : rootOneSubDev) {
        zet_metric_group_handle_t hMetricGroup = MetricIpSamplingMultiDevFixture::getMetricGroupForDevice(device);

        EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), device->toHandle(), 1, &hMetricGroup), ZE_RESULT_SUCCESS);
        device->activateMetricGroups();

        // Disable Metrics
        EXPECT_EQ(zetDeviceDisableMetricsExp(device->toHandle()), ZE_RESULT_ERROR_HANDLE_OBJECT_IN_USE);
        // De-Activate all metric groups.
        EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), device->toHandle(), 0, nullptr), ZE_RESULT_SUCCESS);
        // Disable Metrics with all groups deactivated should return success
        EXPECT_EQ(zetDeviceDisableMetricsExp(device->toHandle()), ZE_RESULT_SUCCESS);
        // Activate metric group on a disabled device should be failure.
        EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), device->toHandle(), 1, &hMetricGroup), ZE_RESULT_ERROR_UNINITIALIZED);
        EXPECT_EQ(zetDeviceEnableMetricsExp(device->toHandle()), ZE_RESULT_SUCCESS);
    }
}

HWTEST2_F(MetricIpSamplingEnumerationTest, GivenEuStallSamplingIsPredefinedMetricGroupThenProgrammableApisAreUnsupported, HasIPSamplingSupport) {

    for (auto device : rootOneSubDev) {
        zet_metric_group_handle_t hMetricGroup = MetricIpSamplingMultiDevFixture::getMetricGroupForDevice(device);

        zet_metric_handle_t hMetric{};
        EXPECT_EQ(zetMetricGroupAddMetricExp(hMetricGroup, hMetric, nullptr, nullptr), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
        EXPECT_EQ(zetMetricGroupRemoveMetricExp(hMetricGroup, hMetric), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
        EXPECT_EQ(zetMetricGroupCloseExp(hMetricGroup), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
        EXPECT_EQ(zetMetricGroupDestroyExp(hMetricGroup), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);

        EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), device->toHandle(), 1, &hMetricGroup), ZE_RESULT_SUCCESS);
        device->activateMetricGroups();
        EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), device->toHandle(), 0, nullptr), ZE_RESULT_SUCCESS);

        EXPECT_EQ(zetCommandListAppendMarkerExp(nullptr, hMetricGroup, 0), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
    }
}

HWTEST2_F(MetricIpSamplingEnumerationTest, GivenEnumerationIsSuccessfulWhenReadingMetricsFrequencyAndValidBitsThenConfirmAreTheSameAsDevice, HasIPSamplingSupport) {

    for (auto device : rootOneSubDev) {
        ze_device_properties_t deviceProps = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES_1_2, nullptr};
        device->getProperties(&deviceProps);

        zet_metric_group_handle_t hMetricGroup = MetricIpSamplingMultiDevFixture::getMetricGroupForDevice(device);

        zet_metric_global_timestamps_resolution_exp_t metricTimestampProperties = {ZET_STRUCTURE_TYPE_METRIC_GLOBAL_TIMESTAMPS_RESOLUTION_EXP, nullptr};

        zet_metric_group_properties_t metricGroupProperties = {ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES, &metricTimestampProperties};
        EXPECT_EQ(zetMetricGroupGetProperties(hMetricGroup, &metricGroupProperties), ZE_RESULT_SUCCESS);
        EXPECT_EQ(strcmp(metricGroupProperties.name, "EuStallSampling"), 0);
        EXPECT_EQ(metricTimestampProperties.timerResolution, deviceProps.timerResolution);
        EXPECT_EQ(metricTimestampProperties.timestampValidBits, deviceProps.timestampValidBits);
    }
}

HWTEST2_F(MetricIpSamplingEnumerationTest, GivenEnumerationIsSuccessfulWhenQueryingMetricGroupTypeThenAppropriateGroupTypeIsReturned, HasIPSamplingSupport) {

    for (auto device : rootOneSubDev) {
        ze_device_properties_t deviceProps = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES_1_2, nullptr};
        device->getProperties(&deviceProps);

        zet_metric_group_handle_t hMetricGroup = MetricIpSamplingMultiDevFixture::getMetricGroupForDevice(device);

        zet_metric_group_type_exp_t metricGroupType{};
        metricGroupType.stype = ZET_STRUCTURE_TYPE_METRIC_GROUP_TYPE_EXP;
        metricGroupType.pNext = nullptr;
        metricGroupType.type = ZET_METRIC_GROUP_TYPE_EXP_FLAG_FORCE_UINT32;

        zet_metric_group_properties_t metricGroupProperties = {ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES, &metricGroupType};
        EXPECT_EQ(zetMetricGroupGetProperties(hMetricGroup, &metricGroupProperties), ZE_RESULT_SUCCESS);
        EXPECT_EQ(strcmp(metricGroupProperties.name, "EuStallSampling"), 0);
        EXPECT_EQ(metricGroupType.type, ZET_METRIC_GROUP_TYPE_EXP_FLAG_OTHER);
    }
}

HWTEST2_F(MetricIpSamplingEnumerationTest, GivenEnumerationIsSuccessfulWhenQueryingUnsupportedPropertyThenErrorIsReturned, HasIPSamplingSupport) {

    for (auto device : rootOneSubDev) {
        ze_device_properties_t deviceProps = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES_1_2, nullptr};
        device->getProperties(&deviceProps);

        zet_metric_group_handle_t hMetricGroup = MetricIpSamplingMultiDevFixture::getMetricGroupForDevice(device);

        zet_metric_group_type_exp_t metricGroupType{};
        metricGroupType.stype = ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES;
        metricGroupType.pNext = nullptr;
        metricGroupType.type = ZET_METRIC_GROUP_TYPE_EXP_FLAG_FORCE_UINT32;

        zet_metric_group_properties_t metricGroupProperties = {ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES, &metricGroupType};
        EXPECT_EQ(zetMetricGroupGetProperties(hMetricGroup, &metricGroupProperties), ZE_RESULT_ERROR_INVALID_ARGUMENT);
    }
}

HWTEST2_F(MetricIpSamplingEnumerationTest, GivenEnumerationIsSuccessfulOnMultiDeviceWhenReadingMetricsTimestampThenResultIsSuccess, HasIPSamplingSupport) {

    for (auto device : rootOneSubDev) {
        ze_device_properties_t deviceProps = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES_1_2, nullptr};
        device->getProperties(&deviceProps);
        zet_metric_group_handle_t hMetricGroup = MetricIpSamplingMultiDevFixture::getMetricGroupForDevice(device);
        ze_bool_t synchronizedWithHost = true;
        uint64_t globalTimestamp = 0;
        uint64_t metricTimestamp = 0;

        EXPECT_EQ(L0::zetMetricGroupGetGlobalTimestampsExp(hMetricGroup, synchronizedWithHost, &globalTimestamp, &metricTimestamp), ZE_RESULT_SUCCESS);
    }
}

using MetricIpSamplingTimestampTest = MetricIpSamplingFixture;

HWTEST2_F(MetricIpSamplingTimestampTest, GivenEnumerationIsSuccessfulWhenReadingMetricsFrequencyThenValuesAreUpdated, HasIPSamplingSupport) {

    ze_device_properties_t deviceProps = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES_1_2, nullptr};
    device->getProperties(&deviceProps);
    zet_metric_group_handle_t hMetricGroup = MetricIpSamplingMultiDevFixture::getMetricGroupForDevice(device);

    ze_bool_t synchronizedWithHost = true;
    uint64_t globalTimestamp = 0;
    uint64_t metricTimestamp = 0;

    EXPECT_EQ(L0::zetMetricGroupGetGlobalTimestampsExp(hMetricGroup, synchronizedWithHost, &globalTimestamp, &metricTimestamp), ZE_RESULT_SUCCESS);
    EXPECT_NE(globalTimestamp, 0UL);
    EXPECT_NE(metricTimestamp, 0UL);

    synchronizedWithHost = false;
    globalTimestamp = 0;
    metricTimestamp = 0;

    EXPECT_EQ(L0::zetMetricGroupGetGlobalTimestampsExp(hMetricGroup, synchronizedWithHost, &globalTimestamp, &metricTimestamp), ZE_RESULT_SUCCESS);
    EXPECT_NE(globalTimestamp, 0UL);
    EXPECT_NE(metricTimestamp, 0UL);

    debugManager.flags.EnableImplicitScaling.set(1);
    globalTimestamp = 0;
    metricTimestamp = 0;

    EXPECT_EQ(L0::zetMetricGroupGetGlobalTimestampsExp(hMetricGroup, synchronizedWithHost, &globalTimestamp, &metricTimestamp), ZE_RESULT_SUCCESS);
    EXPECT_NE(globalTimestamp, 0UL);
    EXPECT_NE(metricTimestamp, 0UL);
}

HWTEST2_F(MetricIpSamplingTimestampTest, GivenGetGpuCpuTimeIsFalseWhenReadingMetricsFrequencyThenValuesAreZero, HasIPSamplingSupport) {

    neoDevice->setOSTime(new FalseGpuCpuTime());

    ze_device_properties_t deviceProps = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES_1_2, nullptr};
    device->getProperties(&deviceProps);

    zet_metric_group_handle_t hMetricGroup = MetricIpSamplingMultiDevFixture::getMetricGroupForDevice(device);

    ze_bool_t synchronizedWithHost = true;
    uint64_t globalTimestamp = 1;
    uint64_t metricTimestamp = 1;

    EXPECT_EQ(L0::zetMetricGroupGetGlobalTimestampsExp(hMetricGroup, synchronizedWithHost, &globalTimestamp, &metricTimestamp), ZE_RESULT_ERROR_DEVICE_LOST);
    EXPECT_EQ(globalTimestamp, 0UL);
    EXPECT_EQ(metricTimestamp, 0UL);
}

using MetricIpSamplingCalculateMetricGroupTest = MetricIpSamplingCalculateMetricGroupFixture;

HWTEST2_F(MetricIpSamplingCalculateMetricGroupTest, GivenValidDataWhenCalculateMultipleMetricValuesExpIsCalledThenValidDataIsReturned, HasIPSamplingSupport) {

    for (auto device : rootOneSubDev) {
        bool isRootdevice = true;
        ze_result_t expectedResult = ZE_RESULT_SUCCESS;
        ze_device_properties_t props = {};
        device->getProperties(&props);

        if (props.flags & ZE_DEVICE_PROPERTY_FLAG_SUBDEVICE) {
            // Root device data (rawDataWithHeader) will work only with root device metric group handle.
            // Calling calculate with sub-device metric group handle will return INVALID ARGUMENT.
            isRootdevice = false;
            expectedResult = ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }

        zet_metric_group_handle_t hMetricGroup = MetricIpSamplingMultiDevFixture::getMetricGroupForDevice(device);

        zet_metric_group_properties_t metricGroupProperties = {ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES, nullptr};
        EXPECT_EQ(zetMetricGroupGetProperties(hMetricGroup, &metricGroupProperties), ZE_RESULT_SUCCESS);
        uint32_t expectedResultCountPerSet = metricGroupProperties.metricCount * IpSamplingTestProductHelper::numberOfIpsInRawData;

        // Simulate data from two tiles
        std::vector<uint8_t> rawDataWithHeader((rawReportsBytesSize + sizeof(IpSamplingMultiDevDataHeader)) * 2);
        MockRawDataHelper::addMultiSubDevHeader(rawDataWithHeader.data(), rawDataWithHeader.size(), reinterpret_cast<uint8_t *>(rawReports.data()), rawReportsBytesSize, 0);
        MockRawDataHelper::addMultiSubDevHeader(rawDataWithHeader.data() + sizeof(IpSamplingMultiDevDataHeader) + rawReportsBytesSize,
                                                rawDataWithHeader.size() - sizeof(IpSamplingMultiDevDataHeader) - rawReportsBytesSize,
                                                reinterpret_cast<uint8_t *>(rawReports.data()), rawReportsBytesSize, 1);

        uint32_t setCount = 0;
        // Use random initializer
        uint32_t totalMetricValueCount = std::numeric_limits<uint32_t>::max();

        EXPECT_EQ(L0::zetMetricGroupCalculateMultipleMetricValuesExp(hMetricGroup,
                                                                     ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, rawDataWithHeader.size(), reinterpret_cast<uint8_t *>(rawDataWithHeader.data()),
                                                                     &setCount, &totalMetricValueCount, nullptr, nullptr),
                  expectedResult);
        if (isRootdevice) {
            EXPECT_EQ(setCount, 2u);
            EXPECT_EQ(totalMetricValueCount, expectedResultCountPerSet * setCount);
            std::vector<zet_typed_value_t> metricValues(totalMetricValueCount);
            std::vector<uint32_t> metricCounts(setCount);
            EXPECT_EQ(L0::zetMetricGroupCalculateMultipleMetricValuesExp(hMetricGroup,
                                                                         ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, rawDataWithHeader.size(), reinterpret_cast<uint8_t *>(rawDataWithHeader.data()),
                                                                         &setCount, &totalMetricValueCount, metricCounts.data(), metricValues.data()),
                      ZE_RESULT_SUCCESS);
            EXPECT_EQ(setCount, 2u);
            EXPECT_EQ(totalMetricValueCount, expectedResultCountPerSet * setCount);
            EXPECT_EQ(metricCounts[0], expectedResultCountPerSet);
            EXPECT_EQ(metricCounts[1], expectedResultCountPerSet);

            std::vector<uint64_t> expectedMetricCalculateValues = {};
            ipSamplingTestProductHelper->getExpectedCalculateResults(productFamily, IpSamplingTestProductHelper::CalculationResultType::CompleteResults, expectedMetricCalculateValues);

            for (uint32_t i = 0; i < totalMetricValueCount; i++) {
                EXPECT_EQ(metricValues[i].type, ZET_VALUE_TYPE_UINT64);
                EXPECT_EQ(metricValues[i].value.ui64, expectedMetricCalculateValues[i % expectedMetricCalculateValues.size()]);
            }
        }
    }
}

HWTEST2_F(MetricIpSamplingCalculateMetricGroupTest, GivenInvalidHeaderWhenCalculateMultipleMetricValuesExpIsCalledThenErrorIsReturned, HasIPSamplingSupport) {

    // Root device data (rawDataWithHeader) makes sense only for calculating with root device mg handle.
    zet_metric_group_handle_t hMetricGroup = MetricIpSamplingMultiDevFixture::getMetricGroupForDevice(rootDevice);

    std::vector<uint8_t> rawDataWithHeader(rawReportsBytesSize + sizeof(IpSamplingMultiDevDataHeader));
    MockRawDataHelper::addMultiSubDevHeader(rawDataWithHeader.data(), rawDataWithHeader.size(), reinterpret_cast<uint8_t *>(rawReports.data()), rawReportsBytesSize, 0);
    auto header = reinterpret_cast<IpSamplingMultiDevDataHeader *>(rawDataWithHeader.data());
    header->magic = IpSamplingMultiDevDataHeader::magicValue - 1;

    uint32_t setCount = 0;
    // Use random initializer
    uint32_t totalMetricValueCount = std::numeric_limits<uint32_t>::max();
    EXPECT_EQ(L0::zetMetricGroupCalculateMultipleMetricValuesExp(hMetricGroup,
                                                                 ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, rawDataWithHeader.size(), reinterpret_cast<uint8_t *>(rawDataWithHeader.data()),
                                                                 &setCount, &totalMetricValueCount, nullptr, nullptr),
              ZE_RESULT_ERROR_INVALID_ARGUMENT);
}

HWTEST2_F(MetricIpSamplingCalculateMetricGroupTest, GivenDataFromSingleDeviceWhenCalculateMultipleMetricValuesExpIsCalledThenValidDataIsReturned, HasIPSamplingSupport) {

    for (auto device : rootOneSubDev) {
        ze_device_properties_t props = {};
        device->getProperties(&props);

        if (props.flags & ZE_DEVICE_PROPERTY_FLAG_SUBDEVICE) {
            zet_metric_group_handle_t hMetricGroup = MetricIpSamplingMultiDevFixture::getMetricGroupForDevice(device);

            zet_metric_group_properties_t metricGroupProperties = {ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES, nullptr};
            EXPECT_EQ(zetMetricGroupGetProperties(hMetricGroup, &metricGroupProperties), ZE_RESULT_SUCCESS);
            uint32_t expectedResultCountPerSet = metricGroupProperties.metricCount * IpSamplingTestProductHelper::numberOfIpsInRawData;
            uint32_t setCount = 0;
            // Use random initializer
            uint32_t totalMetricValueCount = std::numeric_limits<uint32_t>::max();

            EXPECT_EQ(L0::zetMetricGroupCalculateMultipleMetricValuesExp(hMetricGroup,
                                                                         ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, rawReportsBytesSize, reinterpret_cast<uint8_t *>(rawReports.data()),
                                                                         &setCount, &totalMetricValueCount, nullptr, nullptr),
                      ZE_RESULT_SUCCESS);
            EXPECT_EQ(setCount, 1u);
            EXPECT_EQ(totalMetricValueCount, expectedResultCountPerSet);
            std::vector<uint32_t> metricCounts(setCount);
            std::vector<zet_typed_value_t> metricValues(totalMetricValueCount);
            EXPECT_EQ(L0::zetMetricGroupCalculateMultipleMetricValuesExp(hMetricGroup,
                                                                         ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, rawReportsBytesSize, reinterpret_cast<uint8_t *>(rawReports.data()),
                                                                         &setCount, &totalMetricValueCount, metricCounts.data(), metricValues.data()),
                      ZE_RESULT_SUCCESS);
            EXPECT_EQ(setCount, 1u);
            EXPECT_EQ(totalMetricValueCount, expectedResultCountPerSet);
            EXPECT_EQ(metricCounts[0], expectedResultCountPerSet);

            std::vector<uint64_t> expectedMetricCalculateValues = {};
            ipSamplingTestProductHelper->getExpectedCalculateResults(productFamily, IpSamplingTestProductHelper::CalculationResultType::CompleteResults, expectedMetricCalculateValues);

            for (uint32_t i = 0; i < totalMetricValueCount; i++) {
                EXPECT_EQ(metricValues[i].type, ZET_VALUE_TYPE_UINT64);
                EXPECT_EQ(metricValues[i].value.ui64, expectedMetricCalculateValues[i]);
            }
        }
    }
}

HWTEST2_F(MetricIpSamplingCalculateMetricGroupTest, GivenInvalidDataFromSingleDeviceWhenCalculateMultipleMetricValuesExpIsCalledThenErrorIsReturned, HasIPSamplingSupport) {

    for (auto device : rootOneSubDev) {
        ze_device_properties_t props = {};
        device->getProperties(&props);
        if (props.flags & ZE_DEVICE_PROPERTY_FLAG_SUBDEVICE) {
            zet_metric_group_handle_t hMetricGroup = MetricIpSamplingMultiDevFixture::getMetricGroupForDevice(device);

            uint32_t setCount = 0;
            uint32_t totalMetricValueCount = std::numeric_limits<uint32_t>::max();
            EXPECT_EQ(L0::zetMetricGroupCalculateMultipleMetricValuesExp(hMetricGroup,
                                                                         ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, 1,
                                                                         reinterpret_cast<uint8_t *>(rawReports.data()),
                                                                         &setCount, &totalMetricValueCount, nullptr, nullptr),
                      ZE_RESULT_ERROR_INVALID_SIZE);
        }
    }
}

HWTEST2_F(MetricIpSamplingCalculateMetricGroupTest, GivenRootDeviceDataAndLessThanRequiredMetricCountWhenCalculateMultipleMetricValuesExpIsCalledThenValidDataIsReturned, HasIPSamplingSupport) {

    // Root device data (rawDataWithHeader) makes sense only for calculating with root device mg handle.
    zet_metric_group_handle_t hMetricGroup = MetricIpSamplingMultiDevFixture::getMetricGroupForDevice(rootDevice);

    zet_metric_group_properties_t metricGroupProperties = {ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES, nullptr};
    EXPECT_EQ(zetMetricGroupGetProperties(hMetricGroup, &metricGroupProperties), ZE_RESULT_SUCCESS);
    uint32_t expectedResultCountPerSet = metricGroupProperties.metricCount * IpSamplingTestProductHelper::numberOfIpsInRawData;

    // Allocate for 2 sub-devices
    std::vector<uint8_t> rawDataWithHeader((rawReportsBytesSize + sizeof(IpSamplingMultiDevDataHeader)) * 2);
    MockRawDataHelper::addMultiSubDevHeader(rawDataWithHeader.data(), rawDataWithHeader.size(), reinterpret_cast<uint8_t *>(rawReports.data()), rawReportsBytesSize, 0);
    MockRawDataHelper::addMultiSubDevHeader(rawDataWithHeader.data() + rawReportsBytesSize + sizeof(IpSamplingMultiDevDataHeader),
                                            rawDataWithHeader.size(), reinterpret_cast<uint8_t *>(rawReports.data()), rawReportsBytesSize, 1);

    // Use random initializer
    uint32_t setCount = std::numeric_limits<uint32_t>::max();
    uint32_t totalMetricValueCount = 0;
    std::vector<uint32_t> metricCounts(2);
    EXPECT_EQ(L0::zetMetricGroupCalculateMultipleMetricValuesExp(hMetricGroup,
                                                                 ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, rawDataWithHeader.size(),
                                                                 reinterpret_cast<uint8_t *>(rawDataWithHeader.data()),
                                                                 &setCount, &totalMetricValueCount, metricCounts.data(), nullptr),
              ZE_RESULT_SUCCESS);

    EXPECT_EQ(setCount, 2u);
    EXPECT_EQ(totalMetricValueCount, expectedResultCountPerSet * setCount);
    totalMetricValueCount = 10;
    std::vector<zet_typed_value_t> metricValues(totalMetricValueCount);
    EXPECT_EQ(L0::zetMetricGroupCalculateMultipleMetricValuesExp(hMetricGroup,
                                                                 ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, rawDataWithHeader.size(),
                                                                 reinterpret_cast<uint8_t *>(rawDataWithHeader.data()),
                                                                 &setCount, &totalMetricValueCount, metricCounts.data(), metricValues.data()),
              ZE_RESULT_SUCCESS);
    EXPECT_EQ(setCount, 2u);
    EXPECT_EQ(totalMetricValueCount, 10u);
    EXPECT_EQ(metricCounts[0], 10u);

    std::vector<uint64_t> expectedMetricCalculateValues = {};
    ipSamplingTestProductHelper->getExpectedCalculateResults(productFamily, IpSamplingTestProductHelper::CalculationResultType::CompleteResults, expectedMetricCalculateValues);

    for (uint32_t i = 0; i < totalMetricValueCount; i++) {
        EXPECT_EQ(metricValues[i].type, ZET_VALUE_TYPE_UINT64);
        EXPECT_EQ(metricValues[i].value.ui64, expectedMetricCalculateValues[i]);
    }
}

HWTEST2_F(MetricIpSamplingCalculateMetricGroupTest, GivenInvalidRawDataSizeWhenCalculateMultipleMetricValuesExpIsCalledThenErrorIsReturned, HasIPSamplingSupport) {

    for (auto device : rootOneSubDev) {
        zet_metric_group_handle_t hMetricGroup = MetricIpSamplingMultiDevFixture::getMetricGroupForDevice(device);

        std::vector<uint8_t> rawDataWithHeader(rawReportsBytesSize + sizeof(IpSamplingMultiDevDataHeader) + 1);
        MockRawDataHelper::addMultiSubDevHeader(rawDataWithHeader.data(), rawDataWithHeader.size(), reinterpret_cast<uint8_t *>(rawReports.data()), rawReportsBytesSize, 0);

        uint32_t setCount = 0;
        uint32_t totalMetricValueCount = 0;
        EXPECT_EQ(L0::zetMetricGroupCalculateMultipleMetricValuesExp(hMetricGroup,
                                                                     ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, rawDataWithHeader.size() + 1, reinterpret_cast<uint8_t *>(rawDataWithHeader.data()),
                                                                     &setCount, &totalMetricValueCount, nullptr, nullptr),
                  ZE_RESULT_ERROR_INVALID_ARGUMENT);
    }
}

HWTEST2_F(MetricIpSamplingCalculateMetricGroupTest, GivenInvalidRawDataSizeDuringValueCalculationPhaseWhenCalculateMultipleMetricValuesExpIsCalledThenErrorIsReturned, HasIPSamplingSupport) {

    std::vector<zet_typed_value_t> metricValues(30);

    for (auto device : rootOneSubDev) {
        bool isRootdevice = false;
        ze_device_properties_t props = {};
        device->getProperties(&props);
        if (!(props.flags & ZE_DEVICE_PROPERTY_FLAG_SUBDEVICE)) {
            isRootdevice = true;
        }

        zet_metric_group_handle_t hMetricGroup = MetricIpSamplingMultiDevFixture::getMetricGroupForDevice(device);

        std::vector<uint8_t> rawData;
        if (isRootdevice) { // Root device data (rawDataWithHeader) makes sense only for calculating with root device mg handle.
            rawData.resize(rawReportsBytesSize + sizeof(IpSamplingMultiDevDataHeader));
            MockRawDataHelper::addMultiSubDevHeader(rawData.data(), rawData.size(), reinterpret_cast<uint8_t *>(rawReports.data()), rawReportsBytesSize, 0);
        } else {
            rawData.resize(rawReportsBytesSize);
            memcpy_s(rawData.data(), rawData.size(), reinterpret_cast<uint8_t *>(rawReports.data()), rawReportsBytesSize);
        }

        uint32_t setCount = 0;
        uint32_t totalMetricValueCount = 0;
        std::vector<uint32_t> metricCounts(2);
        EXPECT_EQ(L0::zetMetricGroupCalculateMultipleMetricValuesExp(hMetricGroup, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES,
                                                                     rawData.size(), reinterpret_cast<uint8_t *>(rawData.data()),
                                                                     &setCount, &totalMetricValueCount, metricCounts.data(), nullptr),
                  ZE_RESULT_SUCCESS);
        // Force incorrect raw data size
        size_t rawDatSize = rawData.size();
        if (isRootdevice) {
            auto header = reinterpret_cast<IpSamplingMultiDevDataHeader *>(rawData.data());
            header->rawDataSize = static_cast<uint32_t>(rawReportsBytesSize - 1);
        } else {
            rawDatSize = rawData.size() - 1;
        }

        EXPECT_EQ(L0::zetMetricGroupCalculateMultipleMetricValuesExp(hMetricGroup, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES,
                                                                     rawDatSize, reinterpret_cast<uint8_t *>(rawData.data()),
                                                                     &setCount, &totalMetricValueCount, metricCounts.data(), metricValues.data()),
                  ZE_RESULT_ERROR_INVALID_SIZE);
    }
}

HWTEST2_F(MetricIpSamplingCalculateMetricGroupTest, GivenInvalidRawDataSizeInHeaderDuringSizeCalculationWhenCalculateMultipleMetricValuesExpThenErrorIsReturned, HasIPSamplingSupport) {

    // Root device data (rawDataWithHeader) makes sense only for calculating with root device mg handle.
    zet_metric_group_handle_t hMetricGroup = MetricIpSamplingMultiDevFixture::getMetricGroupForDevice(rootDevice);

    std::vector<uint8_t> rawDataWithHeader(rawReportsBytesSize + sizeof(IpSamplingMultiDevDataHeader) + 1);
    MockRawDataHelper::addMultiSubDevHeader(rawDataWithHeader.data(), rawDataWithHeader.size(), reinterpret_cast<uint8_t *>(rawReports.data()), rawReportsBytesSize - 1, 0);

    uint32_t setCount = 0;
    uint32_t totalMetricValueCount = 0;
    std::vector<uint32_t> metricCounts(2);
    EXPECT_EQ(L0::zetMetricGroupCalculateMultipleMetricValuesExp(hMetricGroup,
                                                                 ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, rawDataWithHeader.size(), reinterpret_cast<uint8_t *>(rawDataWithHeader.data()),
                                                                 &setCount, &totalMetricValueCount, metricCounts.data(), nullptr),
              ZE_RESULT_ERROR_INVALID_SIZE);
}

HWTEST2_F(MetricIpSamplingCalculateMetricGroupTest, GivenInvalidRawDataSizeInHeaderWhenCalculateMultipleMetricValuesExpCalculateSizeIsCalledThenErrorIsReturned, HasIPSamplingSupport) {

    // Root device data (rawDataWithHeader) makes sense only for calculating with root device mg handle.
    zet_metric_group_handle_t hMetricGroup = MetricIpSamplingMultiDevFixture::getMetricGroupForDevice(rootDevice);

    std::vector<uint8_t> rawDataWithHeader(rawReportsBytesSize + sizeof(IpSamplingMultiDevDataHeader) + 1);
    MockRawDataHelper::addMultiSubDevHeader(rawDataWithHeader.data(), rawDataWithHeader.size(), reinterpret_cast<uint8_t *>(rawReports.data()), rawReportsBytesSize - 1, 0);

    uint32_t setCount = 0;
    uint32_t totalMetricValueCount = 10;
    std::vector<uint32_t> metricCounts(2);
    std::vector<zet_typed_value_t> metricValues(30);
    EXPECT_EQ(L0::zetMetricGroupCalculateMultipleMetricValuesExp(hMetricGroup,
                                                                 ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, rawDataWithHeader.size(), reinterpret_cast<uint8_t *>(rawDataWithHeader.data()),
                                                                 &setCount, &totalMetricValueCount, metricCounts.data(), metricValues.data()),
              ZE_RESULT_ERROR_INVALID_SIZE);
}

HWTEST2_F(MetricIpSamplingCalculateMetricGroupTest, GivenInvalidRawDataSizeInHeaderWhenCalledWhenCalculateMultipleMetricValuesExpCalculateDataThenErrorUnknownIsReturned, HasIPSamplingSupport) {

    // Root device data (rawDataWithHeader) makes sense only for calculating with root device mg handle.
    zet_metric_group_handle_t hMetricGroup = MetricIpSamplingMultiDevFixture::getMetricGroupForDevice(rootDevice);

    zet_metric_group_properties_t metricGroupProperties = {ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES, nullptr};
    EXPECT_EQ(zetMetricGroupGetProperties(hMetricGroup, &metricGroupProperties), ZE_RESULT_SUCCESS);
    uint32_t expectedResultCountPerSet = metricGroupProperties.metricCount * IpSamplingTestProductHelper::numberOfIpsInRawData;

    // Allocate for 2 sub-devices
    std::vector<uint8_t> rawDataWithHeader((rawReportsBytesSize + sizeof(IpSamplingMultiDevDataHeader)) * 2);
    MockRawDataHelper::addMultiSubDevHeader(rawDataWithHeader.data(), rawDataWithHeader.size(), reinterpret_cast<uint8_t *>(rawReports.data()), rawReportsBytesSize, 0);
    MockRawDataHelper::addMultiSubDevHeader(rawDataWithHeader.data() + rawReportsBytesSize + sizeof(IpSamplingMultiDevDataHeader),
                                            rawDataWithHeader.size(), reinterpret_cast<uint8_t *>(rawReports.data()), rawReportsBytesSize, 1);

    uint32_t setCount = 0;
    uint32_t totalMetricValueCount = 0;
    std::vector<uint32_t> metricCounts(2);
    EXPECT_EQ(L0::zetMetricGroupCalculateMultipleMetricValuesExp(hMetricGroup,
                                                                 ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, rawDataWithHeader.size(), reinterpret_cast<uint8_t *>(rawDataWithHeader.data()),
                                                                 &setCount, &totalMetricValueCount, metricCounts.data(), nullptr),
              ZE_RESULT_SUCCESS);

    EXPECT_EQ(setCount, 2u);
    EXPECT_EQ(totalMetricValueCount, expectedResultCountPerSet * setCount);
    std::vector<zet_typed_value_t> metricValues(totalMetricValueCount);
    totalMetricValueCount += 1;
    EXPECT_EQ(L0::zetMetricGroupCalculateMultipleMetricValuesExp(hMetricGroup,
                                                                 ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, rawDataWithHeader.size() + 1, reinterpret_cast<uint8_t *>(rawDataWithHeader.data()),
                                                                 &setCount, &totalMetricValueCount, metricCounts.data(), metricValues.data()),
              ZE_RESULT_ERROR_INVALID_ARGUMENT);
}

HWTEST2_F(MetricIpSamplingCalculateMetricGroupTest, GivenSubDeviceDataWhenCalculateMetricValuesIsCalledThenValidDataIsReturned, HasIPSamplingSupport) {

    for (auto device : rootOneSubDev) {
        bool isRootdevice = false;
        ze_result_t expectedResult = ZE_RESULT_SUCCESS;
        ze_device_properties_t props = {};
        device->getProperties(&props);

        if (!(props.flags & ZE_DEVICE_PROPERTY_FLAG_SUBDEVICE)) {
            // Sub-device data (rawReports) will work only with sub-device metric group handle.
            // Calling calculate with root-device metric group handle will return INVALID ARGUMENT.
            isRootdevice = true;
            expectedResult = ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }

        zet_metric_group_handle_t hMetricGroup = MetricIpSamplingMultiDevFixture::getMetricGroupForDevice(device);

        uint32_t metricValueCount = 0;
        EXPECT_EQ(zetMetricGroupCalculateMetricValues(hMetricGroup, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES,
                                                      rawReportsBytesSize, reinterpret_cast<uint8_t *>(rawReports.data()), &metricValueCount, nullptr),
                  expectedResult);
        if (!isRootdevice) {
            zet_metric_group_properties_t metricGroupProperties = {ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES, nullptr};
            EXPECT_EQ(zetMetricGroupGetProperties(hMetricGroup, &metricGroupProperties), ZE_RESULT_SUCCESS);
            uint32_t expectedResultCount = metricGroupProperties.metricCount * IpSamplingTestProductHelper::numberOfIpsInRawData;
            std::vector<zet_typed_value_t> metricValues(expectedResultCount);
            EXPECT_EQ(zetMetricGroupCalculateMetricValues(hMetricGroup, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES,
                                                          rawReportsBytesSize, reinterpret_cast<uint8_t *>(rawReports.data()), &metricValueCount, metricValues.data()),
                      expectedResult);

            EXPECT_EQ(metricValueCount, expectedResultCount);

            std::vector<uint64_t> expectedMetricCalculateValues = {};
            ipSamplingTestProductHelper->getExpectedCalculateResults(productFamily, IpSamplingTestProductHelper::CalculationResultType::CompleteResults, expectedMetricCalculateValues);

            for (uint32_t i = 0; i < metricValueCount; i++) {
                EXPECT_EQ(metricValues[i].type, ZET_VALUE_TYPE_UINT64);
                EXPECT_EQ(metricValues[i].value.ui64, expectedMetricCalculateValues[i]);
            }
        }
    }
}

HWTEST2_F(MetricIpSamplingCalculateMetricGroupTest, GivenRootDeviceDataWhenCalculateMetricValuesIsCalledThenReturnError, HasIPSamplingSupport) {

    for (auto device : rootOneSubDev) {
        zet_metric_group_handle_t hMetricGroup = MetricIpSamplingMultiDevFixture::getMetricGroupForDevice(device);

        std::vector<uint8_t> rawDataWithHeader(rawReportsBytesSize + sizeof(IpSamplingMultiDevDataHeader));
        MockRawDataHelper::addMultiSubDevHeader(rawDataWithHeader.data(), rawDataWithHeader.size(), reinterpret_cast<uint8_t *>(rawReports.data()), rawReportsBytesSize, 0);
        uint32_t metricValueCount = 0;
        EXPECT_EQ(zetMetricGroupCalculateMetricValues(hMetricGroup, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES,
                                                      rawDataWithHeader.size(), reinterpret_cast<uint8_t *>(rawDataWithHeader.data()),
                                                      &metricValueCount, nullptr),
                  ZE_RESULT_ERROR_INVALID_ARGUMENT);
    }
}

HWTEST2_F(MetricIpSamplingCalculateMetricGroupTest, GivenSmallValueCountWhenCalculateMetricValuesIsCalledThenValidDataIsReturned, HasIPSamplingSupport) {

    for (auto device : rootOneSubDev) {
        ze_device_properties_t props = {};
        device->getProperties(&props);
        if (props.flags & ZE_DEVICE_PROPERTY_FLAG_SUBDEVICE) { // Sub-device data (rawReports) will work only with sub-device metric group handle.
            zet_metric_group_handle_t hMetricGroup = MetricIpSamplingMultiDevFixture::getMetricGroupForDevice(device);

            zet_metric_group_properties_t metricGroupProperties = {ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES, nullptr};
            EXPECT_EQ(zetMetricGroupGetProperties(hMetricGroup, &metricGroupProperties), ZE_RESULT_SUCCESS);
            uint32_t expectedResultCount = metricGroupProperties.metricCount * IpSamplingTestProductHelper::numberOfIpsInRawData;
            std::vector<zet_typed_value_t> metricValues(expectedResultCount);

            uint32_t metricValueCount = 0;
            EXPECT_EQ(zetMetricGroupCalculateMetricValues(hMetricGroup, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES,
                                                          rawReportsBytesSize, reinterpret_cast<uint8_t *>(rawReports.data()), &metricValueCount, nullptr),
                      ZE_RESULT_SUCCESS);

            EXPECT_EQ(metricValueCount, expectedResultCount);
            metricValueCount = 15;
            EXPECT_EQ(zetMetricGroupCalculateMetricValues(hMetricGroup, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES,
                                                          rawReportsBytesSize, reinterpret_cast<uint8_t *>(rawReports.data()), &metricValueCount, metricValues.data()),
                      ZE_RESULT_SUCCESS);
            EXPECT_EQ(metricValueCount, 15U);

            std::vector<uint64_t> expectedMetricCalculateValues = {};
            ipSamplingTestProductHelper->getExpectedCalculateResults(productFamily, IpSamplingTestProductHelper::CalculationResultType::CompleteResults, expectedMetricCalculateValues);

            for (uint32_t i = 0; i < metricValueCount; i++) {
                EXPECT_EQ(metricValues[i].type, ZET_VALUE_TYPE_UINT64);
                EXPECT_EQ(metricValues[i].value.ui64, expectedMetricCalculateValues[i]);
            }
        }
    }
}

HWTEST2_F(MetricIpSamplingCalculateMetricGroupTest, GivenBadRawDataSizeWhenCalculateMetricValuesIsCalledForValueCountThenErrorIsReturned, HasIPSamplingSupport) {

    for (auto device : rootOneSubDev) {
        ze_device_properties_t props = {};
        device->getProperties(&props);
        if (props.flags & ZE_DEVICE_PROPERTY_FLAG_SUBDEVICE) { // Sub-device data (rawReports) will work only with sub-device metric group handle.
            zet_metric_group_handle_t hMetricGroup = MetricIpSamplingMultiDevFixture::getMetricGroupForDevice(device);

            uint32_t metricValueCount = 0;
            EXPECT_EQ(zetMetricGroupCalculateMetricValues(hMetricGroup, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES,
                                                          rawReportsBytesSize - 1, reinterpret_cast<uint8_t *>(rawReports.data()), &metricValueCount, nullptr),
                      ZE_RESULT_ERROR_INVALID_SIZE);
        }
    }
}

HWTEST2_F(MetricIpSamplingCalculateMetricGroupTest, GivenBadRawDataSizeDuringCalculationWhenCalculateMetricValuesIsCalledThenErrorIsReturned, HasIPSamplingSupport) {

    for (auto device : rootOneSubDev) {
        ze_device_properties_t props = {};
        device->getProperties(&props);
        if (props.flags & ZE_DEVICE_PROPERTY_FLAG_SUBDEVICE) { // Sub-device data (rawReports) will work only with sub-device metric group handle.

            zet_metric_group_handle_t hMetricGroup = MetricIpSamplingMultiDevFixture::getMetricGroupForDevice(device);
            zet_metric_group_properties_t metricGroupProperties = {ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES, nullptr};
            EXPECT_EQ(zetMetricGroupGetProperties(hMetricGroup, &metricGroupProperties), ZE_RESULT_SUCCESS);
            uint32_t expectedResultCount = metricGroupProperties.metricCount * IpSamplingTestProductHelper::numberOfIpsInRawData;

            uint32_t metricValueCount = 0;
            EXPECT_EQ(zetMetricGroupCalculateMetricValues(hMetricGroup, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES,
                                                          rawReportsBytesSize, reinterpret_cast<uint8_t *>(rawReports.data()), &metricValueCount, nullptr),
                      ZE_RESULT_SUCCESS);

            EXPECT_EQ(metricValueCount, expectedResultCount);
            std::vector<zet_typed_value_t> metricValues(expectedResultCount);
            EXPECT_EQ(zetMetricGroupCalculateMetricValues(hMetricGroup, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES,
                                                          rawReportsBytesSize - 1, reinterpret_cast<uint8_t *>(rawReports.data()), &metricValueCount, metricValues.data()),
                      ZE_RESULT_ERROR_INVALID_SIZE);
        }
    }
}

// Only PVC has the overflow bit in raw data, so this test is only for PVC. This test verifies that when overflow happens and streamer read data is called,
// calculate will return valid metrics values with overflow warning.
HWTEST2_F(MetricIpSamplingCalculateMetricGroupTest, GivenDataOverflowOccurredWhenStreamerReadDataIscalledThenCalculateMultipleMetricsValuesExpReturnsOverflowWarning, IsPVC) {
    for (auto device : rootOneSubDev) {
        uint32_t expectedSetCount = 1u;
        bool isRootdevice = false;
        ze_device_properties_t props = {};
        device->getProperties(&props);
        if (!(props.flags & ZE_DEVICE_PROPERTY_FLAG_SUBDEVICE)) {
            isRootdevice = true;
            expectedSetCount = 2u;
        }

        zet_metric_group_handle_t hMetricGroup = MetricIpSamplingMultiDevFixture::getMetricGroupForDevice(device);
        zet_metric_group_properties_t metricGroupProperties = {ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES, nullptr};
        EXPECT_EQ(zetMetricGroupGetProperties(hMetricGroup, &metricGroupProperties), ZE_RESULT_SUCCESS);
        uint32_t expectedResultCountPerSet = metricGroupProperties.metricCount * IpSamplingTestProductHelper::numberOfIpsInRawData;

        // Prepare raw data with overflow bit set.
        //  NOTE: Override the default fixture SeTup() for rawReports
        std::vector<std::array<uint64_t, 8>> rawReportsOverflow;
        ipSamplingTestProductHelper->rawElementsToRawReports(productFamily, IpSamplingTestProductHelper::InitReportType::OverflowType, &rawReportsOverflow);

        std::vector<uint8_t> rawData;
        if (isRootdevice) { // Root device data (rawDataWithHeader) makes sense only for calculating with root device mg handle.
            rawData.resize((rawReportsBytesSize + sizeof(IpSamplingMultiDevDataHeader)) * 2);
            MockRawDataHelper::addMultiSubDevHeader(rawData.data(), rawData.size(), reinterpret_cast<uint8_t *>(rawReportsOverflow.data()), rawReportsBytesSize, 0);
            MockRawDataHelper::addMultiSubDevHeader(rawData.data() + rawReportsBytesSize + sizeof(IpSamplingMultiDevDataHeader),
                                                    rawData.size(), reinterpret_cast<uint8_t *>(rawReportsOverflow.data()), rawReportsBytesSize, 1);
        } else {
            rawData.resize(rawReportsBytesSize);
            memcpy_s(rawData.data(), rawData.size(), reinterpret_cast<uint8_t *>(rawReportsOverflow.data()), rawReportsBytesSize);
        }

        uint32_t setCount = 0;
        uint32_t totalMetricValueCount = 0;
        std::vector<uint32_t> metricCounts(2);
        EXPECT_EQ(L0::zetMetricGroupCalculateMultipleMetricValuesExp(hMetricGroup, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES,
                                                                     rawData.size(), reinterpret_cast<uint8_t *>(rawData.data()),
                                                                     &setCount, &totalMetricValueCount, metricCounts.data(), nullptr),
                  ZE_RESULT_SUCCESS);

        EXPECT_EQ(setCount, expectedSetCount);
        EXPECT_EQ(totalMetricValueCount, expectedResultCountPerSet * setCount);
        std::vector<zet_typed_value_t> metricValues(totalMetricValueCount, {ZET_VALUE_TYPE_UINT32, {0U}});
        EXPECT_EQ(L0::zetMetricGroupCalculateMultipleMetricValuesExp(hMetricGroup, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES,
                                                                     rawData.size(), reinterpret_cast<uint8_t *>(rawData.data()),
                                                                     &setCount, &totalMetricValueCount, metricCounts.data(), metricValues.data()),
                  ZE_RESULT_WARNING_DROPPED_DATA);

        EXPECT_EQ(setCount, expectedSetCount);
        EXPECT_EQ(totalMetricValueCount, expectedResultCountPerSet * setCount);
        EXPECT_EQ(metricCounts[0], expectedResultCountPerSet);

        std::vector<uint64_t> expectedMetricCalculateValues = {};
        ipSamplingTestProductHelper->getExpectedCalculateResults(productFamily, IpSamplingTestProductHelper::CalculationResultType::CompleteResults, expectedMetricCalculateValues);

        for (uint32_t i = 0; i < totalMetricValueCount; i++) {
            EXPECT_EQ(metricValues[i].type, ZET_VALUE_TYPE_UINT64);
            EXPECT_EQ(metricValues[i].value.ui64, expectedMetricCalculateValues[i % expectedMetricCalculateValues.size()]);
        }
    }
}

HWTEST2_F(MetricIpSamplingCalculateMetricGroupTest, GivenRequestForMaxMetricValuesWhenCalculateMetricValuesIsCalledThenErrorUnsupportedFeatureIsReturned, HasIPSamplingSupport) {

    std::vector<zet_typed_value_t> metricValues(30);

    for (auto device : rootOneSubDev) {
        ze_device_properties_t props = {};
        device->getProperties(&props);
        if (props.flags & ZE_DEVICE_PROPERTY_FLAG_SUBDEVICE) { // Sub-device data (rawReports) will work only with sub-device metric group handle.
            zet_metric_group_handle_t hMetricGroup = MetricIpSamplingMultiDevFixture::getMetricGroupForDevice(device);
            zet_metric_group_properties_t metricGroupProperties = {ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES, nullptr};
            EXPECT_EQ(zetMetricGroupGetProperties(hMetricGroup, &metricGroupProperties), ZE_RESULT_SUCCESS);
            uint32_t expectedResultCountPerSet = metricGroupProperties.metricCount * IpSamplingTestProductHelper::numberOfIpsInRawData;

            uint32_t metricValueCount = 0;
            EXPECT_EQ(zetMetricGroupCalculateMetricValues(hMetricGroup, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES,
                                                          rawReportsBytesSize, reinterpret_cast<uint8_t *>(rawReports.data()), &metricValueCount, nullptr),
                      ZE_RESULT_SUCCESS);

            EXPECT_EQ(metricValueCount, expectedResultCountPerSet);
            std::vector<zet_typed_value_t> metricValues(expectedResultCountPerSet);
            EXPECT_EQ(zetMetricGroupCalculateMetricValues(hMetricGroup, ZET_METRIC_GROUP_CALCULATION_TYPE_MAX_METRIC_VALUES,
                                                          rawReportsBytesSize, reinterpret_cast<uint8_t *>(rawReports.data()), &metricValueCount, metricValues.data()),
                      ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
        }
    }
}

HWTEST2_F(MetricIpSamplingEnumerationTest, WhenQueryPoolCreateIsCalledThenUnsupportedFeatureIsReturned, HasIPSamplingSupport) {

    for (auto device : rootOneSubDev) {
        zet_metric_group_handle_t hMetricGroup = MetricIpSamplingMultiDevFixture::getMetricGroupForDevice(device);

        zet_metric_query_pool_desc_t poolDesc = {};
        poolDesc.stype = ZET_STRUCTURE_TYPE_METRIC_QUERY_POOL_DESC;
        poolDesc.count = 1;
        poolDesc.type = ZET_METRIC_QUERY_POOL_TYPE_PERFORMANCE;
        EXPECT_EQ(zetMetricQueryPoolCreate(context->toHandle(), device, hMetricGroup, &poolDesc, nullptr), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
    }
}

HWTEST2_F(MetricIpSamplingEnumerationTest, WhenAppendMetricMemoryBarrierIsCalledThenUnsupportedFeatureIsReturned, HasIPSamplingSupport) {

    auto &device = testDevices[0];
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false));
    zet_command_list_handle_t commandListHandle = commandList->toHandle();
    EXPECT_EQ(zetCommandListAppendMetricMemoryBarrier(commandListHandle), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
}

using MetricExportDataIpSamplingTest = MetricIpSamplingMultiDevFixture;

HWTEST2_F(MetricExportDataIpSamplingTest, WhenMetricGroupGetExportDataIsCalledThenReturnSuccess, HasIPSamplingSupport) {

    for (auto device : rootOneSubDev) {
        zet_metric_group_handle_t hMetricGroup = MetricIpSamplingMultiDevFixture::getMetricGroupForDevice(device);

        uint8_t dummyRawData = 8;
        size_t exportDataSize = 0;
        const auto dummyRawDataSize = 1u;
        EXPECT_EQ(zetMetricGroupGetExportDataExp(hMetricGroup,
                                                 &dummyRawData, dummyRawDataSize, &exportDataSize, nullptr),
                  ZE_RESULT_SUCCESS);
        EXPECT_GE(exportDataSize, 0u);
        std::vector<uint8_t> exportDataMem(exportDataSize);
        EXPECT_EQ(zetMetricGroupGetExportDataExp(hMetricGroup,
                                                 &dummyRawData, dummyRawDataSize, &exportDataSize, exportDataMem.data()),
                  ZE_RESULT_SUCCESS);
        zet_intel_metric_df_gpu_export_data_format_t *exportData = reinterpret_cast<zet_intel_metric_df_gpu_export_data_format_t *>(exportDataMem.data());
        EXPECT_EQ(exportData->header.type, ZET_INTEL_METRIC_DF_SOURCE_TYPE_IPSAMPLING);
        EXPECT_EQ(dummyRawData, exportDataMem[exportData->header.rawDataOffset]);
    }
}

HWTEST2_F(MetricExportDataIpSamplingTest, GivenIncorrectExportDataSizeWhenMetricGroupGetExportDataIsCalledThenErrorIsReturned, HasIPSamplingSupport) {

    for (auto device : rootOneSubDev) {
        zet_metric_group_handle_t hMetricGroup = MetricIpSamplingMultiDevFixture::getMetricGroupForDevice(device);

        uint8_t dummyRawData = 0;
        size_t exportDataSize = 0;
        const auto dummyRawDataSize = 1u;
        EXPECT_EQ(zetMetricGroupGetExportDataExp(hMetricGroup,
                                                 &dummyRawData, dummyRawDataSize, &exportDataSize, nullptr),
                  ZE_RESULT_SUCCESS);
        EXPECT_GE(exportDataSize, 0u);
        exportDataSize -= 1;
        std::vector<uint8_t> exportDataMem(exportDataSize);
        EXPECT_EQ(zetMetricGroupGetExportDataExp(hMetricGroup,
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

HWTEST2_F(MetricIpSamplingEnumerationTest, WhenActivationFailsThenErrorIsReturned, HasIPSamplingSupport) {

    for (auto device : rootOneSubDev) {
        auto &metricSource = (static_cast<Device *>(device))->getMetricDeviceContext().getMetricSource<IpSamplingMetricSourceImp>();
        MockMultiDomainDeferredActivationTracker *mockTracker = new MockMultiDomainDeferredActivationTracker(0);
        metricSource.setActivationTracker(mockTracker);
    }

    for (auto device : rootOneSubDev) {
        zet_metric_group_handle_t hMetricGroup = MetricIpSamplingMultiDevFixture::getMetricGroupForDevice(device);

        EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), device->toHandle(), 1, &hMetricGroup), ZE_RESULT_ERROR_UNKNOWN);
    }
}

HWTEST2_F(MetricIpSamplingEnumerationTest, WhenUnsupportedFunctionsAreCalledThenUnsupportedErrorIsReturned, HasIPSamplingSupport) {

    for (auto device : rootOneSubDev) {
        auto &metricSource = (static_cast<Device *>(device))->getMetricDeviceContext().getMetricSource<IpSamplingMetricSourceImp>();

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

HWTEST2_F(MetricIpSamplingEnumerationTest, WhenUnsupportedFunctionsForDeviceContextAreCalledUnsupportedErrorIsReturned, HasIPSamplingSupport) {

    for (auto device : rootOneSubDev) {
        auto &deviceContext = (static_cast<Device *>(device))->getMetricDeviceContext();

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

HWTEST2_F(MetricIpSamplingEnumerationTest, GivenValidIpSamplingSourceThenComputeMetricScopesAreEnumeratedOnce, HasIPSamplingSupport) {

    MetricDeviceContext &metricsDevContext = testDevices[0]->getMetricDeviceContext();
    EXPECT_EQ(ZE_RESULT_SUCCESS, metricsDevContext.enableMetricApi());

    metricsDevContext.setComputeMetricScopeInitialized();

    auto &metricSource = metricsDevContext.getMetricSource<IpSamplingMetricSourceImp>();
    EXPECT_EQ(metricSource.isAvailable(), true);

    uint32_t metricScopesCount = 0;
    metricsDevContext.metricScopesGet(context->toHandle(), &metricScopesCount, nullptr);
    EXPECT_EQ(metricScopesCount, 0u);
}

} // namespace ult
} // namespace L0
