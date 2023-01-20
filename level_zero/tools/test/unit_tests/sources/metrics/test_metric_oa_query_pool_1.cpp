/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test_base.h"

#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_device.h"
#include "level_zero/core/test/unit_tests/mocks/mock_driver_handle.h"
#include "level_zero/tools/source/metrics/metric_oa_source.h"
#include "level_zero/tools/test/unit_tests/sources/metrics/metric_query_pool_fixture.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

using ::testing::_;
using ::testing::Return;

namespace L0 {
namespace ult {
TEST_F(MetricQueryPoolTest, givenCorrectArgumentsWhenZetMetricQueryPoolCreateIsCalledThenQueryPoolIsObtained) {

    zet_device_handle_t metricDevice = device->toHandle();

    Mock<MetricGroup> metricGroup;
    zet_metric_group_properties_t metricGroupProperties = {};
    metricGroupProperties.samplingType = ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_EVENT_BASED;

    zet_metric_query_pool_handle_t poolHandle = {};
    zet_metric_query_pool_desc_t poolDesc = {};
    poolDesc.stype = ZET_STRUCTURE_TYPE_METRIC_QUERY_POOL_DESC;
    poolDesc.count = 1;
    poolDesc.type = ZET_METRIC_QUERY_POOL_TYPE_PERFORMANCE;

    TypedValue_1_0 value = {};
    value.Type = ValueType::Uint32;
    value.ValueUInt32 = 64;

    QueryHandle_1_0 queryHandle = {&value};
    ContextHandle_1_0 contextHandle = {&value};

    EXPECT_CALL(*mockMetricEnumeration, isInitialized())
        .Times(1)
        .WillOnce(Return(true));

    EXPECT_CALL(*mockMetricsLibrary, getContextData(_, _))
        .Times(1)
        .WillOnce(Return(true));

    EXPECT_CALL(*mockMetricsLibrary, load())
        .Times(0);

    EXPECT_CALL(metricGroup, getProperties(_))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<0>(metricGroupProperties), Return(ZE_RESULT_SUCCESS)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockQueryCreate(_, _))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<1>(queryHandle), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockQueryDelete(_))
        .Times(1)
        .WillOnce(Return(StatusCode::Success));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockGetParameter(_, _, _))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<2>(value), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockContextCreate(_, _, _))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<2>(contextHandle), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockContextDelete(_))
        .Times(1)
        .WillOnce(Return(StatusCode::Success));

    EXPECT_EQ(zetMetricQueryPoolCreate(context->toHandle(), metricDevice, metricGroup.toHandle(), &poolDesc, &poolHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(poolHandle, nullptr);
    EXPECT_EQ(zetMetricQueryPoolDestroy(poolHandle), ZE_RESULT_SUCCESS);
}

TEST_F(MetricQueryPoolTest, givenIncorrectMetricGroupTypeWhenZetMetricQueryPoolCreateIsCalledThenReturnsFail) {

    zet_device_handle_t metricDevice = device->toHandle();

    Mock<MetricGroup> metricGroup;
    zet_metric_group_properties_t metricGroupProperties = {};
    metricGroupProperties.samplingType = ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_TIME_BASED;

    zet_metric_query_pool_handle_t poolHandle = {};
    zet_metric_query_pool_desc_t poolDesc = {};
    poolDesc.stype = ZET_STRUCTURE_TYPE_METRIC_QUERY_POOL_DESC;
    poolDesc.count = 1;
    poolDesc.type = ZET_METRIC_QUERY_POOL_TYPE_PERFORMANCE;

    TypedValue_1_0 value = {};
    value.Type = ValueType::Uint32;
    value.ValueUInt32 = 64;

    ContextHandle_1_0 contextHandle = {&value};

    EXPECT_CALL(*mockMetricEnumeration, isInitialized())
        .Times(0);

    EXPECT_CALL(*mockMetricsLibrary, getContextData(_, _))
        .Times(0);

    EXPECT_CALL(*mockMetricsLibrary, load())
        .Times(0);

    EXPECT_CALL(metricGroup, getProperties(_))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<0>(metricGroupProperties), Return(ZE_RESULT_SUCCESS)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockQueryCreate(_, _))
        .Times(0);

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockQueryDelete(_))
        .Times(0);

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockGetParameter(_, _, _))
        .Times(0);

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockContextCreate(_, _, _))
        .Times(0);

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockContextDelete(_))
        .Times(0);

    EXPECT_EQ(zetMetricQueryPoolCreate(context->toHandle(), metricDevice, metricGroup.toHandle(), &poolDesc, &poolHandle), ZE_RESULT_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(poolHandle, nullptr);
}

TEST_F(MetricQueryPoolTest, givenIncorrectParameterWhenZetMetricQueryPoolCreateIsCalledThenReturnsFail) {

    zet_device_handle_t metricDevice = device->toHandle();

    Mock<MetricGroup> metricGroup;
    zet_metric_group_properties_t metricGroupProperties = {};
    metricGroupProperties.samplingType = ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_EVENT_BASED;

    zet_metric_query_pool_handle_t poolHandle = {};
    zet_metric_query_pool_desc_t poolDesc = {};
    poolDesc.stype = ZET_STRUCTURE_TYPE_METRIC_QUERY_POOL_DESC;
    poolDesc.count = 1;
    poolDesc.type = ZET_METRIC_QUERY_POOL_TYPE_PERFORMANCE;

    TypedValue_1_0 value = {};
    value.Type = ValueType::Uint32;
    value.ValueUInt32 = 64;

    QueryHandle_1_0 queryHandle = {&value};
    ContextHandle_1_0 contextHandle = {&value};

    EXPECT_CALL(*mockMetricEnumeration, isInitialized())
        .Times(1)
        .WillOnce(Return(true));

    EXPECT_CALL(*mockMetricsLibrary, getContextData(_, _))
        .Times(1)
        .WillOnce(Return(true));

    EXPECT_CALL(*mockMetricsLibrary, load())
        .Times(0);

    EXPECT_CALL(metricGroup, getProperties(_))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<0>(metricGroupProperties), Return(ZE_RESULT_SUCCESS)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockQueryCreate(_, _))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<1>(queryHandle), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockQueryDelete(_))
        .Times(1)
        .WillOnce(Return(StatusCode::Success));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockGetParameter(_, _, _))
        .Times(1)
        .WillOnce(Return(StatusCode::Failed));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockContextCreate(_, _, _))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<2>(contextHandle), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockContextDelete(_))
        .Times(1)
        .WillOnce(Return(StatusCode::Success));

    EXPECT_EQ(zetMetricQueryPoolCreate(context->toHandle(), metricDevice, metricGroup.toHandle(), &poolDesc, &poolHandle), ZE_RESULT_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(poolHandle, nullptr);
}

TEST_F(MetricQueryPoolTest, givenIncorrectContextWhenZetMetricQueryPoolCreateIsCalledThenReturnsFail) {

    zet_device_handle_t metricDevice = device->toHandle();

    Mock<MetricGroup> metricGroup;
    zet_metric_group_properties_t metricGroupProperties = {};
    metricGroupProperties.samplingType = ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_EVENT_BASED;

    zet_metric_query_pool_handle_t poolHandle = {};
    zet_metric_query_pool_desc_t poolDesc = {};
    poolDesc.stype = ZET_STRUCTURE_TYPE_METRIC_QUERY_POOL_DESC;
    poolDesc.count = 1;
    poolDesc.type = ZET_METRIC_QUERY_POOL_TYPE_PERFORMANCE;

    EXPECT_CALL(*mockMetricEnumeration, isInitialized())
        .Times(1)
        .WillOnce(Return(true));

    EXPECT_CALL(*mockMetricsLibrary, getContextData(_, _))
        .Times(1)
        .WillOnce(Return(true));

    EXPECT_CALL(*mockMetricsLibrary, load())
        .Times(0);

    EXPECT_CALL(metricGroup, getProperties(_))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<0>(metricGroupProperties), Return(ZE_RESULT_SUCCESS)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockContextCreate(_, _, _))
        .Times(1)
        .WillOnce(Return(StatusCode::Failed));

    EXPECT_EQ(zetMetricQueryPoolCreate(context->toHandle(), metricDevice, metricGroup.toHandle(), &poolDesc, &poolHandle), ZE_RESULT_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(poolHandle, nullptr);
}

TEST_F(MetricQueryPoolTest, givenIncorrectContextDataWhenZetMetricQueryPoolCreateIsCalledThenReturnsFail) {

    zet_device_handle_t metricDevice = device->toHandle();

    Mock<MetricGroup> metricGroup;
    zet_metric_group_properties_t metricGroupProperties = {};
    metricGroupProperties.samplingType = ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_EVENT_BASED;

    zet_metric_query_pool_handle_t poolHandle = {};
    zet_metric_query_pool_desc_t poolDesc = {};
    poolDesc.stype = ZET_STRUCTURE_TYPE_METRIC_QUERY_POOL_DESC;
    poolDesc.count = 1;
    poolDesc.type = ZET_METRIC_QUERY_POOL_TYPE_PERFORMANCE;

    EXPECT_CALL(*mockMetricEnumeration, isInitialized())
        .Times(1)
        .WillOnce(Return(true));

    EXPECT_CALL(*mockMetricsLibrary, getContextData(_, _))
        .Times(1)
        .WillOnce(Return(false));

    EXPECT_CALL(*mockMetricsLibrary, load())
        .Times(0);

    EXPECT_CALL(metricGroup, getProperties(_))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<0>(metricGroupProperties), Return(ZE_RESULT_SUCCESS)));

    EXPECT_EQ(zetMetricQueryPoolCreate(context->toHandle(), metricDevice, metricGroup.toHandle(), &poolDesc, &poolHandle), ZE_RESULT_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(poolHandle, nullptr);
}

TEST_F(MetricQueryPoolTest, givenIncorrectGpuReportSizeWhenZetMetricQueryPoolCreateIsCalledThenReturnsFail) {

    zet_device_handle_t metricDevice = device->toHandle();

    Mock<MetricGroup> metricGroup;
    zet_metric_group_properties_t metricGroupProperties = {};
    metricGroupProperties.samplingType = ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_EVENT_BASED;

    zet_metric_query_pool_handle_t poolHandle = {};
    zet_metric_query_pool_desc_t poolDesc = {};
    poolDesc.stype = ZET_STRUCTURE_TYPE_METRIC_QUERY_POOL_DESC;
    poolDesc.count = 1;
    poolDesc.type = ZET_METRIC_QUERY_POOL_TYPE_PERFORMANCE;

    TypedValue_1_0 value = {};
    value.Type = ValueType::Uint32;
    value.ValueUInt32 = 0;

    QueryHandle_1_0 queryHandle = {&value};
    ContextHandle_1_0 contextHandle = {&value};

    EXPECT_CALL(*mockMetricEnumeration, isInitialized())
        .Times(1)
        .WillOnce(Return(true));

    EXPECT_CALL(*mockMetricsLibrary, getContextData(_, _))
        .Times(1)
        .WillOnce(Return(true));

    EXPECT_CALL(*mockMetricsLibrary, load())
        .Times(0);

    EXPECT_CALL(metricGroup, getProperties(_))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<0>(metricGroupProperties), Return(ZE_RESULT_SUCCESS)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockQueryCreate(_, _))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<1>(queryHandle), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockQueryDelete(_))
        .Times(1)
        .WillOnce(Return(StatusCode::Success));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockGetParameter(_, _, _))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<2>(value), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockContextCreate(_, _, _))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<2>(contextHandle), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockContextDelete(_))
        .Times(1)
        .WillOnce(Return(StatusCode::Success));

    EXPECT_EQ(zetMetricQueryPoolCreate(context->toHandle(), metricDevice, metricGroup.toHandle(), &poolDesc, &poolHandle), ZE_RESULT_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(poolHandle, nullptr);
}

TEST_F(MetricQueryPoolTest, givenCorrectArgumentsWhenZetMetricQueryCreateIsCalledThenMetricQueryIsObtained) {

    zet_device_handle_t metricDevice = device->toHandle();

    Mock<MetricGroup> metricGroup;
    zet_metric_group_properties_t metricGroupProperties = {};
    metricGroupProperties.samplingType = ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_EVENT_BASED;

    zet_metric_query_handle_t queryHandle = {};
    zet_metric_query_pool_handle_t poolHandle = {};
    zet_metric_query_pool_desc_t poolDesc = {};

    poolDesc.stype = ZET_STRUCTURE_TYPE_METRIC_QUERY_POOL_DESC;
    poolDesc.count = 1;
    poolDesc.type = ZET_METRIC_QUERY_POOL_TYPE_PERFORMANCE;

    TypedValue_1_0 value = {};
    value.Type = ValueType::Uint32;
    value.ValueUInt32 = 64;

    QueryHandle_1_0 metricsLibraryQueryHandle = {&value};
    ContextHandle_1_0 metricsLibraryContextHandle = {&value};

    EXPECT_CALL(*mockMetricEnumeration, isInitialized())
        .Times(1)
        .WillOnce(Return(true));

    EXPECT_CALL(*mockMetricsLibrary, getContextData(_, _))
        .Times(1)
        .WillOnce(Return(true));

    EXPECT_CALL(*mockMetricsLibrary, load())
        .Times(0);

    EXPECT_CALL(metricGroup, getProperties(_))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<0>(metricGroupProperties), Return(ZE_RESULT_SUCCESS)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockQueryCreate(_, _))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<1>(metricsLibraryQueryHandle), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockQueryDelete(_))
        .Times(1)
        .WillOnce(Return(StatusCode::Success));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockGetParameter(_, _, _))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<2>(value), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockContextCreate(_, _, _))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<2>(metricsLibraryContextHandle), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockContextDelete(_))
        .Times(1)
        .WillOnce(Return(StatusCode::Success));

    // Create metric query pool.
    EXPECT_EQ(zetMetricQueryPoolCreate(context->toHandle(), metricDevice, metricGroup.toHandle(), &poolDesc, &poolHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(poolHandle, nullptr);

    // Create metric query.
    EXPECT_EQ(zetMetricQueryCreate(poolHandle, 0, &queryHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(queryHandle, nullptr);

    // Destroy query and its pool.
    EXPECT_EQ(zetMetricQueryDestroy(queryHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(zetMetricQueryPoolDestroy(poolHandle), ZE_RESULT_SUCCESS);
}

TEST_F(MetricQueryPoolTest, givenIncorrectSlotIndexWhenZetMetricQueryCreateIsCalledThenReturnsFail) {

    zet_device_handle_t metricDevice = device->toHandle();

    Mock<MetricGroup> metricGroup;
    zet_metric_group_properties_t metricGroupProperties = {};
    metricGroupProperties.samplingType = ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_EVENT_BASED;

    zet_metric_query_handle_t queryHandle = {};
    zet_metric_query_pool_handle_t poolHandle = {};
    zet_metric_query_pool_desc_t poolDesc = {};
    poolDesc.stype = ZET_STRUCTURE_TYPE_METRIC_QUERY_POOL_DESC;
    poolDesc.count = 1;
    poolDesc.type = ZET_METRIC_QUERY_POOL_TYPE_PERFORMANCE;

    TypedValue_1_0 value = {};
    value.Type = ValueType::Uint32;
    value.ValueUInt32 = 64;

    QueryHandle_1_0 metricsLibraryQueryHandle = {&value};
    ContextHandle_1_0 metricsLibraryContextHandle = {&value};

    EXPECT_CALL(*mockMetricEnumeration, isInitialized())
        .Times(1)
        .WillOnce(Return(true));

    EXPECT_CALL(*mockMetricsLibrary, getContextData(_, _))
        .Times(1)
        .WillOnce(Return(true));

    EXPECT_CALL(*mockMetricsLibrary, load())
        .Times(0);

    EXPECT_CALL(metricGroup, getProperties(_))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<0>(metricGroupProperties), Return(ZE_RESULT_SUCCESS)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockQueryCreate(_, _))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<1>(metricsLibraryQueryHandle), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockQueryDelete(_))
        .Times(1)
        .WillOnce(Return(StatusCode::Success));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockGetParameter(_, _, _))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<2>(value), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockContextCreate(_, _, _))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<2>(metricsLibraryContextHandle), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockContextDelete(_))
        .Times(1)
        .WillOnce(Return(StatusCode::Success));

    // Create metric query pool.
    EXPECT_EQ(zetMetricQueryPoolCreate(context->toHandle(), metricDevice, metricGroup.toHandle(), &poolDesc, &poolHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(poolHandle, nullptr);

    // Create metric query.
    EXPECT_EQ(zetMetricQueryCreate(poolHandle, 1, &queryHandle), ZE_RESULT_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(queryHandle, nullptr);

    // Destroy metric query pool.
    EXPECT_EQ(zetMetricQueryPoolDestroy(poolHandle), ZE_RESULT_SUCCESS);
}

TEST_F(MetricQueryPoolTest, givenCorrectArgumentsWhenZetMetricQueryResetIsCalledThenReturnsSuccess) {

    zet_device_handle_t metricDevice = device->toHandle();

    Mock<MetricGroup> metricGroup;
    zet_metric_group_properties_t metricGroupProperties = {};
    metricGroupProperties.samplingType = ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_EVENT_BASED;

    zet_metric_query_handle_t queryHandle = {};
    zet_metric_query_pool_handle_t poolHandle = {};
    zet_metric_query_pool_desc_t poolDesc = {};

    poolDesc.stype = ZET_STRUCTURE_TYPE_METRIC_QUERY_POOL_DESC;
    poolDesc.count = 1;
    poolDesc.type = ZET_METRIC_QUERY_POOL_TYPE_PERFORMANCE;

    TypedValue_1_0 value = {};
    value.Type = ValueType::Uint32;
    value.ValueUInt32 = 64;

    QueryHandle_1_0 metricsLibraryQueryHandle = {&value};
    ContextHandle_1_0 metricsLibraryContextHandle = {&value};

    EXPECT_CALL(*mockMetricEnumeration, isInitialized())
        .Times(1)
        .WillOnce(Return(true));

    EXPECT_CALL(*mockMetricsLibrary, getContextData(_, _))
        .Times(1)
        .WillOnce(Return(true));

    EXPECT_CALL(*mockMetricsLibrary, load())
        .Times(0);

    EXPECT_CALL(metricGroup, getProperties(_))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<0>(metricGroupProperties), Return(ZE_RESULT_SUCCESS)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockQueryCreate(_, _))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<1>(metricsLibraryQueryHandle), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockQueryDelete(_))
        .Times(1)
        .WillOnce(Return(StatusCode::Success));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockGetParameter(_, _, _))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<2>(value), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockContextCreate(_, _, _))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<2>(metricsLibraryContextHandle), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockContextDelete(_))
        .Times(1)
        .WillOnce(Return(StatusCode::Success));

    // Create metric query pool.
    EXPECT_EQ(zetMetricQueryPoolCreate(context->toHandle(), metricDevice, metricGroup.toHandle(), &poolDesc, &poolHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(poolHandle, nullptr);

    // Create metric query.
    EXPECT_EQ(zetMetricQueryCreate(poolHandle, 0, &queryHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(queryHandle, nullptr);

    // Reset metric query.
    EXPECT_EQ(zetMetricQueryReset(queryHandle), ZE_RESULT_SUCCESS);

    // Destroy query and its pool.
    EXPECT_EQ(zetMetricQueryDestroy(queryHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(zetMetricQueryPoolDestroy(poolHandle), ZE_RESULT_SUCCESS);
}

TEST_F(MetricQueryPoolTest, givenIncorrectArgumentsWhenZetCommandListAppendMetricQueryBeginIsCalledThenReturnsFail) {

    zet_device_handle_t metricDevice = device->toHandle();

    Mock<MetricGroup> metricGroup;
    zet_metric_group_properties_t metricGroupProperties = {};
    metricGroupProperties.samplingType = ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_EVENT_BASED;

    zet_metric_query_handle_t queryHandle = {};
    zet_metric_query_pool_handle_t poolHandle = {};
    zet_metric_query_pool_desc_t poolDesc = {};
    poolDesc.stype = ZET_STRUCTURE_TYPE_METRIC_QUERY_POOL_DESC;
    poolDesc.count = 1;
    poolDesc.type = ZET_METRIC_QUERY_POOL_TYPE_PERFORMANCE;

    TypedValue_1_0 value = {};
    value.Type = ValueType::Uint32;
    value.ValueUInt32 = 64;

    QueryHandle_1_0 metricsLibraryQueryHandle = {&value};
    ContextHandle_1_0 metricsLibraryContextHandle = {&value};

    EXPECT_CALL(*mockMetricEnumeration, isInitialized())
        .Times(1)
        .WillOnce(Return(true));

    EXPECT_CALL(*mockMetricsLibrary, getContextData(_, _))
        .Times(1)
        .WillOnce(Return(true));

    EXPECT_CALL(*mockMetricsLibrary, load())
        .Times(0);

    EXPECT_CALL(metricGroup, getProperties(_))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<0>(metricGroupProperties), Return(ZE_RESULT_SUCCESS)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockQueryCreate(_, _))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<1>(metricsLibraryQueryHandle), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockQueryDelete(_))
        .Times(1)
        .WillOnce(Return(StatusCode::Success));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockGetParameter(_, _, _))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<2>(value), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockContextCreate(_, _, _))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<2>(metricsLibraryContextHandle), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockContextDelete(_))
        .Times(1)
        .WillOnce(Return(StatusCode::Success));

    // Create metric query pool.
    EXPECT_EQ(zetMetricQueryPoolCreate(context->toHandle(), metricDevice, metricGroup.toHandle(), &poolDesc, &poolHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(poolHandle, nullptr);

    // Create metric query.
    EXPECT_EQ(zetMetricQueryCreate(poolHandle, 0, &queryHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(queryHandle, nullptr);

    // Reset metric query.
    EXPECT_EQ(zetMetricQueryReset(queryHandle), ZE_RESULT_SUCCESS);

    // Destroy query and its pool.
    EXPECT_EQ(zetMetricQueryDestroy(queryHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(zetMetricQueryPoolDestroy(poolHandle), ZE_RESULT_SUCCESS);
}

TEST_F(MetricQueryPoolTest, givenCorrectArgumentsWhenZetCommandListAppendMetricQueryBeginIsCalledThenReturnsSuccess) {

    zet_device_handle_t metricDevice = device->toHandle();

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    zet_command_list_handle_t commandListHandle = commandList->toHandle();

    Mock<MetricGroup> metricGroup;
    zet_metric_group_properties_t metricGroupProperties = {};
    metricGroupProperties.samplingType = ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_EVENT_BASED;

    zet_metric_query_handle_t queryHandle = {};
    zet_metric_query_pool_handle_t poolHandle = {};
    zet_metric_query_pool_desc_t poolDesc = {};
    poolDesc.stype = ZET_STRUCTURE_TYPE_METRIC_QUERY_POOL_DESC;
    poolDesc.count = 1;
    poolDesc.type = ZET_METRIC_QUERY_POOL_TYPE_PERFORMANCE;

    TypedValue_1_0 value = {};
    value.Type = ValueType::Uint32;
    value.ValueUInt32 = 64;

    QueryHandle_1_0 metricsLibraryQueryHandle = {&value};
    ContextHandle_1_0 metricsLibraryContextHandle = {&value};

    CommandBufferSize_1_0 commandBufferSize = {};
    commandBufferSize.GpuMemorySize = 100;

    EXPECT_CALL(*mockMetricEnumeration, isInitialized())
        .Times(1)
        .WillOnce(Return(true));

    EXPECT_CALL(*mockMetricsLibrary, getContextData(_, _))
        .Times(1)
        .WillOnce(Return(true));

    EXPECT_CALL(*mockMetricsLibrary, load())
        .Times(0);

    EXPECT_CALL(metricGroup, getProperties(_))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<0>(metricGroupProperties), Return(ZE_RESULT_SUCCESS)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockQueryCreate(_, _))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<1>(metricsLibraryQueryHandle), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockQueryDelete(_))
        .Times(1)
        .WillOnce(Return(StatusCode::Success));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockGetParameter(_, _, _))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<2>(value), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockContextCreate(_, _, _))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<2>(metricsLibraryContextHandle), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockCommandBufferGetSize(_, _))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<1>(::testing::ByRef(commandBufferSize)), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockCommandBufferGet(_))
        .Times(1)
        .WillOnce(Return(StatusCode::Success));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockContextDelete(_))
        .Times(1)
        .WillOnce(Return(StatusCode::Success));

    // Create metric query pool.
    EXPECT_EQ(zetMetricQueryPoolCreate(context->toHandle(), metricDevice, metricGroup.toHandle(), &poolDesc, &poolHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(poolHandle, nullptr);

    // Create metric query.
    EXPECT_EQ(zetMetricQueryCreate(poolHandle, 0, &queryHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(queryHandle, nullptr);

    // Write BEGIN metric query to command list.
    EXPECT_EQ(zetCommandListAppendMetricQueryBegin(commandListHandle, queryHandle), ZE_RESULT_SUCCESS);

    // Destroy query and its pool.
    EXPECT_EQ(zetMetricQueryDestroy(queryHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(zetMetricQueryPoolDestroy(poolHandle), ZE_RESULT_SUCCESS);
}

TEST_F(MetricQueryPoolTest, givenCorrectArgumentsWhenZeEventPoolCreateIsCalledThenReturnsSuccess) {

    zet_device_handle_t metricDevice = device->toHandle();

    ze_event_pool_handle_t eventPoolHandle = {};
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = 0;
    eventPoolDesc.stype = ZE_STRUCTURE_TYPE_EVENT_POOL_DESC;

    // Create event pool.
    EXPECT_EQ(zeEventPoolCreate(context->toHandle(), &eventPoolDesc, 1, &metricDevice, &eventPoolHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(eventPoolHandle, nullptr);

    // Destroy event pool.
    EXPECT_EQ(zeEventPoolDestroy(eventPoolHandle), ZE_RESULT_SUCCESS);
}

TEST_F(MetricQueryPoolTest, givenIncorrectArgumentsWhenZeEventCreateIsCalledThenReturnsFail) {
    zet_device_handle_t metricDevice = device->toHandle();

    ze_event_pool_handle_t eventPoolHandle = {};
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = 0;
    eventPoolDesc.stype = ZE_STRUCTURE_TYPE_EVENT_POOL_DESC;

    // Create event pool.
    EXPECT_EQ(zeEventPoolCreate(context->toHandle(), &eventPoolDesc, 1, &metricDevice, &eventPoolHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(eventPoolHandle, nullptr);

    // Destroy event and its pool.
    EXPECT_EQ(zeEventPoolDestroy(eventPoolHandle), ZE_RESULT_SUCCESS);
}

TEST_F(MetricQueryPoolTest, givenCorrectArgumentsWhenZeEventCreateIsCalledThenReturnsSuccess) {

    zet_device_handle_t metricDevice = device->toHandle();

    ze_event_pool_handle_t eventPoolHandle = {};
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = 0;
    eventPoolDesc.stype = ZE_STRUCTURE_TYPE_EVENT_POOL_DESC;

    ze_event_handle_t eventHandle = {};
    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.stype = ZE_STRUCTURE_TYPE_EVENT_DESC;
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_DEVICE;

    // Create event pool.
    EXPECT_EQ(zeEventPoolCreate(context->toHandle(), &eventPoolDesc, 1, &metricDevice, &eventPoolHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(eventPoolHandle, nullptr);

    // Create event.
    EXPECT_EQ(zeEventCreate(eventPoolHandle, &eventDesc, &eventHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(eventHandle, nullptr);

    // Destroy event and its pool.
    EXPECT_EQ(zeEventDestroy(eventHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(zeEventPoolDestroy(eventPoolHandle), ZE_RESULT_SUCCESS);
}

TEST_F(MetricQueryPoolTest, givenIncorrectArgumentsWhenZetCommandListAppendMetricQueryEndIsCalledThenReturnsFail) {

    zet_device_handle_t metricDevice = device->toHandle();

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    zet_command_list_handle_t commandListHandle = commandList->toHandle();

    ze_event_pool_handle_t eventPoolHandle = {};
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = 0;
    eventPoolDesc.stype = ZE_STRUCTURE_TYPE_EVENT_POOL_DESC;

    ze_event_handle_t eventHandle = {};
    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.stype = ZE_STRUCTURE_TYPE_EVENT_DESC;
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_DEVICE;

    Mock<MetricGroup> metricGroup;
    zet_metric_group_properties_t metricGroupProperties = {};
    metricGroupProperties.samplingType = ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_EVENT_BASED;

    zet_metric_query_handle_t queryHandle = {};
    zet_metric_query_pool_handle_t poolHandle = {};
    zet_metric_query_pool_desc_t poolDesc = {};
    poolDesc.stype = ZET_STRUCTURE_TYPE_METRIC_QUERY_POOL_DESC;
    poolDesc.count = 1;
    poolDesc.type = ZET_METRIC_QUERY_POOL_TYPE_PERFORMANCE;

    TypedValue_1_0 value = {};
    value.Type = ValueType::Uint32;
    value.ValueUInt32 = 64;

    QueryHandle_1_0 metricsLibraryQueryHandle = {&value};
    ContextHandle_1_0 metricsLibraryContextHandle = {&value};

    CommandBufferSize_1_0 commandBufferSize = {};
    commandBufferSize.GpuMemorySize = 100;

    EXPECT_CALL(*mockMetricEnumeration, isInitialized())
        .Times(1)
        .WillOnce(Return(true));

    EXPECT_CALL(*mockMetricsLibrary, getContextData(_, _))
        .Times(1)
        .WillOnce(Return(true));

    EXPECT_CALL(*mockMetricsLibrary, load())
        .Times(0);

    EXPECT_CALL(metricGroup, getProperties(_))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<0>(metricGroupProperties), Return(ZE_RESULT_SUCCESS)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockQueryCreate(_, _))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<1>(metricsLibraryQueryHandle), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockQueryDelete(_))
        .Times(1)
        .WillOnce(Return(StatusCode::Success));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockGetParameter(_, _, _))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<2>(value), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockContextCreate(_, _, _))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<2>(metricsLibraryContextHandle), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockCommandBufferGetSize(_, _))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<1>(::testing::ByRef(commandBufferSize)), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockCommandBufferGet(_))
        .Times(1)
        .WillOnce(Return(StatusCode::Success));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockContextDelete(_))
        .Times(1)
        .WillOnce(Return(StatusCode::Success));

    // Create metric query pool.
    EXPECT_EQ(zetMetricQueryPoolCreate(context->toHandle(), metricDevice, metricGroup.toHandle(), &poolDesc, &poolHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(poolHandle, nullptr);

    // Create metric query.
    EXPECT_EQ(zetMetricQueryCreate(poolHandle, 0, &queryHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(queryHandle, nullptr);

    // Create event pool.
    EXPECT_EQ(zeEventPoolCreate(context->toHandle(), &eventPoolDesc, 1, &metricDevice, &eventPoolHandle), ZE_RESULT_SUCCESS);

    // Create event.
    EXPECT_EQ(zeEventCreate(eventPoolHandle, &eventDesc, &eventHandle), ZE_RESULT_SUCCESS);

    // Write BEGIN metric query to command list.
    EXPECT_EQ(zetCommandListAppendMetricQueryBegin(commandListHandle, queryHandle), ZE_RESULT_SUCCESS);

    // Destroy event and its pool.
    EXPECT_EQ(zeEventDestroy(eventHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(zeEventPoolDestroy(eventPoolHandle), ZE_RESULT_SUCCESS);

    // Destroy query and its pool.
    EXPECT_EQ(zetMetricQueryDestroy(queryHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(zetMetricQueryPoolDestroy(poolHandle), ZE_RESULT_SUCCESS);
}

TEST_F(MetricQueryPoolTest, givenCorrectArgumentsWhenZetCommandListAppendMetricQueryEndIsCalledThenReturnsSuccess) {

    zet_device_handle_t metricDevice = device->toHandle();

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    zet_command_list_handle_t commandListHandle = commandList->toHandle();

    ze_event_pool_handle_t eventPoolHandle = {};
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = 0;
    eventPoolDesc.stype = ZE_STRUCTURE_TYPE_EVENT_POOL_DESC;

    ze_event_handle_t eventHandle = {};
    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.stype = ZE_STRUCTURE_TYPE_EVENT_DESC;
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_DEVICE;

    Mock<MetricGroup> metricGroup;

    zet_metric_group_properties_t metricGroupProperties = {};
    metricGroupProperties.samplingType = ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_EVENT_BASED;

    zet_metric_query_handle_t queryHandle = {};
    zet_metric_query_pool_handle_t poolHandle = {};
    zet_metric_query_pool_desc_t poolDesc = {};
    poolDesc.stype = ZET_STRUCTURE_TYPE_METRIC_QUERY_POOL_DESC;
    poolDesc.count = 1;
    poolDesc.type = ZET_METRIC_QUERY_POOL_TYPE_PERFORMANCE;

    TypedValue_1_0 value = {};
    value.Type = ValueType::Uint32;
    value.ValueUInt32 = 64;

    QueryHandle_1_0 metricsLibraryQueryHandle = {&value};
    ContextHandle_1_0 metricsLibraryContextHandle = {&value};

    CommandBufferSize_1_0 commandBufferSize = {};
    commandBufferSize.GpuMemorySize = 100;

    EXPECT_CALL(*mockMetricEnumeration, isInitialized())
        .Times(1)
        .WillOnce(Return(true));

    EXPECT_CALL(*mockMetricsLibrary, getContextData(_, _))
        .Times(1)
        .WillOnce(Return(true));

    EXPECT_CALL(*mockMetricsLibrary, load())
        .Times(0);

    EXPECT_CALL(metricGroup, getProperties(_))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<0>(metricGroupProperties), Return(ZE_RESULT_SUCCESS)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockQueryCreate(_, _))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<1>(metricsLibraryQueryHandle), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockQueryDelete(_))
        .Times(1)
        .WillOnce(Return(StatusCode::Success));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockGetParameter(_, _, _))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<2>(value), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockContextCreate(_, _, _))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<2>(metricsLibraryContextHandle), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockCommandBufferGetSize(_, _))
        .Times(2)
        .WillRepeatedly(DoAll(::testing::SetArgPointee<1>(::testing::ByRef(commandBufferSize)), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockCommandBufferGet(_))
        .Times(2)
        .WillRepeatedly(Return(StatusCode::Success));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockContextDelete(_))
        .Times(1)
        .WillOnce(Return(StatusCode::Success));

    // Create metric query pool.
    EXPECT_EQ(zetMetricQueryPoolCreate(context->toHandle(), metricDevice, metricGroup.toHandle(), &poolDesc, &poolHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(poolHandle, nullptr);

    // Create metric query.
    EXPECT_EQ(zetMetricQueryCreate(poolHandle, 0, &queryHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(queryHandle, nullptr);

    // Create event pool.
    EXPECT_EQ(zeEventPoolCreate(context->toHandle(), &eventPoolDesc, 1, &metricDevice, &eventPoolHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(eventPoolHandle, nullptr);

    // Create event.
    EXPECT_EQ(zeEventCreate(eventPoolHandle, &eventDesc, &eventHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(eventHandle, nullptr);

    // Write BEGIN metric query to command list.
    EXPECT_EQ(zetCommandListAppendMetricQueryBegin(commandListHandle, queryHandle), ZE_RESULT_SUCCESS);

    // Write END metric query to command list, use an event to determine if the data is available.
    EXPECT_EQ(zetCommandListAppendMetricQueryEnd(commandListHandle, queryHandle, eventHandle, 0, nullptr), ZE_RESULT_SUCCESS);

    // Destroy event and its pool.
    EXPECT_EQ(zeEventDestroy(eventHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(zeEventPoolDestroy(eventPoolHandle), ZE_RESULT_SUCCESS);

    // Destroy query and its pool.
    EXPECT_EQ(zetMetricQueryDestroy(queryHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(zetMetricQueryPoolDestroy(poolHandle), ZE_RESULT_SUCCESS);
}

TEST_F(MetricQueryPoolTest, givenIncorrectArgumentsWhenZetMetricQueryGetDataIsCalledThenReturnsFail) {

    zet_device_handle_t metricDevice = device->toHandle();

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    zet_command_list_handle_t commandListHandle = commandList->toHandle();

    ze_event_pool_handle_t eventPoolHandle = {};
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = 0;
    eventPoolDesc.stype = ZE_STRUCTURE_TYPE_EVENT_POOL_DESC;

    ze_event_handle_t eventHandle = {};
    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.stype = ZE_STRUCTURE_TYPE_EVENT_DESC;
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_DEVICE;

    Mock<MetricGroup> metricGroup;
    zet_metric_group_properties_t metricGroupProperties = {};
    metricGroupProperties.samplingType = ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_EVENT_BASED;

    zet_metric_query_handle_t queryHandle = {};
    zet_metric_query_pool_handle_t poolHandle = {};
    zet_metric_query_pool_desc_t poolDesc = {};
    poolDesc.stype = ZET_STRUCTURE_TYPE_METRIC_QUERY_POOL_DESC;
    poolDesc.count = 1;
    poolDesc.type = ZET_METRIC_QUERY_POOL_TYPE_PERFORMANCE;

    TypedValue_1_0 value = {};
    value.Type = ValueType::Uint32;
    value.ValueUInt32 = 64;

    QueryHandle_1_0 metricsLibraryQueryHandle = {&value};
    ContextHandle_1_0 metricsLibraryContextHandle = {&value};

    CommandBufferSize_1_0 commandBufferSize = {};
    commandBufferSize.GpuMemorySize = 100;

    EXPECT_CALL(*mockMetricEnumeration, isInitialized())
        .Times(1)
        .WillOnce(Return(true));

    EXPECT_CALL(*mockMetricsLibrary, getContextData(_, _))
        .Times(1)
        .WillOnce(Return(true));

    EXPECT_CALL(*mockMetricsLibrary, load())
        .Times(0);

    EXPECT_CALL(metricGroup, getProperties(_))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<0>(metricGroupProperties), Return(ZE_RESULT_SUCCESS)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockQueryCreate(_, _))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<1>(metricsLibraryQueryHandle), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockQueryDelete(_))
        .Times(1)
        .WillOnce(Return(StatusCode::Success));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockGetParameter(_, _, _))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<2>(value), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockContextCreate(_, _, _))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<2>(metricsLibraryContextHandle), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockCommandBufferGetSize(_, _))
        .Times(2)
        .WillRepeatedly(DoAll(::testing::SetArgPointee<1>(::testing::ByRef(commandBufferSize)), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockCommandBufferGet(_))
        .Times(2)
        .WillRepeatedly(Return(StatusCode::Success));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockContextDelete(_))
        .Times(1)
        .WillOnce(Return(StatusCode::Success));

    // Create metric query pool.
    EXPECT_EQ(zetMetricQueryPoolCreate(context->toHandle(), metricDevice, metricGroup.toHandle(), &poolDesc, &poolHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(poolHandle, nullptr);

    // Create metric query.
    EXPECT_EQ(zetMetricQueryCreate(poolHandle, 0, &queryHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(queryHandle, nullptr);

    // Create event pool.
    EXPECT_EQ(zeEventPoolCreate(context->toHandle(), &eventPoolDesc, 1, &metricDevice, &eventPoolHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(eventPoolHandle, nullptr);

    // Create event.
    EXPECT_EQ(zeEventCreate(eventPoolHandle, &eventDesc, &eventHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(eventHandle, nullptr);

    // Write BEGIN metric query to command list.
    EXPECT_EQ(zetCommandListAppendMetricQueryBegin(commandListHandle, queryHandle), ZE_RESULT_SUCCESS);

    // Write END metric query to command list, use an event to determine if the data is available.
    EXPECT_EQ(zetCommandListAppendMetricQueryEnd(commandListHandle, queryHandle, eventHandle, 0, nullptr), ZE_RESULT_SUCCESS);

    // Destroy event and its pool.
    EXPECT_EQ(zeEventDestroy(eventHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(zeEventPoolDestroy(eventPoolHandle), ZE_RESULT_SUCCESS);

    // Destroy query and its pool.
    EXPECT_EQ(zetMetricQueryDestroy(queryHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(zetMetricQueryPoolDestroy(poolHandle), ZE_RESULT_SUCCESS);
}

TEST_F(MetricQueryPoolTest, givenCorrectArgumentsWhenZetMetricQueryGetDataIsCalledThenReturnsSuccess) {

    zet_device_handle_t metricDevice = device->toHandle();

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    zet_command_list_handle_t commandListHandle = commandList->toHandle();

    ze_event_pool_handle_t eventPoolHandle = {};
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = 0;
    eventPoolDesc.stype = ZE_STRUCTURE_TYPE_EVENT_POOL_DESC;

    ze_event_handle_t eventHandle = {};
    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.stype = ZE_STRUCTURE_TYPE_EVENT_DESC;
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_DEVICE;

    Mock<MetricGroup> metricGroup;
    zet_metric_group_properties_t metricGroupProperties = {};
    metricGroupProperties.samplingType = ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_EVENT_BASED;

    zet_metric_query_handle_t queryHandle = {};
    zet_metric_query_pool_handle_t poolHandle = {};
    zet_metric_query_pool_desc_t poolDesc = {};
    poolDesc.stype = ZET_STRUCTURE_TYPE_METRIC_QUERY_POOL_DESC;
    poolDesc.count = 1;
    poolDesc.type = ZET_METRIC_QUERY_POOL_TYPE_PERFORMANCE;

    TypedValue_1_0 value = {};
    value.Type = ValueType::Uint32;
    value.ValueUInt32 = 64;

    size_t reportSize = 256;

    QueryHandle_1_0 metricsLibraryQueryHandle = {&value};
    ContextHandle_1_0 metricsLibraryContextHandle = {&value};

    CommandBufferSize_1_0 commandBufferSize = {};
    commandBufferSize.GpuMemorySize = 100;

    EXPECT_CALL(*mockMetricEnumeration, isInitialized())
        .Times(1)
        .WillOnce(Return(true));

    EXPECT_CALL(*mockMetricsLibrary, getContextData(_, _))
        .Times(1)
        .WillOnce(Return(true));

    EXPECT_CALL(*mockMetricsLibrary, load())
        .Times(0);

    EXPECT_CALL(metricGroup, getProperties(_))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<0>(metricGroupProperties), Return(ZE_RESULT_SUCCESS)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockQueryCreate(_, _))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<1>(metricsLibraryQueryHandle), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockQueryDelete(_))
        .Times(1)
        .WillOnce(Return(StatusCode::Success));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockGetParameter(_, _, _))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<2>(value), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockContextCreate(_, _, _))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<2>(metricsLibraryContextHandle), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockCommandBufferGetSize(_, _))
        .Times(2)
        .WillRepeatedly(DoAll(::testing::SetArgPointee<1>(::testing::ByRef(commandBufferSize)), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockCommandBufferGet(_))
        .Times(2)
        .WillRepeatedly(Return(StatusCode::Success));

    EXPECT_CALL(*mockMetricsLibrary, getMetricQueryReportSize(_))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgReferee<0>(reportSize), Return(true)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockGetData(_))
        .Times(1)
        .WillOnce(Return(StatusCode::Success));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockContextDelete(_))
        .Times(1)
        .WillOnce(Return(StatusCode::Success));

    // Create metric query pool.
    EXPECT_EQ(zetMetricQueryPoolCreate(context->toHandle(), metricDevice, metricGroup.toHandle(), &poolDesc, &poolHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(poolHandle, nullptr);

    // Create metric query.
    EXPECT_EQ(zetMetricQueryCreate(poolHandle, 0, &queryHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(queryHandle, nullptr);

    // Create event pool.
    EXPECT_EQ(zeEventPoolCreate(context->toHandle(), &eventPoolDesc, 1, &metricDevice, &eventPoolHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(eventPoolHandle, nullptr);

    // Create event.
    EXPECT_EQ(zeEventCreate(eventPoolHandle, &eventDesc, &eventHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(eventHandle, nullptr);

    // Write BEGIN metric query to command list.
    EXPECT_EQ(zetCommandListAppendMetricQueryBegin(commandListHandle, queryHandle), ZE_RESULT_SUCCESS);

    // Write END metric query to command list, use an event to determine if the data is available.
    EXPECT_EQ(zetCommandListAppendMetricQueryEnd(commandListHandle, queryHandle, eventHandle, 0, nullptr), ZE_RESULT_SUCCESS);

    // Get desired raw data size.
    size_t rawSize = 0;
    EXPECT_EQ(zetMetricQueryGetData(queryHandle, &rawSize, nullptr), ZE_RESULT_SUCCESS);

    // Get data.
    std::vector<uint8_t> rawData;
    rawData.resize(rawSize);
    EXPECT_EQ(zetMetricQueryGetData(queryHandle, &rawSize, rawData.data()), ZE_RESULT_SUCCESS);

    // Destroy event and its pool.
    EXPECT_EQ(zeEventDestroy(eventHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(zeEventPoolDestroy(eventPoolHandle), ZE_RESULT_SUCCESS);

    // Destroy query and its pool.
    EXPECT_EQ(zetMetricQueryDestroy(queryHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(zetMetricQueryPoolDestroy(poolHandle), ZE_RESULT_SUCCESS);
}

MATCHER_P(reportDataAreEqual, reportData, "") {
    return (arg->Query.Slot == reportData->Query.Slot) &&
           (arg->Query.SlotsCount == reportData->Query.SlotsCount) &&
           (arg->Query.Handle.data == reportData->Query.Handle.data) &&
           (arg->Query.Data == reportData->Query.Data) &&
           (arg->Query.DataSize == reportData->Query.DataSize) &&
           (arg->Type == reportData->Type);
}

TEST_F(MetricQueryPoolTest, givenCorrectArgumentsWhenZetMetricQueryGetDataIsCalledThenReturnsSuccessWithProperFilledStructure) {

    zet_device_handle_t metricDevice = device->toHandle();

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    zet_command_list_handle_t commandListHandle = commandList->toHandle();

    ze_event_pool_handle_t eventPoolHandle = {};
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = 0;
    eventPoolDesc.stype = ZE_STRUCTURE_TYPE_EVENT_POOL_DESC;

    ze_event_handle_t eventHandle = {};
    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.stype = ZE_STRUCTURE_TYPE_EVENT_DESC;
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_DEVICE;

    Mock<MetricGroup> metricGroup;
    zet_metric_group_properties_t metricGroupProperties = {};
    metricGroupProperties.samplingType = ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_EVENT_BASED;

    uint32_t queriesCount = 10;

    zet_metric_query_handle_t queryHandle = {};
    zet_metric_query_pool_handle_t poolHandle = {};
    zet_metric_query_pool_desc_t poolDesc = {};
    poolDesc.stype = ZET_STRUCTURE_TYPE_METRIC_QUERY_POOL_DESC;
    poolDesc.count = queriesCount;
    poolDesc.type = ZET_METRIC_QUERY_POOL_TYPE_PERFORMANCE;

    TypedValue_1_0 value = {};
    value.Type = ValueType::Uint32;
    value.ValueUInt32 = 64;

    size_t reportSize = 256;

    QueryHandle_1_0 metricsLibraryQueryHandle = {&value};
    ContextHandle_1_0 metricsLibraryContextHandle = {&value};

    CommandBufferSize_1_0 commandBufferSize = {};
    commandBufferSize.GpuMemorySize = 100;

    GetReportData_1_0 reportData = {};
    reportData.Type = ObjectType::QueryHwCounters;
    reportData.Query.Handle = metricsLibraryQueryHandle;
    reportData.Query.Slot = queriesCount - 1;
    reportData.Query.SlotsCount = 1;

    EXPECT_CALL(*mockMetricEnumeration, isInitialized())
        .Times(1)
        .WillOnce(Return(true));

    EXPECT_CALL(*mockMetricsLibrary, getContextData(_, _))
        .Times(1)
        .WillOnce(Return(true));

    EXPECT_CALL(*mockMetricsLibrary, load())
        .Times(0);

    EXPECT_CALL(metricGroup, getProperties(_))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<0>(metricGroupProperties), Return(ZE_RESULT_SUCCESS)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockQueryCreate(_, _))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<1>(metricsLibraryQueryHandle), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockQueryDelete(_))
        .Times(1)
        .WillOnce(Return(StatusCode::Success));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockGetParameter(_, _, _))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<2>(value), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockContextCreate(_, _, _))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<2>(metricsLibraryContextHandle), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockCommandBufferGetSize(_, _))
        .Times(2)
        .WillRepeatedly(DoAll(::testing::SetArgPointee<1>(::testing::ByRef(commandBufferSize)), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockCommandBufferGet(_))
        .Times(2)
        .WillRepeatedly(Return(StatusCode::Success));

    EXPECT_CALL(*mockMetricsLibrary, getMetricQueryReportSize(_))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgReferee<0>(reportSize), Return(true)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockGetData(reportDataAreEqual(&reportData)))
        .Times(1)
        .WillOnce(Return(StatusCode::Success));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockContextDelete(_))
        .Times(1)
        .WillOnce(Return(StatusCode::Success));

    // Create metric query pool.
    EXPECT_EQ(zetMetricQueryPoolCreate(context->toHandle(), metricDevice, metricGroup.toHandle(), &poolDesc, &poolHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(poolHandle, nullptr);

    // Create metric query.
    EXPECT_EQ(zetMetricQueryCreate(poolHandle, queriesCount - 1, &queryHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(queryHandle, nullptr);

    // Create event pool.
    EXPECT_EQ(zeEventPoolCreate(context->toHandle(), &eventPoolDesc, 1, &metricDevice, &eventPoolHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(eventPoolHandle, nullptr);

    // Create event.
    EXPECT_EQ(zeEventCreate(eventPoolHandle, &eventDesc, &eventHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(eventHandle, nullptr);

    // Write BEGIN metric query to command list.
    EXPECT_EQ(zetCommandListAppendMetricQueryBegin(commandListHandle, queryHandle), ZE_RESULT_SUCCESS);

    // Write END metric query to command list, use an event to determine if the data is available.
    EXPECT_EQ(zetCommandListAppendMetricQueryEnd(commandListHandle, queryHandle, eventHandle, 0, nullptr), ZE_RESULT_SUCCESS);

    // Get desired raw data size.
    size_t rawSize = 0;
    EXPECT_EQ(zetMetricQueryGetData(queryHandle, &rawSize, nullptr), ZE_RESULT_SUCCESS);

    // Get data.
    std::vector<uint8_t> rawData;
    rawData.resize(rawSize);
    reportData.Query.Data = rawData.data();
    reportData.Query.DataSize = static_cast<uint32_t>(rawSize);
    EXPECT_EQ(zetMetricQueryGetData(queryHandle, &rawSize, rawData.data()), ZE_RESULT_SUCCESS);

    // Destroy event and its pool.
    EXPECT_EQ(zeEventDestroy(eventHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(zeEventPoolDestroy(eventPoolHandle), ZE_RESULT_SUCCESS);

    // Destroy query and its pool.
    EXPECT_EQ(zetMetricQueryDestroy(queryHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(zetMetricQueryPoolDestroy(poolHandle), ZE_RESULT_SUCCESS);
}

} // namespace ult
} // namespace L0
