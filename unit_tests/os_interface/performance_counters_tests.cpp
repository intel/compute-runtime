/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/unit_tests/helpers/debug_manager_state_restore.h"
#include "runtime/helpers/options.h"
#include "runtime/os_interface/os_interface.h"
#include "runtime/os_interface/os_time.h"
#include "runtime/utilities/tag_allocator.h"
#include "unit_tests/fixtures/device_instrumentation_fixture.h"
#include "unit_tests/mocks/mock_device.h"
#include "unit_tests/os_interface/mock_performance_counters.h"

#include "gtest/gtest.h"

using namespace NEO;

struct PerformanceCountersDeviceTest : public PerformanceCountersDeviceFixture,
                                       public DeviceInstrumentationFixture,
                                       public ::testing::Test {
    void SetUp() override {
        PerformanceCountersDeviceFixture::SetUp();
    }
    void TearDown() override {
        PerformanceCountersDeviceFixture::TearDown();
    }
};

TEST_F(PerformanceCountersDeviceTest, createDeviceWithPerformanceCounters) {
    DeviceInstrumentationFixture::SetUp(true);
    EXPECT_NE(nullptr, device->getPerformanceCounters());
}

TEST_F(PerformanceCountersDeviceTest, createDeviceWithoutPerformanceCounters) {
    DeviceInstrumentationFixture::SetUp(false);
    EXPECT_EQ(nullptr, device->getPerformanceCounters());
}

struct PerformanceCountersTest : public PerformanceCountersFixture,
                                 public ::testing::Test {
  public:
    void SetUp() override {
        PerformanceCountersFixture::SetUp();
    }

    void TearDown() override {
        PerformanceCountersFixture::TearDown();
    }
};

TEST_F(PerformanceCountersTest, createPerformanceCounters) {
    auto performanceCounters = PerformanceCounters::create(device.get());
    EXPECT_NE(nullptr, performanceCounters);
    EXPECT_NE(nullptr, performanceCounters.get());

    EXPECT_FALSE(performanceCounters->isAvailable());
}

TEST_F(PerformanceCountersTest, givenPerformanceCountersWhenCreatedThenAllValuesProperlyInitialized) {
    createPerfCounters();

    EXPECT_NE(nullptr, performanceCountersBase->getMetricsLibraryInterface());
    EXPECT_EQ(0u, performanceCountersBase->getReferenceNumber());
}

struct PerformanceCountersProcessEventTest : public PerformanceCountersTest,
                                             public ::testing::WithParamInterface<bool> {

    void SetUp() override {
        PerformanceCountersTest::SetUp();
        createPerfCounters();
        eventComplete = true;
        outputParamSize = 0;
        inputParamSize = performanceCountersBase->getApiReportSize();
        inputParam.reset(new uint8_t);
    }
    void TearDown() override {
        performanceCountersBase->shutdown();
        PerformanceCountersTest::TearDown();
    }

    std::unique_ptr<uint8_t> inputParam;
    size_t inputParamSize;
    size_t outputParamSize;
    bool eventComplete;
};

TEST_P(PerformanceCountersProcessEventTest, givenNullptrInputParamWhenProcessEventPerfCountersIsCalledThenReturnsFalse) {
    eventComplete = GetParam();
    auto retVal = performanceCountersBase->getApiReport(inputParamSize, nullptr, &outputParamSize, eventComplete);

    EXPECT_FALSE(retVal);
}

TEST_P(PerformanceCountersProcessEventTest, givenCorrectInputParamWhenProcessEventPerfCountersIsCalledAndEventIsCompletedThenReturnsTrue) {
    eventComplete = GetParam();
    EXPECT_EQ(0ull, outputParamSize);
    auto retVal = performanceCountersBase->getApiReport(inputParamSize, inputParam.get(), &outputParamSize, eventComplete);

    if (eventComplete) {
        EXPECT_TRUE(retVal);
        EXPECT_EQ(outputParamSize, inputParamSize);
    } else {
        EXPECT_FALSE(retVal);
        EXPECT_EQ(inputParamSize, outputParamSize);
    }
}

TEST_F(PerformanceCountersProcessEventTest, givenInvalidInputParamSizeWhenProcessEventPerfCountersIsCalledThenReturnsFalse) {
    EXPECT_EQ(0ull, outputParamSize);
    auto retVal = performanceCountersBase->getApiReport(inputParamSize - 1, inputParam.get(), &outputParamSize, eventComplete);

    EXPECT_FALSE(retVal);
    EXPECT_EQ(outputParamSize, inputParamSize);
}

TEST_F(PerformanceCountersProcessEventTest, givenNullptrOutputParamSizeWhenProcessEventPerfCountersIsCalledThenDoesNotReturnsOutputSize) {
    EXPECT_EQ(0ull, outputParamSize);
    auto retVal = performanceCountersBase->getApiReport(inputParamSize, inputParam.get(), nullptr, eventComplete);

    EXPECT_TRUE(retVal);
    EXPECT_EQ(0ull, outputParamSize);
}

TEST_F(PerformanceCountersProcessEventTest, givenNullptrInputZeroSizeWhenProcessEventPerfCountersIsCalledThenQueryProperSize) {
    EXPECT_EQ(0ull, outputParamSize);
    auto retVal = performanceCountersBase->getApiReport(0, nullptr, &outputParamSize, eventComplete);

    EXPECT_TRUE(retVal);
    EXPECT_EQ(inputParamSize, outputParamSize);
}

TEST_F(PerformanceCountersProcessEventTest, givenNullptrInputZeroSizeAndNullptrOutputSizeWhenProcessEventPerfCountersIsCalledThenReturnFalse) {
    EXPECT_EQ(0ull, outputParamSize);
    auto retVal = performanceCountersBase->getApiReport(0, nullptr, nullptr, eventComplete);

    EXPECT_FALSE(retVal);
    EXPECT_EQ(0ull, outputParamSize);
}

INSTANTIATE_TEST_CASE_P(
    PerfCountersTests,
    PerformanceCountersProcessEventTest,
    testing::Bool());

struct PerformanceCountersMetricsLibraryTest : public PerformanceCountersMetricsLibraryFixture,
                                               public ::testing::Test {

  public:
    void SetUp() override {
        PerformanceCountersMetricsLibraryFixture::SetUp();
    }

    void TearDown() override {
        PerformanceCountersMetricsLibraryFixture::TearDown();
    }
};

TEST_F(PerformanceCountersMetricsLibraryTest, givenPerformanceCountersWhenMetricLibraryIsValidThenQueryIsCreated) {
    // Create performance counters.
    createPerformanceCounters(true, true);
    EXPECT_NE(nullptr, performanceCountersBase);
    performanceCountersBase->enable();
    EXPECT_EQ(1u, performanceCountersBase->getReferenceNumber());
    EXPECT_TRUE(performanceCountersBase->isAvailable());

    // Check metric library context.
    auto context = static_cast<MockMetricsLibrary *>(performanceCountersBase->getMetricsLibraryContext().data);
    EXPECT_NE(nullptr, context);
    EXPECT_EQ(1u, context->contextCount);

    // Close library.
    performanceCountersBase->shutdown();
    EXPECT_EQ(0u, performanceCountersBase->getReferenceNumber());
}

TEST_F(PerformanceCountersMetricsLibraryTest, givenPerformanceCountersWhenMetricLibraryIsValidThenQueryReturnsValidGpuCommands) {
    // Create performance counters.
    createPerformanceCounters(true, false);
    EXPECT_NE(nullptr, performanceCountersBase);
    performanceCountersBase->enable();
    EXPECT_EQ(1u, performanceCountersBase->getReferenceNumber());
    EXPECT_TRUE(performanceCountersBase->isAvailable());

    // Obtain required command buffer size.
    uint32_t commandsSize = performanceCountersBase->getGpuCommandsSize(true);
    EXPECT_NE(0u, commandsSize);

    // Fill command buffer.
    uint8_t buffer[1000] = {};
    HwPerfCounter perfCounter = {};
    TagNode<HwPerfCounter> query = {};
    query.tagForCpuAccess = &perfCounter;
    EXPECT_TRUE(performanceCountersBase->getGpuCommands(query, true, sizeof(buffer), buffer));

    // Close library.
    performanceCountersBase->shutdown();
    EXPECT_EQ(0u, performanceCountersBase->getReferenceNumber());
}

TEST_F(PerformanceCountersMetricsLibraryTest, givenPerformanceCountersWhenMetricLibraryIsInvalidThenQueryReturnsInvalidGpuCommands) {
    // Create performance counters.
    createPerformanceCounters(false, false);
    EXPECT_NE(nullptr, performanceCountersBase);
    performanceCountersBase->enable();

    // Obtain required command buffer size.
    uint32_t commandsSize = performanceCountersBase->getGpuCommandsSize(true);
    EXPECT_EQ(0u, commandsSize);

    // Close library.
    performanceCountersBase->shutdown();
    EXPECT_EQ(0u, performanceCountersBase->getReferenceNumber());
}

TEST_F(PerformanceCountersMetricsLibraryTest, givenPerformanceCountersWhenMetricLibraryIsValidThenApiReportSizeIsValid) {
    // Create performance counters.
    createPerformanceCounters(true, true);
    EXPECT_NE(nullptr, performanceCountersBase);
    performanceCountersBase->enable();
    EXPECT_EQ(1u, performanceCountersBase->getReferenceNumber());
    EXPECT_TRUE(performanceCountersBase->isAvailable());

    // Obtain api report size.
    uint32_t apiReportSize = performanceCountersBase->getApiReportSize();
    EXPECT_GT(apiReportSize, 0u);

    // Close library.
    performanceCountersBase->shutdown();
    EXPECT_EQ(0u, performanceCountersBase->getReferenceNumber());
}

TEST_F(PerformanceCountersMetricsLibraryTest, givenPerformanceCountersWhenMetricLibraryIsInvalidThenApiReportSizeIsInvalid) {
    // Create performance counters.
    createPerformanceCounters(false, false);
    EXPECT_NE(nullptr, performanceCountersBase);
    performanceCountersBase->enable();
    EXPECT_EQ(1u, performanceCountersBase->getReferenceNumber());

    // Obtain api report size.
    uint32_t apiReportSize = performanceCountersBase->getApiReportSize();
    EXPECT_EQ(0u, apiReportSize);

    // Close library.
    performanceCountersBase->shutdown();
    EXPECT_EQ(0u, performanceCountersBase->getReferenceNumber());
}

TEST_F(PerformanceCountersMetricsLibraryTest, givenPerformanceCountersWhenMetricLibraryIsInvalidThenGpuReportSizeIsInvalid) {
    // Create performance counters.
    createPerformanceCounters(false, false);
    EXPECT_NE(nullptr, performanceCountersBase);
    performanceCountersBase->enable();
    EXPECT_EQ(1u, performanceCountersBase->getReferenceNumber());

    // Obtain gpu report size.
    uint32_t gpuReportSize = performanceCountersBase->getGpuReportSize();
    EXPECT_EQ(0u, gpuReportSize);

    // Close library.
    performanceCountersBase->shutdown();
    EXPECT_EQ(0u, performanceCountersBase->getReferenceNumber());
}

TEST_F(PerformanceCountersMetricsLibraryTest, givenPerformanceCountersWhenMetricLibraryIsValidThenQueryIsAvailable) {
    // Create performance counters.
    createPerformanceCounters(true, true);
    EXPECT_NE(nullptr, performanceCountersBase);
    performanceCountersBase->enable();
    EXPECT_EQ(1u, performanceCountersBase->getReferenceNumber());
    EXPECT_TRUE(performanceCountersBase->isAvailable());

    // Close library.
    performanceCountersBase->shutdown();
    EXPECT_EQ(0u, performanceCountersBase->getReferenceNumber());
}

TEST_F(PerformanceCountersMetricsLibraryTest, givenPerformanceCountersWhenMetricLibraryIsInvalidThenQueryIsNotAvailable) {
    // Create performance counters.
    createPerformanceCounters(false, false);
    EXPECT_NE(nullptr, performanceCountersBase);
    performanceCountersBase->enable();
    EXPECT_EQ(1u, performanceCountersBase->getReferenceNumber());
    EXPECT_FALSE(performanceCountersBase->isAvailable());

    // Close library.
    performanceCountersBase->shutdown();
    EXPECT_EQ(0u, performanceCountersBase->getReferenceNumber());
}

TEST_F(PerformanceCountersMetricsLibraryTest, givenPerformanceCountersWhenMetricLibraryHasInvalidExportFunctionsDestroyThenQueryIsNotAvailable) {
    createPerformanceCounters(true, false);
    auto metricsLibraryInterface = performanceCountersBase->getMetricsLibraryInterface();
    auto metricsLibraryDll = reinterpret_cast<MockMetricsLibraryDll *>(metricsLibraryInterface->osLibrary.get());
    metricsLibraryDll->validContextCreate = true;
    metricsLibraryDll->validContextDelete = false;
    EXPECT_NE(nullptr, performanceCountersBase);
    EXPECT_EQ(0u, performanceCountersBase->getReferenceNumber());
    performanceCountersBase->enable();
    EXPECT_EQ(1u, performanceCountersBase->getReferenceNumber());
    EXPECT_FALSE(performanceCountersBase->isAvailable());

    // Close library.
    performanceCountersBase->shutdown();
    EXPECT_EQ(0u, performanceCountersBase->getReferenceNumber());
}

TEST_F(PerformanceCountersMetricsLibraryTest, givenPerformanceCountersWhenMetricLibraryHasInvalidExportFunctionsCreateAndDestroyThenQueryIsNotAvailable) {
    createPerformanceCounters(true, false);
    auto metricsLibraryInterface = performanceCountersBase->getMetricsLibraryInterface();
    auto metricsLibraryDll = reinterpret_cast<MockMetricsLibraryDll *>(metricsLibraryInterface->osLibrary.get());
    metricsLibraryDll->validContextCreate = false;
    metricsLibraryDll->validContextDelete = false;
    EXPECT_NE(nullptr, performanceCountersBase);
    EXPECT_EQ(0u, performanceCountersBase->getReferenceNumber());
    performanceCountersBase->enable();
    EXPECT_EQ(1u, performanceCountersBase->getReferenceNumber());
    EXPECT_FALSE(performanceCountersBase->isAvailable());

    // Close library.
    performanceCountersBase->shutdown();
    EXPECT_EQ(0u, performanceCountersBase->getReferenceNumber());
}

TEST_F(PerformanceCountersMetricsLibraryTest, givenPerformanceCountersWhenMetricLibraryIsValidThenQueryReturnsCorrectApiReport) {
    // Create performance counters.
    createPerformanceCounters(true, false);
    EXPECT_NE(nullptr, performanceCountersBase);
    performanceCountersBase->enable();
    EXPECT_EQ(1u, performanceCountersBase->getReferenceNumber());
    EXPECT_TRUE(performanceCountersBase->isAvailable());

    // Obtain required command buffer size.
    uint32_t commandsSize = performanceCountersBase->getGpuCommandsSize(true);
    EXPECT_NE(0u, commandsSize);

    // Fill command buffer.
    uint8_t buffer[1000] = {};
    TagNode<HwPerfCounter> query = {};
    HwPerfCounter perfCounter = {};
    query.tagForCpuAccess = &perfCounter;
    EXPECT_TRUE(performanceCountersBase->getGpuCommands(query, true, sizeof(buffer), buffer));

    // Obtain api report size.
    uint32_t apiReportSize = performanceCountersBase->getApiReportSize();
    EXPECT_GT(apiReportSize, 0u);

    // Obtain gpu report size.
    uint32_t gpuReportSize = performanceCountersBase->getGpuReportSize();
    EXPECT_GT(gpuReportSize, 0u);

    // Allocate memory for api report.
    uint8_t *apiReport = new uint8_t[apiReportSize];
    EXPECT_NE(apiReport, nullptr);

    // Obtain api report.
    EXPECT_TRUE(performanceCountersBase->getApiReport(apiReportSize, apiReport, nullptr, true));

    delete[] apiReport;
    apiReport = nullptr;

    // Close library.
    performanceCountersBase->shutdown();
    EXPECT_EQ(0u, performanceCountersBase->getReferenceNumber());
}

TEST_F(PerformanceCountersMetricsLibraryTest, givenPerformanceCountersWhenMetricLibraryIsValidThenReferenceCounterIsValid) {
    createPerformanceCounters(true, true);
    EXPECT_NE(nullptr, performanceCountersBase);
    EXPECT_EQ(0u, performanceCountersBase->getReferenceNumber());
    performanceCountersBase->enable();
    EXPECT_EQ(1u, performanceCountersBase->getReferenceNumber());
    performanceCountersBase->enable();
    EXPECT_EQ(2u, performanceCountersBase->getReferenceNumber());
    performanceCountersBase->shutdown();
    EXPECT_EQ(1u, performanceCountersBase->getReferenceNumber());
    performanceCountersBase->shutdown();
    EXPECT_EQ(0u, performanceCountersBase->getReferenceNumber());
    performanceCountersBase->shutdown();
    EXPECT_EQ(0u, performanceCountersBase->getReferenceNumber());
}

TEST_F(PerformanceCountersMetricsLibraryTest, givenPerformanceCountersWhenMetricLibraryIsValidThenQueryHandleIsValid) {
    createPerformanceCounters(true, false);
    EXPECT_NE(nullptr, performanceCountersBase);

    EXPECT_EQ(0u, performanceCountersBase->getReferenceNumber());
    performanceCountersBase->enable();
    EXPECT_TRUE(performanceCountersBase->getQueryHandle().IsValid());
    EXPECT_TRUE(performanceCountersBase->getQueryHandle().IsValid());

    performanceCountersBase->shutdown();
    EXPECT_EQ(0u, performanceCountersBase->getReferenceNumber());
}

TEST_F(PerformanceCountersMetricsLibraryTest, givenPerformanceCountersWhenOaConfigurationIsInvalidThenGpuReportSizeIsInvalid) {
    createPerformanceCounters(true, false);
    EXPECT_NE(nullptr, performanceCountersBase);
    performanceCountersBase->enable();
    EXPECT_EQ(1u, performanceCountersBase->getReferenceNumber());

    auto metricLibraryApi = static_cast<MockMetricsLibraryValidInterface *>(performanceCountersBase->getMetricsLibraryContext().data);
    metricLibraryApi->validCreateConfigurationOa = false;

    EXPECT_EQ(0u, performanceCountersBase->getGpuCommandsSize(true));
    EXPECT_GT(performanceCountersBase->getGpuCommandsSize(false), 0u);

    performanceCountersBase->shutdown();
    EXPECT_EQ(0u, performanceCountersBase->getReferenceNumber());
}

TEST_F(PerformanceCountersMetricsLibraryTest, givenPerformanceCountersWhenMetricsLibraryIsInvalidGpuReportSizeIsInvalid) {
    createPerformanceCounters(true, false);
    EXPECT_NE(nullptr, performanceCountersBase);
    performanceCountersBase->enable();
    EXPECT_EQ(1u, performanceCountersBase->getReferenceNumber());

    auto metricLibraryApi = static_cast<MockMetricsLibraryValidInterface *>(performanceCountersBase->getMetricsLibraryContext().data);
    metricLibraryApi->validGpuReportSize = false;

    EXPECT_EQ(0u, performanceCountersBase->getGpuCommandsSize(true));
    EXPECT_EQ(0u, performanceCountersBase->getGpuCommandsSize(false));

    performanceCountersBase->shutdown();
    EXPECT_EQ(0u, performanceCountersBase->getReferenceNumber());
}

TEST_F(PerformanceCountersMetricsLibraryTest, givenPerformanceCountersWhenAllConfigurationsAreValidThenGpuReportSizeIsValid) {
    createPerformanceCounters(true, false);
    EXPECT_NE(nullptr, performanceCountersBase);
    performanceCountersBase->enable();
    EXPECT_EQ(1u, performanceCountersBase->getReferenceNumber());

    auto metricLibraryApi = static_cast<MockMetricsLibraryValidInterface *>(performanceCountersBase->getMetricsLibraryContext().data);
    metricLibraryApi->validCreateConfigurationOa = true;
    metricLibraryApi->validCreateConfigurationUser = true;

    EXPECT_GT(performanceCountersBase->getGpuCommandsSize(true), 0u);
    EXPECT_GT(performanceCountersBase->getGpuCommandsSize(false), 0u);

    performanceCountersBase->shutdown();
    EXPECT_EQ(0u, performanceCountersBase->getReferenceNumber());
}

TEST_F(PerformanceCountersMetricsLibraryTest, givenPerformanceCountersWhenOaConfigurationsActivationIsInvalidThenGpuReportSizeIsInvalid) {
    createPerformanceCounters(true, false);
    EXPECT_NE(nullptr, performanceCountersBase);
    performanceCountersBase->enable();
    EXPECT_EQ(1u, performanceCountersBase->getReferenceNumber());

    auto metricLibraryApi = static_cast<MockMetricsLibraryValidInterface *>(performanceCountersBase->getMetricsLibraryContext().data);
    metricLibraryApi->validActivateConfigurationOa = false;

    EXPECT_EQ(0u, performanceCountersBase->getGpuCommandsSize(true));
    EXPECT_GT(performanceCountersBase->getGpuCommandsSize(false), 0u);

    performanceCountersBase->shutdown();
    EXPECT_EQ(0u, performanceCountersBase->getReferenceNumber());
}

TEST_F(PerformanceCountersMetricsLibraryTest, givenPerformanceCountersWhenCreatingUserConfigurationThenReturnSuccess) {
    createPerformanceCounters(true, false);
    EXPECT_NE(nullptr, performanceCountersBase);
    performanceCountersBase->enable();
    EXPECT_EQ(1u, performanceCountersBase->getReferenceNumber());

    ConfigurationHandle_1_0 configurationHandle = {};
    auto metricsLibrary = performanceCountersBase->getMetricsLibraryInterface();
    auto contextHandle = performanceCountersBase->getMetricsLibraryContext();
    EXPECT_TRUE(metricsLibrary->userConfigurationCreate(contextHandle, configurationHandle));
    EXPECT_TRUE(metricsLibrary->userConfigurationDelete(configurationHandle));

    performanceCountersBase->shutdown();
    EXPECT_EQ(0u, performanceCountersBase->getReferenceNumber());
}

TEST_F(PerformanceCountersMetricsLibraryTest, getHwPerfCounterReturnsValidPointer) {
    createPerformanceCounters(true, false);
    EXPECT_NE(nullptr, performanceCountersBase);
    performanceCountersBase->enable();
    EXPECT_EQ(1u, performanceCountersBase->getReferenceNumber());

    ASSERT_NE(nullptr, queue->getPerfCounters());

    std::unique_ptr<Event> event(new Event(queue.get(), CL_COMMAND_COPY_BUFFER, 0, 0));
    ASSERT_NE(nullptr, event);

    HwPerfCounter *perfCounter = event->getHwPerfCounterNode()->tagForCpuAccess;
    ASSERT_NE(nullptr, perfCounter);

    ASSERT_EQ(0ULL, perfCounter->report[0]);
    EXPECT_TRUE(perfCounter->canBeReleased());

    HwPerfCounter *perfCounter2 = event->getHwPerfCounterNode()->tagForCpuAccess;
    ASSERT_EQ(perfCounter, perfCounter2);

    performanceCountersBase->shutdown();
    EXPECT_EQ(0u, performanceCountersBase->getReferenceNumber());
}

TEST_F(PerformanceCountersMetricsLibraryTest, getHwPerfCounterAllocationReturnsValidPointer) {
    createPerformanceCounters(true, false);
    EXPECT_NE(nullptr, performanceCountersBase);
    performanceCountersBase->enable();
    EXPECT_EQ(1u, performanceCountersBase->getReferenceNumber());
    ASSERT_NE(nullptr, queue->getPerfCounters());

    std::unique_ptr<Event> event(new Event(queue.get(), CL_COMMAND_COPY_BUFFER, 0, 0));
    ASSERT_NE(nullptr, event);

    GraphicsAllocation *allocation = event->getHwPerfCounterNode()->getBaseGraphicsAllocation();
    ASSERT_NE(nullptr, allocation);

    void *memoryStorage = allocation->getUnderlyingBuffer();
    size_t memoryStorageSize = allocation->getUnderlyingBufferSize();

    EXPECT_NE(nullptr, memoryStorage);
    EXPECT_GT(memoryStorageSize, 0u);

    performanceCountersBase->shutdown();
    EXPECT_EQ(0u, performanceCountersBase->getReferenceNumber());
}

TEST_F(PerformanceCountersMetricsLibraryTest, hwPerfCounterMemoryIsPlacedInGraphicsAllocation) {
    createPerformanceCounters(true, false);
    EXPECT_NE(nullptr, performanceCountersBase);
    performanceCountersBase->enable();
    EXPECT_EQ(1u, performanceCountersBase->getReferenceNumber());
    ASSERT_NE(nullptr, queue->getPerfCounters());

    std::unique_ptr<Event> event(new Event(queue.get(), CL_COMMAND_COPY_BUFFER, 0, 0));
    ASSERT_NE(nullptr, event);

    HwPerfCounter *perfCounter = event->getHwPerfCounterNode()->tagForCpuAccess;
    ASSERT_NE(nullptr, perfCounter);

    GraphicsAllocation *allocation = event->getHwPerfCounterNode()->getBaseGraphicsAllocation();
    ASSERT_NE(nullptr, allocation);

    void *memoryStorage = allocation->getUnderlyingBuffer();
    size_t graphicsAllocationSize = allocation->getUnderlyingBufferSize();

    EXPECT_GE(perfCounter, memoryStorage);
    EXPECT_LE(perfCounter + 1, ptrOffset(memoryStorage, graphicsAllocationSize));

    performanceCountersBase->shutdown();
    EXPECT_EQ(0u, performanceCountersBase->getReferenceNumber());
}

TEST_F(PerformanceCountersMetricsLibraryTest, hwPerfCounterNodeWhenPerformanceCountersObjectIsNotPresentThenNodeisNull) {
    std::unique_ptr<Event> event(new Event(queue.get(), CL_COMMAND_COPY_BUFFER, 0, 0));
    ASSERT_NE(nullptr, event);

    auto node = event->getHwPerfCounterNode();
    ASSERT_EQ(nullptr, node);
}

TEST_F(PerformanceCountersTest, givenRenderCoreFamilyThenMetricsLibraryGenIdentifierAreValid) {
    const auto &hwInfo = device->getHardwareInfo();
    const auto gen = hwInfo.platform.eRenderCoreFamily;
    EXPECT_NE(ClientGen::Unknown, static_cast<ClientGen>(HwHelper::get(gen).getMetricsLibraryGenId()));
}
