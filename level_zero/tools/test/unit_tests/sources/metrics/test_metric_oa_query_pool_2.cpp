/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/helpers/in_order_cmd_helpers.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/common/test_macros/test_base.h"

#include "level_zero/core/source/cmdqueue/cmdqueue.h"
#include "level_zero/core/source/context/context.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/tools/source/metrics/metric_oa_query_imp.h"
#include "level_zero/tools/source/metrics/metric_oa_source.h"
#include "level_zero/tools/test/unit_tests/sources/metrics/metric_query_pool_fixture.h"

#include "gtest/gtest.h"

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

    auto &metricSource = device->getMetricDeviceContext().getMetricSource<OaMetricSourceImp>();
    Mock<OaMetricGroupImp> metricGroup(metricSource);
    zet_metric_group_handle_t metricGroupHandle = metricGroup.toHandle();
    zet_metric_group_properties_t metricGroupProperties = {ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES, nullptr};

    metricsDeviceParams.ConcurrentGroupsCount = 1;

    Mock<IConcurrentGroup_1_13> metricsConcurrentGroup;
    TConcurrentGroupParams_1_13 metricsConcurrentGroupParams = {};
    metricsConcurrentGroupParams.MetricSetsCount = 1;
    metricsConcurrentGroupParams.SymbolName = "OA";
    metricsConcurrentGroupParams.Description = "OA description";
    metricsConcurrentGroupParams.IoMeasurementInformationCount = 1;

    Mock<MetricsDiscovery::IEquation_1_0> ioReadEquation;
    MetricsDiscovery::TEquationElement_1_0 ioEquationElement = {};
    ioEquationElement.Type = MetricsDiscovery::EQUATION_ELEM_IMM_UINT64;
    ioEquationElement.ImmediateUInt64 = 0;

    ioReadEquation.getEquationElement.push_back(&ioEquationElement);

    Mock<MetricsDiscovery::IInformation_1_0> ioMeasurement;
    MetricsDiscovery::TInformationParams_1_0 oaInformation = {};
    oaInformation.SymbolName = "BufferOverflow";
    oaInformation.IoReadEquation = &ioReadEquation;

    Mock<MetricsDiscovery::IMetricSet_1_13> metricsSet;
    MetricsDiscovery::TMetricSetParams_1_11 metricsSetParams = {};
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

    openMetricsAdapter();

    setupDefaultMocksForMetricDevice(metricsDevice);

    metricsDevice.getConcurrentGroupResults.push_back(&metricsConcurrentGroup);

    metricsConcurrentGroup.GetParamsResult = &metricsConcurrentGroupParams;
    metricsConcurrentGroup.getMetricSetResult = &metricsSet;
    metricsConcurrentGroup.GetIoMeasurementInformationResult = &ioMeasurement;
    ioMeasurement.GetParamsResult = &oaInformation;

    metricsSet.GetParamsResult = &metricsSetParams;

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
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false));
    zet_command_list_handle_t commandListHandle = commandList->toHandle();

    TypedValue_1_0 value = {};
    value.Type = ValueType::Uint32;
    value.ValueUInt32 = 64;
    ContextHandle_1_0 metricsLibraryContextHandle = {&value};

    CommandBufferSize_1_0 commandBufferSize = {};
    commandBufferSize.GpuMemorySize = 100;

    mockMetricsLibrary->g_mockApi->contextCreateOutHandle = metricsLibraryContextHandle;
    mockMetricsLibrary->g_mockApi->commandBufferGetSizeOutSize = commandBufferSize;

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
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false));
    zet_command_list_handle_t commandListHandle = commandList->toHandle();

    TypedValue_1_0 value = {};
    value.Type = ValueType::Uint32;
    value.ValueUInt32 = 64;
    ContextHandle_1_0 metricsLibraryContextHandle = {&value};

    CommandBufferSize_1_0 commandBufferSize = {};
    commandBufferSize.GpuMemorySize = 100;

    mockMetricsLibrary->g_mockApi->contextCreateOutHandle = metricsLibraryContextHandle;
    mockMetricsLibrary->g_mockApi->commandBufferGetSizeOutSize = commandBufferSize;

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
    ze_result_t returnValue;

    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_DEFAULT;

    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(device, &desc, false, NEO::EngineGroupType::renderCompute, returnValue));

    zet_device_handle_t metricDevice = device->toHandle();

    zet_metric_query_pool_handle_t poolHandle = {};
    zet_metric_query_handle_t queryHandle = {};
    zet_metric_query_pool_desc_t poolDesc = {};
    poolDesc.stype = ZET_STRUCTURE_TYPE_METRIC_QUERY_POOL_DESC;
    poolDesc.count = 1;
    poolDesc.type = ZET_METRIC_QUERY_POOL_TYPE_EXECUTION;

    ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);
    ASSERT_NE(nullptr, commandList);
    auto whiteBoxCmdList = CommandList::whiteboxCast(commandList.get());

    EXPECT_EQ(device, commandList->getDevice());
    EXPECT_TRUE(commandList->isImmediateType());
    EXPECT_NE(nullptr, whiteBoxCmdList->cmdQImmediate);

    zet_command_list_handle_t commandListHandle = commandList->toHandle();

    TypedValue_1_0 value = {};
    value.Type = ValueType::Uint32;
    value.ValueUInt32 = 64;
    ContextHandle_1_0 metricsLibraryContextHandle = {&value};

    CommandBufferSize_1_0 commandBufferSize = {};
    commandBufferSize.GpuMemorySize = 100;

    mockMetricsLibrary->g_mockApi->contextCreateOutHandle = metricsLibraryContextHandle;
    mockMetricsLibrary->g_mockApi->commandBufferGetSizeOutSize = commandBufferSize;

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
    ze_result_t returnValue;

    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_DEFAULT;

    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(device, &desc, false, NEO::EngineGroupType::renderCompute, returnValue));

    zet_device_handle_t metricDevice = device->toHandle();

    zet_metric_query_pool_handle_t poolHandle = {};
    zet_metric_query_handle_t queryHandle = {};
    zet_metric_query_pool_desc_t poolDesc = {};
    poolDesc.stype = ZET_STRUCTURE_TYPE_METRIC_QUERY_POOL_DESC;
    poolDesc.count = 1;
    poolDesc.type = ZET_METRIC_QUERY_POOL_TYPE_EXECUTION;

    ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);
    ASSERT_NE(nullptr, commandList);
    auto whiteBoxCmdList = CommandList::whiteboxCast(commandList.get());

    EXPECT_EQ(device, commandList->getDevice());
    EXPECT_TRUE(commandList->isImmediateType());
    EXPECT_NE(nullptr, whiteBoxCmdList->cmdQImmediate);
    zet_command_list_handle_t commandListHandle = commandList->toHandle();

    TypedValue_1_0 value = {};
    value.Type = ValueType::Uint32;
    value.ValueUInt32 = 64;
    ContextHandle_1_0 metricsLibraryContextHandle = {&value};

    CommandBufferSize_1_0 commandBufferSize = {};
    commandBufferSize.GpuMemorySize = 100;

    mockMetricsLibrary->g_mockApi->contextCreateOutHandle = metricsLibraryContextHandle;
    mockMetricsLibrary->g_mockApi->commandBufferGetSizeOutSize = commandBufferSize;

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

struct MetricQueryPoolImmediateCommandListTest : public MetricQueryPoolTest {
    void SetUp() override {
        MetricQueryPoolTest::SetUp();

        ze_command_queue_desc_t desc = {};
        desc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
        ze_result_t returnValue = ZE_RESULT_SUCCESS;
        commandList.reset(CommandList::createImmediate(device, &desc, false, NEO::EngineGroupType::renderCompute, returnValue));
        ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);
        ASSERT_EQ(ZE_RESULT_SUCCESS, commandList->ensureImmediateResourcesInitialized());
        whiteBoxCmdList = CommandList::whiteboxCast(commandList.get());
        csr = whiteBoxCmdList->cmdQImmediate->getCsr();

        value.Type = ValueType::Uint32;
        value.ValueUInt32 = 64;
        mockMetricsLibrary->g_mockApi->contextCreateOutHandle = {&value};
        CommandBufferSize_1_0 commandBufferSize = {};
        commandBufferSize.GpuMemorySize = metricCommandsSize;
        mockMetricsLibrary->g_mockApi->commandBufferGetSizeOutSize = commandBufferSize;

        zet_metric_query_pool_desc_t poolDesc = {ZET_STRUCTURE_TYPE_METRIC_QUERY_POOL_DESC};
        poolDesc.count = 1;
        poolDesc.type = ZET_METRIC_QUERY_POOL_TYPE_EXECUTION;
        ASSERT_EQ(ZE_RESULT_SUCCESS, zetMetricQueryPoolCreate(context->toHandle(), device->toHandle(), nullptr, &poolDesc, &poolHandle));
        ASSERT_EQ(ZE_RESULT_SUCCESS, zetMetricQueryCreate(poolHandle, 0, &queryHandle));
    }

    void TearDown() override {
        EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricQueryDestroy(queryHandle));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricQueryPoolDestroy(poolHandle));
        commandList.reset();
        MetricQueryPoolTest::TearDown();
    }

    void leaveAvailableSpaceInCommandStream(size_t availableSpace) {
        auto commandStream = whiteBoxCmdList->commandContainer.getCommandStream();
        commandStream->getSpace(commandStream->getAvailableSpace() - availableSpace);
        whiteBoxCmdList->cmdListCurrentStartOffset = commandStream->getUsed();
    }

    bool hasPendingCommands() {
        return whiteBoxCmdList->commandContainer.getCommandStream()->getUsed() != whiteBoxCmdList->cmdListCurrentStartOffset;
    }

    static constexpr uint32_t metricCommandsSize = 100;
    std::unique_ptr<L0::CommandList> commandList;
    CommandList *whiteBoxCmdList = nullptr;
    NEO::CommandStreamReceiver *csr = nullptr;
    TypedValue_1_0 value = {};
    zet_metric_query_pool_handle_t poolHandle = nullptr;
    zet_metric_query_handle_t queryHandle = nullptr;
};

TEST_F(MetricQueryPoolImmediateCommandListTest, givenLowAvailableSpaceInCommandStreamWhenAppendMetricQueryBeginIsCalledThenQueryCommandsAreWrittenToNewCommandBufferAndSubmitted) {
    leaveAvailableSpaceInCommandStream(commonImmediateCommandSize - 1);
    auto oldCommandBuffer = whiteBoxCmdList->commandContainer.getCommandStream()->getGraphicsAllocation();
    auto taskCountBefore = csr->peekTaskCount();

    EXPECT_EQ(ZE_RESULT_SUCCESS, zetCommandListAppendMetricQueryBegin(commandList->toHandle(), queryHandle));

    EXPECT_NE(oldCommandBuffer, whiteBoxCmdList->commandContainer.getCommandStream()->getGraphicsAllocation());
    EXPECT_GE(whiteBoxCmdList->cmdListCurrentStartOffset, metricCommandsSize);
    EXPECT_FALSE(hasPendingCommands());
    EXPECT_GT(csr->peekTaskCount(), taskCountBefore);
}

TEST_F(MetricQueryPoolImmediateCommandListTest, givenNoSignalEventWhenAppendMetricQueryEndIsCalledThenQueryCommandsAreSubmitted) {
    auto taskCountBefore = csr->peekTaskCount();

    EXPECT_EQ(ZE_RESULT_SUCCESS, zetCommandListAppendMetricQueryEnd(commandList->toHandle(), queryHandle, nullptr, 0, nullptr));

    EXPECT_FALSE(hasPendingCommands());
    EXPECT_EQ(taskCountBefore + 1, csr->peekTaskCount());
}

TEST_F(MetricQueryPoolImmediateCommandListTest, givenWaitEventAndSignalEventWhenAppendMetricQueryEndIsCalledThenCommandsAreSubmittedOnce) {
    ze_event_pool_desc_t eventPoolDesc = {ZE_STRUCTURE_TYPE_EVENT_POOL_DESC};
    eventPoolDesc.count = 2;
    ze_event_pool_handle_t eventPoolHandle = nullptr;
    zet_device_handle_t deviceHandle = device->toHandle();
    ASSERT_EQ(ZE_RESULT_SUCCESS, zeEventPoolCreate(context->toHandle(), &eventPoolDesc, 1, &deviceHandle, &eventPoolHandle));
    ze_event_desc_t eventDesc = {ZE_STRUCTURE_TYPE_EVENT_DESC};
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_DEVICE;
    ze_event_handle_t eventHandle = nullptr;
    ASSERT_EQ(ZE_RESULT_SUCCESS, zeEventCreate(eventPoolHandle, &eventDesc, &eventHandle));
    eventDesc.index = 1;
    ze_event_handle_t waitEventHandle = nullptr;
    ASSERT_EQ(ZE_RESULT_SUCCESS, zeEventCreate(eventPoolHandle, &eventDesc, &waitEventHandle));

    ASSERT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendEventReset(commandList->toHandle(), eventHandle));

    auto taskCountBefore = csr->peekTaskCount();
    ASSERT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendEventReset(commandList->toHandle(), eventHandle));
    auto singleFlushTaskCount = csr->peekTaskCount() - taskCountBefore;

    taskCountBefore = csr->peekTaskCount();
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetCommandListAppendMetricQueryEnd(commandList->toHandle(), queryHandle, eventHandle, 1, &waitEventHandle));

    EXPECT_EQ(taskCountBefore + singleFlushTaskCount, csr->peekTaskCount());
    EXPECT_FALSE(hasPendingCommands());

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventDestroy(waitEventHandle));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventDestroy(eventHandle));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventPoolDestroy(eventPoolHandle));
}

TEST_F(MetricQueryPoolImmediateCommandListTest, givenPerformanceQueryAndWaitEventAndSignalEventWhenAppendMetricQueryEndIsCalledThenCommandsAreSubmittedOnce) {
    auto &metricSource = device->getMetricDeviceContext().getMetricSource<OaMetricSourceImp>();
    Mock<OaMetricGroupImp> metricGroup(metricSource);
    zet_metric_group_properties_t metricGroupProperties = {ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES, nullptr};
    metricGroupProperties.samplingType = ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_EVENT_BASED;
    metricGroup.getPropertiesOutProperties = &metricGroupProperties;
    mockMetricsLibrary->g_mockApi->queryCreateOutHandle = {&value};
    mockMetricsLibrary->g_mockApi->getParameterOutValue = value;

    zet_metric_query_pool_desc_t performancePoolDesc = {ZET_STRUCTURE_TYPE_METRIC_QUERY_POOL_DESC};
    performancePoolDesc.count = 1;
    performancePoolDesc.type = ZET_METRIC_QUERY_POOL_TYPE_PERFORMANCE;
    zet_metric_query_pool_handle_t performancePoolHandle = nullptr;
    zet_metric_query_handle_t performanceQueryHandle = nullptr;
    ASSERT_EQ(ZE_RESULT_SUCCESS, zetMetricQueryPoolCreate(context->toHandle(), device->toHandle(), metricGroup.toHandle(), &performancePoolDesc, &performancePoolHandle));
    ASSERT_EQ(ZE_RESULT_SUCCESS, zetMetricQueryCreate(performancePoolHandle, 0, &performanceQueryHandle));

    ze_event_pool_desc_t eventPoolDesc = {ZE_STRUCTURE_TYPE_EVENT_POOL_DESC};
    eventPoolDesc.count = 2;
    ze_event_pool_handle_t eventPoolHandle = nullptr;
    zet_device_handle_t deviceHandle = device->toHandle();
    ASSERT_EQ(ZE_RESULT_SUCCESS, zeEventPoolCreate(context->toHandle(), &eventPoolDesc, 1, &deviceHandle, &eventPoolHandle));
    ze_event_desc_t eventDesc = {ZE_STRUCTURE_TYPE_EVENT_DESC};
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_DEVICE;
    ze_event_handle_t eventHandle = nullptr;
    ASSERT_EQ(ZE_RESULT_SUCCESS, zeEventCreate(eventPoolHandle, &eventDesc, &eventHandle));
    eventDesc.index = 1;
    ze_event_handle_t waitEventHandle = nullptr;
    ASSERT_EQ(ZE_RESULT_SUCCESS, zeEventCreate(eventPoolHandle, &eventDesc, &waitEventHandle));

    ASSERT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendEventReset(commandList->toHandle(), eventHandle));

    auto taskCountBefore = csr->peekTaskCount();
    ASSERT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendEventReset(commandList->toHandle(), eventHandle));
    auto singleFlushTaskCount = csr->peekTaskCount() - taskCountBefore;

    taskCountBefore = csr->peekTaskCount();
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetCommandListAppendMetricQueryEnd(commandList->toHandle(), performanceQueryHandle, eventHandle, 1, &waitEventHandle));

    EXPECT_EQ(taskCountBefore + singleFlushTaskCount, csr->peekTaskCount());
    EXPECT_FALSE(hasPendingCommands());

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventDestroy(waitEventHandle));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventDestroy(eventHandle));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventPoolDestroy(eventPoolHandle));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricQueryDestroy(performanceQueryHandle));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricQueryPoolDestroy(performancePoolHandle));
}

TEST_F(MetricQueryPoolImmediateCommandListTest, givenPerformanceQueryAndNotSignaledWaitEventWhenAppendMetricQueryEndIsCalledThenQueryPoolIsResidentForSubmissionOfQueryCommands) {
    auto &metricSource = device->getMetricDeviceContext().getMetricSource<OaMetricSourceImp>();
    Mock<OaMetricGroupImp> metricGroup(metricSource);
    zet_metric_group_properties_t metricGroupProperties = {ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES, nullptr};
    metricGroupProperties.samplingType = ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_EVENT_BASED;
    metricGroup.getPropertiesOutProperties = &metricGroupProperties;
    mockMetricsLibrary->g_mockApi->queryCreateOutHandle = {&value};
    mockMetricsLibrary->g_mockApi->getParameterOutValue = value;

    zet_metric_query_pool_desc_t poolDesc = {ZET_STRUCTURE_TYPE_METRIC_QUERY_POOL_DESC};
    poolDesc.count = 1;
    poolDesc.type = ZET_METRIC_QUERY_POOL_TYPE_PERFORMANCE;
    zet_metric_query_pool_handle_t performancePoolHandle = nullptr;
    ASSERT_EQ(ZE_RESULT_SUCCESS, zetMetricQueryPoolCreate(context->toHandle(), device->toHandle(), metricGroup.toHandle(), &poolDesc, &performancePoolHandle));
    zet_metric_query_handle_t performanceQueryHandle = nullptr;
    ASSERT_EQ(ZE_RESULT_SUCCESS, zetMetricQueryCreate(performancePoolHandle, 0, &performanceQueryHandle));
    auto poolAllocation = static_cast<OaMetricQueryPoolImp *>(MetricQueryPool::fromHandle(performancePoolHandle))->pAllocation;
    ASSERT_NE(nullptr, poolAllocation);

    ze_event_pool_desc_t eventPoolDesc = {ZE_STRUCTURE_TYPE_EVENT_POOL_DESC};
    eventPoolDesc.count = 1;
    ze_event_pool_handle_t eventPoolHandle = nullptr;
    zet_device_handle_t deviceHandle = device->toHandle();
    ASSERT_EQ(ZE_RESULT_SUCCESS, zeEventPoolCreate(context->toHandle(), &eventPoolDesc, 1, &deviceHandle, &eventPoolHandle));
    ze_event_desc_t eventDesc = {ZE_STRUCTURE_TYPE_EVENT_DESC};
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_DEVICE;
    ze_event_handle_t waitEventHandle = nullptr;
    ASSERT_EQ(ZE_RESULT_SUCCESS, zeEventCreate(eventPoolHandle, &eventDesc, &waitEventHandle));

    EXPECT_EQ(ZE_RESULT_SUCCESS, zetCommandListAppendMetricQueryEnd(commandList->toHandle(), performanceQueryHandle, nullptr, 1, &waitEventHandle));

    EXPECT_FALSE(hasPendingCommands());
    EXPECT_EQ(csr->peekTaskCount(), poolAllocation->getTaskCount(csr->getOsContext().getContextId()));

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventDestroy(waitEventHandle));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventPoolDestroy(eventPoolHandle));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricQueryDestroy(performanceQueryHandle));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricQueryPoolDestroy(performancePoolHandle));
}

TEST_F(MetricQueryPoolImmediateCommandListTest, givenInOrderImmediateCommandListWhenAppendMetricQueryEndIsCalledThenInOrderCounterIsAdvanced) {
    ze_command_queue_desc_t queueDesc = {};
    queueDesc.flags = ZE_COMMAND_QUEUE_FLAG_IN_ORDER;
    queueDesc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::CommandList> inOrderCommandList(CommandList::createImmediate(device, &queueDesc, false, NEO::EngineGroupType::renderCompute, returnValue));
    ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);
    ASSERT_TRUE(inOrderCommandList->isInOrderExecutionEnabled());
    auto inOrderExecInfo = inOrderCommandList->getInOrderExecInfo();
    auto counterValueBefore = inOrderExecInfo->getCounterValue();

    EXPECT_EQ(ZE_RESULT_SUCCESS, zetCommandListAppendMetricQueryEnd(inOrderCommandList->toHandle(), queryHandle, nullptr, 0, nullptr));

    EXPECT_EQ(counterValueBefore + 1, inOrderExecInfo->getCounterValue());
    EXPECT_EQ(inOrderExecInfo->getCounterValue(), inOrderExecInfo->getProgrammedCounterValue());
}

TEST_F(MetricQueryPoolImmediateCommandListTest, givenInOrderImmediateCommandListAndImplicitlyCounterBasedSignalEventWhenAppendMetricQueryEndIsCalledThenEventIsSignaledWithCounterValueAfterQueryCommands) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.EnableImplicitConvertionToCounterBasedEvents.set(1);
    ze_command_queue_desc_t queueDesc = {};
    queueDesc.flags = ZE_COMMAND_QUEUE_FLAG_IN_ORDER;
    queueDesc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::CommandList> inOrderCommandList(CommandList::createImmediate(device, &queueDesc, false, NEO::EngineGroupType::renderCompute, returnValue));
    ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);
    ASSERT_TRUE(inOrderCommandList->isInOrderExecutionEnabled());

    ze_event_pool_desc_t eventPoolDesc = {ZE_STRUCTURE_TYPE_EVENT_POOL_DESC};
    eventPoolDesc.count = 1;
    ze_event_pool_handle_t eventPoolHandle = nullptr;
    zet_device_handle_t deviceHandle = device->toHandle();
    ASSERT_EQ(ZE_RESULT_SUCCESS, zeEventPoolCreate(context->toHandle(), &eventPoolDesc, 1, &deviceHandle, &eventPoolHandle));
    ze_event_desc_t eventDesc = {ZE_STRUCTURE_TYPE_EVENT_DESC};
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_DEVICE;
    ze_event_handle_t eventHandle = nullptr;
    ASSERT_EQ(ZE_RESULT_SUCCESS, zeEventCreate(eventPoolHandle, &eventDesc, &eventHandle));
    ASSERT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendSignalEvent(inOrderCommandList->toHandle(), eventHandle));
    auto event = Event::fromHandle(eventHandle);
    ASSERT_TRUE(event->isCounterBased());
    ASSERT_FALSE(event->isCounterBasedExplicitlyEnabled());

    EXPECT_EQ(ZE_RESULT_SUCCESS, zetCommandListAppendMetricQueryEnd(inOrderCommandList->toHandle(), queryHandle, eventHandle, 0, nullptr));

    EXPECT_EQ(inOrderCommandList->getInOrderExecInfo()->getCounterValue(), event->getInOrderExecBaseSignalValue());

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventDestroy(eventHandle));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventPoolDestroy(eventPoolHandle));
}

TEST_F(MetricQueryPoolImmediateCommandListTest, givenInOrderImmediateCommandListAndCounterBasedSignalEventWhenAppendMetricQueryEndIsCalledThenInvalidSynchronizationObjectIsReturned) {
    ze_command_queue_desc_t queueDesc = {};
    queueDesc.flags = ZE_COMMAND_QUEUE_FLAG_IN_ORDER;
    queueDesc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::CommandList> inOrderCommandList(CommandList::createImmediate(device, &queueDesc, false, NEO::EngineGroupType::renderCompute, returnValue));
    ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);
    ASSERT_TRUE(inOrderCommandList->isInOrderExecutionEnabled());

    ze_event_pool_counter_based_exp_desc_t counterBasedDesc = {ZE_STRUCTURE_TYPE_COUNTER_BASED_EVENT_POOL_EXP_DESC};
    counterBasedDesc.flags = ZE_EVENT_POOL_COUNTER_BASED_EXP_FLAG_IMMEDIATE;
    ze_event_pool_desc_t eventPoolDesc = {ZE_STRUCTURE_TYPE_EVENT_POOL_DESC, &counterBasedDesc};
    eventPoolDesc.count = 1;
    ze_event_pool_handle_t eventPoolHandle = nullptr;
    zet_device_handle_t deviceHandle = device->toHandle();
    ASSERT_EQ(ZE_RESULT_SUCCESS, zeEventPoolCreate(context->toHandle(), &eventPoolDesc, 1, &deviceHandle, &eventPoolHandle));
    ze_event_desc_t eventDesc = {ZE_STRUCTURE_TYPE_EVENT_DESC};
    ze_event_handle_t eventHandle = nullptr;
    ASSERT_EQ(ZE_RESULT_SUCCESS, zeEventCreate(eventPoolHandle, &eventDesc, &eventHandle));
    ASSERT_TRUE(Event::fromHandle(eventHandle)->isCounterBasedExplicitlyEnabled());
    auto inOrderExecInfo = inOrderCommandList->getInOrderExecInfo();
    auto counterValueBefore = inOrderExecInfo->getCounterValue();

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_SYNCHRONIZATION_OBJECT, zetCommandListAppendMetricQueryEnd(inOrderCommandList->toHandle(), queryHandle, eventHandle, 0, nullptr));

    EXPECT_EQ(counterValueBefore, inOrderExecInfo->getCounterValue());

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventDestroy(eventHandle));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventPoolDestroy(eventPoolHandle));
}

TEST_F(MetricQueryPoolImmediateCommandListTest, givenInOrderImmediateCommandListAndSignalEventWhenMetricsLibraryFailsToWriteQueryEndCommandsThenErrorIsReturnedAndInOrderCounterIsNotAdvanced) {
    ze_command_queue_desc_t queueDesc = {};
    queueDesc.flags = ZE_COMMAND_QUEUE_FLAG_IN_ORDER;
    queueDesc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::CommandList> inOrderCommandList(CommandList::createImmediate(device, &queueDesc, false, NEO::EngineGroupType::renderCompute, returnValue));
    ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);
    ASSERT_TRUE(inOrderCommandList->isInOrderExecutionEnabled());

    ze_event_pool_desc_t eventPoolDesc = {ZE_STRUCTURE_TYPE_EVENT_POOL_DESC};
    eventPoolDesc.count = 1;
    ze_event_pool_handle_t eventPoolHandle = nullptr;
    zet_device_handle_t deviceHandle = device->toHandle();
    ASSERT_EQ(ZE_RESULT_SUCCESS, zeEventPoolCreate(context->toHandle(), &eventPoolDesc, 1, &deviceHandle, &eventPoolHandle));
    ze_event_desc_t eventDesc = {ZE_STRUCTURE_TYPE_EVENT_DESC};
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_DEVICE;
    ze_event_handle_t eventHandle = nullptr;
    ASSERT_EQ(ZE_RESULT_SUCCESS, zeEventCreate(eventPoolHandle, &eventDesc, &eventHandle));
    auto inOrderExecInfo = inOrderCommandList->getInOrderExecInfo();
    auto counterValueBefore = inOrderExecInfo->getCounterValue();
    mockMetricsLibrary->g_mockApi->commandBufferGetResult = StatusCode::Failed;

    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zetCommandListAppendMetricQueryEnd(inOrderCommandList->toHandle(), queryHandle, eventHandle, 0, nullptr));

    EXPECT_EQ(counterValueBefore, inOrderExecInfo->getCounterValue());

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventDestroy(eventHandle));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventPoolDestroy(eventPoolHandle));
}

HWTEST_F(MetricQueryPoolImmediateCommandListTest, givenNotSignaledWaitEventWhenAppendMetricQueryEndIsCalledThenDependenciesArePresent) {
    ze_event_pool_desc_t eventPoolDesc = {ZE_STRUCTURE_TYPE_EVENT_POOL_DESC};
    eventPoolDesc.count = 1;
    ze_event_pool_handle_t eventPoolHandle = nullptr;
    zet_device_handle_t deviceHandle = device->toHandle();
    ASSERT_EQ(ZE_RESULT_SUCCESS, zeEventPoolCreate(context->toHandle(), &eventPoolDesc, 1, &deviceHandle, &eventPoolHandle));
    ze_event_desc_t eventDesc = {ZE_STRUCTURE_TYPE_EVENT_DESC};
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_DEVICE;
    ze_event_handle_t waitEventHandle = nullptr;
    ASSERT_EQ(ZE_RESULT_SUCCESS, zeEventCreate(eventPoolHandle, &eventDesc, &waitEventHandle));
    auto immediateCommandList = static_cast<WhiteBox<CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>> *>(commandList.get());
    immediateCommandList->dependenciesPresent = false;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zetCommandListAppendMetricQueryEnd(commandList->toHandle(), queryHandle, nullptr, 1, &waitEventHandle));

    EXPECT_TRUE(immediateCommandList->dependenciesPresent);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventDestroy(waitEventHandle));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventPoolDestroy(eventPoolHandle));
}

HWTEST_F(MetricQueryPoolImmediateCommandListTest, givenInOrderImmediateCommandListWithNotCompletedPreviousOperationWhenAppendMetricMemoryBarrierIsCalledThenItWaitsForInOrderCounter) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    if (NEO::InOrderProgrammingHelpers::isLriFor64bDataProgrammingRequired(FamilyType::isQwordInOrderCounter, device->getNEODevice()->getDeviceInfo().semaphore64bCmdSupport)) {
        GTEST_SKIP();
    }
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.ResolveDependenciesViaPipeControls.set(0);
    ze_command_queue_desc_t queueDesc = {};
    queueDesc.flags = ZE_COMMAND_QUEUE_FLAG_IN_ORDER;
    queueDesc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::CommandList> inOrderCommandList(CommandList::createImmediate(device, &queueDesc, false, NEO::EngineGroupType::renderCompute, returnValue));
    ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);
    ASSERT_TRUE(inOrderCommandList->isInOrderExecutionEnabled());
    auto inOrderExecInfo = inOrderCommandList->getInOrderExecInfo();

    ASSERT_EQ(ZE_RESULT_SUCCESS, zetCommandListAppendMetricMemoryBarrier(inOrderCommandList->toHandle()));
    auto commandStream = inOrderCommandList->getCmdContainer().getCommandStream();
    auto offset = commandStream->getUsed();

    ASSERT_EQ(ZE_RESULT_SUCCESS, zetCommandListAppendMetricMemoryBarrier(inOrderCommandList->toHandle()));

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(*commandStream, offset);
    auto semaphores = findAll<MI_SEMAPHORE_WAIT *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    auto counterAddress = inOrderExecInfo->getBaseDeviceAddress() + inOrderExecInfo->getAllocationOffset();
    bool counterWaitFound = false;
    for (auto &semaphore : semaphores) {
        counterWaitFound |= genCmdCast<MI_SEMAPHORE_WAIT *>(*semaphore)->getSemaphoreGraphicsAddress() == counterAddress;
    }
    EXPECT_TRUE(counterWaitFound);
}

TEST_F(MetricQueryPoolImmediateCommandListTest, givenLowAvailableSpaceInCommandStreamWhenAppendMetricMemoryBarrierIsCalledThenBarrierCommandsAreWrittenToNewCommandBufferAndSubmitted) {
    leaveAvailableSpaceInCommandStream(commonImmediateCommandSize - 1);
    auto oldCommandBuffer = whiteBoxCmdList->commandContainer.getCommandStream()->getGraphicsAllocation();
    auto taskCountBefore = csr->peekTaskCount();

    EXPECT_EQ(ZE_RESULT_SUCCESS, zetCommandListAppendMetricMemoryBarrier(commandList->toHandle()));

    EXPECT_NE(oldCommandBuffer, whiteBoxCmdList->commandContainer.getCommandStream()->getGraphicsAllocation());
    EXPECT_GE(whiteBoxCmdList->cmdListCurrentStartOffset, metricCommandsSize);
    EXPECT_FALSE(hasPendingCommands());
    EXPECT_GT(csr->peekTaskCount(), taskCountBefore);
}

TEST_F(MetricQueryPoolImmediateCommandListTest, givenMetricsLibraryFailsToWriteCommandsWhenAppendMetricQueryBeginIsCalledThenErrorIsReturnedAndNothingIsSubmitted) {
    mockMetricsLibrary->g_mockApi->commandBufferGetResult = StatusCode::Failed;
    auto taskCountBefore = csr->peekTaskCount();

    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zetCommandListAppendMetricQueryBegin(commandList->toHandle(), queryHandle));

    EXPECT_EQ(taskCountBefore, csr->peekTaskCount());
    EXPECT_FALSE(hasPendingCommands());
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
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false));
    zet_command_list_handle_t commandListHandle = commandList->toHandle();

    TypedValue_1_0 value = {};
    value.Type = ValueType::Uint32;
    value.ValueUInt32 = 64;
    ContextHandle_1_0 metricsLibraryContextHandle = {&value};

    mockMetricsLibrary->g_mockApi->contextCreateOutHandle = metricsLibraryContextHandle;
    mockMetricsLibrary->g_mockApi->commandBufferGetSizeResult = StatusCode::Failed;

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

    mockMetricEnumeration->isInitializedResult = false;

    const uint32_t invalidSize = metricsLibrary.getQueryReportGpuSize();

    EXPECT_EQ(invalidSize, 0u);
}

TEST_F(MetricQueryPoolTest, givenCorrectArgumentsWhenActivateMetricGroupsIsCalledThenReturnsSuccess) {

    zet_device_handle_t metricDevice = device->toHandle();

    metricsDeviceParams.ConcurrentGroupsCount = 1;

    Mock<IConcurrentGroup_1_13> metricsConcurrentGroup;
    TConcurrentGroupParams_1_13 metricsConcurrentGroupParams = {};
    metricsConcurrentGroupParams.MetricSetsCount = 1;
    metricsConcurrentGroupParams.SymbolName = "OA";
    metricsConcurrentGroupParams.Description = "OA description";
    metricsConcurrentGroupParams.IoMeasurementInformationCount = 1;

    Mock<MetricsDiscovery::IEquation_1_0> ioReadEquation;
    MetricsDiscovery::TEquationElement_1_0 ioEquationElement = {};
    ioEquationElement.Type = MetricsDiscovery::EQUATION_ELEM_IMM_UINT64;
    ioEquationElement.ImmediateUInt64 = 0;

    ioReadEquation.getEquationElement.push_back(&ioEquationElement);

    Mock<MetricsDiscovery::IInformation_1_0> ioMeasurement;
    MetricsDiscovery::TInformationParams_1_0 oaInformation = {};
    oaInformation.SymbolName = "BufferOverflow";
    oaInformation.IoReadEquation = &ioReadEquation;

    Mock<MetricsDiscovery::IMetricSet_1_13> metricsSet;
    MetricsDiscovery::TMetricSetParams_1_11 metricsSetParams = {};
    metricsSetParams.ApiMask = MetricsDiscovery::API_TYPE_OCL;
    metricsSetParams.MetricsCount = 0;
    metricsSetParams.SymbolName = "Metric set name";
    metricsSetParams.ShortName = "Metric set description";
    metricsSetParams.MetricsCount = 1;

    Mock<IMetric_1_13> metric;
    TMetricParams_1_13 metricParams = {};
    metricParams.SymbolName = "Metric symbol name";
    metricParams.ShortName = "Metric short name";
    metricParams.LongName = "Metric long name";
    metricParams.ResultType = MetricsDiscovery::TMetricResultType::RESULT_UINT64;
    metricParams.MetricType = MetricsDiscovery::TMetricType::METRIC_TYPE_RATIO;

    zet_metric_group_handle_t metricGroupHandle = {};

    TypedValue_1_0 value = {};
    value.Type = ValueType::Uint32;
    value.ValueUInt32 = 64;

    ConfigurationHandle_1_0 metricsLibraryConfigurationHandle = {&value};
    ContextHandle_1_0 metricsLibraryContextHandle = {&value};

    openMetricsAdapter();

    setupDefaultMocksForMetricDevice(metricsDevice);

    metricsDevice.getConcurrentGroupResults.push_back(&metricsConcurrentGroup);

    metricsConcurrentGroup.GetParamsResult = &metricsConcurrentGroupParams;
    metricsConcurrentGroup.getMetricSetResult = &metricsSet;
    metricsConcurrentGroup.GetIoMeasurementInformationResult = &ioMeasurement;
    ioMeasurement.GetParamsResult = &oaInformation;

    metricsSet.GetParamsResult = &metricsSetParams;
    metricsSet.GetMetricResult = &metric;

    metric.GetParamsResult = &metricParams;

    mockMetricsLibrary->g_mockApi->contextCreateOutHandle = metricsLibraryContextHandle;
    mockMetricsLibrary->g_mockApi->configurationCreateOutHandle = metricsLibraryConfigurationHandle;
    mockMetricsLibrary->g_mockApi->configurationActivationCounter = 1;
    mockMetricsLibrary->g_mockApi->configurationDeactivationCounter = 1;

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
