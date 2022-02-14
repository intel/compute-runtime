/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/device_factory.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/driver/driver_imp.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_device.h"
#include "level_zero/core/test/unit_tests/mocks/mock_driver_handle.h"
#include "level_zero/tools/source/metrics/metric_oa_source.h"
#include "level_zero/tools/test/unit_tests/sources/metrics/mock_metric_oa.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

using ::testing::_;
using ::testing::Return;

namespace L0 {
namespace ult {

class MultiDeviceMetricQueryPoolTest : public MetricMultiDeviceFixture,
                                       public ::testing::Test {
  public:
    void SetUp() override {
        ze_result_t returnValue = ZE_RESULT_SUCCESS;
        MetricMultiDeviceFixture::SetUp();
        auto executionEnvironment = new NEO::ExecutionEnvironment();
        driverHandle.reset(DriverHandle::create(NEO::DeviceFactory::createDevices(*executionEnvironment), L0EnvVariables{}, &returnValue));
    }

    void TearDown() override {
        MetricMultiDeviceFixture::TearDown();
        driverHandle.reset();
        GlobalDriver = nullptr;
    }
    std::unique_ptr<L0::DriverHandle> driverHandle;
};

TEST_F(MultiDeviceMetricQueryPoolTest, givenSubDeviceWhenGetSubDeviceClientOptionsIsCalledThenReturnSubDeviceProperties) {

    auto &deviceImp = *static_cast<DeviceImp *>(devices[0]);
    auto subDevice = ClientOptionsData_1_0{};
    auto subDeviceIndex = ClientOptionsData_1_0{};
    auto subDeviceCount = ClientOptionsData_1_0{};
    auto workloadPartition = ClientOptionsData_1_0{};

    // Sub devices
    for (uint32_t i = 0, count = deviceImp.numSubDevices; i < count; ++i) {

        auto &metricSource = deviceImp.subDevices[i]->getMetricDeviceContext().getMetricSource<OaMetricSourceImp>();
        auto &metricsLibrary = metricSource.getMetricsLibrary();

        metricsLibrary.getSubDeviceClientOptions(subDevice, subDeviceIndex, subDeviceCount, workloadPartition);

        EXPECT_EQ(subDevice.Type, MetricsLibraryApi::ClientOptionsType::SubDevice);
        EXPECT_EQ(subDevice.SubDevice.Enabled, true);

        EXPECT_EQ(subDeviceIndex.Type, MetricsLibraryApi::ClientOptionsType::SubDeviceIndex);
        EXPECT_EQ(subDeviceIndex.SubDeviceIndex.Index, i);

        EXPECT_EQ(subDeviceCount.Type, MetricsLibraryApi::ClientOptionsType::SubDeviceCount);
        EXPECT_EQ(subDeviceCount.SubDeviceCount.Count, std::max(deviceImp.numSubDevices, 1u));

        EXPECT_EQ(workloadPartition.Type, MetricsLibraryApi::ClientOptionsType::WorkloadPartition);
        EXPECT_EQ(workloadPartition.WorkloadPartition.Enabled, false);
    }
}

TEST_F(MultiDeviceMetricQueryPoolTest, givenSubDeviceWithWorkloadPartitionWhenGetSubDeviceClientOptionsIsCalledThenReturnSubDeviceProperties) {

    auto &deviceImp = *static_cast<DeviceImp *>(devices[0]);
    auto subDevice = ClientOptionsData_1_0{};
    auto subDeviceIndex = ClientOptionsData_1_0{};
    auto subDeviceCount = ClientOptionsData_1_0{};
    auto workloadPartition = ClientOptionsData_1_0{};

    // Sub devices
    for (uint32_t i = 0, count = deviceImp.numSubDevices; i < count; ++i) {

        auto &metricSource = deviceImp.subDevices[i]->getMetricDeviceContext().getMetricSource<OaMetricSourceImp>();
        auto &metricsLibrary = metricSource.getMetricsLibrary();

        metricsLibrary.enableWorkloadPartition();

        metricsLibrary.getSubDeviceClientOptions(subDevice, subDeviceIndex, subDeviceCount, workloadPartition);

        EXPECT_EQ(subDevice.Type, MetricsLibraryApi::ClientOptionsType::SubDevice);
        EXPECT_EQ(subDevice.SubDevice.Enabled, true);

        EXPECT_EQ(subDeviceIndex.Type, MetricsLibraryApi::ClientOptionsType::SubDeviceIndex);
        EXPECT_EQ(subDeviceIndex.SubDeviceIndex.Index, i);

        EXPECT_EQ(subDeviceCount.Type, MetricsLibraryApi::ClientOptionsType::SubDeviceCount);
        EXPECT_EQ(subDeviceCount.SubDeviceCount.Count, std::max(deviceImp.numSubDevices, 1u));

        EXPECT_EQ(workloadPartition.Type, MetricsLibraryApi::ClientOptionsType::WorkloadPartition);
        EXPECT_EQ(workloadPartition.WorkloadPartition.Enabled, true);
    }
}

TEST_F(MultiDeviceMetricQueryPoolTest, givenCorrectArgumentsWhenZetMetricQueryPoolCreateIsCalledThenReturnsSuccess) {

    zet_device_handle_t metricDevice = devices[0]->toHandle();
    auto &deviceImp = *static_cast<DeviceImp *>(devices[0]);
    const uint32_t subDeviceCount = static_cast<uint32_t>(deviceImp.subDevices.size());

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

    openMetricsAdapter();

    EXPECT_CALL(metricsDevice, GetParams())
        .WillRepeatedly(Return(&metricsDeviceParams));

    EXPECT_CALL(metricsDevice, GetConcurrentGroup(_))
        .Times(subDeviceCount)
        .WillRepeatedly(Return(&metricsConcurrentGroup));

    EXPECT_CALL(metricsConcurrentGroup, GetParams())
        .Times(subDeviceCount)
        .WillRepeatedly(Return(&metricsConcurrentGroupParams));

    EXPECT_CALL(metricsConcurrentGroup, GetMetricSet(_))
        .WillRepeatedly(Return(&metricsSet));

    EXPECT_CALL(metricsSet, GetParams())
        .WillRepeatedly(Return(&metricsSetParams));

    EXPECT_CALL(metricsSet, GetMetric(_))
        .Times(subDeviceCount)
        .WillRepeatedly(Return(&metric));

    EXPECT_CALL(metric, GetParams())
        .Times(subDeviceCount)
        .WillRepeatedly(Return(&metricParams));

    EXPECT_CALL(metricsSet, SetApiFiltering(_))
        .WillRepeatedly(Return(TCompletionCode::CC_OK));

    for (uint32_t i = 0; i < subDeviceCount; ++i) {
        EXPECT_CALL(*mockMetricEnumerationSubDevices[i], isInitialized())
            .Times(1)
            .WillOnce(Return(true));

        EXPECT_CALL(*mockMetricsLibrarySubDevices[i], getContextData(_, _))
            .Times(1)
            .WillOnce(Return(true));
    }

    EXPECT_CALL(*mockMetricsLibrary, load())
        .Times(0);

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockQueryCreate(_, _))
        .Times(subDeviceCount)
        .WillRepeatedly(DoAll(::testing::SetArgPointee<1>(metricsLibraryQueryHandle), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockQueryDelete(_))
        .Times(subDeviceCount)
        .WillRepeatedly(Return(StatusCode::Success));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockGetParameter(_, _, _))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<2>(value), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockContextCreate(_, _, _))
        .Times(subDeviceCount)
        .WillRepeatedly(DoAll(::testing::SetArgPointee<2>(metricsLibraryContextHandle), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockContextDelete(_))
        .Times(subDeviceCount)
        .WillRepeatedly(Return(StatusCode::Success));

    // Metric group count.
    uint32_t metricGroupCount = 0;
    EXPECT_EQ(zetMetricGroupGet(devices[0]->toHandle(), &metricGroupCount, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);

    // Metric group handle.
    EXPECT_EQ(zetMetricGroupGet(devices[0]->toHandle(), &metricGroupCount, &metricGroupHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);
    EXPECT_NE(metricGroupHandle, nullptr);

    // Activate metric group.
    EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), devices[0]->toHandle(), 1, &metricGroupHandle), ZE_RESULT_SUCCESS);

    // Create metric query pool.
    EXPECT_EQ(zetMetricQueryPoolCreate(context->toHandle(), metricDevice, metricGroupHandle, &poolDesc, &poolHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(poolHandle, nullptr);

    // Create metric query.
    EXPECT_EQ(zetMetricQueryCreate(poolHandle, 0, &queryHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(queryHandle, nullptr);

    // Destroy query and its pool.
    EXPECT_EQ(zetMetricQueryDestroy(queryHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(zetMetricQueryPoolDestroy(poolHandle), ZE_RESULT_SUCCESS);
}

TEST_F(MultiDeviceMetricQueryPoolTest, givenEnableWalkerPartitionIsOnWhenZetCommandListAppendMetricQueryBeginEndIsCalledForSubDeviceThenReturnsSuccess) {

    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableWalkerPartition.set(1);

    auto &deviceImp = *static_cast<DeviceImp *>(devices[0]);
    zet_device_handle_t metricDeviceHandle = deviceImp.subDevices[0]->toHandle();

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

    openMetricsAdapterSubDevice(0);

    EXPECT_CALL(metricsDevice, GetParams())
        .WillRepeatedly(Return(&metricsDeviceParams));

    EXPECT_CALL(metricsDevice, GetConcurrentGroup(_))
        .Times(1)
        .WillRepeatedly(Return(&metricsConcurrentGroup));

    EXPECT_CALL(metricsConcurrentGroup, GetParams())
        .Times(1)
        .WillRepeatedly(Return(&metricsConcurrentGroupParams));

    EXPECT_CALL(metricsConcurrentGroup, GetMetricSet(_))
        .WillRepeatedly(Return(&metricsSet));

    EXPECT_CALL(metricsSet, GetParams())
        .WillRepeatedly(Return(&metricsSetParams));

    EXPECT_CALL(metricsSet, GetMetric(_))
        .Times(1)
        .WillRepeatedly(Return(&metric));

    EXPECT_CALL(metric, GetParams())
        .Times(1)
        .WillRepeatedly(Return(&metricParams));

    EXPECT_CALL(metricsSet, SetApiFiltering(_))
        .WillRepeatedly(Return(TCompletionCode::CC_OK));

    EXPECT_CALL(*mockMetricEnumerationSubDevices[0], isInitialized())
        .Times(1)
        .WillOnce(Return(true));

    EXPECT_CALL(*mockMetricsLibrarySubDevices[0], getContextData(_, _))
        .Times(1)
        .WillOnce(Return(true));

    EXPECT_CALL(*mockMetricsLibrary, load())
        .Times(0);

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockQueryCreate(_, _))
        .Times(1)
        .WillRepeatedly(DoAll(::testing::SetArgPointee<1>(metricsLibraryQueryHandle), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockQueryDelete(_))
        .Times(1)
        .WillRepeatedly(Return(StatusCode::Success));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockGetParameter(_, _, _))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<2>(value), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockContextCreate(_, _, _))
        .Times(1)
        .WillRepeatedly(DoAll(::testing::SetArgPointee<2>(metricsLibraryContextHandle), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockContextDelete(_))
        .Times(1)
        .WillRepeatedly(Return(StatusCode::Success));

    uint32_t metricGroupCount = 0;
    EXPECT_EQ(zetMetricGroupGet(metricDeviceHandle, &metricGroupCount, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);

    EXPECT_EQ(zetMetricGroupGet(metricDeviceHandle, &metricGroupCount, &metricGroupHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);
    EXPECT_NE(metricGroupHandle, nullptr);

    EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), metricDeviceHandle, 1, &metricGroupHandle), ZE_RESULT_SUCCESS);

    EXPECT_EQ(zetMetricQueryPoolCreate(context->toHandle(), metricDeviceHandle, metricGroupHandle, &poolDesc, &poolHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(poolHandle, nullptr);

    EXPECT_EQ(zetMetricQueryCreate(poolHandle, 0, &queryHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(queryHandle, nullptr);

    EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), metricDeviceHandle, 0, nullptr), ZE_RESULT_SUCCESS);

    EXPECT_EQ(zetMetricQueryDestroy(queryHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(zetMetricQueryPoolDestroy(poolHandle), ZE_RESULT_SUCCESS);
}

TEST_F(MultiDeviceMetricQueryPoolTest, givenEnableWalkerPartitionIsOnWhenZetCommandListAppendMetricMemoryBarrierIsCalledForSubDeviceThenReturnsSuccess) {

    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableWalkerPartition.set(1);

    auto &deviceImp = *static_cast<DeviceImp *>(devices[0]);

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, deviceImp.subDevices[0], NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    zet_command_list_handle_t commandListHandle = commandList->toHandle();

    TypedValue_1_0 value = {};
    value.Type = ValueType::Uint32;
    value.ValueUInt32 = 64;

    QueryHandle_1_0 metricsLibraryQueryHandle = {&value};
    ContextHandle_1_0 metricsLibraryContextHandle = {&value};

    CommandBufferSize_1_0 commandBufferSize = {};
    commandBufferSize.GpuMemorySize = 100;

    EXPECT_CALL(*mockMetricEnumerationSubDevices[0], isInitialized())
        .Times(1)
        .WillOnce(Return(true));

    EXPECT_CALL(*mockMetricsLibrarySubDevices[0], getContextData(_, _))
        .Times(1)
        .WillOnce(Return(true));

    EXPECT_CALL(*mockMetricsLibrary, load())
        .Times(0);

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockCommandBufferGetSize(_, _))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<1>(::testing::ByRef(commandBufferSize)), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockCommandBufferGet(_))
        .Times(1)
        .WillOnce(Return(StatusCode::Success));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockContextCreate(_, _, _))
        .Times(1)
        .WillRepeatedly(DoAll(::testing::SetArgPointee<2>(metricsLibraryContextHandle), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockContextDelete(_))
        .Times(1)
        .WillRepeatedly(Return(StatusCode::Success));

    EXPECT_EQ(zetCommandListAppendMetricMemoryBarrier(commandListHandle), ZE_RESULT_SUCCESS);
}

TEST_F(MultiDeviceMetricQueryPoolTest, givenFailedMetricsLibraryContextWhenZetMetricQueryPoolCreateIsCalledThenReturnFail) {

    zet_device_handle_t metricDevice = devices[0]->toHandle();
    auto &deviceImp = *static_cast<DeviceImp *>(devices[0]);
    const uint32_t subDeviceCount = static_cast<uint32_t>(deviceImp.subDevices.size());

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

    openMetricsAdapter();

    EXPECT_CALL(metricsDevice, GetParams())
        .WillRepeatedly(Return(&metricsDeviceParams));

    EXPECT_CALL(metricsDevice, GetConcurrentGroup(_))
        .Times(subDeviceCount)
        .WillRepeatedly(Return(&metricsConcurrentGroup));

    EXPECT_CALL(metricsConcurrentGroup, GetParams())
        .Times(subDeviceCount)
        .WillRepeatedly(Return(&metricsConcurrentGroupParams));

    EXPECT_CALL(metricsConcurrentGroup, GetMetricSet(_))
        .WillRepeatedly(Return(&metricsSet));

    EXPECT_CALL(metricsSet, GetParams())
        .WillRepeatedly(Return(&metricsSetParams));

    EXPECT_CALL(metricsSet, GetMetric(_))
        .Times(subDeviceCount)
        .WillRepeatedly(Return(&metric));

    EXPECT_CALL(metric, GetParams())
        .Times(subDeviceCount)
        .WillRepeatedly(Return(&metricParams));

    EXPECT_CALL(metricsSet, SetApiFiltering(_))
        .WillRepeatedly(Return(TCompletionCode::CC_OK));

    for (uint32_t i = 0; i < subDeviceCount; ++i) {
        EXPECT_CALL(*mockMetricEnumerationSubDevices[i], isInitialized())
            .Times(1)
            .WillOnce(Return(true));

        EXPECT_CALL(*mockMetricsLibrarySubDevices[i], getContextData(_, _))
            .Times(1)
            .WillOnce(Return(true));
    }

    EXPECT_CALL(*mockMetricsLibrary, load())
        .Times(0);

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockQueryCreate(_, _))
        .Times(subDeviceCount)
        .WillOnce(DoAll(::testing::SetArgPointee<1>(metricsLibraryQueryHandle), Return(StatusCode::Success)))
        .WillOnce(Return(StatusCode::Failed));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockQueryDelete(_))
        .Times(1)
        .WillOnce(Return(StatusCode::Success));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockGetParameter(_, _, _))
        .Times(0);

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockContextCreate(_, _, _))
        .Times(subDeviceCount)
        .WillRepeatedly(DoAll(::testing::SetArgPointee<2>(metricsLibraryContextHandle), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockContextDelete(_))
        .Times(subDeviceCount)
        .WillRepeatedly(Return(StatusCode::Success));

    // Metric group count.
    uint32_t metricGroupCount = 0;
    EXPECT_EQ(zetMetricGroupGet(devices[0]->toHandle(), &metricGroupCount, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);

    // Metric group handle.
    EXPECT_EQ(zetMetricGroupGet(devices[0]->toHandle(), &metricGroupCount, &metricGroupHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);
    EXPECT_NE(metricGroupHandle, nullptr);

    // Activate metric group.
    EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), devices[0]->toHandle(), 1, &metricGroupHandle), ZE_RESULT_SUCCESS);

    // Create metric query pool.
    EXPECT_EQ(zetMetricQueryPoolCreate(context->toHandle(), metricDevice, metricGroupHandle, &poolDesc, &poolHandle), ZE_RESULT_ERROR_INVALID_ARGUMENT);
}

TEST_F(MultiDeviceMetricQueryPoolTest, givenExecutionQueryTypeWhenZetMetricQueryPoolCreateIsCalledThenQueryPoolIsObtained) {
    zet_device_handle_t metricDevice = devices[0]->toHandle();

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

TEST_F(MultiDeviceMetricQueryPoolTest, givenFailedGetDataWhenZetMetricQueryGetDataIsCalledThenReturnsFail) {

    zet_device_handle_t metricDevice = devices[0]->toHandle();
    auto &deviceImp = *static_cast<DeviceImp *>(devices[0]);
    const uint32_t subDeviceCount = static_cast<uint32_t>(deviceImp.subDevices.size());

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

    openMetricsAdapter();

    EXPECT_CALL(metricsDevice, GetParams())
        .WillRepeatedly(Return(&metricsDeviceParams));

    EXPECT_CALL(metricsDevice, GetConcurrentGroup(_))
        .Times(subDeviceCount)
        .WillRepeatedly(Return(&metricsConcurrentGroup));

    EXPECT_CALL(metricsConcurrentGroup, GetParams())
        .Times(subDeviceCount)
        .WillRepeatedly(Return(&metricsConcurrentGroupParams));

    EXPECT_CALL(metricsConcurrentGroup, GetMetricSet(_))
        .WillRepeatedly(Return(&metricsSet));

    EXPECT_CALL(metricsSet, GetParams())
        .WillRepeatedly(Return(&metricsSetParams));

    EXPECT_CALL(metricsSet, GetMetric(_))
        .Times(subDeviceCount)
        .WillRepeatedly(Return(&metric));

    EXPECT_CALL(metric, GetParams())
        .Times(subDeviceCount)
        .WillRepeatedly(Return(&metricParams));

    EXPECT_CALL(metricsSet, SetApiFiltering(_))
        .WillRepeatedly(Return(TCompletionCode::CC_OK));

    for (uint32_t i = 0; i < subDeviceCount; ++i) {
        EXPECT_CALL(*mockMetricEnumerationSubDevices[i], isInitialized())
            .Times(1)
            .WillOnce(Return(true));

        EXPECT_CALL(*mockMetricsLibrarySubDevices[i], getContextData(_, _))
            .Times(1)
            .WillOnce(Return(true));
    }

    EXPECT_CALL(*mockMetricsLibrarySubDevices[0], getMetricQueryReportSize(_))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgReferee<0>(reportSize), Return(true)));

    EXPECT_CALL(*mockMetricsLibrary, load())
        .Times(0);

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockQueryCreate(_, _))
        .Times(subDeviceCount)
        .WillRepeatedly(DoAll(::testing::SetArgPointee<1>(metricsLibraryQueryHandle), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockQueryDelete(_))
        .Times(subDeviceCount)
        .WillRepeatedly(Return(StatusCode::Success));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockGetParameter(_, _, _))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<2>(value), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockContextCreate(_, _, _))
        .Times(subDeviceCount)
        .WillRepeatedly(DoAll(::testing::SetArgPointee<2>(metricsLibraryContextHandle), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockGetData(_))
        .Times(subDeviceCount)
        .WillOnce(Return(StatusCode::Success))
        .WillOnce(Return(StatusCode::Failed));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockContextDelete(_))
        .Times(subDeviceCount)
        .WillRepeatedly(Return(StatusCode::Success));

    // Metric group count.
    uint32_t metricGroupCount = 0;
    EXPECT_EQ(zetMetricGroupGet(devices[0]->toHandle(), &metricGroupCount, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);

    // Metric group handle.
    EXPECT_EQ(zetMetricGroupGet(devices[0]->toHandle(), &metricGroupCount, &metricGroupHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);
    EXPECT_NE(metricGroupHandle, nullptr);

    // Create metric query pool.
    EXPECT_EQ(zetMetricQueryPoolCreate(context->toHandle(), metricDevice, metricGroupHandle, &poolDesc, &poolHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(poolHandle, nullptr);

    // Create metric query.
    EXPECT_EQ(zetMetricQueryCreate(poolHandle, 0, &queryHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(queryHandle, nullptr);

    // Get desired raw data size.
    size_t rawSize = 0;
    EXPECT_EQ(zetMetricQueryGetData(queryHandle, &rawSize, nullptr), ZE_RESULT_SUCCESS);
    const size_t expectedRawSize = (reportSize * subDeviceCount) + sizeof(MetricGroupCalculateHeader) + (2 * sizeof(uint32_t) * subDeviceCount);
    EXPECT_EQ(rawSize, expectedRawSize);

    // Get data.
    std::vector<uint8_t> rawData;
    rawData.resize(rawSize);
    EXPECT_EQ(zetMetricQueryGetData(queryHandle, &rawSize, rawData.data()), ZE_RESULT_ERROR_UNKNOWN);

    // Destroy query and its pool.
    EXPECT_EQ(zetMetricQueryDestroy(queryHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(zetMetricQueryPoolDestroy(poolHandle), ZE_RESULT_SUCCESS);
}

TEST_F(MultiDeviceMetricQueryPoolTest, givenCorrectArgumentsWhenZetCommandListAppendMetricQueryBeginEndIsCalledThenReturnsSuccess) {

    zet_device_handle_t metricDevice = devices[0]->toHandle();
    auto &deviceImp = *static_cast<DeviceImp *>(devices[0]);
    const uint32_t subDeviceCount = static_cast<uint32_t>(deviceImp.subDevices.size());

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, devices[0], NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    zet_command_list_handle_t commandListHandle = commandList->toHandle();

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

    openMetricsAdapter();

    EXPECT_CALL(metricsDevice, GetParams())
        .WillRepeatedly(Return(&metricsDeviceParams));

    EXPECT_CALL(metricsDevice, GetConcurrentGroup(_))
        .Times(subDeviceCount)
        .WillRepeatedly(Return(&metricsConcurrentGroup));

    EXPECT_CALL(metricsConcurrentGroup, GetParams())
        .Times(subDeviceCount)
        .WillRepeatedly(Return(&metricsConcurrentGroupParams));

    EXPECT_CALL(metricsConcurrentGroup, GetMetricSet(_))
        .WillRepeatedly(Return(&metricsSet));

    EXPECT_CALL(metricsSet, GetParams())
        .WillRepeatedly(Return(&metricsSetParams));

    EXPECT_CALL(metricsSet, GetMetric(_))
        .Times(subDeviceCount)
        .WillRepeatedly(Return(&metric));

    EXPECT_CALL(metric, GetParams())
        .Times(subDeviceCount)
        .WillRepeatedly(Return(&metricParams));

    EXPECT_CALL(metricsSet, SetApiFiltering(_))
        .WillRepeatedly(Return(TCompletionCode::CC_OK));

    for (uint32_t i = 0; i < subDeviceCount; ++i) {
        EXPECT_CALL(*mockMetricEnumerationSubDevices[i], isInitialized())
            .Times(1)
            .WillOnce(Return(true));

        EXPECT_CALL(*mockMetricsLibrarySubDevices[i], getContextData(_, _))
            .Times(1)
            .WillOnce(Return(true));
    }

    EXPECT_CALL(*mockMetricsLibrary, load())
        .Times(0);

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockQueryCreate(_, _))
        .Times(subDeviceCount)
        .WillRepeatedly(DoAll(::testing::SetArgPointee<1>(metricsLibraryQueryHandle), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockQueryDelete(_))
        .Times(subDeviceCount)
        .WillRepeatedly(Return(StatusCode::Success));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockGetParameter(_, _, _))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<2>(value), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockContextCreate(_, _, _))
        .Times(subDeviceCount)
        .WillRepeatedly(DoAll(::testing::SetArgPointee<2>(metricsLibraryContextHandle), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockContextDelete(_))
        .Times(subDeviceCount)
        .WillRepeatedly(Return(StatusCode::Success));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockCommandBufferGetSize(_, _))
        .Times(2 * subDeviceCount + 1)
        .WillRepeatedly(DoAll(::testing::SetArgPointee<1>(::testing::ByRef(commandBufferSize)), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockCommandBufferGet(_))
        .Times(2 * subDeviceCount + 1)
        .WillRepeatedly(Return(StatusCode::Success));

    // Metric group count.
    uint32_t metricGroupCount = 0;
    EXPECT_EQ(zetMetricGroupGet(devices[0]->toHandle(), &metricGroupCount, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);

    // Metric group handle.
    EXPECT_EQ(zetMetricGroupGet(devices[0]->toHandle(), &metricGroupCount, &metricGroupHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);
    EXPECT_NE(metricGroupHandle, nullptr);

    // Activate metric group.
    EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), devices[0]->toHandle(), 1, &metricGroupHandle), ZE_RESULT_SUCCESS);

    // Create metric query pool.
    EXPECT_EQ(zetMetricQueryPoolCreate(context->toHandle(), metricDevice, metricGroupHandle, &poolDesc, &poolHandle), ZE_RESULT_SUCCESS);
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

    // Write memory barrier to command list.
    EXPECT_EQ(zetCommandListAppendMetricMemoryBarrier(commandListHandle), ZE_RESULT_SUCCESS);

    // Destroy event and its pool.
    EXPECT_EQ(zeEventDestroy(eventHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(zeEventPoolDestroy(eventPoolHandle), ZE_RESULT_SUCCESS);

    // Destroy query and its pool.
    EXPECT_EQ(zetMetricQueryDestroy(queryHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(zetMetricQueryPoolDestroy(poolHandle), ZE_RESULT_SUCCESS);
}

TEST_F(MultiDeviceMetricQueryPoolTest, givenInvalidCommandBufferGetSizeWhenZetCommandListAppendMetricQueryBeginIsCalledThenReturnsFail) {

    zet_device_handle_t metricDevice = devices[0]->toHandle();
    auto &deviceImp = *static_cast<DeviceImp *>(devices[0]);
    const uint32_t subDeviceCount = static_cast<uint32_t>(deviceImp.subDevices.size());

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, devices[0], NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    zet_command_list_handle_t commandListHandle = commandList->toHandle();

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
    commandBufferSize.GpuMemorySize = 0;

    openMetricsAdapter();

    EXPECT_CALL(metricsDevice, GetParams())
        .WillRepeatedly(Return(&metricsDeviceParams));

    EXPECT_CALL(metricsDevice, GetConcurrentGroup(_))
        .Times(subDeviceCount)
        .WillRepeatedly(Return(&metricsConcurrentGroup));

    EXPECT_CALL(metricsConcurrentGroup, GetParams())
        .Times(subDeviceCount)
        .WillRepeatedly(Return(&metricsConcurrentGroupParams));

    EXPECT_CALL(metricsConcurrentGroup, GetMetricSet(_))
        .WillRepeatedly(Return(&metricsSet));

    EXPECT_CALL(metricsSet, GetParams())
        .WillRepeatedly(Return(&metricsSetParams));

    EXPECT_CALL(metricsSet, GetMetric(_))
        .Times(subDeviceCount)
        .WillRepeatedly(Return(&metric));

    EXPECT_CALL(metric, GetParams())
        .Times(subDeviceCount)
        .WillRepeatedly(Return(&metricParams));

    EXPECT_CALL(metricsSet, SetApiFiltering(_))
        .WillRepeatedly(Return(TCompletionCode::CC_OK));

    for (uint32_t i = 0; i < subDeviceCount; ++i) {
        EXPECT_CALL(*mockMetricEnumerationSubDevices[i], isInitialized())
            .Times(1)
            .WillOnce(Return(true));

        EXPECT_CALL(*mockMetricsLibrarySubDevices[i], getContextData(_, _))
            .Times(1)
            .WillOnce(Return(true));
    }

    EXPECT_CALL(*mockMetricsLibrary, load())
        .Times(0);

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockQueryCreate(_, _))
        .Times(subDeviceCount)
        .WillRepeatedly(DoAll(::testing::SetArgPointee<1>(metricsLibraryQueryHandle), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockQueryDelete(_))
        .Times(subDeviceCount)
        .WillRepeatedly(Return(StatusCode::Success));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockGetParameter(_, _, _))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<2>(value), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockContextCreate(_, _, _))
        .Times(subDeviceCount)
        .WillRepeatedly(DoAll(::testing::SetArgPointee<2>(metricsLibraryContextHandle), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockContextDelete(_))
        .Times(subDeviceCount)
        .WillRepeatedly(Return(StatusCode::Success));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockCommandBufferGetSize(_, _))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<1>(::testing::ByRef(commandBufferSize)), Return(StatusCode::Success)));

    // Metric group count.
    uint32_t metricGroupCount = 0;
    EXPECT_EQ(zetMetricGroupGet(devices[0]->toHandle(), &metricGroupCount, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);

    // Metric group handle.
    EXPECT_EQ(zetMetricGroupGet(devices[0]->toHandle(), &metricGroupCount, &metricGroupHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);
    EXPECT_NE(metricGroupHandle, nullptr);

    // Create metric query pool.
    EXPECT_EQ(zetMetricQueryPoolCreate(context->toHandle(), metricDevice, metricGroupHandle, &poolDesc, &poolHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(poolHandle, nullptr);

    // Create metric query.
    EXPECT_EQ(zetMetricQueryCreate(poolHandle, 0, &queryHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(queryHandle, nullptr);

    // Write BEGIN metric query to command list.
    EXPECT_EQ(zetCommandListAppendMetricQueryBegin(commandListHandle, queryHandle), ZE_RESULT_ERROR_UNKNOWN);

    // Destroy query and its pool.
    EXPECT_EQ(zetMetricQueryDestroy(queryHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(zetMetricQueryPoolDestroy(poolHandle), ZE_RESULT_SUCCESS);
}

TEST_F(MultiDeviceMetricQueryPoolTest, givenInvalidCommandBufferGetWhenZetCommandListAppendMetricQueryBeginIsCalledThenReturnsFail) {

    zet_device_handle_t metricDevice = devices[0]->toHandle();
    auto &deviceImp = *static_cast<DeviceImp *>(devices[0]);
    const uint32_t subDeviceCount = static_cast<uint32_t>(deviceImp.subDevices.size());

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, devices[0], NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    zet_command_list_handle_t commandListHandle = commandList->toHandle();

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

    openMetricsAdapter();

    EXPECT_CALL(metricsDevice, GetParams())
        .WillRepeatedly(Return(&metricsDeviceParams));

    EXPECT_CALL(metricsDevice, GetConcurrentGroup(_))
        .Times(subDeviceCount)
        .WillRepeatedly(Return(&metricsConcurrentGroup));

    EXPECT_CALL(metricsConcurrentGroup, GetParams())
        .Times(subDeviceCount)
        .WillRepeatedly(Return(&metricsConcurrentGroupParams));

    EXPECT_CALL(metricsConcurrentGroup, GetMetricSet(_))
        .WillRepeatedly(Return(&metricsSet));

    EXPECT_CALL(metricsSet, GetParams())
        .WillRepeatedly(Return(&metricsSetParams));

    EXPECT_CALL(metricsSet, GetMetric(_))
        .Times(subDeviceCount)
        .WillRepeatedly(Return(&metric));

    EXPECT_CALL(metric, GetParams())
        .Times(subDeviceCount)
        .WillRepeatedly(Return(&metricParams));

    EXPECT_CALL(metricsSet, SetApiFiltering(_))
        .WillRepeatedly(Return(TCompletionCode::CC_OK));

    for (uint32_t i = 0; i < subDeviceCount; ++i) {
        EXPECT_CALL(*mockMetricEnumerationSubDevices[i], isInitialized())
            .Times(1)
            .WillOnce(Return(true));

        EXPECT_CALL(*mockMetricsLibrarySubDevices[i], getContextData(_, _))
            .Times(1)
            .WillOnce(Return(true));
    }

    EXPECT_CALL(*mockMetricsLibrary, load())
        .Times(0);

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockQueryCreate(_, _))
        .Times(subDeviceCount)
        .WillRepeatedly(DoAll(::testing::SetArgPointee<1>(metricsLibraryQueryHandle), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockQueryDelete(_))
        .Times(subDeviceCount)
        .WillRepeatedly(Return(StatusCode::Success));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockGetParameter(_, _, _))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<2>(value), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockContextCreate(_, _, _))
        .Times(subDeviceCount)
        .WillRepeatedly(DoAll(::testing::SetArgPointee<2>(metricsLibraryContextHandle), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockContextDelete(_))
        .Times(subDeviceCount)
        .WillRepeatedly(Return(StatusCode::Success));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockCommandBufferGetSize(_, _))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<1>(::testing::ByRef(commandBufferSize)), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockCommandBufferGet(_))
        .Times(1)
        .WillRepeatedly(Return(StatusCode::Failed));

    // Metric group count.
    uint32_t metricGroupCount = 0;
    EXPECT_EQ(zetMetricGroupGet(devices[0]->toHandle(), &metricGroupCount, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);

    // Metric group handle.
    EXPECT_EQ(zetMetricGroupGet(devices[0]->toHandle(), &metricGroupCount, &metricGroupHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);
    EXPECT_NE(metricGroupHandle, nullptr);

    // Create metric query pool.
    EXPECT_EQ(zetMetricQueryPoolCreate(context->toHandle(), metricDevice, metricGroupHandle, &poolDesc, &poolHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(poolHandle, nullptr);

    // Create metric query.
    EXPECT_EQ(zetMetricQueryCreate(poolHandle, 0, &queryHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(queryHandle, nullptr);

    // Write BEGIN metric query to command list.
    EXPECT_EQ(zetCommandListAppendMetricQueryBegin(commandListHandle, queryHandle), ZE_RESULT_ERROR_UNKNOWN);

    // Destroy query and its pool.
    EXPECT_EQ(zetMetricQueryDestroy(queryHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(zetMetricQueryPoolDestroy(poolHandle), ZE_RESULT_SUCCESS);
}

TEST_F(MultiDeviceMetricQueryPoolTest, givenUninitializedMetricsLibraryWhenGetGpuCommandsIsCalledThenReturnsFail) {

    auto &metricSource = devices[0]->getMetricDeviceContext().getMetricSource<OaMetricSourceImp>();
    auto &metricsLibrary = metricSource.getMetricsLibrary();
    CommandBufferData_1_0 commandBuffer = {};

    EXPECT_CALL(*mockMetricEnumeration, isInitialized())
        .Times(1)
        .WillOnce(Return(false));

    const bool result = metricsLibrary.getGpuCommands(commandBuffer);

    EXPECT_EQ(result, false);
}

TEST_F(MultiDeviceMetricQueryPoolTest, givenValidArgumentsWhenZetMetricGroupCalculateMetricValuesExpThenReturnsSuccess) {

    zet_device_handle_t metricDevice = devices[0]->toHandle();
    auto &deviceImp = *static_cast<DeviceImp *>(devices[0]);
    const uint32_t subDeviceCount = static_cast<uint32_t>(deviceImp.subDevices.size());

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
    metricsSetParams.QueryReportSize = 256;
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

    uint32_t returnedMetricCount = 1;

    openMetricsAdapter();

    EXPECT_CALL(metricsDevice, GetParams())
        .WillRepeatedly(Return(&metricsDeviceParams));

    EXPECT_CALL(metricsDevice, GetConcurrentGroup(_))
        .Times(subDeviceCount)
        .WillRepeatedly(Return(&metricsConcurrentGroup));

    EXPECT_CALL(metricsConcurrentGroup, GetParams())
        .Times(subDeviceCount)
        .WillRepeatedly(Return(&metricsConcurrentGroupParams));

    EXPECT_CALL(metricsConcurrentGroup, GetMetricSet(_))
        .WillRepeatedly(Return(&metricsSet));

    EXPECT_CALL(metricsSet, GetParams())
        .WillRepeatedly(Return(&metricsSetParams));

    EXPECT_CALL(metricsSet, GetMetric(_))
        .Times(subDeviceCount)
        .WillRepeatedly(Return(&metric));

    EXPECT_CALL(metric, GetParams())
        .Times(subDeviceCount)
        .WillRepeatedly(Return(&metricParams));

    EXPECT_CALL(metricsSet, SetApiFiltering(_))
        .WillRepeatedly(Return(TCompletionCode::CC_OK));

    for (uint32_t i = 0; i < subDeviceCount; ++i) {
        EXPECT_CALL(*mockMetricEnumerationSubDevices[i], isInitialized())
            .Times(1)
            .WillOnce(Return(true));

        EXPECT_CALL(*mockMetricsLibrarySubDevices[i], getContextData(_, _))
            .Times(1)
            .WillOnce(Return(true));
    }

    EXPECT_CALL(*mockMetricsLibrarySubDevices[0], getMetricQueryReportSize(_))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgReferee<0>(metricsSetParams.QueryReportSize), Return(true)));

    EXPECT_CALL(*mockMetricsLibrary, load())
        .Times(0);

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockQueryCreate(_, _))
        .Times(subDeviceCount)
        .WillRepeatedly(DoAll(::testing::SetArgPointee<1>(metricsLibraryQueryHandle), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockQueryDelete(_))
        .Times(subDeviceCount)
        .WillRepeatedly(Return(StatusCode::Success));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockGetParameter(_, _, _))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<2>(value), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockContextCreate(_, _, _))
        .Times(subDeviceCount)
        .WillRepeatedly(DoAll(::testing::SetArgPointee<2>(metricsLibraryContextHandle), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockGetData(_))
        .Times(subDeviceCount)
        .WillRepeatedly(Return(StatusCode::Success));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockContextDelete(_))
        .Times(subDeviceCount)
        .WillRepeatedly(Return(StatusCode::Success));

    EXPECT_CALL(metricsSet, CalculateMetrics(_, _, _, _, _, _, _))
        .Times(subDeviceCount)
        .WillRepeatedly(DoAll(::testing::SetArgPointee<4>(returnedMetricCount), Return(TCompletionCode::CC_OK)));

    // Metric group count.
    uint32_t metricGroupCount = 0;
    EXPECT_EQ(zetMetricGroupGet(devices[0]->toHandle(), &metricGroupCount, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);

    // Metric group handle.
    EXPECT_EQ(zetMetricGroupGet(devices[0]->toHandle(), &metricGroupCount, &metricGroupHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);
    EXPECT_NE(metricGroupHandle, nullptr);

    // Create metric query pool.
    EXPECT_EQ(zetMetricQueryPoolCreate(context->toHandle(), metricDevice, metricGroupHandle, &poolDesc, &poolHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(poolHandle, nullptr);

    // Create metric query.
    EXPECT_EQ(zetMetricQueryCreate(poolHandle, 0, &queryHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(queryHandle, nullptr);

    // Get desired raw data size.
    size_t rawSize = 0;
    EXPECT_EQ(zetMetricQueryGetData(queryHandle, &rawSize, nullptr), ZE_RESULT_SUCCESS);
    const size_t expectedRawSize = (metricsSetParams.QueryReportSize * subDeviceCount) + sizeof(MetricGroupCalculateHeader) + (2 * sizeof(uint32_t) * subDeviceCount);
    EXPECT_EQ(rawSize, expectedRawSize);

    // Get data.
    std::vector<uint8_t> rawData;
    rawData.resize(rawSize);
    EXPECT_EQ(zetMetricQueryGetData(queryHandle, &rawSize, rawData.data()), ZE_RESULT_SUCCESS);

    uint32_t dataCount = 0;
    uint32_t totalMetricCount = 0;
    EXPECT_EQ(zetMetricGroupCalculateMultipleMetricValuesExp(metricGroupHandle, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, rawSize, rawData.data(), &dataCount, &totalMetricCount, nullptr, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(dataCount, subDeviceCount);
    EXPECT_EQ(totalMetricCount, subDeviceCount * metricsSetParams.MetricsCount);

    std::vector<uint32_t> metricCounts(dataCount);
    std::vector<zet_typed_value_t> caculatedRawResults(totalMetricCount);
    EXPECT_EQ(zetMetricGroupCalculateMultipleMetricValuesExp(metricGroupHandle, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, rawSize, rawData.data(), &dataCount, &totalMetricCount, metricCounts.data(), caculatedRawResults.data()), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricCounts[0], metricsSetParams.MetricsCount);
    EXPECT_EQ(metricCounts[1], metricsSetParams.MetricsCount);

    // Destroy query and its pool.
    EXPECT_EQ(zetMetricQueryDestroy(queryHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(zetMetricQueryPoolDestroy(poolHandle), ZE_RESULT_SUCCESS);
}

TEST_F(MultiDeviceMetricQueryPoolTest, givenCorrectArgumentsWhenActivateMetricGroupsIsCalledThenReturnsSuccess) {

    zet_device_handle_t metricDevice = devices[0]->toHandle();
    auto &deviceImp = *static_cast<DeviceImp *>(devices[0]);
    const uint32_t subDeviceCount = static_cast<uint32_t>(deviceImp.subDevices.size());

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
        .Times(subDeviceCount)
        .WillRepeatedly(Return(&metricsConcurrentGroup));

    EXPECT_CALL(metricsConcurrentGroup, GetParams())
        .Times(subDeviceCount)
        .WillRepeatedly(Return(&metricsConcurrentGroupParams));

    EXPECT_CALL(metricsConcurrentGroup, GetMetricSet(_))
        .WillRepeatedly(Return(&metricsSet));

    EXPECT_CALL(metricsSet, GetParams())
        .WillRepeatedly(Return(&metricsSetParams));

    EXPECT_CALL(metricsSet, GetMetric(_))
        .Times(subDeviceCount)
        .WillRepeatedly(Return(&metric));

    EXPECT_CALL(metricsSet, Activate())
        .Times(subDeviceCount)
        .WillRepeatedly(Return(MetricsDiscovery::CC_OK));

    EXPECT_CALL(metricsSet, Deactivate())
        .Times(subDeviceCount)
        .WillRepeatedly(Return(MetricsDiscovery::CC_OK));

    EXPECT_CALL(metric, GetParams())
        .Times(subDeviceCount)
        .WillRepeatedly(Return(&metricParams));

    EXPECT_CALL(metricsSet, SetApiFiltering(_))
        .WillRepeatedly(Return(TCompletionCode::CC_OK));

    for (uint32_t i = 0; i < subDeviceCount; ++i) {
        EXPECT_CALL(*mockMetricEnumerationSubDevices[i], isInitialized())
            .Times(1)
            .WillOnce(Return(true));

        EXPECT_CALL(*mockMetricsLibrarySubDevices[i], getContextData(_, _))
            .Times(1)
            .WillOnce(Return(true));
    }

    EXPECT_CALL(*mockMetricsLibrary, load())
        .Times(0);

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockContextCreate(_, _, _))
        .Times(subDeviceCount)
        .WillRepeatedly(DoAll(::testing::SetArgPointee<2>(metricsLibraryContextHandle), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockContextDelete(_))
        .Times(subDeviceCount)
        .WillRepeatedly(Return(StatusCode::Success));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockConfigurationCreate(_, _))
        .Times(subDeviceCount)
        .WillRepeatedly(DoAll(::testing::SetArgPointee<1>(metricsLibraryConfigurationHandle), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockConfigurationActivate(_, _))
        .Times(subDeviceCount)
        .WillRepeatedly(Return(StatusCode::Success));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockConfigurationDeactivate(_))
        .Times(subDeviceCount)
        .WillRepeatedly(Return(StatusCode::Success));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockConfigurationDelete(_))
        .Times(subDeviceCount)
        .WillRepeatedly(Return(StatusCode::Success));

    // Metric group count.
    uint32_t metricGroupCount = 0;
    EXPECT_EQ(zetMetricGroupGet(devices[0]->toHandle(), &metricGroupCount, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);

    // Metric group handle.
    EXPECT_EQ(zetMetricGroupGet(devices[0]->toHandle(), &metricGroupCount, &metricGroupHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);
    EXPECT_NE(metricGroupHandle, nullptr);

    // Activate metric group (deferred).
    EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), devices[0]->toHandle(), 1, &metricGroupHandle), ZE_RESULT_SUCCESS);

    // Activate metric groups.
    devices[0]->activateMetricGroups();

    // Deactivate metric groups.
    EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), metricDevice, 0, nullptr), ZE_RESULT_SUCCESS);
}

} // namespace ult
} // namespace L0
