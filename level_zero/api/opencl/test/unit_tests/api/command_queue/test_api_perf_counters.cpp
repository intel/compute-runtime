/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/performance_counters.h"
#include "shared/source/utilities/metrics_library.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/extensions/public/cl_ext_private.h"
#include "level_zero/api/opencl/source/api/leo_api.h"
#include "level_zero/api/opencl/source/command_queue/leo_command_queue.h"
#include "level_zero/api/opencl/source/context/leo_context.h"
#include "level_zero/api/opencl/source/event/leo_event.h"
#include "level_zero/api/opencl/source/helpers/leo_base_object.h"
#include "level_zero/api/opencl/test/common/fixtures/ocl_fixture.h"

#include "CL/cl.h"

#include <optional>

namespace NEO {
namespace LEO {
namespace ult {

class MockMetricsLibrary : public NEO::MetricsLibrary {
  public:
    bool open() override { return validOpen; }

    bool contextCreate(const MetricsLibraryApi::ClientType_1_0 &client,
                       MetricsLibraryApi::ClientOptionsSubDeviceData_1_0 &subDevice,
                       MetricsLibraryApi::ClientOptionsSubDeviceIndexData_1_0 &subDeviceIndex,
                       MetricsLibraryApi::ClientOptionsSubDeviceCountData_1_0 &subDeviceCount,
                       MetricsLibraryApi::ClientData_1_0 &clientData,
                       MetricsLibraryApi::ContextCreateData_1_0 &createData,
                       MetricsLibraryApi::ContextHandle_1_0 &handle) override {
        handle.data = reinterpret_cast<void *>(this);
        return true;
    }

    bool contextDelete(const MetricsLibraryApi::ContextHandle_1_0 &handle) override { return true; }

    uint32_t hwCountersGetGpuReportSize() override { return 256u; }

    bool validOpen = true;
};

class MockPerformanceCounters : public NEO::PerformanceCounters {
  public:
    MockPerformanceCounters() {
        setMetricsLibraryInterface(std::make_unique<MockMetricsLibrary>());
    }

    bool enableCountersConfiguration() override { return true; }
    void releaseCountersConfiguration() override {}
};

struct MockPerfCountersFixture : public Test<OclFixture> {
    void SetUp() override {
        Test<OclFixture>::SetUp();
        clDevice = platform->getDevices()[0].get();
        cl_device_id clDeviceId = clDevice;
        cl_int errcode = CL_SUCCESS;
        clContext = clCreateContext(nullptr, 1, &clDeviceId, nullptr, nullptr, &errcode);
        ASSERT_EQ(CL_SUCCESS, errcode);
        ASSERT_NE(nullptr, clContext);
        leoContext = NEO::LEO::castToObject<Context>(clContext);
    }

    void TearDown() override {
        if (instrumentationBackup.has_value()) {
            auto &hwInfo = const_cast<NEO::HardwareInfo &>(clDevice->getDevice().getHardwareInfo());
            hwInfo.capabilityTable.instrumentationEnabled = instrumentationBackup.value();
        }
        clReleaseContext(clContext);
        Test<OclFixture>::TearDown();
    }

    void setInstrumentationEnabled(bool enabled) {
        auto &hwInfo = const_cast<NEO::HardwareInfo &>(clDevice->getDevice().getHardwareInfo());
        if (!instrumentationBackup.has_value()) {
            instrumentationBackup = hwInfo.capabilityTable.instrumentationEnabled;
        }
        hwInfo.capabilityTable.instrumentationEnabled = enabled;
    }

    MockPerformanceCounters *setupDevicePerfCounters() {
        auto perfCounters = std::make_unique<MockPerformanceCounters>();
        auto rawPerfCounters = perfCounters.get();
        neoDevice->setPerfCounters(std::move(perfCounters));
        return rawPerfCounters;
    }

    cl_context getClContext() { return clContext; }

    ClDevice *clDevice = nullptr;
    cl_context clContext = nullptr;
    Context *leoContext = nullptr;
    std::optional<bool> instrumentationBackup;
};

TEST_F(MockPerfCountersFixture, givenDeviceWithoutPerformanceCountersWhenGettingPerfCountersThenNullptrIsReturned) {
    cl_int errcode = CL_SUCCESS;
    auto queue = clCreateCommandQueue(getClContext(), clDevice, 0, &errcode);
    ASSERT_NE(nullptr, queue);
    EXPECT_EQ(CL_SUCCESS, errcode);

    auto leoQueue = NEO::LEO::castToObject<CommandQueue>(queue);
    EXPECT_EQ(nullptr, leoQueue->getPerfCounters());

    clReleaseCommandQueue(queue);
}

TEST_F(MockPerfCountersFixture, givenDeviceWithPerformanceCountersWhenGettingPerfCountersFromMultipleQueuesThenSameDeviceInstanceIsReturned) {
    auto devicePerfCounters = setupDevicePerfCounters();

    cl_int errcode = CL_SUCCESS;
    auto queue1 = clCreateCommandQueue(getClContext(), clDevice, 0, &errcode);
    ASSERT_NE(nullptr, queue1);
    auto queue2 = clCreateCommandQueue(getClContext(), clDevice, 0, &errcode);
    ASSERT_NE(nullptr, queue2);

    auto leoQueue1 = NEO::LEO::castToObject<CommandQueue>(queue1);
    auto leoQueue2 = NEO::LEO::castToObject<CommandQueue>(queue2);

    EXPECT_EQ(devicePerfCounters, leoQueue1->getPerfCounters());
    EXPECT_EQ(leoQueue1->getPerfCounters(), leoQueue2->getPerfCounters());

    clReleaseCommandQueue(queue1);
    clReleaseCommandQueue(queue2);
}

TEST_F(MockPerfCountersFixture, givenDeviceWithoutPerformanceCountersWhenSettingPerfCountersEnabledThenFalseIsReturnedAndQueueStaysDisabled) {
    cl_int errcode = CL_SUCCESS;
    auto queue = clCreateCommandQueue(getClContext(), clDevice, 0, &errcode);
    ASSERT_NE(nullptr, queue);

    auto leoQueue = NEO::LEO::castToObject<CommandQueue>(queue);
    EXPECT_FALSE(leoQueue->setPerfCountersEnabled());
    EXPECT_FALSE(leoQueue->isPerfCountersEnabled());

    clReleaseCommandQueue(queue);
}

TEST_F(MockPerfCountersFixture, givenDeviceWithPerformanceCountersWhenSettingPerfCountersEnabledThenReferenceCounterIsIncrementedAndReleasedOnQueueDestruction) {
    auto devicePerfCounters = setupDevicePerfCounters();

    cl_int errcode = CL_SUCCESS;
    auto queue = clCreateCommandQueue(getClContext(), clDevice, 0, &errcode);
    ASSERT_NE(nullptr, queue);

    auto leoQueue = NEO::LEO::castToObject<CommandQueue>(queue);
    EXPECT_TRUE(leoQueue->setPerfCountersEnabled());
    EXPECT_TRUE(leoQueue->isPerfCountersEnabled());
    EXPECT_EQ(1u, devicePerfCounters->getReferenceNumber());

    clReleaseCommandQueue(queue);
    EXPECT_EQ(0u, devicePerfCounters->getReferenceNumber());
}

TEST_F(MockPerfCountersFixture, givenTwoQueuesSharingDevicePerformanceCountersWhenOneIsDestroyedThenReferenceCounterIsDecrementedButCountersRemainOpen) {
    auto devicePerfCounters = setupDevicePerfCounters();

    cl_int errcode = CL_SUCCESS;
    auto queue1 = clCreateCommandQueue(getClContext(), clDevice, 0, &errcode);
    ASSERT_NE(nullptr, queue1);
    auto queue2 = clCreateCommandQueue(getClContext(), clDevice, 0, &errcode);
    ASSERT_NE(nullptr, queue2);

    auto leoQueue1 = NEO::LEO::castToObject<CommandQueue>(queue1);
    auto leoQueue2 = NEO::LEO::castToObject<CommandQueue>(queue2);
    EXPECT_TRUE(leoQueue1->setPerfCountersEnabled());
    EXPECT_TRUE(leoQueue2->setPerfCountersEnabled());
    EXPECT_EQ(2u, devicePerfCounters->getReferenceNumber());

    clReleaseCommandQueue(queue1);
    EXPECT_EQ(1u, devicePerfCounters->getReferenceNumber());

    clReleaseCommandQueue(queue2);
    EXPECT_EQ(0u, devicePerfCounters->getReferenceNumber());
}

TEST_F(MockPerfCountersFixture, givenNonInstrumentedDeviceWhenCreatingPerfCountersCommandQueueThenInvalidDeviceIsReturned) {
    setInstrumentationEnabled(false);

    cl_int errcode = CL_SUCCESS;
    auto queue = clCreatePerfCountersCommandQueueINTEL(getClContext(), clDevice, CL_QUEUE_PROFILING_ENABLE, 0, &errcode);
    EXPECT_EQ(nullptr, queue);
    EXPECT_EQ(CL_INVALID_DEVICE, errcode);
}

TEST_F(MockPerfCountersFixture, givenQueueWithoutProfilingWhenCreatingPerfCountersCommandQueueThenInvalidQueuePropertiesIsReturned) {
    setInstrumentationEnabled(true);

    cl_int errcode = CL_SUCCESS;
    auto queue = clCreatePerfCountersCommandQueueINTEL(getClContext(), clDevice, 0, 0, &errcode);
    EXPECT_EQ(nullptr, queue);
    EXPECT_EQ(CL_INVALID_QUEUE_PROPERTIES, errcode);
}

TEST_F(MockPerfCountersFixture, givenNonZeroConfigurationWhenCreatingPerfCountersCommandQueueThenInvalidOperationIsReturned) {
    setInstrumentationEnabled(true);

    cl_int errcode = CL_SUCCESS;
    auto queue = clCreatePerfCountersCommandQueueINTEL(getClContext(), clDevice, CL_QUEUE_PROFILING_ENABLE, 1, &errcode);
    EXPECT_EQ(nullptr, queue);
    EXPECT_EQ(CL_INVALID_OPERATION, errcode);
}

TEST_F(MockPerfCountersFixture, givenDeviceWithoutPerformanceCountersWhenCreatingPerfCountersCommandQueueThenOutOfResourcesIsReturned) {
    setInstrumentationEnabled(true);

    cl_int errcode = CL_SUCCESS;
    auto queue = clCreatePerfCountersCommandQueueINTEL(getClContext(), clDevice, CL_QUEUE_PROFILING_ENABLE, 0, &errcode);
    EXPECT_EQ(nullptr, queue);
    EXPECT_EQ(CL_OUT_OF_RESOURCES, errcode);
}

TEST_F(MockPerfCountersFixture, givenWorkingPerformanceCountersWhenCreatingPerfCountersCommandQueueThenQueueIsCreatedWithPerfCountersEnabled) {
    setInstrumentationEnabled(true);
    auto devicePerfCounters = setupDevicePerfCounters();

    cl_int errcode = CL_SUCCESS;
    auto queue = clCreatePerfCountersCommandQueueINTEL(getClContext(), clDevice, CL_QUEUE_PROFILING_ENABLE, 0, &errcode);
    ASSERT_NE(nullptr, queue);
    EXPECT_EQ(CL_SUCCESS, errcode);

    auto leoQueue = NEO::LEO::castToObject<CommandQueue>(queue);
    EXPECT_TRUE(leoQueue->isPerfCountersEnabled());
    EXPECT_EQ(1u, devicePerfCounters->getReferenceNumber());

    clReleaseCommandQueue(queue);
    EXPECT_EQ(0u, devicePerfCounters->getReferenceNumber());
}

TEST_F(MockPerfCountersFixture, givenUserEventWhenGettingProfilingInfoThenProfilingInfoNotAvailableIsReturned) {
    auto event = std::make_unique<Event>(leoContext);

    cl_ulong timestamp = 0;
    auto retVal = event->getProfilingInfo(CL_PROFILING_COMMAND_START, sizeof(timestamp), &timestamp, nullptr);
    EXPECT_EQ(CL_PROFILING_INFO_NOT_AVAILABLE, retVal);

    retVal = event->getProfilingInfo(CL_PROFILING_COMMAND_PERFCOUNTERS_INTEL, sizeof(timestamp), &timestamp, nullptr);
    EXPECT_EQ(CL_PROFILING_INFO_NOT_AVAILABLE, retVal);
}

TEST_F(MockPerfCountersFixture, givenUserEventWhenGettingEventInfoThenUserCommandTypeAndNoQueueAreReported) {
    auto event = std::make_unique<Event>(leoContext);

    cl_command_type commandType = 0;
    EXPECT_EQ(CL_SUCCESS, event->getEventInfo(CL_EVENT_COMMAND_TYPE, sizeof(commandType), &commandType, nullptr));
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_USER), commandType);

    cl_command_queue eventQueue = reinterpret_cast<cl_command_queue>(0x1234);
    EXPECT_EQ(CL_SUCCESS, event->getEventInfo(CL_EVENT_COMMAND_QUEUE, sizeof(eventQueue), &eventQueue, nullptr));
    EXPECT_EQ(nullptr, eventQueue);

    cl_context eventContext = nullptr;
    EXPECT_EQ(CL_SUCCESS, event->getEventInfo(CL_EVENT_CONTEXT, sizeof(eventContext), &eventContext, nullptr));
    EXPECT_EQ(getClContext(), eventContext);
}

} // namespace ult
} // namespace LEO
} // namespace NEO
