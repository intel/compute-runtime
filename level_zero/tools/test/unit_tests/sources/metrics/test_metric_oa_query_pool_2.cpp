/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/ult_device_factory.h"
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
TEST_F(MetricQueryPoolTest, givenCorrectArgumentsWhenStreamerIsOpenThenQueryPoolIsNotAvailable) {

    zet_device_handle_t metricDevice = device->toHandle();

    ze_event_handle_t eventHandle = {};

    zet_metric_streamer_handle_t streamerHandle = {};
    zet_metric_streamer_desc_t streamerDesc = {};

    streamerDesc.stype = ZET_STRUCTURE_TYPE_METRIC_STREAMER_DESC;
    streamerDesc.notifyEveryNReports = 32768;
    streamerDesc.samplingPeriod = 1000;

    Mock<MetricGroup> metricGroup;
    zet_metric_group_handle_t metricGroupHandle = metricGroup.toHandle();
    zet_metric_group_properties_t metricGroupProperties = {};

    metricsDeviceParams.ConcurrentGroupsCount = 1;

    Mock<IConcurrentGroup_1_5> metricsConcurrentGroup;
    TConcurrentGroupParams_1_0 metricsConcurrentGroupParams = {};
    metricsConcurrentGroupParams.MetricSetsCount = 1;
    metricsConcurrentGroupParams.SymbolName = "OA";
    metricsConcurrentGroupParams.Description = "OA description";

    Mock<MetricsDiscovery::IMetricSet_1_5> metricsSet;
    MetricsDiscovery::TMetricSetParams_1_4 metricsSetParams = {};
    metricsSetParams.ApiMask = MetricsDiscovery::API_TYPE_IOSTREAM;
    metricsSetParams.MetricsCount = 0;
    metricsSetParams.SymbolName = "Metric set name";
    metricsSetParams.ShortName = "Metric set description";
    metricsSetParams.RawReportSize = 256;

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

    EXPECT_CALL(*mockMetricsLibrary, load())
        .Times(0);

    openMetricsAdapter();

    EXPECT_CALL(metricsDevice, GetParams())
        .WillRepeatedly(Return(&metricsDeviceParams));

    EXPECT_CALL(metricsDevice, GetConcurrentGroup(_))
        .Times(1)
        .WillOnce(Return(&metricsConcurrentGroup));

    EXPECT_CALL(metricsConcurrentGroup, GetParams())
        .Times(1)
        .WillRepeatedly(Return(&metricsConcurrentGroupParams));

    EXPECT_CALL(metricsConcurrentGroup, GetMetricSet(_))
        .WillRepeatedly(Return(&metricsSet));

    EXPECT_CALL(metricsSet, GetParams())
        .WillRepeatedly(Return(&metricsSetParams));

    EXPECT_CALL(metricsSet, SetApiFiltering(_))
        .WillRepeatedly(Return(TCompletionCode::CC_OK));

    EXPECT_CALL(metricsConcurrentGroup, OpenIoStream(_, _, _, _))
        .Times(1)
        .WillOnce(Return(TCompletionCode::CC_OK));

    EXPECT_CALL(metricsConcurrentGroup, CloseIoStream())
        .Times(1)
        .WillOnce(Return(TCompletionCode::CC_OK));

    uint32_t metricGroupCount = 0;
    EXPECT_EQ(zetMetricGroupGet(metricDevice, &metricGroupCount, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);

    EXPECT_EQ(zetMetricGroupGet(metricDevice, &metricGroupCount, &metricGroupHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);
    EXPECT_NE(metricGroupHandle, nullptr);

    EXPECT_EQ(zetMetricGroupGetProperties(metricGroupHandle, &metricGroupProperties), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupProperties.domain, 0u);
    EXPECT_EQ(metricGroupProperties.samplingType, ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_TIME_BASED);
    EXPECT_EQ(metricGroupProperties.metricCount, metricsSetParams.MetricsCount);
    EXPECT_EQ(strcmp(metricGroupProperties.description, metricsSetParams.ShortName), 0);
    EXPECT_EQ(strcmp(metricGroupProperties.name, metricsSetParams.SymbolName), 0);

    EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), metricDevice, 1, &metricGroupHandle), ZE_RESULT_SUCCESS);

    EXPECT_EQ(zetMetricStreamerOpen(context->toHandle(), metricDevice, metricGroupHandle, &streamerDesc, eventHandle, &streamerHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(streamerHandle, nullptr);

    EXPECT_EQ(zetMetricQueryPoolCreate(context->toHandle(), metricDevice, metricGroupHandle, &poolDesc, &poolHandle), ZE_RESULT_ERROR_NOT_AVAILABLE);

    EXPECT_EQ(zetMetricStreamerClose(streamerHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(streamerHandle, nullptr);
}

TEST_F(MetricQueryPoolTest, givenExecutionQueryTypeWhenZetMetricQueryPoolCreateIsCalledThenQueryPoolIsObtained) {

    zet_device_handle_t metricDevice = device->toHandle();

    zet_metric_query_pool_handle_t poolHandle = {};
    zet_metric_query_pool_desc_t poolDesc = {};
    poolDesc.stype = ZET_STRUCTURE_TYPE_METRIC_QUERY_POOL_DESC;
    poolDesc.count = 1;
    poolDesc.type = ZET_METRIC_QUERY_POOL_TYPE_EXECUTION;

    EXPECT_CALL(*mockMetricsLibrary, load())
        .Times(0);

    EXPECT_EQ(zetMetricQueryPoolCreate(context->toHandle(), metricDevice, nullptr, &poolDesc, &poolHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(poolHandle, nullptr);
    EXPECT_EQ(zetMetricQueryPoolDestroy(poolHandle), ZE_RESULT_SUCCESS);
}

TEST_F(MetricQueryPoolTest, givenExecutionQueryTypeWhenAppendMetricQueryBeginAndEndIsCalledThenReturnSuccess) {

    zet_device_handle_t metricDevice = device->toHandle();

    zet_metric_query_pool_handle_t poolHandle = {};
    zet_metric_query_handle_t queryHandle = {};
    zet_metric_query_pool_desc_t poolDesc = {};
    poolDesc.stype = ZET_STRUCTURE_TYPE_METRIC_QUERY_POOL_DESC;
    poolDesc.count = 1;
    poolDesc.type = ZET_METRIC_QUERY_POOL_TYPE_EXECUTION;

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    zet_command_list_handle_t commandListHandle = commandList->toHandle();

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

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockContextCreate(_, _, _))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<2>(metricsLibraryContextHandle), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockContextDelete(_))
        .Times(1)
        .WillOnce(Return(StatusCode::Success));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockCommandBufferGetSize(_, _))
        .Times(3)
        .WillRepeatedly(DoAll(::testing::SetArgPointee<1>(::testing::ByRef(commandBufferSize)), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockCommandBufferGet(_))
        .Times(3)
        .WillRepeatedly(Return(StatusCode::Success));

    EXPECT_EQ(zetMetricQueryPoolCreate(context->toHandle(), metricDevice, nullptr, &poolDesc, &poolHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(zetMetricQueryCreate(poolHandle, 0, &queryHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(queryHandle, nullptr);

    EXPECT_EQ(zetCommandListAppendMetricQueryBegin(commandListHandle, queryHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(zetCommandListAppendMetricQueryEnd(commandListHandle, queryHandle, nullptr, 0, nullptr), ZE_RESULT_SUCCESS);

    EXPECT_EQ(zetCommandListAppendMetricMemoryBarrier(commandListHandle), ZE_RESULT_SUCCESS);

    EXPECT_EQ(zetMetricQueryDestroy(queryHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(zetMetricQueryPoolDestroy(poolHandle), ZE_RESULT_SUCCESS);
}

TEST_F(MetricQueryPoolTest, givenExecutionQueryTypeAndCompletionEventWhenAppendMetricQueryBeginAndEndIsCalledThenReturnSuccess) {

    zet_device_handle_t metricDevice = device->toHandle();

    zet_metric_query_pool_handle_t poolHandle = {};
    zet_metric_query_handle_t queryHandle = {};
    zet_metric_query_pool_desc_t poolDesc = {};
    poolDesc.stype = ZET_STRUCTURE_TYPE_METRIC_QUERY_POOL_DESC;
    poolDesc.count = 1;
    poolDesc.type = ZET_METRIC_QUERY_POOL_TYPE_EXECUTION;

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    zet_command_list_handle_t commandListHandle = commandList->toHandle();

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

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockContextCreate(_, _, _))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<2>(metricsLibraryContextHandle), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockContextDelete(_))
        .Times(1)
        .WillOnce(Return(StatusCode::Success));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockCommandBufferGetSize(_, _))
        .Times(2)
        .WillRepeatedly(DoAll(::testing::SetArgPointee<1>(::testing::ByRef(commandBufferSize)), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockCommandBufferGet(_))
        .Times(2)
        .WillRepeatedly(Return(StatusCode::Success));

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

    EXPECT_EQ(zeEventPoolCreate(context->toHandle(), &eventPoolDesc, 1, &metricDevice, &eventPoolHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(eventPoolHandle, nullptr);

    EXPECT_EQ(zeEventCreate(eventPoolHandle, &eventDesc, &eventHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(eventHandle, nullptr);

    EXPECT_EQ(zetMetricQueryPoolCreate(context->toHandle(), metricDevice, nullptr, &poolDesc, &poolHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(zetMetricQueryCreate(poolHandle, 0, &queryHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(queryHandle, nullptr);

    EXPECT_EQ(zetCommandListAppendMetricQueryBegin(commandListHandle, queryHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(zetCommandListAppendMetricQueryEnd(commandListHandle, queryHandle, eventHandle, 0, nullptr), ZE_RESULT_SUCCESS);

    EXPECT_EQ(zetMetricQueryDestroy(queryHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(zetMetricQueryPoolDestroy(poolHandle), ZE_RESULT_SUCCESS);

    EXPECT_EQ(zeEventDestroy(eventHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(zeEventPoolDestroy(eventPoolHandle), ZE_RESULT_SUCCESS);
}

TEST_F(MetricQueryPoolTest, givenExecutionQueryTypeWithImmediateCommandListDefaultModeAndFlushTaskEnabledAndCompletionEventWhenAppendMetricQueryBeginAndEndIsCalledThenReturnSuccess) {
    DebugManagerStateRestore restorer;

    ze_result_t returnValue;
    DebugManager.flags.EnableFlushTaskSubmission.set(1);

    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_DEFAULT;

    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::RenderCompute, returnValue));

    zet_device_handle_t metricDevice = device->toHandle();

    zet_metric_query_pool_handle_t poolHandle = {};
    zet_metric_query_handle_t queryHandle = {};
    zet_metric_query_pool_desc_t poolDesc = {};
    poolDesc.stype = ZET_STRUCTURE_TYPE_METRIC_QUERY_POOL_DESC;
    poolDesc.count = 1;
    poolDesc.type = ZET_METRIC_QUERY_POOL_TYPE_EXECUTION;

    ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);
    ASSERT_NE(nullptr, commandList);

    EXPECT_EQ(device, commandList->device);
    EXPECT_EQ(CommandList::CommandListType::TYPE_IMMEDIATE, commandList->cmdListType);
    EXPECT_NE(nullptr, commandList->cmdQImmediate);
    zet_command_list_handle_t commandListHandle = commandList->toHandle();

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

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockContextCreate(_, _, _))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<2>(metricsLibraryContextHandle), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockContextDelete(_))
        .Times(1)
        .WillOnce(Return(StatusCode::Success));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockCommandBufferGetSize(_, _))
        .Times(2)
        .WillRepeatedly(DoAll(::testing::SetArgPointee<1>(::testing::ByRef(commandBufferSize)), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockCommandBufferGet(_))
        .Times(2)
        .WillRepeatedly(Return(StatusCode::Success));

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

    EXPECT_EQ(zeEventPoolCreate(context->toHandle(), &eventPoolDesc, 1, &metricDevice, &eventPoolHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(eventPoolHandle, nullptr);

    EXPECT_EQ(zeEventCreate(eventPoolHandle, &eventDesc, &eventHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(eventHandle, nullptr);

    EXPECT_EQ(zetMetricQueryPoolCreate(context->toHandle(), metricDevice, nullptr, &poolDesc, &poolHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(zetMetricQueryCreate(poolHandle, 0, &queryHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(queryHandle, nullptr);

    EXPECT_EQ(zetCommandListAppendMetricQueryBegin(commandListHandle, queryHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(zetCommandListAppendMetricQueryEnd(commandListHandle, queryHandle, eventHandle, 0, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(zetMetricQueryDestroy(queryHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(zetMetricQueryPoolDestroy(poolHandle), ZE_RESULT_SUCCESS);

    EXPECT_EQ(zeEventDestroy(eventHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(zeEventPoolDestroy(eventPoolHandle), ZE_RESULT_SUCCESS);
}

TEST_F(MetricQueryPoolTest, givenExecutionQueryTypeWithImmediateCommandListDefaultModeAndFlushTaskDisabledAndCompletionEventWhenAppendMetricQueryBeginAndEndIsCalledThenReturnSuccess) {
    DebugManagerStateRestore restorer;

    ze_result_t returnValue;
    DebugManager.flags.EnableFlushTaskSubmission.set(0);

    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_DEFAULT;

    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::RenderCompute, returnValue));

    zet_device_handle_t metricDevice = device->toHandle();

    zet_metric_query_pool_handle_t poolHandle = {};
    zet_metric_query_handle_t queryHandle = {};
    zet_metric_query_pool_desc_t poolDesc = {};
    poolDesc.stype = ZET_STRUCTURE_TYPE_METRIC_QUERY_POOL_DESC;
    poolDesc.count = 1;
    poolDesc.type = ZET_METRIC_QUERY_POOL_TYPE_EXECUTION;

    ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);
    ASSERT_NE(nullptr, commandList);

    EXPECT_EQ(device, commandList->device);
    EXPECT_EQ(CommandList::CommandListType::TYPE_IMMEDIATE, commandList->cmdListType);
    EXPECT_NE(nullptr, commandList->cmdQImmediate);
    zet_command_list_handle_t commandListHandle = commandList->toHandle();

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

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockContextCreate(_, _, _))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<2>(metricsLibraryContextHandle), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockContextDelete(_))
        .Times(1)
        .WillOnce(Return(StatusCode::Success));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockCommandBufferGetSize(_, _))
        .Times(2)
        .WillRepeatedly(DoAll(::testing::SetArgPointee<1>(::testing::ByRef(commandBufferSize)), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockCommandBufferGet(_))
        .Times(2)
        .WillRepeatedly(Return(StatusCode::Success));

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

    EXPECT_EQ(zeEventPoolCreate(context->toHandle(), &eventPoolDesc, 1, &metricDevice, &eventPoolHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(eventPoolHandle, nullptr);

    EXPECT_EQ(zeEventCreate(eventPoolHandle, &eventDesc, &eventHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(eventHandle, nullptr);

    EXPECT_EQ(zetMetricQueryPoolCreate(context->toHandle(), metricDevice, nullptr, &poolDesc, &poolHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(zetMetricQueryCreate(poolHandle, 0, &queryHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(queryHandle, nullptr);

    EXPECT_EQ(zetCommandListAppendMetricQueryBegin(commandListHandle, queryHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(zetCommandListAppendMetricQueryEnd(commandListHandle, queryHandle, eventHandle, 0, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(zetMetricQueryDestroy(queryHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(zetMetricQueryPoolDestroy(poolHandle), ZE_RESULT_SUCCESS);

    EXPECT_EQ(zeEventDestroy(eventHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(zeEventPoolDestroy(eventPoolHandle), ZE_RESULT_SUCCESS);
}

TEST_F(MetricQueryPoolTest, givenExecutionQueryTypeAndMetricsLibraryWillFailWhenAppendMetricQueryBeginAndEndIsCalledThenReturnFail) {

    zet_device_handle_t metricDevice = device->toHandle();

    zet_metric_query_pool_handle_t poolHandle = {};
    zet_metric_query_handle_t queryHandle = {};
    zet_metric_query_pool_desc_t poolDesc = {};
    poolDesc.stype = ZET_STRUCTURE_TYPE_METRIC_QUERY_POOL_DESC;
    poolDesc.count = 1;
    poolDesc.type = ZET_METRIC_QUERY_POOL_TYPE_EXECUTION;

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    zet_command_list_handle_t commandListHandle = commandList->toHandle();

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

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockContextCreate(_, _, _))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<2>(metricsLibraryContextHandle), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockContextDelete(_))
        .Times(1)
        .WillOnce(Return(StatusCode::Success));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockCommandBufferGetSize(_, _))
        .Times(2)
        .WillRepeatedly(DoAll(::testing::SetArgPointee<1>(::testing::ByRef(commandBufferSize)), Return(StatusCode::Failed)));

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

    EXPECT_EQ(zeEventPoolCreate(context->toHandle(), &eventPoolDesc, 1, &metricDevice, &eventPoolHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(eventPoolHandle, nullptr);

    EXPECT_EQ(zeEventCreate(eventPoolHandle, &eventDesc, &eventHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(eventHandle, nullptr);

    EXPECT_EQ(zetMetricQueryPoolCreate(context->toHandle(), metricDevice, nullptr, &poolDesc, &poolHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(zetMetricQueryCreate(poolHandle, 0, &queryHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(queryHandle, nullptr);

    EXPECT_NE(zetCommandListAppendMetricQueryBegin(commandListHandle, queryHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(zetCommandListAppendMetricQueryEnd(commandListHandle, queryHandle, eventHandle, 0, nullptr), ZE_RESULT_SUCCESS);

    EXPECT_EQ(zetMetricQueryDestroy(queryHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(zetMetricQueryPoolDestroy(poolHandle), ZE_RESULT_SUCCESS);

    EXPECT_EQ(zeEventDestroy(eventHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(zeEventPoolDestroy(eventPoolHandle), ZE_RESULT_SUCCESS);
}

TEST_F(MetricQueryPoolTest, givenRootDeviceWhenGetSubDeviceClientOptionsIsCalledThenReturnRootDeviceProperties) {

    auto &metricSource = device->getMetricDeviceContext().getMetricSource<OaMetricSourceImp>();
    auto &metricsLibrary = metricSource.getMetricsLibrary();
    auto subDevice = ClientOptionsData_1_0{};
    auto subDeviceIndex = ClientOptionsData_1_0{};
    auto subDeviceCount = ClientOptionsData_1_0{};
    auto workloadPartition = ClientOptionsData_1_0{};

    metricsLibrary.getSubDeviceClientOptions(subDevice, subDeviceIndex, subDeviceCount, workloadPartition);

    // Root device
    EXPECT_EQ(subDevice.Type, MetricsLibraryApi::ClientOptionsType::SubDevice);
    EXPECT_EQ(subDevice.SubDevice.Enabled, false);

    EXPECT_EQ(subDeviceIndex.Type, MetricsLibraryApi::ClientOptionsType::SubDeviceIndex);
    EXPECT_EQ(subDeviceIndex.SubDeviceIndex.Index, 0u);

    EXPECT_EQ(subDeviceCount.Type, MetricsLibraryApi::ClientOptionsType::SubDeviceCount);
    EXPECT_EQ(subDeviceCount.SubDeviceCount.Count, std::max(device->getNEODevice()->getNumSubDevices(), 1u));

    EXPECT_EQ(workloadPartition.Type, MetricsLibraryApi::ClientOptionsType::WorkloadPartition);
    EXPECT_EQ(workloadPartition.WorkloadPartition.Enabled, false);
}

TEST_F(MetricQueryPoolTest, givenUninitializedMetricEnumerationWhenGetQueryReportGpuSizeIsCalledThenReturnInvalidSize) {

    auto &metricSource = device->getMetricDeviceContext().getMetricSource<OaMetricSourceImp>();
    auto &metricsLibrary = metricSource.getMetricsLibrary();

    EXPECT_CALL(*mockMetricEnumeration, isInitialized())
        .Times(1)
        .WillOnce(Return(false));

    const uint32_t invalidSize = metricsLibrary.getQueryReportGpuSize();

    EXPECT_EQ(invalidSize, 0u);
}

TEST_F(MetricQueryPoolTest, givenCorrectArgumentsWhenActivateMetricGroupsIsCalledThenReturnsSuccess) {

    zet_device_handle_t metricDevice = device->toHandle();

    metricsDeviceParams.ConcurrentGroupsCount = 1;

    Mock<IConcurrentGroup_1_5> metricsConcurrentGroup;
    TConcurrentGroupParams_1_0 metricsConcurrentGroupParams = {};
    metricsConcurrentGroupParams.MetricSetsCount = 1;
    metricsConcurrentGroupParams.SymbolName = "OA";
    metricsConcurrentGroupParams.Description = "OA description";

    Mock<MetricsDiscovery::IMetricSet_1_5> metricsSet;
    MetricsDiscovery::TMetricSetParams_1_4 metricsSetParams = {};
    metricsSetParams.ApiMask = MetricsDiscovery::API_TYPE_OCL;
    metricsSetParams.MetricsCount = 0;
    metricsSetParams.SymbolName = "Metric set name";
    metricsSetParams.ShortName = "Metric set description";
    metricsSetParams.MetricsCount = 1;

    Mock<IMetric_1_0> metric;
    TMetricParams_1_0 metricParams = {};
    metricParams.SymbolName = "Metric symbol name";
    metricParams.ShortName = "Metric short name";
    metricParams.LongName = "Metric long name";
    metricParams.ResultType = MetricsDiscovery::TMetricResultType::RESULT_UINT64;
    metricParams.MetricType = MetricsDiscovery::TMetricType::METRIC_TYPE_RATIO;

    zet_metric_group_handle_t metricGroupHandle = {};

    zet_metric_group_properties_t metricGroupProperties = {};
    metricGroupProperties.samplingType = ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_EVENT_BASED;

    TypedValue_1_0 value = {};
    value.Type = ValueType::Uint32;
    value.ValueUInt32 = 64;

    ConfigurationHandle_1_0 metricsLibraryConfigurationHandle = {&value};
    ContextHandle_1_0 metricsLibraryContextHandle = {&value};

    openMetricsAdapter();

    EXPECT_CALL(metricsDevice, GetParams())
        .WillRepeatedly(Return(&metricsDeviceParams));

    EXPECT_CALL(metricsDevice, GetConcurrentGroup(_))
        .Times(1)
        .WillOnce(Return(&metricsConcurrentGroup));

    EXPECT_CALL(metricsConcurrentGroup, GetParams())
        .Times(1)
        .WillOnce(Return(&metricsConcurrentGroupParams));

    EXPECT_CALL(metricsConcurrentGroup, GetMetricSet(_))
        .WillRepeatedly(Return(&metricsSet));

    EXPECT_CALL(metricsSet, GetParams())
        .WillRepeatedly(Return(&metricsSetParams));

    EXPECT_CALL(metricsSet, GetMetric(_))
        .Times(1)
        .WillOnce(Return(&metric));

    EXPECT_CALL(metricsSet, Activate())
        .Times(1)
        .WillOnce(Return(MetricsDiscovery::CC_OK));

    EXPECT_CALL(metricsSet, Deactivate())
        .Times(1)
        .WillOnce(Return(MetricsDiscovery::CC_OK));

    EXPECT_CALL(metric, GetParams())
        .Times(1)
        .WillOnce(Return(&metricParams));

    EXPECT_CALL(metricsSet, SetApiFiltering(_))
        .WillRepeatedly(Return(TCompletionCode::CC_OK));

    EXPECT_CALL(*mockMetricEnumeration, isInitialized())
        .Times(1)
        .WillOnce(Return(true));

    EXPECT_CALL(*mockMetricsLibrary, getContextData(_, _))
        .Times(1)
        .WillOnce(Return(true));

    EXPECT_CALL(*mockMetricsLibrary, load())
        .Times(0);

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockContextCreate(_, _, _))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<2>(metricsLibraryContextHandle), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockContextDelete(_))
        .Times(1)
        .WillOnce(Return(StatusCode::Success));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockConfigurationCreate(_, _))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<1>(metricsLibraryConfigurationHandle), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockConfigurationActivate(_, _))
        .Times(1)
        .WillOnce(Return(StatusCode::Success));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockConfigurationDeactivate(_))
        .Times(1)
        .WillOnce(Return(StatusCode::Success));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockConfigurationDelete(_))
        .Times(1)
        .WillOnce(Return(StatusCode::Success));

    // Metric group count.
    uint32_t metricGroupCount = 0;
    EXPECT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);

    // Metric group handle.
    EXPECT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, &metricGroupHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);
    EXPECT_NE(metricGroupHandle, nullptr);

    // Activate metric group.
    EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), device->toHandle(), 1, &metricGroupHandle), ZE_RESULT_SUCCESS);

    // Activate metric groups.
    device->activateMetricGroups();

    EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), metricDevice, 0, nullptr), ZE_RESULT_SUCCESS);
}

} // namespace ult
} // namespace L0
