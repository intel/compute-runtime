/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/utilities/perf_counter.h"
#include "shared/source/utilities/tag_allocator.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_os_context.h"

#include "opencl/source/event/event.h"
#include "opencl/test/unit_test/fixtures/device_instrumentation_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/os_interface/mock_performance_counters.h"

#include "gtest/gtest.h"

using namespace NEO;

struct PerformanceCountersDeviceTest : public PerformanceCountersDeviceFixture,
                                       public DeviceInstrumentationFixture,
                                       public ::testing::Test {
    void SetUp() override {
        PerformanceCountersDeviceFixture::setUp();
    }
    void TearDown() override {
        PerformanceCountersDeviceFixture::tearDown();
    }
};

TEST_F(PerformanceCountersDeviceTest, GivenEnabledInstrumentationWhenGettingPerformanceCountersThenNonNullPtrIsReturned) {
    DeviceInstrumentationFixture::setUp(true);
    EXPECT_NE(nullptr, device->getPerformanceCounters());
}

TEST_F(PerformanceCountersDeviceTest, GivenDisabledInstrumentationWhenGettingPerformanceCountersThenNullPtrIsReturned) {
    DeviceInstrumentationFixture::setUp(false);
    EXPECT_EQ(nullptr, device->getPerformanceCounters());
}

struct PerformanceCountersTest : public PerformanceCountersFixture,
                                 public ::testing::Test {
  public:
    void SetUp() override {
        PerformanceCountersFixture::setUp();
    }

    void TearDown() override {
        PerformanceCountersFixture::tearDown();
    }
};

TEST_F(PerformanceCountersTest, WhenCreatingPerformanceCountersThenObjectIsNotNull) {
    auto performanceCounters = PerformanceCounters::create(&device->getDevice());
    EXPECT_NE(nullptr, performanceCounters);
    EXPECT_NE(nullptr, performanceCounters.get());
}

TEST_F(PerformanceCountersTest, GivenValidDeviceHandleWhenFlushCommandBufferCallbackIsCalledThenReturnsSuccess) {
    auto performanceCounters = PerformanceCounters::create(&device->getDevice());
    EXPECT_NE(nullptr, performanceCounters);
    EXPECT_NE(nullptr, performanceCounters.get());

    MetricsLibraryApi::ClientHandle_1_0 handle = reinterpret_cast<void *>(&device->getDevice());
    auto callback = performanceCounters->getMetricsLibraryInterface()->api->callbacks.CommandBufferFlush;

    EXPECT_NE(nullptr, callback);
    EXPECT_EQ(MetricsLibraryApi::StatusCode::Success, callback(handle));
}

TEST_F(PerformanceCountersTest, GivenInvalidDeviceHandleWhenFlushCommandBufferCallbackIsCalledThenReturnsFailed) {
    auto performanceCounters = PerformanceCounters::create(&device->getDevice());
    EXPECT_NE(nullptr, performanceCounters);
    EXPECT_NE(nullptr, performanceCounters.get());

    MetricsLibraryApi::ClientHandle_1_0 handle = {};
    auto callback = performanceCounters->getMetricsLibraryInterface()->api->callbacks.CommandBufferFlush;

    EXPECT_NE(nullptr, callback);
    EXPECT_EQ(MetricsLibraryApi::StatusCode::Failed, callback(handle));
}

TEST_F(PerformanceCountersTest, givenPerformanceCountersWhenCreatedThenAllValuesProperlyInitialized) {
    auto performanceCounters = MockPerformanceCounters::create();

    EXPECT_NE(nullptr, performanceCounters->getMetricsLibraryInterface());
    EXPECT_EQ(0u, performanceCounters->getReferenceNumber());
}

struct PerformanceCountersProcessEventTest : public PerformanceCountersTest,
                                             public ::testing::WithParamInterface<bool> {

    void SetUp() override {
        PerformanceCountersTest::SetUp();
        perfCounters = MockPerformanceCounters::create();
        eventComplete = true;
        outputParamSize = 0;
        inputParamSize = perfCounters->getApiReportSize();
        inputParam.reset(new uint8_t);
    }
    void TearDown() override {
        perfCounters->shutdown();
        PerformanceCountersTest::TearDown();
    }

    std::unique_ptr<uint8_t> inputParam;
    std::unique_ptr<PerformanceCounters> perfCounters;
    size_t inputParamSize;
    size_t outputParamSize;
    bool eventComplete;
};

TEST_P(PerformanceCountersProcessEventTest, givenNullptrInputParamWhenProcessEventPerfCountersIsCalledThenReturnsFalse) {
    eventComplete = GetParam();

    HwPerfCounter counters = {};
    TagNode<HwPerfCounter> query = {};
    query.tagForCpuAccess = &counters;

    perfCounters->getQueryHandleRef(counters.query.handle);
    auto retVal = perfCounters->getApiReport(&query, inputParamSize, nullptr, &outputParamSize, eventComplete);
    perfCounters->deleteQuery(counters.query.handle);

    EXPECT_FALSE(retVal);
}

TEST_P(PerformanceCountersProcessEventTest, givenCorrectInputParamWhenProcessEventPerfCountersIsCalledAndEventIsCompletedThenReturnsTrue) {
    eventComplete = GetParam();
    EXPECT_EQ(0ull, outputParamSize);
    HwPerfCounter counters = {};
    TagNode<HwPerfCounter> query = {};
    query.tagForCpuAccess = &counters;

    perfCounters->getQueryHandleRef(counters.query.handle);
    auto retVal = perfCounters->getApiReport(&query, inputParamSize, inputParam.get(), &outputParamSize, eventComplete);
    perfCounters->deleteQuery(counters.query.handle);

    if (eventComplete) {
        EXPECT_TRUE(retVal);
        EXPECT_EQ(outputParamSize, inputParamSize);
    } else {
        EXPECT_FALSE(retVal);
        EXPECT_EQ(inputParamSize, outputParamSize);
    }
}

TEST_P(PerformanceCountersProcessEventTest, givenCorrectInputParamWhenProcessEventPerfCountersIsNotCalledThenReturnsFalse) {
    eventComplete = GetParam();
    EXPECT_EQ(0ull, outputParamSize);
    HwPerfCounter tag = {};
    TagNode<HwPerfCounter> query = {};
    query.tagForCpuAccess = &tag;

    auto retVal = perfCounters->getApiReport(&query, inputParamSize, inputParam.get(), &outputParamSize, eventComplete);
    EXPECT_EQ(eventComplete, retVal);
}

TEST_F(PerformanceCountersProcessEventTest, givenInvalidInputParamSizeWhenProcessEventPerfCountersIsCalledThenReturnsFalse) {
    EXPECT_EQ(0ull, outputParamSize);

    HwPerfCounter counters = {};
    TagNode<HwPerfCounter> query = {};
    query.tagForCpuAccess = &counters;

    perfCounters->getQueryHandleRef(counters.query.handle);
    auto retVal = perfCounters->getApiReport(&query, inputParamSize - 1, inputParam.get(), &outputParamSize, eventComplete);
    perfCounters->deleteQuery(counters.query.handle);

    EXPECT_FALSE(retVal);
    EXPECT_EQ(outputParamSize, inputParamSize);
}

TEST_F(PerformanceCountersProcessEventTest, givenNullptrOutputParamSizeWhenProcessEventPerfCountersIsCalledThenDoesNotReturnsOutputSize) {
    EXPECT_EQ(0ull, outputParamSize);

    HwPerfCounter counters = {};
    TagNode<HwPerfCounter> query = {};
    query.tagForCpuAccess = &counters;

    perfCounters->getQueryHandleRef(counters.query.handle);
    auto retVal = perfCounters->getApiReport(&query, inputParamSize, inputParam.get(), nullptr, eventComplete);
    perfCounters->deleteQuery(counters.query.handle);

    EXPECT_TRUE(retVal);
    EXPECT_EQ(0ull, outputParamSize);
}

TEST_F(PerformanceCountersProcessEventTest, givenNullptrInputZeroSizeWhenProcessEventPerfCountersIsCalledThenQueryProperSize) {
    EXPECT_EQ(0ull, outputParamSize);

    HwPerfCounter counters = {};
    TagNode<HwPerfCounter> query = {};
    query.tagForCpuAccess = &counters;

    perfCounters->getQueryHandleRef(counters.query.handle);
    auto retVal = perfCounters->getApiReport(&query, 0, nullptr, &outputParamSize, eventComplete);
    perfCounters->deleteQuery(counters.query.handle);

    EXPECT_TRUE(retVal);
    EXPECT_EQ(inputParamSize, outputParamSize);
}

TEST_F(PerformanceCountersProcessEventTest, givenNullptrInputZeroSizeAndNullptrOutputSizeWhenProcessEventPerfCountersIsCalledThenReturnFalse) {
    EXPECT_EQ(0ull, outputParamSize);

    HwPerfCounter counters = {};
    TagNode<HwPerfCounter> query = {};
    query.tagForCpuAccess = &counters;

    perfCounters->getQueryHandleRef(counters.query.handle);
    auto retVal = perfCounters->getApiReport(&query, 0, nullptr, nullptr, eventComplete);
    perfCounters->deleteQuery(counters.query.handle);

    EXPECT_FALSE(retVal);
    EXPECT_EQ(0ull, outputParamSize);
}

TEST_F(PerformanceCountersProcessEventTest, givenNullptrQueryWhenProcessEventPerfCountersIsCalledThenReturnFalse) {
    EXPECT_EQ(0ull, outputParamSize);

    auto retVal = perfCounters->getApiReport(nullptr, 0, nullptr, nullptr, eventComplete);
    EXPECT_FALSE(retVal);
}

INSTANTIATE_TEST_SUITE_P(
    PerfCountersTests,
    PerformanceCountersProcessEventTest,
    testing::Bool());

struct PerformanceCountersMetricsLibraryTest : public PerformanceCountersMetricsLibraryFixture,
                                               public ::testing::Test {

  public:
    void SetUp() override {
        PerformanceCountersMetricsLibraryFixture::setUp();
        auto &rootDeviceEnvironment = *executionEnvironment->rootDeviceEnvironments[0];
        auto hwInfo = rootDeviceEnvironment.getHardwareInfo();
        auto &gfxCoreHelper = rootDeviceEnvironment.getHelper<GfxCoreHelper>();
        osContext = std::make_unique<MockOsContext>(0, EngineDescriptorHelper::getDefaultDescriptor(gfxCoreHelper.getGpgpuEngineInstances(rootDeviceEnvironment)[0],
                                                                                                    PreemptionHelper::getDefaultPreemptionMode(*hwInfo)));
        queue->getGpgpuCommandStreamReceiver().setupContext(*osContext);
    }

    void TearDown() override {
        PerformanceCountersMetricsLibraryFixture::tearDown();
        queue->getGpgpuCommandStreamReceiver().setupContext(*device->getDefaultEngine().osContext);
    }
    std::unique_ptr<OsContext> osContext;
};

TEST_F(PerformanceCountersMetricsLibraryTest, givenPerformanceCountersWhenMetricLibraryIsValidThenQueryIsCreated) {
    // Create performance counters.
    auto *performanceCounters = initDeviceWithPerformanceCounters(true, true);
    EXPECT_NE(nullptr, performanceCounters);
    EXPECT_TRUE(performanceCounters->enable(false));
    EXPECT_EQ(1u, performanceCounters->getReferenceNumber());

    // Check metric library context.
    auto context = static_cast<MockMetricsLibrary *>(performanceCounters->getMetricsLibraryContext().data);
    EXPECT_NE(nullptr, context);
    EXPECT_EQ(1u, context->contextCount);

    // Close library.
    performanceCounters->shutdown();
    EXPECT_EQ(0u, performanceCounters->getReferenceNumber());
}

TEST_F(PerformanceCountersMetricsLibraryTest, givenPerformanceCountersWhenMetricLibraryIsValidThenQueryReturnsValidGpuCommands) {
    // Create performance counters.
    auto *performanceCounters = initDeviceWithPerformanceCounters(true, false);
    EXPECT_NE(nullptr, performanceCounters);
    EXPECT_TRUE(performanceCounters->enable(false));
    EXPECT_EQ(1u, performanceCounters->getReferenceNumber());

    // Obtain required command buffer size.
    uint32_t commandsSize = performanceCounters->getGpuCommandsSize(MetricsLibraryApi::GpuCommandBufferType::Render, true);
    EXPECT_NE(0u, commandsSize);

    // Fill command buffer.
    uint8_t buffer[1000] = {};
    HwPerfCounter perfCounter = {};
    TagNode<HwPerfCounter> query = {};
    query.tagForCpuAccess = &perfCounter;
    EXPECT_TRUE(performanceCounters->getGpuCommands(MetricsLibraryApi::GpuCommandBufferType::Render, query, true, sizeof(buffer), buffer));

    // Close library.
    performanceCounters->deleteQuery(perfCounter.query.handle);
    performanceCounters->shutdown();
    EXPECT_EQ(0u, performanceCounters->getReferenceNumber());
}

TEST_F(PerformanceCountersMetricsLibraryTest, givenInitialNonCcsEngineWhenEnablingThenDontAllowCcsOnNextCalls) {
    auto *performanceCounters = initDeviceWithPerformanceCounters(true, false);
    EXPECT_NE(nullptr, performanceCounters);

    EXPECT_TRUE(performanceCounters->enable(false));
    EXPECT_TRUE(performanceCounters->enable(false));
    EXPECT_FALSE(performanceCounters->enable(true));

    performanceCounters->shutdown();
    performanceCounters->shutdown();
    performanceCounters->shutdown();

    EXPECT_EQ(0u, performanceCounters->getReferenceNumber());

    EXPECT_TRUE(performanceCounters->enable(true));
    performanceCounters->shutdown();
}

TEST_F(PerformanceCountersMetricsLibraryTest, givenInitialCcsEngineWhenEnablingThenDontAllowNonCcsOnNextCalls) {
    auto *performanceCounters = initDeviceWithPerformanceCounters(true, false);
    EXPECT_NE(nullptr, performanceCounters);

    EXPECT_TRUE(performanceCounters->enable(true));
    EXPECT_TRUE(performanceCounters->enable(true));
    EXPECT_FALSE(performanceCounters->enable(false));

    performanceCounters->shutdown();
    performanceCounters->shutdown();
    performanceCounters->shutdown();

    EXPECT_EQ(0u, performanceCounters->getReferenceNumber());

    EXPECT_TRUE(performanceCounters->enable(false));
    performanceCounters->shutdown();
}

TEST_F(PerformanceCountersMetricsLibraryTest, givenPerformanceCountersWhenMetricLibraryIsInvalidThenQueryReturnsInvalidGpuCommands) {
    // Create performance counters.
    auto *performanceCounters = initDeviceWithPerformanceCounters(false, false);
    EXPECT_NE(nullptr, performanceCounters);
    EXPECT_FALSE(performanceCounters->enable(true));

    // Obtain required command buffer size.
    uint32_t commandsSize = performanceCounters->getGpuCommandsSize(MetricsLibraryApi::GpuCommandBufferType::Render, true);
    EXPECT_EQ(0u, commandsSize);

    // Close library.
    performanceCounters->shutdown();
    EXPECT_EQ(0u, performanceCounters->getReferenceNumber());
}

TEST_F(PerformanceCountersMetricsLibraryTest, givenPerformanceCountersWhenMetricLibraryIsValidThenApiReportSizeIsValid) {
    // Create performance counters.
    auto *performanceCounters = initDeviceWithPerformanceCounters(true, true);
    EXPECT_NE(nullptr, performanceCounters);
    EXPECT_TRUE(performanceCounters->enable(false));
    EXPECT_EQ(1u, performanceCounters->getReferenceNumber());

    // Obtain api report size.
    uint32_t apiReportSize = performanceCounters->getApiReportSize();
    EXPECT_GT(apiReportSize, 0u);

    // Close library.
    performanceCounters->shutdown();
    EXPECT_EQ(0u, performanceCounters->getReferenceNumber());
}

TEST_F(PerformanceCountersMetricsLibraryTest, givenPerformanceCountersWhenMetricLibraryIsInvalidThenApiReportSizeIsInvalid) {
    // Create performance counters.
    auto *performanceCounters = initDeviceWithPerformanceCounters(false, false);
    EXPECT_NE(nullptr, performanceCounters);
    EXPECT_FALSE(performanceCounters->enable(false));
    EXPECT_EQ(1u, performanceCounters->getReferenceNumber());

    // Obtain api report size.
    uint32_t apiReportSize = performanceCounters->getApiReportSize();
    EXPECT_EQ(0u, apiReportSize);

    // Close library.
    performanceCounters->shutdown();
    EXPECT_EQ(0u, performanceCounters->getReferenceNumber());
}

TEST_F(PerformanceCountersMetricsLibraryTest, givenPerformanceCountersWhenMetricLibraryIsInvalidThenGpuReportSizeIsInvalid) {
    // Create performance counters.
    auto *performanceCounters = initDeviceWithPerformanceCounters(false, false);
    EXPECT_NE(nullptr, performanceCounters);
    EXPECT_FALSE(performanceCounters->enable(false));
    EXPECT_EQ(1u, performanceCounters->getReferenceNumber());

    // Obtain gpu report size.
    uint32_t gpuReportSize = performanceCounters->getGpuReportSize();
    EXPECT_EQ(0u, gpuReportSize);

    // Close library.
    performanceCounters->shutdown();
    EXPECT_EQ(0u, performanceCounters->getReferenceNumber());
}

TEST_F(PerformanceCountersMetricsLibraryTest, givenPerformanceCountersWhenMetricLibraryIsValidThenQueryIsAvailable) {
    // Create performance counters.
    auto *performanceCounters = initDeviceWithPerformanceCounters(true, true);
    EXPECT_NE(nullptr, performanceCounters);
    EXPECT_TRUE(performanceCounters->enable(false));
    EXPECT_EQ(1u, performanceCounters->getReferenceNumber());

    // Close library.
    performanceCounters->shutdown();
    EXPECT_EQ(0u, performanceCounters->getReferenceNumber());
}

TEST_F(PerformanceCountersMetricsLibraryTest, givenPerformanceCountersWhenMetricLibraryIsInvalidThenQueryIsNotAvailable) {
    // Create performance counters.
    auto *performanceCounters = initDeviceWithPerformanceCounters(false, false);
    EXPECT_NE(nullptr, performanceCounters);
    EXPECT_FALSE(performanceCounters->enable(false));
    EXPECT_EQ(1u, performanceCounters->getReferenceNumber());

    // Close library.
    performanceCounters->shutdown();
    EXPECT_EQ(0u, performanceCounters->getReferenceNumber());
}

TEST_F(PerformanceCountersMetricsLibraryTest, givenPerformanceCountersWhenMetricLibraryHasInvalidExportFunctionsDestroyThenQueryIsNotAvailable) {
    auto *performanceCounters = initDeviceWithPerformanceCounters(true, false);
    auto metricsLibraryInterface = performanceCounters->getMetricsLibraryInterface();
    auto metricsLibraryDll = reinterpret_cast<MockMetricsLibraryDll *>(metricsLibraryInterface->osLibrary.get());
    metricsLibraryDll->validContextCreate = true;
    metricsLibraryDll->validContextDelete = false;
    EXPECT_NE(nullptr, performanceCounters);
    EXPECT_EQ(0u, performanceCounters->getReferenceNumber());
    EXPECT_FALSE(performanceCounters->enable(false));
    EXPECT_EQ(1u, performanceCounters->getReferenceNumber());

    // Close library.
    performanceCounters->shutdown();
    EXPECT_EQ(0u, performanceCounters->getReferenceNumber());
}

TEST_F(PerformanceCountersMetricsLibraryTest, givenPerformanceCountersWhenMetricLibraryHasInvalidExportFunctionsCreateAndDestroyThenQueryIsNotAvailable) {
    auto *performanceCounters = initDeviceWithPerformanceCounters(true, false);
    auto metricsLibraryInterface = performanceCounters->getMetricsLibraryInterface();
    auto metricsLibraryDll = reinterpret_cast<MockMetricsLibraryDll *>(metricsLibraryInterface->osLibrary.get());
    metricsLibraryDll->validContextCreate = false;
    metricsLibraryDll->validContextDelete = false;
    EXPECT_NE(nullptr, performanceCounters);
    EXPECT_EQ(0u, performanceCounters->getReferenceNumber());
    EXPECT_FALSE(performanceCounters->enable(false));
    EXPECT_EQ(1u, performanceCounters->getReferenceNumber());

    // Close library.
    performanceCounters->shutdown();
    EXPECT_EQ(0u, performanceCounters->getReferenceNumber());
}

TEST_F(PerformanceCountersMetricsLibraryTest, givenPerformanceCountersWhenMetricLibraryIsValidThenQueryReturnsCorrectApiReport) {
    // Create performance counters.
    auto *performanceCounters = initDeviceWithPerformanceCounters(true, false);
    EXPECT_NE(nullptr, performanceCounters);
    EXPECT_TRUE(performanceCounters->enable(false));
    EXPECT_EQ(1u, performanceCounters->getReferenceNumber());

    // Obtain required command buffer size.
    uint32_t commandsSize = performanceCounters->getGpuCommandsSize(MetricsLibraryApi::GpuCommandBufferType::Render, true);
    EXPECT_NE(0u, commandsSize);

    // Fill command buffer.
    uint8_t buffer[1000] = {};
    TagNode<HwPerfCounter> query = {};
    HwPerfCounter perfCounter = {};
    query.tagForCpuAccess = &perfCounter;
    EXPECT_TRUE(performanceCounters->getGpuCommands(MetricsLibraryApi::GpuCommandBufferType::Render, query, true, sizeof(buffer), buffer));

    // Obtain api report size.
    uint32_t apiReportSize = performanceCounters->getApiReportSize();
    EXPECT_GT(apiReportSize, 0u);

    // Obtain gpu report size.
    uint32_t gpuReportSize = performanceCounters->getGpuReportSize();
    EXPECT_GT(gpuReportSize, 0u);

    // Allocate memory for api report.
    uint8_t *apiReport = new uint8_t[apiReportSize];
    EXPECT_NE(apiReport, nullptr);

    // Obtain api report.
    EXPECT_TRUE(performanceCounters->getApiReport(&query, apiReportSize, apiReport, nullptr, true));

    delete[] apiReport;
    apiReport = nullptr;

    // Close library.
    performanceCounters->deleteQuery(perfCounter.query.handle);
    performanceCounters->shutdown();
    EXPECT_EQ(0u, performanceCounters->getReferenceNumber());
}

TEST_F(PerformanceCountersMetricsLibraryTest, givenPerformanceCountersWhenMetricLibraryIsValidThenReferenceCounterIsValid) {
    auto *performanceCounters = initDeviceWithPerformanceCounters(true, true);
    EXPECT_NE(nullptr, performanceCounters);
    EXPECT_EQ(0u, performanceCounters->getReferenceNumber());
    EXPECT_TRUE(performanceCounters->enable(false));
    EXPECT_EQ(1u, performanceCounters->getReferenceNumber());
    EXPECT_TRUE(performanceCounters->enable(false));
    EXPECT_EQ(2u, performanceCounters->getReferenceNumber());
    performanceCounters->shutdown();
    EXPECT_EQ(1u, performanceCounters->getReferenceNumber());
    performanceCounters->shutdown();
    EXPECT_EQ(0u, performanceCounters->getReferenceNumber());
    performanceCounters->shutdown();
    EXPECT_EQ(0u, performanceCounters->getReferenceNumber());
}

TEST_F(PerformanceCountersMetricsLibraryTest, givenPerformanceCountersWhenMetricLibraryIsValidThenQueryHandleIsValid) {
    auto *performanceCounters = initDeviceWithPerformanceCounters(true, false);
    EXPECT_NE(nullptr, performanceCounters);

    MetricsLibraryApi::QueryHandle_1_0 query = {};

    EXPECT_EQ(0u, performanceCounters->getReferenceNumber());
    EXPECT_TRUE(performanceCounters->enable(false));

    performanceCounters->getQueryHandleRef(query);
    EXPECT_TRUE(query.IsValid());

    performanceCounters->deleteQuery(query);
    performanceCounters->shutdown();
    EXPECT_EQ(0u, performanceCounters->getReferenceNumber());
}

TEST_F(PerformanceCountersMetricsLibraryTest, givenPerformanceCountersWhenOaConfigurationIsInvalidThenGpuReportSizeIsInvalid) {
    auto *performanceCounters = initDeviceWithPerformanceCounters(true, false);
    EXPECT_NE(nullptr, performanceCounters);
    EXPECT_TRUE(performanceCounters->enable(false));
    EXPECT_EQ(1u, performanceCounters->getReferenceNumber());

    auto metricLibraryApi = static_cast<MockMetricsLibraryValidInterface *>(performanceCounters->getMetricsLibraryContext().data);
    metricLibraryApi->validCreateConfigurationOa = false;

    EXPECT_EQ(0u, performanceCounters->getGpuCommandsSize(MetricsLibraryApi::GpuCommandBufferType::Render, true));
    EXPECT_GT(performanceCounters->getGpuCommandsSize(MetricsLibraryApi::GpuCommandBufferType::Render, false), 0u);

    performanceCounters->shutdown();
    EXPECT_EQ(0u, performanceCounters->getReferenceNumber());
}

TEST_F(PerformanceCountersMetricsLibraryTest, GivenInvalidMetricsLibraryWhenGettingGpuCommandSizeThenZeroIsReported) {
    auto *performanceCounters = initDeviceWithPerformanceCounters(true, false);
    EXPECT_NE(nullptr, performanceCounters);
    EXPECT_TRUE(performanceCounters->enable(false));
    EXPECT_EQ(1u, performanceCounters->getReferenceNumber());

    auto metricLibraryApi = static_cast<MockMetricsLibraryValidInterface *>(performanceCounters->getMetricsLibraryContext().data);
    metricLibraryApi->validGpuReportSize = false;

    EXPECT_EQ(0u, performanceCounters->getGpuCommandsSize(MetricsLibraryApi::GpuCommandBufferType::Render, true));
    EXPECT_EQ(0u, performanceCounters->getGpuCommandsSize(MetricsLibraryApi::GpuCommandBufferType::Render, false));

    performanceCounters->shutdown();
    EXPECT_EQ(0u, performanceCounters->getReferenceNumber());
}

TEST_F(PerformanceCountersMetricsLibraryTest, givenPerformanceCountersWhenAllConfigurationsAreValidThenGpuReportSizeIsValid) {
    auto *performanceCounters = initDeviceWithPerformanceCounters(true, false);
    EXPECT_NE(nullptr, performanceCounters);
    EXPECT_TRUE(performanceCounters->enable(false));
    EXPECT_EQ(1u, performanceCounters->getReferenceNumber());

    auto metricLibraryApi = static_cast<MockMetricsLibraryValidInterface *>(performanceCounters->getMetricsLibraryContext().data);
    metricLibraryApi->validCreateConfigurationOa = true;
    metricLibraryApi->validCreateConfigurationUser = true;

    EXPECT_GT(performanceCounters->getGpuCommandsSize(MetricsLibraryApi::GpuCommandBufferType::Render, true), 0u);
    EXPECT_GT(performanceCounters->getGpuCommandsSize(MetricsLibraryApi::GpuCommandBufferType::Render, false), 0u);

    performanceCounters->shutdown();
    EXPECT_EQ(0u, performanceCounters->getReferenceNumber());
}

TEST_F(PerformanceCountersMetricsLibraryTest, givenPerformanceCountersWhenOaConfigurationsActivationIsInvalidThenGpuReportSizeIsInvalid) {
    auto *performanceCounters = initDeviceWithPerformanceCounters(true, false);
    EXPECT_NE(nullptr, performanceCounters);
    EXPECT_TRUE(performanceCounters->enable(false));
    EXPECT_EQ(1u, performanceCounters->getReferenceNumber());

    auto metricLibraryApi = static_cast<MockMetricsLibraryValidInterface *>(performanceCounters->getMetricsLibraryContext().data);
    metricLibraryApi->validActivateConfigurationOa = false;

    EXPECT_EQ(0u, performanceCounters->getGpuCommandsSize(MetricsLibraryApi::GpuCommandBufferType::Render, true));
    EXPECT_GT(performanceCounters->getGpuCommandsSize(MetricsLibraryApi::GpuCommandBufferType::Render, false), 0u);

    performanceCounters->shutdown();
    EXPECT_EQ(0u, performanceCounters->getReferenceNumber());
}

TEST_F(PerformanceCountersMetricsLibraryTest, givenPerformanceCountersWhenCreatingUserConfigurationThenReturnSuccess) {
    auto *performanceCounters = initDeviceWithPerformanceCounters(true, false);
    EXPECT_NE(nullptr, performanceCounters);
    EXPECT_TRUE(performanceCounters->enable(false));
    EXPECT_EQ(1u, performanceCounters->getReferenceNumber());

    ConfigurationHandle_1_0 configurationHandle = {};
    auto metricsLibrary = performanceCounters->getMetricsLibraryInterface();
    auto contextHandle = performanceCounters->getMetricsLibraryContext();
    EXPECT_TRUE(metricsLibrary->userConfigurationCreate(contextHandle, configurationHandle));
    EXPECT_TRUE(metricsLibrary->userConfigurationDelete(configurationHandle));

    performanceCounters->shutdown();
    EXPECT_EQ(0u, performanceCounters->getReferenceNumber());
}

TEST_F(PerformanceCountersMetricsLibraryTest, WhenGettingHwPerfCounterThenValidPointerIsReturned) {
    auto *performanceCounters = initDeviceWithPerformanceCounters(true, false);
    EXPECT_NE(nullptr, performanceCounters);
    EXPECT_TRUE(performanceCounters->enable(false));
    EXPECT_EQ(1u, performanceCounters->getReferenceNumber());

    ASSERT_NE(nullptr, queue->getPerfCounters());

    std::unique_ptr<Event> event(new Event(queue.get(), CL_COMMAND_COPY_BUFFER, 0, 0));
    ASSERT_NE(nullptr, event);

    auto perfCounter = static_cast<TagNode<HwPerfCounter> *>(event->getHwPerfCounterNode());
    ASSERT_NE(nullptr, perfCounter);

    ASSERT_EQ(0ULL, perfCounter->tagForCpuAccess->report[0]);

    auto perfCounter2 = event->getHwPerfCounterNode();
    ASSERT_EQ(perfCounter, perfCounter2);

    performanceCounters->shutdown();
    EXPECT_EQ(0u, performanceCounters->getReferenceNumber());
}

TEST_F(PerformanceCountersMetricsLibraryTest, WhenGettingHwPerfCounterAllocationThenValidPointerIsReturned) {
    auto *performanceCounters = initDeviceWithPerformanceCounters(true, false);
    EXPECT_NE(nullptr, performanceCounters);
    EXPECT_TRUE(performanceCounters->enable(false));
    EXPECT_EQ(1u, performanceCounters->getReferenceNumber());
    ASSERT_NE(nullptr, queue->getPerfCounters());

    std::unique_ptr<Event> event(new Event(queue.get(), CL_COMMAND_COPY_BUFFER, 0, 0));
    ASSERT_NE(nullptr, event);

    GraphicsAllocation *allocation = event->getHwPerfCounterNode()->getBaseGraphicsAllocation()->getDefaultGraphicsAllocation();
    ASSERT_NE(nullptr, allocation);

    void *memoryStorage = allocation->getUnderlyingBuffer();
    size_t memoryStorageSize = allocation->getUnderlyingBufferSize();

    EXPECT_NE(nullptr, memoryStorage);
    EXPECT_GT(memoryStorageSize, 0u);

    performanceCounters->shutdown();
    EXPECT_EQ(0u, performanceCounters->getReferenceNumber());
}

TEST_F(PerformanceCountersMetricsLibraryTest, WhenCreatingEventThenHwPerfCounterMemoryIsPlacedInGraphicsAllocation) {
    auto *performanceCounters = initDeviceWithPerformanceCounters(true, false);
    EXPECT_NE(nullptr, performanceCounters);
    EXPECT_TRUE(performanceCounters->enable(false));
    EXPECT_EQ(1u, performanceCounters->getReferenceNumber());
    ASSERT_NE(nullptr, queue->getPerfCounters());

    std::unique_ptr<Event> event(new Event(queue.get(), CL_COMMAND_COPY_BUFFER, 0, 0));
    ASSERT_NE(nullptr, event);

    HwPerfCounter *perfCounter = static_cast<TagNode<HwPerfCounter> *>(event->getHwPerfCounterNode())->tagForCpuAccess;
    ASSERT_NE(nullptr, perfCounter);

    GraphicsAllocation *allocation = event->getHwPerfCounterNode()->getBaseGraphicsAllocation()->getDefaultGraphicsAllocation();
    ASSERT_NE(nullptr, allocation);

    void *memoryStorage = allocation->getUnderlyingBuffer();
    size_t graphicsAllocationSize = allocation->getUnderlyingBufferSize();

    EXPECT_GE(perfCounter, memoryStorage);
    EXPECT_LE(perfCounter + 1, ptrOffset(memoryStorage, graphicsAllocationSize));

    performanceCounters->shutdown();
    EXPECT_EQ(0u, performanceCounters->getReferenceNumber());
}

TEST_F(PerformanceCountersMetricsLibraryTest, GivenPerformanceCountersObjectIsNotPresentWhenCreatingEventThenNodeisNull) {
    std::unique_ptr<Event> event(new Event(queue.get(), CL_COMMAND_COPY_BUFFER, 0, 0));
    ASSERT_NE(nullptr, event);

    auto node = event->getHwPerfCounterNode();
    ASSERT_EQ(nullptr, node);
}

TEST_F(PerformanceCountersTest, givenRenderCoreFamilyWhenGettingGenIdThenMetricsLibraryGenIdentifierAreValid) {

    auto &gfxCoreHelper = device->getGfxCoreHelper();
    EXPECT_NE(ClientGen::Unknown, static_cast<ClientGen>(gfxCoreHelper.getMetricsLibraryGenId()));
}
