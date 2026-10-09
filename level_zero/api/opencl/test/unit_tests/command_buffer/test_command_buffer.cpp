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
#include "level_zero/api/opencl/source/context/leo_context.h"
#include "level_zero/api/opencl/source/helpers/leo_base_object.h"
#include "level_zero/api/opencl/source/kernel/leo_kernel.h"
#include "level_zero/api/opencl/source/mem_obj/leo_buffer.h"
#include "level_zero/api/opencl/source/program/leo_program.h"
#include "level_zero/api/opencl/test/common/fixtures/command_list_create_hook.h"
#include "level_zero/api/opencl/test/common/fixtures/leo_capture_fixture.h"
#include "level_zero/api/opencl/test/common/fixtures/ocl_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_kernel.h"
#include "level_zero/core/test/unit_tests/mocks/mock_module.h"

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
    EXPECT_NE(nullptr, clGetExtensionFunctionAddress("clCommandBarrierWithWaitListKHR"));
    EXPECT_NE(nullptr, clGetExtensionFunctionAddress("clCommandCopyBufferKHR"));
    EXPECT_NE(nullptr, clGetExtensionFunctionAddress("clCommandCopyBufferRectKHR"));
    EXPECT_NE(nullptr, clGetExtensionFunctionAddress("clCommandFillBufferKHR"));
    EXPECT_NE(nullptr, clGetExtensionFunctionAddress("clCommandSVMMemcpyKHR"));
    EXPECT_NE(nullptr, clGetExtensionFunctionAddress("clCommandSVMMemFillKHR"));
    EXPECT_NE(nullptr, clGetExtensionFunctionAddress("clCommandNDRangeKernelKHR"));
}

TEST_F(LeoCommandBufferDisabledTest, givenCommandBufferDisabledWhenGettingExtensionFunctionAddressThenNoEntryPointIsReturned) {
    EXPECT_EQ(nullptr, clGetExtensionFunctionAddress("clCreateCommandBufferKHR"));
    EXPECT_EQ(nullptr, clGetExtensionFunctionAddress("clFinalizeCommandBufferKHR"));
    EXPECT_EQ(nullptr, clGetExtensionFunctionAddress("clRetainCommandBufferKHR"));
    EXPECT_EQ(nullptr, clGetExtensionFunctionAddress("clReleaseCommandBufferKHR"));
    EXPECT_EQ(nullptr, clGetExtensionFunctionAddress("clEnqueueCommandBufferKHR"));
    EXPECT_EQ(nullptr, clGetExtensionFunctionAddress("clGetCommandBufferInfoKHR"));
    EXPECT_EQ(nullptr, clGetExtensionFunctionAddress("clCommandBarrierWithWaitListKHR"));
    EXPECT_EQ(nullptr, clGetExtensionFunctionAddress("clCommandCopyBufferKHR"));
    EXPECT_EQ(nullptr, clGetExtensionFunctionAddress("clCommandCopyBufferRectKHR"));
    EXPECT_EQ(nullptr, clGetExtensionFunctionAddress("clCommandFillBufferKHR"));
    EXPECT_EQ(nullptr, clGetExtensionFunctionAddress("clCommandSVMMemcpyKHR"));
    EXPECT_EQ(nullptr, clGetExtensionFunctionAddress("clCommandSVMMemFillKHR"));
    EXPECT_EQ(nullptr, clGetExtensionFunctionAddress("clCommandNDRangeKernelKHR"));
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

TEST_F(LeoCommandBufferTest, givenOutOfOrderQueueWhenCreatingCommandBufferThenRecordedListIsInOrder) {
    cl_int errcode = CL_SUCCESS;
    cl_queue_properties properties[] = {CL_QUEUE_PROPERTIES, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, 0};
    auto oooQueue = clCreateCommandQueueWithProperties(clContext, clDeviceId, properties, &errcode);
    ASSERT_NE(nullptr, oooQueue);
    ASSERT_TRUE(castToObject<CommandQueue>(oooQueue)->isOutOfOrder());

    cl_command_queue queues[] = {oooQueue};
    auto commandBuffer = clCreateCommandBufferKHR(1, queues, nullptr, &errcode);
    ASSERT_NE(nullptr, commandBuffer);

    auto pCommandBuffer = castToObject<CommandBuffer>(commandBuffer);
    EXPECT_TRUE(L0::CommandList::fromHandle(pCommandBuffer->getL0Handle())->isInOrderExecutionEnabled());

    EXPECT_EQ(CL_SUCCESS, clReleaseCommandBufferKHR(commandBuffer));
    EXPECT_EQ(CL_SUCCESS, clReleaseCommandQueue(oooQueue));
}

TEST_F(LeoCommandBufferTest, givenValidQueueWhenCreatingCommandBufferThenRecordedListUsesPatchPreamble) {
    cl_int errcode = CL_SUCCESS;
    auto commandBuffer = createCommandBuffer(nullptr, errcode);
    ASSERT_NE(nullptr, commandBuffer);

    auto pCommandBuffer = castToObject<CommandBuffer>(commandBuffer);
    EXPECT_TRUE(L0::CommandList::fromHandle(pCommandBuffer->getL0Handle())->isPatchPreambleEnabled());

    EXPECT_EQ(CL_SUCCESS, clReleaseCommandBufferKHR(commandBuffer));
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

TEST_F(LeoCommandBufferCaptureTest, givenAppendCommandListsFailureWhenEnqueuedThenErrorIsPropagatedAndReleaseDoesNotSynchronizeQueue) {
    cl_command_queue clQueue = commandQueue;
    cl_command_queue queues[] = {clQueue};

    cl_int errcode = CL_SUCCESS;
    auto commandBuffer = clCreateCommandBufferKHR(1, queues, nullptr, &errcode);
    ASSERT_NE(nullptr, commandBuffer);
    ASSERT_EQ(CL_SUCCESS, clFinalizeCommandBufferKHR(commandBuffer));

    capturingCmdList.appendCommandListsResult = ZE_RESULT_ERROR_INVALID_ARGUMENT;
    EXPECT_EQ(CL_INVALID_VALUE, clEnqueueCommandBufferKHR(1, queues, commandBuffer, 0, nullptr, nullptr));
    capturingCmdList.clearCaptures();

    EXPECT_EQ(CL_SUCCESS, clReleaseCommandBufferKHR(commandBuffer));
    EXPECT_EQ(0u, capturingCmdList.hostSynchronizeArgs.count());
}

TEST_F(LeoCommandBufferCaptureTest, givenQueueOrdinalWhenCreatingCommandBufferThenRecordedListIsInOrderOnThatOrdinal) {
    CommandListCreateHook createHook{};
    CapturingCommandList recordedCmdList{};
    createHook.cmdListToReturn = recordedCmdList.toHandle();
    capturingCmdList.getOrdinalResult = 3u;

    cl_command_queue clQueue = commandQueue;
    cl_command_queue queues[] = {clQueue};
    cl_int errcode = CL_SUCCESS;
    auto commandBuffer = clCreateCommandBufferKHR(1, queues, nullptr, &errcode);
    ASSERT_NE(nullptr, commandBuffer);

    ASSERT_EQ(1u, createHook.requestedDescs.size());
    EXPECT_EQ(3u, createHook.requestedDescs[0].commandQueueGroupOrdinal);
    EXPECT_EQ(static_cast<ze_command_list_flags_t>(ZE_COMMAND_LIST_FLAG_COPY_OFFLOAD_HINT | ZE_COMMAND_LIST_FLAG_IN_ORDER), createHook.requestedDescs[0].flags);
    EXPECT_TRUE(recordedCmdList.isPatchPreambleEnabled());

    EXPECT_EQ(CL_SUCCESS, clReleaseCommandBufferKHR(commandBuffer));
}

TEST_F(LeoCommandBufferCaptureTest, givenQueueOnAnotherOrdinalWhenEnqueuedThenIncompatibleCommandQueueIsReturned) {
    cl_command_queue clQueue = commandQueue;
    cl_command_queue queues[] = {clQueue};

    cl_int errcode = CL_SUCCESS;
    auto commandBuffer = clCreateCommandBufferKHR(1, queues, nullptr, &errcode);
    ASSERT_NE(nullptr, commandBuffer);
    ASSERT_EQ(CL_SUCCESS, clFinalizeCommandBufferKHR(commandBuffer));

    CapturingCommandList otherCmdList{};
    otherCmdList.getOrdinalResult = castToObject<CommandBuffer>(commandBuffer)->getOrdinal() + 1u;
    auto otherQueue = new CommandQueue(context, clDevice, nullptr, otherCmdList.toHandle());
    cl_command_queue otherQueues[] = {otherQueue};

    EXPECT_EQ(CL_INCOMPATIBLE_COMMAND_QUEUE_KHR, clEnqueueCommandBufferKHR(1, otherQueues, commandBuffer, 0, nullptr, nullptr));
    EXPECT_EQ(0u, otherCmdList.appendCommandListsArgs.count());

    otherQueue->decRefApi();
    EXPECT_EQ(CL_SUCCESS, clReleaseCommandBufferKHR(commandBuffer));
}

// Command recording

struct LeoCommandBufferRecordTest : public LeoCommandBufferCaptureTest {
    void SetUp() override {
        LeoCommandBufferCaptureTest::SetUp();
        createHook.cmdListToReturn = recordedCmdList.toHandle();

        cl_command_queue clQueue = commandQueue;
        cl_command_queue queues[] = {clQueue};
        cl_int errcode = CL_SUCCESS;
        commandBuffer = clCreateCommandBufferKHR(1, queues, nullptr, &errcode);
        ASSERT_EQ(CL_SUCCESS, errcode);
        ASSERT_NE(nullptr, commandBuffer);
        pCommandBuffer = castToObject<CommandBuffer>(commandBuffer);
    }

    void TearDown() override {
        releaseCommandBuffer();
        LeoCommandBufferCaptureTest::TearDown();
    }

    void releaseCommandBuffer() {
        if (commandBuffer != nullptr) {
            clReleaseCommandBufferKHR(commandBuffer);
            commandBuffer = nullptr;
            pCommandBuffer = nullptr;
        }
    }

    cl_int recordBarrier(cl_uint numSyncPoints, const cl_sync_point_khr *syncPointWaitList, cl_sync_point_khr *syncPoint) {
        return clCommandBarrierWithWaitListKHR(commandBuffer, nullptr, nullptr, numSyncPoints, syncPointWaitList, syncPoint, nullptr);
    }

    CommandListCreateHook createHook{};
    CapturingCommandList recordedCmdList{};
    cl_command_buffer_khr commandBuffer = nullptr;
    CommandBuffer *pCommandBuffer = nullptr;
};

TEST_F(LeoCommandBufferRecordTest, givenCommandBufferEnqueuedOnQueuesWhenReleasedThenEachQueueIsSynchronizedOnce) {
    ASSERT_EQ(CL_SUCCESS, clFinalizeCommandBufferKHR(commandBuffer));

    CapturingCommandList otherCmdList{};
    auto otherQueue = new CommandQueue(context, clDevice, nullptr, otherCmdList.toHandle());
    cl_command_queue otherQueues[] = {otherQueue};

    EXPECT_EQ(CL_SUCCESS, clEnqueueCommandBufferKHR(0u, nullptr, commandBuffer, 0, nullptr, nullptr));
    EXPECT_EQ(CL_SUCCESS, clEnqueueCommandBufferKHR(1u, otherQueues, commandBuffer, 0, nullptr, nullptr));
    EXPECT_EQ(CL_SUCCESS, clEnqueueCommandBufferKHR(1u, otherQueues, commandBuffer, 0, nullptr, nullptr));
    otherQueue->decRefApi();

    capturingCmdList.clearCaptures();
    otherCmdList.clearCaptures();

    releaseCommandBuffer();
    EXPECT_EQ(1u, capturingCmdList.hostSynchronizeArgs.count());
    EXPECT_EQ(1u, otherCmdList.hostSynchronizeArgs.count());
}

TEST_F(LeoCommandBufferRecordTest, givenCommandsRecordedWhenReturningSyncPointsThenTheyAreNumberedInRecordingOrder) {
    cl_sync_point_khr firstSyncPoint = 7u;
    cl_sync_point_khr thirdSyncPoint = 7u;
    const cl_sync_point_khr waitList[] = {0u, 1u};
    EXPECT_EQ(CL_SUCCESS, recordBarrier(0u, nullptr, &firstSyncPoint));
    EXPECT_EQ(CL_SUCCESS, recordBarrier(0u, nullptr, nullptr));
    EXPECT_EQ(CL_SUCCESS, recordBarrier(2u, waitList, &thirdSyncPoint));

    EXPECT_EQ(0u, firstSyncPoint);
    EXPECT_EQ(2u, thirdSyncPoint);
    EXPECT_EQ(3u, recordedCmdList.appendBarrierArgs.count());
    EXPECT_EQ(0u, capturingCmdList.totalCalls());
}

TEST_F(LeoCommandBufferRecordTest, givenBarrierRecordedThenBarrierWithoutEventsIsAppendedToRecordedList) {
    const cl_sync_point_khr waitList[] = {0u};
    ASSERT_EQ(CL_SUCCESS, recordBarrier(0u, nullptr, nullptr));
    EXPECT_EQ(CL_SUCCESS, recordBarrier(1u, waitList, nullptr));

    ASSERT_EQ(2u, recordedCmdList.appendBarrierArgs.count());
    EXPECT_EQ(nullptr, recordedCmdList.appendBarrierArgs[1].signalEvent);
    EXPECT_TRUE(recordedCmdList.appendBarrierArgs[1].waitEvents.empty());
}

TEST_F(LeoCommandBufferRecordTest, givenFinalizedCommandBufferWhenRecordingThenInvalidOperationIsReturned) {
    ASSERT_EQ(CL_SUCCESS, clFinalizeCommandBufferKHR(commandBuffer));

    EXPECT_EQ(CL_INVALID_OPERATION, recordBarrier(0u, nullptr, nullptr));
    EXPECT_EQ(0u, recordedCmdList.totalCalls());
}

TEST_F(LeoCommandBufferRecordTest, givenCommandQueueWhenRecordingThenInvalidCommandQueueIsReturned) {
    cl_command_queue clQueue = commandQueue;
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, clCommandBarrierWithWaitListKHR(commandBuffer, clQueue, nullptr, 0u, nullptr, nullptr, nullptr));
    EXPECT_EQ(0u, recordedCmdList.totalCalls());
}

TEST_F(LeoCommandBufferRecordTest, givenPropertiesWhenRecordingThenOnlyAnEmptyListIsAccepted) {
    const cl_command_properties_khr emptyProperties[] = {0};
    const cl_command_properties_khr properties[] = {CL_COMMAND_BUFFER_FLAGS_KHR, 0, 0};

    EXPECT_EQ(CL_INVALID_VALUE, clCommandBarrierWithWaitListKHR(commandBuffer, nullptr, properties, 0u, nullptr, nullptr, nullptr));
    EXPECT_EQ(0u, recordedCmdList.totalCalls());
    EXPECT_EQ(CL_SUCCESS, clCommandBarrierWithWaitListKHR(commandBuffer, nullptr, emptyProperties, 0u, nullptr, nullptr, nullptr));
}

TEST_F(LeoCommandBufferRecordTest, givenMutableHandleWhenRecordingThenInvalidValueIsReturned) {
    cl_mutable_command_khr mutableHandle = nullptr;
    EXPECT_EQ(CL_INVALID_VALUE, clCommandBarrierWithWaitListKHR(commandBuffer, nullptr, nullptr, 0u, nullptr, nullptr, &mutableHandle));
    EXPECT_EQ(0u, recordedCmdList.totalCalls());
}

TEST_F(LeoCommandBufferRecordTest, givenInconsistentSyncPointWaitListWhenRecordingThenInvalidSyncPointWaitListIsReturned) {
    cl_sync_point_khr syncPoint = 0u;
    ASSERT_EQ(CL_SUCCESS, recordBarrier(0u, nullptr, &syncPoint));
    recordedCmdList.clearCaptures();

    EXPECT_EQ(CL_INVALID_SYNC_POINT_WAIT_LIST_KHR, recordBarrier(1u, nullptr, nullptr));
    EXPECT_EQ(CL_INVALID_SYNC_POINT_WAIT_LIST_KHR, recordBarrier(0u, &syncPoint, nullptr));
    EXPECT_EQ(0u, recordedCmdList.totalCalls());
}

TEST_F(LeoCommandBufferRecordTest, givenSyncPointNotYetReturnedWhenRecordingThenInvalidSyncPointWaitListIsReturned) {
    const cl_sync_point_khr firstSyncPoint[] = {0u};
    EXPECT_EQ(CL_INVALID_SYNC_POINT_WAIT_LIST_KHR, recordBarrier(1u, firstSyncPoint, nullptr));

    ASSERT_EQ(CL_SUCCESS, recordBarrier(0u, nullptr, nullptr));
    const cl_sync_point_khr secondSyncPoint[] = {0u, 1u};
    EXPECT_EQ(CL_INVALID_SYNC_POINT_WAIT_LIST_KHR, recordBarrier(2u, secondSyncPoint, nullptr));
    EXPECT_EQ(CL_SUCCESS, recordBarrier(1u, firstSyncPoint, nullptr));
    EXPECT_EQ(2u, recordedCmdList.appendBarrierArgs.count());
}

TEST_F(LeoCommandBufferRecordTest, givenAppendFailureWhenRecordingThenErrorIsReturnedAndNoSyncPointIsConsumed) {
    recordedCmdList.appendBarrierResult = ZE_RESULT_ERROR_INVALID_ARGUMENT;
    cl_sync_point_khr syncPoint = 7u;
    EXPECT_EQ(CL_INVALID_VALUE, recordBarrier(0u, nullptr, &syncPoint));
    EXPECT_EQ(7u, syncPoint);

    recordedCmdList.appendBarrierResult = ZE_RESULT_SUCCESS;
    EXPECT_EQ(CL_SUCCESS, recordBarrier(0u, nullptr, &syncPoint));
    EXPECT_EQ(0u, syncPoint);
}

TEST_F(LeoCommandBufferRecordTest, givenBuffersWhenRecordingCopyBufferThenCopyIsAppendedAndBuffersAreRetained) {
    auto srcBuffer = createBuffer(64u);
    auto dstBuffer = createBuffer(64u);
    auto pSrcBuffer = castToObject<Buffer>(srcBuffer);
    auto pDstBuffer = castToObject<Buffer>(dstBuffer);
    const auto srcRefCount = pSrcBuffer->getRefInternalCount();
    const auto dstRefCount = pDstBuffer->getRefInternalCount();

    cl_sync_point_khr syncPoint = 7u;
    EXPECT_EQ(CL_SUCCESS, clCommandCopyBufferKHR(commandBuffer, nullptr, nullptr, srcBuffer, dstBuffer, 8u, 16u, 32u, 0u, nullptr, &syncPoint, nullptr));
    EXPECT_EQ(0u, syncPoint);

    ASSERT_EQ(1u, recordedCmdList.appendMemoryCopyArgs.count());
    const auto &args = recordedCmdList.appendMemoryCopyArgs[0];
    EXPECT_EQ(ptrOffset(pDstBuffer->getUsmPtr(), 16u), args.dstptr);
    EXPECT_EQ(ptrOffset(pSrcBuffer->getUsmPtr(), 8u), args.srcptr);
    EXPECT_EQ(32u, args.size);
    EXPECT_EQ(nullptr, args.signalEvent);
    EXPECT_TRUE(args.waitEvents.empty());
    EXPECT_EQ(srcRefCount + 1, pSrcBuffer->getRefInternalCount());
    EXPECT_EQ(dstRefCount + 1, pDstBuffer->getRefInternalCount());

    releaseCommandBuffer();
    EXPECT_EQ(srcRefCount, pSrcBuffer->getRefInternalCount());
    EXPECT_EQ(dstRefCount, pDstBuffer->getRefInternalCount());

    clReleaseMemObject(srcBuffer);
    clReleaseMemObject(dstBuffer);
}

TEST_F(LeoCommandBufferRecordTest, givenCopyAppendFailureWhenRecordingCopyBufferThenBuffersAreNotRetained) {
    auto buffer = createBuffer(64u);
    auto pBuffer = castToObject<Buffer>(buffer);
    const auto refCount = pBuffer->getRefInternalCount();

    recordedCmdList.appendMemoryCopyResult = ZE_RESULT_ERROR_INVALID_ARGUMENT;
    EXPECT_EQ(CL_INVALID_VALUE, clCommandCopyBufferKHR(commandBuffer, nullptr, nullptr, buffer, buffer, 0u, 0u, 64u, 0u, nullptr, nullptr, nullptr));
    EXPECT_EQ(refCount, pBuffer->getRefInternalCount());

    clReleaseMemObject(buffer);
}

TEST_F(LeoCommandBufferRecordTest, givenBufferFromAnotherContextWhenRecordingThenInvalidContextIsReturned) {
    cl_device_id clDeviceId = clDevice;
    cl_int errcode = CL_SUCCESS;
    auto otherContext = clCreateContext(nullptr, 1, &clDeviceId, nullptr, nullptr, &errcode);
    ASSERT_NE(nullptr, otherContext);
    auto otherBuffer = clCreateBuffer(otherContext, CL_MEM_READ_WRITE, 64u, nullptr, &errcode);
    ASSERT_NE(nullptr, otherBuffer);
    auto buffer = createBuffer(64u);
    const size_t origin[3] = {0u, 0u, 0u};
    const size_t region[3] = {4u, 1u, 1u};
    const uint32_t pattern = 0u;

    EXPECT_EQ(CL_INVALID_CONTEXT, clCommandCopyBufferKHR(commandBuffer, nullptr, nullptr, buffer, otherBuffer, 0u, 0u, 4u, 0u, nullptr, nullptr, nullptr));
    EXPECT_EQ(CL_INVALID_CONTEXT, clCommandCopyBufferKHR(commandBuffer, nullptr, nullptr, otherBuffer, buffer, 0u, 0u, 4u, 0u, nullptr, nullptr, nullptr));
    EXPECT_EQ(CL_INVALID_CONTEXT, clCommandCopyBufferRectKHR(commandBuffer, nullptr, nullptr, buffer, otherBuffer, origin, origin, region, 0u, 0u, 0u, 0u, 0u, nullptr, nullptr, nullptr));
    EXPECT_EQ(CL_INVALID_CONTEXT, clCommandFillBufferKHR(commandBuffer, nullptr, nullptr, otherBuffer, &pattern, sizeof(pattern), 0u, 4u, 0u, nullptr, nullptr, nullptr));
    EXPECT_EQ(0u, recordedCmdList.totalCalls());

    clReleaseMemObject(buffer);
    clReleaseMemObject(otherBuffer);
    clReleaseContext(otherContext);
}

TEST_F(LeoCommandBufferRecordTest, givenInvalidCommandArgumentsWhenRecordingThenCommandIsRejected) {
    auto buffer = createBuffer(64u);
    const size_t origin[3] = {0u, 0u, 0u};
    const size_t region[3] = {4u, 1u, 1u};
    const uint32_t pattern = 0u;
    cl_command_queue clQueue = commandQueue;

    EXPECT_EQ(CL_INVALID_MEM_OBJECT, clCommandCopyBufferKHR(commandBuffer, nullptr, nullptr, buffer, nullptr, 0u, 0u, 4u, 0u, nullptr, nullptr, nullptr));
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, clCommandCopyBufferKHR(commandBuffer, clQueue, nullptr, buffer, buffer, 0u, 0u, 4u, 0u, nullptr, nullptr, nullptr));
    EXPECT_EQ(CL_INVALID_MEM_OBJECT, clCommandCopyBufferRectKHR(commandBuffer, nullptr, nullptr, nullptr, buffer, origin, origin, region, 0u, 0u, 0u, 0u, 0u, nullptr, nullptr, nullptr));
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, clCommandCopyBufferRectKHR(commandBuffer, clQueue, nullptr, buffer, buffer, origin, origin, region, 0u, 0u, 0u, 0u, 0u, nullptr, nullptr, nullptr));
    EXPECT_EQ(CL_INVALID_VALUE, clCommandFillBufferKHR(commandBuffer, nullptr, nullptr, buffer, nullptr, sizeof(pattern), 0u, 4u, 0u, nullptr, nullptr, nullptr));
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, clCommandFillBufferKHR(commandBuffer, clQueue, nullptr, buffer, &pattern, sizeof(pattern), 0u, 4u, 0u, nullptr, nullptr, nullptr));
    EXPECT_EQ(CL_INVALID_VALUE, clCommandSVMMemcpyKHR(commandBuffer, nullptr, nullptr, nullptr, &pattern, 4u, 0u, nullptr, nullptr, nullptr));
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, clCommandSVMMemcpyKHR(commandBuffer, clQueue, nullptr, reinterpret_cast<void *>(0x1000), &pattern, 4u, 0u, nullptr, nullptr, nullptr));
    EXPECT_EQ(CL_INVALID_VALUE, clCommandSVMMemFillKHR(commandBuffer, nullptr, nullptr, reinterpret_cast<void *>(0x1000), nullptr, sizeof(pattern), 4u, 0u, nullptr, nullptr, nullptr));
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, clCommandSVMMemFillKHR(commandBuffer, clQueue, nullptr, reinterpret_cast<void *>(0x1000), &pattern, sizeof(pattern), 4u, 0u, nullptr, nullptr, nullptr));
    EXPECT_EQ(CL_INVALID_COMMAND_BUFFER_KHR, clCommandBarrierWithWaitListKHR(nullptr, nullptr, nullptr, 0u, nullptr, nullptr, nullptr));
    EXPECT_EQ(CL_INVALID_COMMAND_BUFFER_KHR, clCommandSVMMemcpyKHR(nullptr, nullptr, nullptr, reinterpret_cast<void *>(0x1000), &pattern, 4u, 0u, nullptr, nullptr, nullptr));
    EXPECT_EQ(0u, recordedCmdList.totalCalls());

    clReleaseMemObject(buffer);
}

TEST_F(LeoCommandBufferRecordTest, givenRectRegionWhenRecordingCopyBufferRectThenRegionAndDefaultPitchesAreAppended) {
    auto srcBuffer = createBuffer(256u);
    auto dstBuffer = createBuffer(256u);
    const size_t srcOrigin[3] = {1u, 2u, 0u};
    const size_t dstOrigin[3] = {3u, 0u, 1u};
    const size_t region[3] = {4u, 2u, 2u};

    cl_sync_point_khr syncPoint = 7u;
    EXPECT_EQ(CL_SUCCESS, clCommandCopyBufferRectKHR(commandBuffer, nullptr, nullptr, srcBuffer, dstBuffer, srcOrigin, dstOrigin, region, 0u, 0u, 16u, 0u, 0u, nullptr, &syncPoint, nullptr));
    EXPECT_EQ(0u, syncPoint);

    ASSERT_EQ(1u, recordedCmdList.appendMemoryCopyRegionArgs.count());
    const auto &args = recordedCmdList.appendMemoryCopyRegionArgs[0];
    EXPECT_EQ(castToObject<Buffer>(dstBuffer)->getUsmPtr(), args.dstptr);
    EXPECT_EQ(castToObject<Buffer>(srcBuffer)->getUsmPtr(), args.srcptr);
    EXPECT_EQ(4u, args.srcPitch);
    EXPECT_EQ(8u, args.srcSlicePitch);
    EXPECT_EQ(16u, args.dstPitch);
    EXPECT_EQ(32u, args.dstSlicePitch);
    ASSERT_TRUE(args.srcRegion.has_value());
    ASSERT_TRUE(args.dstRegion.has_value());
    EXPECT_EQ(1u, args.srcRegion->originX);
    EXPECT_EQ(2u, args.srcRegion->originY);
    EXPECT_EQ(3u, args.dstRegion->originX);
    EXPECT_EQ(1u, args.dstRegion->originZ);
    EXPECT_EQ(4u, args.dstRegion->width);
    EXPECT_EQ(2u, args.dstRegion->height);
    EXPECT_EQ(2u, args.dstRegion->depth);
    EXPECT_EQ(nullptr, args.signalEvent);

    releaseCommandBuffer();
    clReleaseMemObject(srcBuffer);
    clReleaseMemObject(dstBuffer);
}

TEST_F(LeoCommandBufferRecordTest, givenPatternWhenRecordingFillBufferThenFillIsAppendedAndBufferIsRetained) {
    auto buffer = createBuffer(64u);
    auto pBuffer = castToObject<Buffer>(buffer);
    const auto refCount = pBuffer->getRefInternalCount();
    const uint32_t pattern = 0xCAFEBABEu;

    cl_sync_point_khr syncPoint = 7u;
    EXPECT_EQ(CL_SUCCESS, clCommandFillBufferKHR(commandBuffer, nullptr, nullptr, buffer, &pattern, sizeof(pattern), 8u, 16u, 0u, nullptr, &syncPoint, nullptr));
    EXPECT_EQ(0u, syncPoint);

    ASSERT_EQ(1u, recordedCmdList.appendMemoryFillArgs.count());
    const auto &args = recordedCmdList.appendMemoryFillArgs[0];
    EXPECT_EQ(ptrOffset(pBuffer->getUsmPtr(), 8u), args.ptr);
    ASSERT_EQ(sizeof(pattern), args.pattern.size());
    EXPECT_EQ(0, memcmp(&pattern, args.pattern.data(), sizeof(pattern)));
    EXPECT_EQ(sizeof(pattern), args.patternSize);
    EXPECT_EQ(16u, args.size);
    EXPECT_EQ(nullptr, args.signalEvent);
    EXPECT_EQ(refCount + 1, pBuffer->getRefInternalCount());

    releaseCommandBuffer();
    EXPECT_EQ(refCount, pBuffer->getRefInternalCount());
    clReleaseMemObject(buffer);
}

TEST_F(LeoCommandBufferRecordTest, givenSvmPointersWhenRecordingSvmCommandsThenCopyAndFillAreAppended) {
    auto dstPtr = reinterpret_cast<void *>(0x1000);
    auto srcPtr = reinterpret_cast<const void *>(0x2000);
    const uint16_t pattern = 0xABCDu;

    cl_sync_point_khr syncPoints[2] = {7u, 7u};
    EXPECT_EQ(CL_SUCCESS, clCommandSVMMemcpyKHR(commandBuffer, nullptr, nullptr, dstPtr, srcPtr, 32u, 0u, nullptr, &syncPoints[0], nullptr));
    EXPECT_EQ(CL_SUCCESS, clCommandSVMMemFillKHR(commandBuffer, nullptr, nullptr, dstPtr, &pattern, sizeof(pattern), 16u, 1u, syncPoints, &syncPoints[1], nullptr));
    EXPECT_EQ(0u, syncPoints[0]);
    EXPECT_EQ(1u, syncPoints[1]);

    ASSERT_EQ(1u, recordedCmdList.appendMemoryCopyArgs.count());
    EXPECT_EQ(dstPtr, recordedCmdList.appendMemoryCopyArgs[0].dstptr);
    EXPECT_EQ(srcPtr, recordedCmdList.appendMemoryCopyArgs[0].srcptr);
    EXPECT_EQ(32u, recordedCmdList.appendMemoryCopyArgs[0].size);

    ASSERT_EQ(1u, recordedCmdList.appendMemoryFillArgs.count());
    EXPECT_EQ(dstPtr, recordedCmdList.appendMemoryFillArgs[0].ptr);
    EXPECT_EQ(sizeof(pattern), recordedCmdList.appendMemoryFillArgs[0].patternSize);
    EXPECT_EQ(16u, recordedCmdList.appendMemoryFillArgs[0].size);
}

struct WhiteBoxLeoKernel : public Kernel {
    using Kernel::Kernel;
    using Kernel::sharedObjArgs;
};

struct LeoCommandBufferKernelTest : public LeoCommandBufferRecordTest {
    void SetUp() override {
        LeoCommandBufferRecordTest::SetUp();
        program = std::make_unique<Program>(context);
        kernel = createKernel(*program, l0Kernel);
    }

    void TearDown() override {
        releaseCommandBuffer();
        kernel.reset();
        program.reset();
        LeoCommandBufferRecordTest::TearDown();
    }

    static std::unique_ptr<WhiteBoxLeoKernel> createKernel(Program &program, L0::ult::Mock<L0::KernelImp> *&l0KernelOut) {
        auto l0KernelMock = std::make_unique<L0::ult::Mock<L0::KernelImp>>();
        l0KernelMock->privateState.groupSize[0] = 16u;
        l0KernelMock->privateState.groupSize[1] = 2u;
        l0KernelMock->privateState.groupSize[2] = 1u;
        l0KernelOut = l0KernelMock.get();
        std::map<uint32_t, ze_kernel_handle_t> kernelHandles{{0u, l0KernelMock.release()->toHandle()}};
        return std::make_unique<WhiteBoxLeoKernel>(std::move(kernelHandles), &program);
    }

    cl_int recordKernel(const size_t *globalWorkSize, const size_t *localWorkSize, cl_sync_point_khr *syncPoint) {
        return clCommandNDRangeKernelKHR(commandBuffer, nullptr, nullptr, kernel.get(), 2u, nullptr, globalWorkSize, localWorkSize,
                                         0u, nullptr, syncPoint, nullptr);
    }

    std::unique_ptr<Program> program;
    std::unique_ptr<WhiteBoxLeoKernel> kernel;
    L0::ult::Mock<L0::KernelImp> *l0Kernel = nullptr;
    const size_t globalWorkSize[3] = {64u, 4u, 1u};
    const size_t localWorkSize[3] = {16u, 2u, 1u};
};

TEST_F(LeoCommandBufferKernelTest, givenKernelWhenRecordingNDRangeKernelThenLaunchIsAppendedAndKernelIsRetained) {
    const auto refCount = kernel->getRefInternalCount();

    cl_sync_point_khr syncPoint = 7u;
    EXPECT_EQ(CL_SUCCESS, recordKernel(globalWorkSize, localWorkSize, &syncPoint));
    EXPECT_EQ(0u, syncPoint);

    ASSERT_EQ(1u, recordedCmdList.appendLaunchKernelArgs.count());
    const auto &args = recordedCmdList.appendLaunchKernelArgs[0];
    EXPECT_EQ(l0Kernel->toHandle(), args.kernel);
    EXPECT_EQ(4u, args.groupCount.groupCountX);
    EXPECT_EQ(2u, args.groupCount.groupCountY);
    EXPECT_EQ(1u, args.groupCount.groupCountZ);
    EXPECT_EQ(nullptr, args.signalEvent);
    EXPECT_TRUE(args.waitEvents.empty());
    EXPECT_EQ(refCount + 1, kernel->getRefInternalCount());
    EXPECT_EQ(0u, capturingCmdList.totalCalls());

    releaseCommandBuffer();
    EXPECT_EQ(refCount, kernel->getRefInternalCount());
}

TEST_F(LeoCommandBufferKernelTest, givenLaunchAppendFailureWhenRecordingNDRangeKernelThenErrorIsReturnedAndKernelIsNotRetained) {
    const auto refCount = kernel->getRefInternalCount();
    recordedCmdList.appendLaunchKernelResult = ZE_RESULT_ERROR_INVALID_ARGUMENT;

    EXPECT_EQ(CL_INVALID_VALUE, recordKernel(globalWorkSize, localWorkSize, nullptr));
    EXPECT_EQ(refCount, kernel->getRefInternalCount());
}

TEST_F(LeoCommandBufferKernelTest, givenZeroGlobalWorkSizeWhenRecordingNDRangeKernelThenBarrierIsAppended) {
    const size_t zeroGlobalWorkSize[3] = {0u, 0u, 0u};

    cl_sync_point_khr syncPoint = 7u;
    EXPECT_EQ(CL_SUCCESS, recordKernel(zeroGlobalWorkSize, localWorkSize, &syncPoint));
    EXPECT_EQ(0u, syncPoint);
    EXPECT_EQ(1u, recordedCmdList.appendBarrierArgs.count());
    EXPECT_EQ(0u, recordedCmdList.appendLaunchKernelArgs.count());
}

TEST_F(LeoCommandBufferKernelTest, givenLocalSizeNotDividingGlobalSizeWhenRecordingNDRangeKernelThenInvalidWorkGroupSizeIsReturned) {
    const size_t indivisibleGlobalWorkSize[3] = {60u, 4u, 1u};
    const auto refCount = kernel->getRefInternalCount();

    EXPECT_EQ(CL_INVALID_WORK_GROUP_SIZE, recordKernel(indivisibleGlobalWorkSize, localWorkSize, nullptr));
    EXPECT_EQ(0u, recordedCmdList.totalCalls());
    EXPECT_EQ(refCount, kernel->getRefInternalCount());
}

TEST_F(LeoCommandBufferKernelTest, givenInvalidWorkDimWhenRecordingNDRangeKernelThenInvalidWorkDimensionIsReturned) {
    EXPECT_EQ(CL_INVALID_WORK_DIMENSION, clCommandNDRangeKernelKHR(commandBuffer, nullptr, nullptr, kernel.get(), 0u, nullptr, globalWorkSize, localWorkSize, 0u, nullptr, nullptr, nullptr));
    EXPECT_EQ(CL_INVALID_WORK_DIMENSION, clCommandNDRangeKernelKHR(commandBuffer, nullptr, nullptr, kernel.get(), 4u, nullptr, globalWorkSize, localWorkSize, 0u, nullptr, nullptr, nullptr));
    EXPECT_EQ(0u, recordedCmdList.totalCalls());
}

TEST_F(LeoCommandBufferKernelTest, givenInvalidArgumentsWhenRecordingNDRangeKernelThenCommandIsRejected) {
    cl_command_queue clQueue = commandQueue;
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, clCommandNDRangeKernelKHR(commandBuffer, clQueue, nullptr, kernel.get(), 2u, nullptr, globalWorkSize, localWorkSize, 0u, nullptr, nullptr, nullptr));
    EXPECT_EQ(CL_INVALID_KERNEL, clCommandNDRangeKernelKHR(commandBuffer, nullptr, nullptr, nullptr, 2u, nullptr, globalWorkSize, localWorkSize, 0u, nullptr, nullptr, nullptr));
    EXPECT_EQ(0u, recordedCmdList.totalCalls());
}

TEST_F(LeoCommandBufferKernelTest, givenKernelWithUnsetArgsWhenRecordingNDRangeKernelThenInvalidKernelArgsIsReturned) {
    auto l0KernelMock = std::make_unique<L0::ult::Mock<L0::KernelImp>>();
    l0KernelMock->privateState.kernelArgHandlers.resize(1u);
    std::map<uint32_t, ze_kernel_handle_t> kernelHandles{{0u, l0KernelMock.release()->toHandle()}};
    auto kernelWithUnsetArg = std::make_unique<Kernel>(std::move(kernelHandles), program.get());

    EXPECT_EQ(CL_INVALID_KERNEL_ARGS, clCommandNDRangeKernelKHR(commandBuffer, nullptr, nullptr, kernelWithUnsetArg.get(), 2u, nullptr, globalWorkSize, localWorkSize, 0u, nullptr, nullptr, nullptr));
    EXPECT_EQ(0u, recordedCmdList.totalCalls());
}

TEST_F(LeoCommandBufferKernelTest, givenUnsupportedKernelWhenRecordingNDRangeKernelThenInvalidKernelIsReturned) {
    l0Kernel->getDescriptor().kernelAttributes.flags.usesSyncBuffer = true;
    EXPECT_EQ(CL_INVALID_KERNEL, recordKernel(globalWorkSize, localWorkSize, nullptr));
    l0Kernel->getDescriptor().kernelAttributes.flags.usesSyncBuffer = false;

    cl_execution_info_kernel_type_intel type = CL_KERNEL_EXEC_INFO_CONCURRENT_TYPE_INTEL;
    ASSERT_EQ(CL_SUCCESS, clSetKernelExecInfo(kernel.get(), CL_KERNEL_EXEC_INFO_KERNEL_TYPE_INTEL, sizeof(type), &type));
    EXPECT_EQ(CL_INVALID_KERNEL, recordKernel(globalWorkSize, localWorkSize, nullptr));
    EXPECT_EQ(0u, recordedCmdList.totalCalls());
}

TEST_F(LeoCommandBufferKernelTest, givenKernelWithImageArgWhenRecordingNDRangeKernelThenInvalidOperationIsReturned) {
    kernel->setImageArg(0u, reinterpret_cast<Image *>(0x1000));
    EXPECT_EQ(CL_INVALID_OPERATION, recordKernel(globalWorkSize, localWorkSize, nullptr));
    kernel->clearImageArg(0u);
    EXPECT_EQ(0u, recordedCmdList.totalCalls());
}

TEST_F(LeoCommandBufferKernelTest, givenKernelWithSharedObjectArgWhenRecordingNDRangeKernelThenInvalidOperationIsReturned) {
    kernel->sharedObjArgs[0u] = reinterpret_cast<Buffer *>(0x1000);
    EXPECT_EQ(CL_INVALID_OPERATION, recordKernel(globalWorkSize, localWorkSize, nullptr));
    kernel->sharedObjArgs.clear();
    EXPECT_EQ(0u, recordedCmdList.totalCalls());
}

struct LeoCommandBufferClonedKernelTest : public LeoCommandBufferKernelTest {
    void SetUp() override {
        LeoCommandBufferKernelTest::SetUp();
        l0Module = std::make_unique<L0::ult::Mock<L0::ult::Module>>(clDevice->getL0Object(), nullptr);

        auto l0KernelMock = std::make_unique<L0::ult::Mock<L0::KernelImp>>();
        l0KernelMock->setModule(l0Module.get());
        l0KernelMock->privateState.groupSize[0] = 16u;
        l0KernelMock->privateState.groupSize[1] = 2u;
        l0KernelMock->privateState.groupSize[2] = 1u;
        std::map<uint32_t, ze_kernel_handle_t> kernelHandles{{0u, l0KernelMock.release()->toHandle()}};
        sourceKernel = std::make_unique<Kernel>(std::move(kernelHandles), program.get());
    }

    void TearDown() override {
        releaseCommandBuffer();
        sourceKernel.reset();
        l0Module.reset();
        LeoCommandBufferKernelTest::TearDown();
    }

    std::unique_ptr<L0::ult::Mock<L0::ult::Module>> l0Module;
    std::unique_ptr<Kernel> sourceKernel;
};

TEST_F(LeoCommandBufferClonedKernelTest, givenClonedKernelWithImageArgWhenRecordingNDRangeKernelThenInvalidOperationIsReturned) {
    sourceKernel->setImageArg(0u, reinterpret_cast<Image *>(0x1000));
    auto clonedKernel = std::make_unique<Kernel>(sourceKernel.get());
    sourceKernel->clearImageArg(0u);

    EXPECT_EQ(CL_INVALID_OPERATION, clCommandNDRangeKernelKHR(commandBuffer, nullptr, nullptr, clonedKernel.get(), 2u, nullptr, globalWorkSize,
                                                              localWorkSize, 0u, nullptr, nullptr, nullptr));
    clonedKernel->clearImageArg(0u);
    EXPECT_EQ(0u, recordedCmdList.totalCalls());
}

TEST_F(LeoCommandBufferKernelTest, givenKernelUsingPrintfWhenRecordingNDRangeKernelThenInvalidOperationIsReturned) {
    l0Kernel->getDescriptor().kernelAttributes.flags.usesPrintf = true;
    EXPECT_EQ(CL_INVALID_OPERATION, recordKernel(globalWorkSize, localWorkSize, nullptr));
    EXPECT_EQ(0u, recordedCmdList.totalCalls());
}

TEST_F(LeoCommandBufferKernelTest, givenWorkDimAboveThreeWhenSettingUpDispatchThenAbortIsRaised) {
    ze_group_count_t groupCount{};
    EXPECT_THROW(kernel->setupDispatch(*clDevice, 4u, nullptr, globalWorkSize, localWorkSize, groupCount), std::exception);
}

TEST_F(LeoCommandBufferKernelTest, givenGroupCountAboveUint32WhenRecordingNDRangeKernelThenInvalidGlobalWorkSizeIsReturned) {
    const size_t largeGlobalWorkSize[3] = {localWorkSize[0] * (static_cast<size_t>(std::numeric_limits<uint32_t>::max()) + 1u), 4u, 1u};
    EXPECT_EQ(CL_INVALID_GLOBAL_WORK_SIZE, recordKernel(largeGlobalWorkSize, localWorkSize, nullptr));
    EXPECT_EQ(0u, recordedCmdList.totalCalls());
}

TEST_F(LeoCommandBufferKernelTest, givenKernelFromAnotherContextWhenRecordingNDRangeKernelThenInvalidContextIsReturned) {
    cl_device_id clDeviceId = clDevice;
    cl_int errcode = CL_SUCCESS;
    auto otherContext = clCreateContext(nullptr, 1, &clDeviceId, nullptr, nullptr, &errcode);
    ASSERT_NE(nullptr, otherContext);
    {
        Program otherProgram(castToObject<Context>(otherContext));
        L0::ult::Mock<L0::KernelImp> *otherL0Kernel = nullptr;
        auto otherKernel = createKernel(otherProgram, otherL0Kernel);

        EXPECT_EQ(CL_INVALID_CONTEXT, clCommandNDRangeKernelKHR(commandBuffer, nullptr, nullptr, otherKernel.get(), 2u, nullptr, globalWorkSize, localWorkSize, 0u, nullptr, nullptr, nullptr));
        EXPECT_EQ(0u, recordedCmdList.totalCalls());
    }
    clReleaseContext(otherContext);
}

} // namespace ult
} // namespace LEO
} // namespace NEO
