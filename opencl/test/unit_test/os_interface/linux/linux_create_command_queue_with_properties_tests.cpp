/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_memory_operations_handler.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/mocks/linux/mock_drm_command_stream_receiver.h"
#include "shared/test/common/mocks/linux/mock_drm_memory_manager.h"
#include "shared/test/common/mocks/linux/mock_ioctl_helper.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/source/command_queue/command_queue_hw.h"
#include "opencl/test/unit_test/fixtures/ult_command_stream_receiver_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

using namespace NEO;
#include "shared/test/common/test_macros/header/heapless_matchers.h"

struct ClCreateCommandQueueWithPropertiesLinux : public UltCommandStreamReceiverTest {
    void SetUp() override {
        UltCommandStreamReceiverTest::SetUp();
        ExecutionEnvironment *executionEnvironment = new MockExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(1);
        drm = new DrmMock(*executionEnvironment->rootDeviceEnvironments[0]);

        auto osInterface = new OSInterface();
        osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
        executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->osInterface.reset(osInterface);
        executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*drm, rootDeviceIndex, false);
        executionEnvironment->memoryManager.reset(new TestedDrmMemoryManager(*executionEnvironment));
        mdevice = std::make_unique<MockClDevice>(MockDevice::create<MockDevice>(executionEnvironment, rootDeviceIndex));
        ASSERT_NE(nullptr, mdevice.get());
        clDevice = mdevice.get();
        retVal = CL_SUCCESS;
        context = std::unique_ptr<Context>(Context::create<MockContext>(nullptr, ClDeviceVector(&clDevice, 1), nullptr, nullptr, retVal));
    }
    void TearDown() override {
        UltCommandStreamReceiverTest::TearDown();
    }
    DrmMock *drm = nullptr;
    std::unique_ptr<MockClDevice> mdevice = nullptr;
    std::unique_ptr<Context> context;
    cl_device_id clDevice = nullptr;
    cl_int retVal = 0;
    const uint32_t rootDeviceIndex = 0u;
};

namespace ULT {

TEST_F(ClCreateCommandQueueWithPropertiesLinux, givenUnPossiblePropertiesWithClQueueSliceCountWhenCreateCommandQueueThenQueueNotCreated) {
    uint64_t newSliceCount = 1;
    size_t maxSliceCount;
    clGetDeviceInfo(clDevice, CL_DEVICE_SLICE_COUNT_INTEL, sizeof(size_t), &maxSliceCount, nullptr);

    newSliceCount = maxSliceCount + 1;

    cl_queue_properties properties[] = {CL_QUEUE_SLICE_COUNT_INTEL, newSliceCount, 0};

    cl_command_queue cmdQ = clCreateCommandQueueWithProperties(context.get(), clDevice, properties, &retVal);

    EXPECT_EQ(nullptr, cmdQ);
    EXPECT_EQ(CL_INVALID_QUEUE_PROPERTIES, retVal);
}

TEST_F(ClCreateCommandQueueWithPropertiesLinux, givenZeroWithClQueueSliceCountWhenCreateCommandQueueThenSliceCountEqualDefaultSliceCount) {

    uint64_t newSliceCount = 0;

    cl_queue_properties properties[] = {CL_QUEUE_SLICE_COUNT_INTEL, newSliceCount, 0};

    cl_command_queue cmdQ = clCreateCommandQueueWithProperties(context.get(), clDevice, properties, &retVal);

    ASSERT_NE(nullptr, cmdQ);
    ASSERT_EQ(CL_SUCCESS, retVal);

    auto commandQueue = castToObject<CommandQueue>(cmdQ);
    EXPECT_EQ(commandQueue->getSliceCount(), QueueSliceCount::defaultSliceCount);

    retVal = clReleaseCommandQueue(cmdQ);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ClCreateCommandQueueWithPropertiesLinux, givenPossiblePropertiesWithClQueueSliceCountWhenCreateCommandQueueThenSliceCountIsSet) {

    uint64_t newSliceCount = 1;
    size_t maxSliceCount;
    clGetDeviceInfo(clDevice, CL_DEVICE_SLICE_COUNT_INTEL, sizeof(size_t), &maxSliceCount, nullptr);
    if (maxSliceCount > 1) {
        newSliceCount = maxSliceCount - 1;
    }

    cl_queue_properties properties[] = {CL_QUEUE_SLICE_COUNT_INTEL, newSliceCount, 0};

    cl_command_queue cmdQ = clCreateCommandQueueWithProperties(context.get(), clDevice, properties, &retVal);

    ASSERT_NE(nullptr, cmdQ);
    ASSERT_EQ(CL_SUCCESS, retVal);

    auto commandQueue = castToObject<CommandQueue>(cmdQ);
    EXPECT_EQ(commandQueue->getSliceCount(), newSliceCount);

    retVal = clReleaseCommandQueue(cmdQ);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

HWTEST2_F(ClCreateCommandQueueWithPropertiesLinux, givenPropertiesWithClQueueSliceCountWhenCreateCommandQueueThenCallFlushTaskAndSliceCountIsSet, IsHeapfulRequired) {
    uint64_t newSliceCount = 1;
    size_t maxSliceCount;
    clGetDeviceInfo(clDevice, CL_DEVICE_SLICE_COUNT_INTEL, sizeof(size_t), &maxSliceCount, nullptr);
    if (maxSliceCount > 1) {
        newSliceCount = maxSliceCount - 1;
    }

    cl_queue_properties properties[] = {CL_QUEUE_SLICE_COUNT_INTEL, newSliceCount, 0};

    auto mockCsr = new TestedDrmCommandStreamReceiver<FamilyType>(*mdevice->executionEnvironment, rootDeviceIndex, 1);
    mockCsr->flushInternalCallBase = false;
    mdevice->resetCommandStreamReceiver(mockCsr);
    if (mockCsr->getHeaplessStateInitEnabled()) {
        GTEST_SKIP();
    }

    cl_command_queue cmdQ = clCreateCommandQueueWithProperties(context.get(), clDevice, properties, &retVal);

    ASSERT_NE(nullptr, cmdQ);
    ASSERT_EQ(CL_SUCCESS, retVal);

    auto commandQueue = castToObject<CommandQueueHw<FamilyType>>(cmdQ);
    auto &commandStream = commandQueue->getCS(1024u);

    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    dispatchFlags.sliceCount = commandQueue->getSliceCount();
    dispatchFlags.implicitFlush = true;

    mockCsr->flushTask(commandStream,
                       0u,
                       &dsh,
                       &ioh,
                       &ssh,
                       taskLevel,
                       dispatchFlags,
                       mdevice->getDevice());
    auto expectedSliceMask = drm->getSliceMask(newSliceCount);
    EXPECT_EQ(expectedSliceMask, drm->storedParamSseu);
    GemContextParamSseu sseu = {};
    EXPECT_EQ(0, drm->getQueueSliceCount(&sseu));
    EXPECT_EQ(expectedSliceMask, sseu.sliceMask);
    EXPECT_EQ(newSliceCount, mockCsr->lastSentSliceCount);

    retVal = clReleaseCommandQueue(cmdQ);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

HWTEST2_F(ClCreateCommandQueueWithPropertiesLinux, givenSameSliceCountAsRecentlySetWhenCreateCommandQueueThenSetQueueSliceCountNotCalled, IsHeapfulRequired) {
    uint64_t newSliceCount = 1;
    size_t maxSliceCount;

    clGetDeviceInfo(clDevice, CL_DEVICE_SLICE_COUNT_INTEL, sizeof(size_t), &maxSliceCount, nullptr);
    if (maxSliceCount > 1) {
        newSliceCount = maxSliceCount - 1;
    }

    cl_queue_properties properties[] = {CL_QUEUE_SLICE_COUNT_INTEL, newSliceCount, 0};

    auto mockCsr = new TestedDrmCommandStreamReceiver<FamilyType>(*mdevice->executionEnvironment, rootDeviceIndex, 1);
    mockCsr->flushInternalCallBase = false;
    mdevice->resetCommandStreamReceiver(mockCsr);

    cl_command_queue cmdQ = clCreateCommandQueueWithProperties(context.get(), clDevice, properties, &retVal);

    ASSERT_NE(nullptr, cmdQ);
    ASSERT_EQ(CL_SUCCESS, retVal);

    auto commandQueue = castToObject<CommandQueueHw<FamilyType>>(cmdQ);
    auto &commandStream = commandQueue->getCS(1024u);

    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    dispatchFlags.sliceCount = commandQueue->getSliceCount();
    dispatchFlags.implicitFlush = true;

    mockCsr->lastSentSliceCount = newSliceCount;
    mockCsr->flushTask(commandStream,
                       0u,
                       &dsh,
                       &ioh,
                       &ssh,
                       taskLevel,
                       dispatchFlags,
                       mdevice->getDevice());
    auto expectedSliceMask = drm->getSliceMask(newSliceCount);
    EXPECT_NE(expectedSliceMask, drm->storedParamSseu);
    GemContextParamSseu sseu = {};
    EXPECT_EQ(0, drm->getQueueSliceCount(&sseu));
    EXPECT_NE(expectedSliceMask, sseu.sliceMask);

    retVal = clReleaseCommandQueue(cmdQ);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

HWTEST2_F(ClCreateCommandQueueWithPropertiesLinux, givenPropertiesWithClQueueSliceCountWhenCreateCommandQueueThenSetReturnFalseAndLastSliceCountNotModify, IsHeapfulRequired) {
    uint64_t newSliceCount = 1;
    size_t maxSliceCount;
    clGetDeviceInfo(clDevice, CL_DEVICE_SLICE_COUNT_INTEL, sizeof(size_t), &maxSliceCount, nullptr);
    if (maxSliceCount > 1) {
        newSliceCount = maxSliceCount - 1;
    }

    cl_queue_properties properties[] = {CL_QUEUE_SLICE_COUNT_INTEL, newSliceCount, 0};

    auto mockCsr = new TestedDrmCommandStreamReceiver<FamilyType>(*mdevice->executionEnvironment, rootDeviceIndex, 1);
    mockCsr->flushInternalCallBase = false;
    mdevice->resetCommandStreamReceiver(mockCsr);

    cl_command_queue cmdQ = clCreateCommandQueueWithProperties(context.get(), clDevice, properties, &retVal);

    ASSERT_NE(nullptr, cmdQ);
    ASSERT_EQ(CL_SUCCESS, retVal);

    auto commandQueue = castToObject<CommandQueueHw<FamilyType>>(cmdQ);
    auto &commandStream = commandQueue->getCS(1024u);

    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    dispatchFlags.sliceCount = commandQueue->getSliceCount();
    drm->storedRetValForSetSSEU = -1;

    auto lastSliceCountBeforeFlushTask = mockCsr->lastSentSliceCount;
    mockCsr->flushTask(commandStream,
                       0u,
                       &dsh,
                       &ioh,
                       &ssh,
                       taskLevel,
                       dispatchFlags,
                       mdevice->getDevice());

    EXPECT_NE(newSliceCount, mockCsr->lastSentSliceCount);
    EXPECT_EQ(lastSliceCountBeforeFlushTask, mockCsr->lastSentSliceCount);

    retVal = clReleaseCommandQueue(cmdQ);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

} // namespace ULT
