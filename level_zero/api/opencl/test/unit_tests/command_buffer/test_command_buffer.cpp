/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/source/api/leo_api.h"
#include "level_zero/api/opencl/source/cl_device/leo_cl_device.h"
#include "level_zero/api/opencl/source/command_buffer/leo_command_buffer.h"
#include "level_zero/api/opencl/source/command_queue/leo_command_queue.h"
#include "level_zero/api/opencl/source/helpers/leo_base_object.h"
#include "level_zero/api/opencl/test/common/fixtures/leo_capture_fixture.h"
#include "level_zero/api/opencl/test/common/fixtures/ocl_fixture.h"

#include "CL/cl.h"

#include <cstring>
#include <limits>
#include <string>
#include <vector>

namespace NEO {
namespace LEO {
namespace ult {

template <int32_t enableClKhrCommandBuffer>
struct LeoCommandBufferFixtureBase : public Test<OclFixture> {
    void SetUp() override {
        debugManager.flags.EnableClKhrCommandBuffer.set(enableClKhrCommandBuffer);
        Test<OclFixture>::SetUp();

        clDevice = platform->getDevices()[0].get();
        clDeviceId = clDevice;

        cl_int errcode = CL_SUCCESS;
        clContext = clCreateContext(nullptr, 1, &clDeviceId, nullptr, nullptr, &errcode);
        ASSERT_EQ(CL_SUCCESS, errcode);
        ASSERT_NE(nullptr, clContext);

        clQueue = clCreateCommandQueue(clContext, clDeviceId, 0, &errcode);
        ASSERT_EQ(CL_SUCCESS, errcode);
        ASSERT_NE(nullptr, clQueue);
    }

    void TearDown() override {
        if (clQueue != nullptr) {
            clReleaseCommandQueue(clQueue);
        }
        if (clContext != nullptr) {
            clReleaseContext(clContext);
        }
        Test<OclFixture>::TearDown();
    }

    cl_command_buffer_khr createCommandBuffer(const cl_command_buffer_properties_khr *properties, cl_int &errcode) {
        cl_command_queue queues[] = {clQueue};
        return clCreateCommandBufferKHR(1, queues, properties, &errcode);
    }

    std::string getDeviceExtensions() {
        size_t extensionsSize = 0;
        EXPECT_EQ(CL_SUCCESS, clGetDeviceInfo(clDeviceId, CL_DEVICE_EXTENSIONS, 0, nullptr, &extensionsSize));
        std::vector<char> extensions(extensionsSize);
        EXPECT_EQ(CL_SUCCESS, clGetDeviceInfo(clDeviceId, CL_DEVICE_EXTENSIONS, extensionsSize, extensions.data(), nullptr));
        return std::string(extensions.data());
    }

    DebugManagerStateRestore debugRestorer;
    ClDevice *clDevice = nullptr;
    cl_device_id clDeviceId = nullptr;
    cl_context clContext = nullptr;
    cl_command_queue clQueue = nullptr;
};

using LeoCommandBufferTest = LeoCommandBufferFixtureBase<1>;
using LeoCommandBufferDisabledTest = LeoCommandBufferFixtureBase<-1>;

struct LeoCommandBufferCaptureTest : public Test<LeoCaptureFixture> {
    void SetUp() override {
        debugManager.flags.EnableClKhrCommandBuffer.set(1);
        Test<LeoCaptureFixture>::SetUp();
    }

    DebugManagerStateRestore debugRestorer;
};

// Extension advertisement gate

TEST_F(LeoCommandBufferTest, givenCommandBufferEnabledWhenGettingDeviceExtensionsThenCommandBufferExtensionIsReported) {
    EXPECT_NE(std::string::npos, getDeviceExtensions().find("cl_khr_command_buffer"));
}

TEST_F(LeoCommandBufferDisabledTest, givenCommandBufferDisabledWhenGettingDeviceExtensionsThenCommandBufferExtensionIsNotReported) {
    EXPECT_EQ(std::string::npos, getDeviceExtensions().find("cl_khr_command_buffer"));
}

TEST_F(LeoCommandBufferTest, givenCommandBufferEnabledWhenGettingExtensionsWithVersionThenProvisionalVersionIsReported) {
    size_t size = 0;
    ASSERT_EQ(CL_SUCCESS, clGetDeviceInfo(clDeviceId, CL_DEVICE_EXTENSIONS_WITH_VERSION, 0, nullptr, &size));
    std::vector<cl_name_version> extensions(size / sizeof(cl_name_version));
    ASSERT_EQ(CL_SUCCESS, clGetDeviceInfo(clDeviceId, CL_DEVICE_EXTENSIONS_WITH_VERSION, size, extensions.data(), nullptr));

    bool found = false;
    for (const auto &extension : extensions) {
        if (strcmp(extension.name, CL_KHR_COMMAND_BUFFER_EXTENSION_NAME) == 0) {
            found = true;
            EXPECT_EQ(static_cast<cl_version>(CL_KHR_COMMAND_BUFFER_EXTENSION_VERSION), extension.version);
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(LeoCommandBufferTest, givenCommandBufferEnabledWhenQueryingCommandBufferDeviceInfoThenNoCapabilityIsClaimed) {
    const cl_device_info params[] = {CL_DEVICE_COMMAND_BUFFER_CAPABILITIES_KHR,
                                     CL_DEVICE_COMMAND_BUFFER_SUPPORTED_QUEUE_PROPERTIES_KHR,
                                     CL_DEVICE_COMMAND_BUFFER_REQUIRED_QUEUE_PROPERTIES_KHR};
    for (auto param : params) {
        cl_bitfield value = std::numeric_limits<cl_bitfield>::max();
        size_t sizeRet = 0;
        EXPECT_EQ(CL_SUCCESS, clGetDeviceInfo(clDeviceId, param, sizeof(value), &value, &sizeRet));
        EXPECT_EQ(sizeof(cl_bitfield), sizeRet);
        EXPECT_EQ(0u, value);
    }
}

TEST_F(LeoCommandBufferDisabledTest, givenCommandBufferDisabledWhenQueryingCommandBufferDeviceInfoThenInvalidValueIsReturned) {
    const cl_device_info params[] = {CL_DEVICE_COMMAND_BUFFER_CAPABILITIES_KHR,
                                     CL_DEVICE_COMMAND_BUFFER_SUPPORTED_QUEUE_PROPERTIES_KHR,
                                     CL_DEVICE_COMMAND_BUFFER_REQUIRED_QUEUE_PROPERTIES_KHR};
    for (auto param : params) {
        cl_bitfield value = 0;
        EXPECT_EQ(CL_INVALID_VALUE, clGetDeviceInfo(clDeviceId, param, sizeof(value), &value, nullptr));
    }
}

TEST_F(LeoCommandBufferDisabledTest, givenCommandBufferDisabledWhenCreatingCommandBufferThenInvalidOperationIsReturned) {
    cl_int errcode = CL_SUCCESS;
    EXPECT_EQ(nullptr, createCommandBuffer(nullptr, errcode));
    EXPECT_EQ(CL_INVALID_OPERATION, errcode);
}

TEST_F(LeoCommandBufferTest, givenCommandBufferEnabledWhenGettingExtensionFunctionAddressThenCommandBufferEntryPointsAreReturned) {
    EXPECT_NE(nullptr, clGetExtensionFunctionAddress("clCreateCommandBufferKHR"));
    EXPECT_NE(nullptr, clGetExtensionFunctionAddress("clFinalizeCommandBufferKHR"));
    EXPECT_NE(nullptr, clGetExtensionFunctionAddress("clRetainCommandBufferKHR"));
    EXPECT_NE(nullptr, clGetExtensionFunctionAddress("clReleaseCommandBufferKHR"));
    EXPECT_NE(nullptr, clGetExtensionFunctionAddress("clEnqueueCommandBufferKHR"));
    EXPECT_NE(nullptr, clGetExtensionFunctionAddress("clGetCommandBufferInfoKHR"));
}

TEST_F(LeoCommandBufferDisabledTest, givenCommandBufferDisabledWhenGettingExtensionFunctionAddressThenNoEntryPointIsReturned) {
    EXPECT_EQ(nullptr, clGetExtensionFunctionAddress("clCreateCommandBufferKHR"));
    EXPECT_EQ(nullptr, clGetExtensionFunctionAddress("clFinalizeCommandBufferKHR"));
    EXPECT_EQ(nullptr, clGetExtensionFunctionAddress("clRetainCommandBufferKHR"));
    EXPECT_EQ(nullptr, clGetExtensionFunctionAddress("clReleaseCommandBufferKHR"));
    EXPECT_EQ(nullptr, clGetExtensionFunctionAddress("clEnqueueCommandBufferKHR"));
    EXPECT_EQ(nullptr, clGetExtensionFunctionAddress("clGetCommandBufferInfoKHR"));
}

// clCreateCommandBufferKHR

TEST_F(LeoCommandBufferTest, givenValidQueueWhenCreatingCommandBufferThenRecordingBufferIsReturned) {
    cl_int errcode = CL_INVALID_VALUE;
    auto commandBuffer = createCommandBuffer(nullptr, errcode);
    ASSERT_NE(nullptr, commandBuffer);
    EXPECT_EQ(CL_SUCCESS, errcode);

    auto pCommandBuffer = castToObject<CommandBuffer>(commandBuffer);
    ASSERT_NE(nullptr, pCommandBuffer);
    EXPECT_FALSE(pCommandBuffer->isFinalized());
    EXPECT_EQ(1, pCommandBuffer->getReference());
    EXPECT_EQ(castToObject<CommandQueue>(clQueue), pCommandBuffer->getCommandQueue());
    EXPECT_NE(nullptr, pCommandBuffer->getL0Handle());

    EXPECT_EQ(CL_SUCCESS, clReleaseCommandBufferKHR(commandBuffer));
}

TEST_F(LeoCommandBufferTest, givenInOrderQueueWhenCreatingCommandBufferThenRecordedListIsInOrder) {
    cl_int errcode = CL_SUCCESS;
    auto commandBuffer = createCommandBuffer(nullptr, errcode);
    ASSERT_NE(nullptr, commandBuffer);

    auto pCommandBuffer = castToObject<CommandBuffer>(commandBuffer);
    ASSERT_FALSE(castToObject<CommandQueue>(clQueue)->isOutOfOrder());
    EXPECT_TRUE(L0::CommandList::fromHandle(pCommandBuffer->getL0Handle())->isInOrderExecutionEnabled());

    EXPECT_EQ(CL_SUCCESS, clReleaseCommandBufferKHR(commandBuffer));
}

TEST_F(LeoCommandBufferTest, givenOutOfOrderQueueWhenCreatingCommandBufferThenRecordedListIsNotInOrder) {
    cl_int errcode = CL_SUCCESS;
    cl_queue_properties properties[] = {CL_QUEUE_PROPERTIES, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, 0};
    auto oooQueue = clCreateCommandQueueWithProperties(clContext, clDeviceId, properties, &errcode);
    ASSERT_NE(nullptr, oooQueue);
    ASSERT_TRUE(castToObject<CommandQueue>(oooQueue)->isOutOfOrder());

    cl_command_queue queues[] = {oooQueue};
    auto commandBuffer = clCreateCommandBufferKHR(1, queues, nullptr, &errcode);
    ASSERT_NE(nullptr, commandBuffer);

    auto pCommandBuffer = castToObject<CommandBuffer>(commandBuffer);
    EXPECT_FALSE(L0::CommandList::fromHandle(pCommandBuffer->getL0Handle())->isInOrderExecutionEnabled());

    EXPECT_EQ(CL_SUCCESS, clReleaseCommandBufferKHR(commandBuffer));
    EXPECT_EQ(CL_SUCCESS, clReleaseCommandQueue(oooQueue));
}

TEST_F(LeoCommandBufferTest, givenUnsupportedNumQueuesWhenCreatingCommandBufferThenInvalidValueIsReturned) {
    cl_command_queue queues[] = {clQueue, clQueue};

    cl_int errcode = CL_SUCCESS;
    EXPECT_EQ(nullptr, clCreateCommandBufferKHR(0, queues, nullptr, &errcode));
    EXPECT_EQ(CL_INVALID_VALUE, errcode);

    errcode = CL_SUCCESS;
    EXPECT_EQ(nullptr, clCreateCommandBufferKHR(2, queues, nullptr, &errcode));
    EXPECT_EQ(CL_INVALID_VALUE, errcode);
}

TEST_F(LeoCommandBufferTest, givenNullQueuesWhenCreatingCommandBufferThenInvalidValueIsReturned) {
    cl_int errcode = CL_SUCCESS;
    EXPECT_EQ(nullptr, clCreateCommandBufferKHR(1, nullptr, nullptr, &errcode));
    EXPECT_EQ(CL_INVALID_VALUE, errcode);
}

TEST_F(LeoCommandBufferTest, givenInvalidQueueWhenCreatingCommandBufferThenInvalidCommandQueueIsReturned) {
    cl_command_queue queues[] = {reinterpret_cast<cl_command_queue>(clContext)};

    cl_int errcode = CL_SUCCESS;
    EXPECT_EQ(nullptr, clCreateCommandBufferKHR(1, queues, nullptr, &errcode));
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, errcode);
}

TEST_F(LeoCommandBufferTest, givenZeroFlagsWhenCreatingCommandBufferThenSuccessIsReturned) {
    cl_command_buffer_properties_khr properties[] = {CL_COMMAND_BUFFER_FLAGS_KHR, 0, 0};

    cl_int errcode = CL_INVALID_VALUE;
    auto commandBuffer = createCommandBuffer(properties, errcode);
    ASSERT_NE(nullptr, commandBuffer);
    EXPECT_EQ(CL_SUCCESS, errcode);

    EXPECT_EQ(CL_SUCCESS, clReleaseCommandBufferKHR(commandBuffer));
}

TEST_F(LeoCommandBufferTest, givenUnknownPropertyNameWhenCreatingCommandBufferThenInvalidPropertyIsReturned) {
    cl_command_buffer_properties_khr properties[] = {CL_COMMAND_BUFFER_QUEUES_KHR, 0, 0};

    cl_int errcode = CL_SUCCESS;
    EXPECT_EQ(nullptr, createCommandBuffer(properties, errcode));
    EXPECT_EQ(CL_INVALID_PROPERTY, errcode);
}

TEST_F(LeoCommandBufferTest, givenUnsupportedFlagValueWhenCreatingCommandBufferThenInvalidPropertyIsReturned) {
    const cl_command_buffer_flags_khr flags[] = {CL_COMMAND_BUFFER_SIMULTANEOUS_USE_KHR,
                                                 CL_COMMAND_BUFFER_MUTABLE_KHR};
    for (auto flag : flags) {
        cl_command_buffer_properties_khr properties[] = {CL_COMMAND_BUFFER_FLAGS_KHR, flag, 0};

        cl_int errcode = CL_SUCCESS;
        EXPECT_EQ(nullptr, createCommandBuffer(properties, errcode));
        EXPECT_EQ(CL_INVALID_PROPERTY, errcode);
    }
}

TEST_F(LeoCommandBufferTest, givenRepeatedPropertyNameWhenCreatingCommandBufferThenInvalidPropertyIsReturned) {
    cl_command_buffer_properties_khr properties[] = {CL_COMMAND_BUFFER_FLAGS_KHR, 0,
                                                     CL_COMMAND_BUFFER_FLAGS_KHR, 0, 0};

    cl_int errcode = CL_SUCCESS;
    EXPECT_EQ(nullptr, createCommandBuffer(properties, errcode));
    EXPECT_EQ(CL_INVALID_PROPERTY, errcode);
}

// clFinalizeCommandBufferKHR

TEST_F(LeoCommandBufferTest, givenRecordingCommandBufferWhenFinalizedThenStateBecomesExecutable) {
    cl_int errcode = CL_SUCCESS;
    auto commandBuffer = createCommandBuffer(nullptr, errcode);
    ASSERT_NE(nullptr, commandBuffer);

    EXPECT_EQ(CL_SUCCESS, clFinalizeCommandBufferKHR(commandBuffer));
    EXPECT_TRUE(castToObject<CommandBuffer>(commandBuffer)->isFinalized());

    EXPECT_EQ(CL_SUCCESS, clReleaseCommandBufferKHR(commandBuffer));
}

TEST_F(LeoCommandBufferTest, givenFinalizedCommandBufferWhenFinalizedAgainThenInvalidOperationIsReturned) {
    cl_int errcode = CL_SUCCESS;
    auto commandBuffer = createCommandBuffer(nullptr, errcode);
    ASSERT_NE(nullptr, commandBuffer);

    EXPECT_EQ(CL_SUCCESS, clFinalizeCommandBufferKHR(commandBuffer));
    EXPECT_EQ(CL_INVALID_OPERATION, clFinalizeCommandBufferKHR(commandBuffer));

    EXPECT_EQ(CL_SUCCESS, clReleaseCommandBufferKHR(commandBuffer));
}

TEST_F(LeoCommandBufferTest, givenInvalidCommandBufferWhenCallingEntryPointsThenInvalidCommandBufferIsReturned) {
    auto invalid = reinterpret_cast<cl_command_buffer_khr>(clContext);

    EXPECT_EQ(CL_INVALID_COMMAND_BUFFER_KHR, clFinalizeCommandBufferKHR(invalid));
    EXPECT_EQ(CL_INVALID_COMMAND_BUFFER_KHR, clRetainCommandBufferKHR(invalid));
    EXPECT_EQ(CL_INVALID_COMMAND_BUFFER_KHR, clReleaseCommandBufferKHR(invalid));
    EXPECT_EQ(CL_INVALID_COMMAND_BUFFER_KHR, clEnqueueCommandBufferKHR(0, nullptr, invalid, 0, nullptr, nullptr));
    EXPECT_EQ(CL_INVALID_COMMAND_BUFFER_KHR, clGetCommandBufferInfoKHR(invalid, CL_COMMAND_BUFFER_STATE_KHR, 0, nullptr, nullptr));
}

// Reference counting

TEST_F(LeoCommandBufferTest, givenCommandBufferWhenRetainedAndReleasedThenReferenceCountIsUpdated) {
    cl_int errcode = CL_SUCCESS;
    auto commandBuffer = createCommandBuffer(nullptr, errcode);
    ASSERT_NE(nullptr, commandBuffer);

    auto pCommandBuffer = castToObject<CommandBuffer>(commandBuffer);
    EXPECT_EQ(1, pCommandBuffer->getReference());

    EXPECT_EQ(CL_SUCCESS, clRetainCommandBufferKHR(commandBuffer));
    EXPECT_EQ(2, pCommandBuffer->getReference());

    cl_uint refCount = 0;
    EXPECT_EQ(CL_SUCCESS, clGetCommandBufferInfoKHR(commandBuffer, CL_COMMAND_BUFFER_REFERENCE_COUNT_KHR,
                                                    sizeof(refCount), &refCount, nullptr));
    EXPECT_EQ(2u, refCount);

    EXPECT_EQ(CL_SUCCESS, clReleaseCommandBufferKHR(commandBuffer));
    EXPECT_EQ(1, pCommandBuffer->getReference());

    EXPECT_EQ(CL_SUCCESS, clReleaseCommandBufferKHR(commandBuffer));
}

// clGetCommandBufferInfoKHR

TEST_F(LeoCommandBufferTest, givenCommandBufferWhenQueryingInfoThenExpectedValuesAreReturned) {
    cl_int errcode = CL_SUCCESS;
    cl_command_buffer_properties_khr properties[] = {CL_COMMAND_BUFFER_FLAGS_KHR, 0, 0};
    auto commandBuffer = createCommandBuffer(properties, errcode);
    ASSERT_NE(nullptr, commandBuffer);

    cl_uint numQueues = 0;
    EXPECT_EQ(CL_SUCCESS, clGetCommandBufferInfoKHR(commandBuffer, CL_COMMAND_BUFFER_NUM_QUEUES_KHR,
                                                    sizeof(numQueues), &numQueues, nullptr));
    EXPECT_EQ(1u, numQueues);

    cl_command_queue queue = nullptr;
    EXPECT_EQ(CL_SUCCESS, clGetCommandBufferInfoKHR(commandBuffer, CL_COMMAND_BUFFER_QUEUES_KHR,
                                                    sizeof(queue), &queue, nullptr));
    EXPECT_EQ(clQueue, queue);

    cl_context context = nullptr;
    EXPECT_EQ(CL_SUCCESS, clGetCommandBufferInfoKHR(commandBuffer, CL_COMMAND_BUFFER_CONTEXT_KHR,
                                                    sizeof(context), &context, nullptr));
    EXPECT_EQ(clContext, context);

    cl_command_buffer_state_khr state = CL_COMMAND_BUFFER_STATE_EXECUTABLE_KHR;
    EXPECT_EQ(CL_SUCCESS, clGetCommandBufferInfoKHR(commandBuffer, CL_COMMAND_BUFFER_STATE_KHR,
                                                    sizeof(state), &state, nullptr));
    EXPECT_EQ(static_cast<cl_command_buffer_state_khr>(CL_COMMAND_BUFFER_STATE_RECORDING_KHR), state);

    size_t propertiesSize = 0;
    EXPECT_EQ(CL_SUCCESS, clGetCommandBufferInfoKHR(commandBuffer, CL_COMMAND_BUFFER_PROPERTIES_ARRAY_KHR,
                                                    0, nullptr, &propertiesSize));
    EXPECT_EQ(sizeof(properties), propertiesSize);
    std::vector<cl_command_buffer_properties_khr> reported(propertiesSize / sizeof(cl_command_buffer_properties_khr));
    EXPECT_EQ(CL_SUCCESS, clGetCommandBufferInfoKHR(commandBuffer, CL_COMMAND_BUFFER_PROPERTIES_ARRAY_KHR,
                                                    propertiesSize, reported.data(), nullptr));
    EXPECT_EQ(0, memcmp(properties, reported.data(), propertiesSize));

    EXPECT_EQ(CL_SUCCESS, clReleaseCommandBufferKHR(commandBuffer));
}

TEST_F(LeoCommandBufferTest, givenUnknownParamNameWhenQueryingInfoThenInvalidValueIsReturned) {
    cl_int errcode = CL_SUCCESS;
    auto commandBuffer = createCommandBuffer(nullptr, errcode);
    ASSERT_NE(nullptr, commandBuffer);

    cl_uint value = 0;
    EXPECT_EQ(CL_INVALID_VALUE, clGetCommandBufferInfoKHR(commandBuffer, CL_COMMAND_BUFFER_FLAGS_KHR,
                                                          sizeof(value), &value, nullptr));

    EXPECT_EQ(CL_SUCCESS, clReleaseCommandBufferKHR(commandBuffer));
}

// clEnqueueCommandBufferKHR

TEST_F(LeoCommandBufferTest, givenRecordingCommandBufferWhenEnqueuedThenInvalidOperationIsReturned) {
    cl_int errcode = CL_SUCCESS;
    auto commandBuffer = createCommandBuffer(nullptr, errcode);
    ASSERT_NE(nullptr, commandBuffer);

    EXPECT_EQ(CL_INVALID_OPERATION, clEnqueueCommandBufferKHR(0, nullptr, commandBuffer, 0, nullptr, nullptr));

    EXPECT_EQ(CL_SUCCESS, clReleaseCommandBufferKHR(commandBuffer));
}

TEST_F(LeoCommandBufferTest, givenInconsistentQueueCountWhenEnqueuedThenInvalidValueIsReturned) {
    cl_int errcode = CL_SUCCESS;
    auto commandBuffer = createCommandBuffer(nullptr, errcode);
    ASSERT_NE(nullptr, commandBuffer);
    ASSERT_EQ(CL_SUCCESS, clFinalizeCommandBufferKHR(commandBuffer));

    cl_command_queue queues[] = {clQueue, clQueue};
    EXPECT_EQ(CL_INVALID_VALUE, clEnqueueCommandBufferKHR(2, queues, commandBuffer, 0, nullptr, nullptr));
    EXPECT_EQ(CL_INVALID_VALUE, clEnqueueCommandBufferKHR(1, nullptr, commandBuffer, 0, nullptr, nullptr));
    EXPECT_EQ(CL_INVALID_VALUE, clEnqueueCommandBufferKHR(0, queues, commandBuffer, 0, nullptr, nullptr));

    EXPECT_EQ(CL_SUCCESS, clReleaseCommandBufferKHR(commandBuffer));
}

TEST_F(LeoCommandBufferTest, givenInvalidQueueWhenEnqueuedThenInvalidCommandQueueIsReturned) {
    cl_int errcode = CL_SUCCESS;
    auto commandBuffer = createCommandBuffer(nullptr, errcode);
    ASSERT_NE(nullptr, commandBuffer);
    ASSERT_EQ(CL_SUCCESS, clFinalizeCommandBufferKHR(commandBuffer));

    cl_command_queue nullQueue[] = {nullptr};
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, clEnqueueCommandBufferKHR(1, nullQueue, commandBuffer, 0, nullptr, nullptr));

    cl_command_queue notAQueue[] = {reinterpret_cast<cl_command_queue>(clContext)};
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, clEnqueueCommandBufferKHR(1, notAQueue, commandBuffer, 0, nullptr, nullptr));

    EXPECT_EQ(CL_SUCCESS, clReleaseCommandBufferKHR(commandBuffer));
}

TEST_F(LeoCommandBufferTest, givenQueueFromAnotherContextWhenEnqueuedThenInvalidContextIsReturned) {
    cl_int errcode = CL_SUCCESS;
    auto commandBuffer = createCommandBuffer(nullptr, errcode);
    ASSERT_NE(nullptr, commandBuffer);
    ASSERT_EQ(CL_SUCCESS, clFinalizeCommandBufferKHR(commandBuffer));

    auto otherContext = clCreateContext(nullptr, 1, &clDeviceId, nullptr, nullptr, &errcode);
    ASSERT_NE(nullptr, otherContext);
    auto otherQueue = clCreateCommandQueue(otherContext, clDeviceId, 0, &errcode);
    ASSERT_NE(nullptr, otherQueue);

    cl_command_queue queues[] = {otherQueue};
    EXPECT_EQ(CL_INVALID_CONTEXT, clEnqueueCommandBufferKHR(1, queues, commandBuffer, 0, nullptr, nullptr));

    EXPECT_EQ(CL_SUCCESS, clReleaseCommandQueue(otherQueue));
    EXPECT_EQ(CL_SUCCESS, clReleaseContext(otherContext));
    EXPECT_EQ(CL_SUCCESS, clReleaseCommandBufferKHR(commandBuffer));
}

TEST_F(LeoCommandBufferCaptureTest, givenFinalizedCommandBufferWhenEnqueuedThenRecordedListIsAppendedToQueue) {
    cl_command_queue clQueue = commandQueue;
    cl_command_queue queues[] = {clQueue};

    cl_int errcode = CL_SUCCESS;
    auto commandBuffer = clCreateCommandBufferKHR(1, queues, nullptr, &errcode);
    ASSERT_NE(nullptr, commandBuffer);
    ASSERT_EQ(CL_SUCCESS, errcode);
    ASSERT_EQ(CL_SUCCESS, clFinalizeCommandBufferKHR(commandBuffer));

    capturingCmdList.clearCaptures();

    EXPECT_EQ(CL_SUCCESS, clEnqueueCommandBufferKHR(1, queues, commandBuffer, 0, nullptr, nullptr));

    ASSERT_EQ(1u, capturingCmdList.appendCommandListsArgs.count());
    const auto &args = capturingCmdList.appendCommandListsArgs[0];
    ASSERT_EQ(1u, args.commandLists.size());
    EXPECT_EQ(castToObject<CommandBuffer>(commandBuffer)->getL0Handle(), args.commandLists[0]);
    EXPECT_TRUE(args.waitEvents.empty());

    EXPECT_EQ(CL_SUCCESS, clReleaseCommandBufferKHR(commandBuffer));
}

TEST_F(LeoCommandBufferCaptureTest, givenAnotherQueueWhenEnqueuedThenRecordedListIsAppendedToThatQueue) {
    cl_command_queue clQueue = commandQueue;
    cl_command_queue queues[] = {clQueue};

    cl_int errcode = CL_SUCCESS;
    auto commandBuffer = clCreateCommandBufferKHR(1, queues, nullptr, &errcode);
    ASSERT_NE(nullptr, commandBuffer);
    ASSERT_EQ(CL_SUCCESS, clFinalizeCommandBufferKHR(commandBuffer));

    CapturingCommandList otherCmdList{};
    auto otherQueue = new CommandQueue(context, clDevice, nullptr, otherCmdList.toHandle());
    cl_command_queue otherQueues[] = {otherQueue};

    capturingCmdList.clearCaptures();

    EXPECT_EQ(CL_SUCCESS, clEnqueueCommandBufferKHR(1, otherQueues, commandBuffer, 0, nullptr, nullptr));

    EXPECT_EQ(0u, capturingCmdList.appendCommandListsArgs.count());
    ASSERT_EQ(1u, otherCmdList.appendCommandListsArgs.count());
    const auto &args = otherCmdList.appendCommandListsArgs[0];
    ASSERT_EQ(1u, args.commandLists.size());
    EXPECT_EQ(castToObject<CommandBuffer>(commandBuffer)->getL0Handle(), args.commandLists[0]);

    otherQueue->decRefApi();
    EXPECT_EQ(CL_SUCCESS, clReleaseCommandBufferKHR(commandBuffer));
}

TEST_F(LeoCommandBufferCaptureTest, givenCommandListCloseFailureWhenFinalizingThenErrorIsPropagatedAndStateIsUnchanged) {
    CapturingCommandList bufferCmdList{};
    bufferCmdList.closeResult = ZE_RESULT_ERROR_INVALID_ARGUMENT;

    auto commandBuffer = new CommandBuffer(context, commandQueue, bufferCmdList.toHandle(), nullptr);

    EXPECT_EQ(CL_INVALID_VALUE, commandBuffer->finalize());
    EXPECT_FALSE(commandBuffer->isFinalized());

    commandBuffer->decRefApi();
}

TEST_F(LeoCommandBufferCaptureTest, givenAppendCommandListsFailureWhenEnqueuedThenErrorIsPropagated) {
    cl_command_queue clQueue = commandQueue;
    cl_command_queue queues[] = {clQueue};

    cl_int errcode = CL_SUCCESS;
    auto commandBuffer = clCreateCommandBufferKHR(1, queues, nullptr, &errcode);
    ASSERT_NE(nullptr, commandBuffer);
    ASSERT_EQ(CL_SUCCESS, clFinalizeCommandBufferKHR(commandBuffer));

    capturingCmdList.appendCommandListsResult = ZE_RESULT_ERROR_INVALID_ARGUMENT;
    EXPECT_EQ(CL_INVALID_VALUE, clEnqueueCommandBufferKHR(1, queues, commandBuffer, 0, nullptr, nullptr));

    EXPECT_EQ(CL_SUCCESS, clReleaseCommandBufferKHR(commandBuffer));
}

} // namespace ult
} // namespace LEO
} // namespace NEO
