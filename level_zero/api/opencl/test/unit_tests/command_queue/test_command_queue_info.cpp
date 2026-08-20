/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/extensions/public/cl_ext_private.h"
#include "level_zero/api/opencl/source/cl_device/leo_cl_device.h"
#include "level_zero/api/opencl/source/command_queue/leo_command_queue.h"
#include "level_zero/api/opencl/source/context/leo_context.h"
#include "level_zero/api/opencl/source/platform/leo_platform.h"
#include "level_zero/api/opencl/test/common/fixtures/capturing_command_list.h"
#include "level_zero/api/opencl/test/common/fixtures/ocl_fixture.h"

#include "CL/cl.h"
#include "CL/cl_ext.h"

#include <memory>
#include <vector>

namespace NEO {
namespace LEO {
namespace ult {

struct CommandQueueInfoFixture : public Test<OclFixture> {
    void SetUp() override {
        Test<OclFixture>::SetUp();
        clDevice = platform->getDevices()[0].get();
        clDeviceId = clDevice;
        context = std::make_unique<Context>(nullptr, this->L0::ult::DeviceFixture::context->toHandle(), 1, &clDeviceId, true);
    }

    void TearDown() override {
        commandQueue.reset();
        context.reset();
        Test<OclFixture>::TearDown();
    }

    CommandQueue *createQueue(const cl_queue_properties *properties) {
        commandQueue = std::make_unique<CommandQueue>(context.get(), clDevice, properties, capturingCmdList.toHandle());
        return commandQueue.get();
    }

    ClDevice *clDevice = nullptr;
    cl_device_id clDeviceId = nullptr;
    std::unique_ptr<Context> context;
    CapturingCommandList capturingCmdList{};
    std::unique_ptr<CommandQueue> commandQueue;
};

TEST_F(CommandQueueInfoFixture, givenQueueWhenQueryingContextThenReturnsOwningContext) {
    auto queue = createQueue(nullptr);

    cl_context queried = nullptr;
    size_t retSize = 0;
    EXPECT_EQ(CL_SUCCESS, queue->getCmdQInfo(CL_QUEUE_CONTEXT, sizeof(queried), &queried, &retSize));
    EXPECT_EQ(sizeof(cl_context), retSize);
    EXPECT_EQ(static_cast<cl_context>(context.get()), queried);
}

TEST_F(CommandQueueInfoFixture, givenQueueWhenQueryingDeviceThenReturnsOwningDevice) {
    auto queue = createQueue(nullptr);

    cl_device_id queried = nullptr;
    size_t retSize = 0;
    EXPECT_EQ(CL_SUCCESS, queue->getCmdQInfo(CL_QUEUE_DEVICE, sizeof(queried), &queried, &retSize));
    EXPECT_EQ(sizeof(cl_device_id), retSize);
    EXPECT_EQ(clDeviceId, queried);
}

TEST_F(CommandQueueInfoFixture, givenQueueWhenQueryingReferenceCountThenReturnsOne) {
    auto queue = createQueue(nullptr);

    cl_int refCount = 0;
    EXPECT_EQ(CL_SUCCESS, queue->getCmdQInfo(CL_QUEUE_REFERENCE_COUNT, sizeof(refCount), &refCount, nullptr));
    EXPECT_EQ(1, refCount);
}

TEST_F(CommandQueueInfoFixture, givenNoPropertiesWhenQueryingQueuePropertiesThenReturnsZero) {
    auto queue = createQueue(nullptr);

    cl_command_queue_properties properties = 0xFFu;
    size_t retSize = 0;
    EXPECT_EQ(CL_SUCCESS, queue->getCmdQInfo(CL_QUEUE_PROPERTIES, sizeof(properties), &properties, &retSize));
    EXPECT_EQ(sizeof(cl_command_queue_properties), retSize);
    EXPECT_EQ(0u, properties);
}

TEST_F(CommandQueueInfoFixture, givenProfilingPropertyWhenQueryingQueuePropertiesThenFlagIsReported) {
    cl_queue_properties queueProperties[] = {CL_QUEUE_PROPERTIES, CL_QUEUE_PROFILING_ENABLE, 0};
    auto queue = createQueue(queueProperties);

    cl_command_queue_properties properties = 0;
    EXPECT_EQ(CL_SUCCESS, queue->getCmdQInfo(CL_QUEUE_PROPERTIES, sizeof(properties), &properties, nullptr));
    EXPECT_EQ(static_cast<cl_command_queue_properties>(CL_QUEUE_PROFILING_ENABLE), properties);
    EXPECT_TRUE(queue->isProfilingEnabled());
}

TEST_F(CommandQueueInfoFixture, givenNoProfilingPropertyWhenQueryingThenProfilingIsDisabled) {
    auto queue = createQueue(nullptr);
    EXPECT_FALSE(queue->isProfilingEnabled());
}

TEST_F(CommandQueueInfoFixture, givenOutOfOrderPropertyWhenQueryingQueuePropertiesThenFlagIsReported) {
    cl_queue_properties queueProperties[] = {CL_QUEUE_PROPERTIES, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, 0};
    auto queue = createQueue(queueProperties);

    cl_command_queue_properties properties = 0;
    EXPECT_EQ(CL_SUCCESS, queue->getCmdQInfo(CL_QUEUE_PROPERTIES, sizeof(properties), &properties, nullptr));
    EXPECT_EQ(static_cast<cl_command_queue_properties>(CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE), properties);
}

TEST_F(CommandQueueInfoFixture, givenCombinedPropertiesWhenQueryingQueuePropertiesThenBothFlagsAreReported) {
    cl_queue_properties queueProperties[] = {
        CL_QUEUE_PROPERTIES, CL_QUEUE_PROFILING_ENABLE | CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, 0};
    auto queue = createQueue(queueProperties);

    cl_command_queue_properties properties = 0;
    EXPECT_EQ(CL_SUCCESS, queue->getCmdQInfo(CL_QUEUE_PROPERTIES, sizeof(properties), &properties, nullptr));
    EXPECT_EQ(static_cast<cl_command_queue_properties>(CL_QUEUE_PROFILING_ENABLE | CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE), properties);
    EXPECT_TRUE(queue->isProfilingEnabled());
}

TEST_F(CommandQueueInfoFixture, givenQueueWhenQueryingDeviceDefaultThenReturnsNull) {
    auto queue = createQueue(nullptr);

    cl_command_queue queried = reinterpret_cast<cl_command_queue>(0x1234);
    size_t retSize = 0;
    EXPECT_EQ(CL_SUCCESS, queue->getCmdQInfo(CL_QUEUE_DEVICE_DEFAULT, sizeof(queried), &queried, &retSize));
    EXPECT_EQ(sizeof(cl_command_queue), retSize);
    EXPECT_EQ(nullptr, queried);
}

TEST_F(CommandQueueInfoFixture, givenQueueWhenQueryingQueueSizeThenReturnsInvalidCommandQueue) {
    auto queue = createQueue(nullptr);

    cl_uint size = 0;
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, queue->getCmdQInfo(CL_QUEUE_SIZE, sizeof(size), &size, nullptr));
}

TEST_F(CommandQueueInfoFixture, givenPropertiesWhenQueryingPropertiesArrayThenStoredArrayIsReturnedNullTerminated) {
    cl_queue_properties queueProperties[] = {CL_QUEUE_PROPERTIES, CL_QUEUE_PROFILING_ENABLE, 0};
    auto queue = createQueue(queueProperties);

    size_t retSize = 0;
    ASSERT_EQ(CL_SUCCESS, queue->getCmdQInfo(CL_QUEUE_PROPERTIES_ARRAY, 0, nullptr, &retSize));
    ASSERT_EQ(3u * sizeof(cl_queue_properties), retSize);

    std::vector<cl_queue_properties> stored(retSize / sizeof(cl_queue_properties));
    ASSERT_EQ(CL_SUCCESS, queue->getCmdQInfo(CL_QUEUE_PROPERTIES_ARRAY, retSize, stored.data(), nullptr));
    EXPECT_EQ(static_cast<cl_queue_properties>(CL_QUEUE_PROPERTIES), stored[0]);
    EXPECT_EQ(static_cast<cl_queue_properties>(CL_QUEUE_PROFILING_ENABLE), stored[1]);
    EXPECT_EQ(0u, stored[2]);
}

TEST_F(CommandQueueInfoFixture, givenNoPropertiesWhenQueryingPropertiesArrayThenNothingIsStored) {
    auto queue = createQueue(nullptr);

    size_t retSize = 42u;
    EXPECT_EQ(CL_SUCCESS, queue->getCmdQInfo(CL_QUEUE_PROPERTIES_ARRAY, 0, nullptr, &retSize));
    EXPECT_EQ(0u, retSize);
}

TEST_F(CommandQueueInfoFixture, givenFamilyAndIndexPropertiesWhenQueryingThenStoredValuesAreReturned) {
    cl_queue_properties queueProperties[] = {CL_QUEUE_FAMILY_INTEL, 3, CL_QUEUE_INDEX_INTEL, 5, 0};
    auto queue = createQueue(queueProperties);

    cl_uint family = 0;
    cl_uint index = 0;
    EXPECT_EQ(CL_SUCCESS, queue->getCmdQInfo(CL_QUEUE_FAMILY_INTEL, sizeof(family), &family, nullptr));
    EXPECT_EQ(CL_SUCCESS, queue->getCmdQInfo(CL_QUEUE_INDEX_INTEL, sizeof(index), &index, nullptr));
    EXPECT_EQ(3u, family);
    EXPECT_EQ(5u, index);
}

TEST_F(CommandQueueInfoFixture, givenNoFamilyPropertiesWhenQueryingThenZeroIsReturned) {
    auto queue = createQueue(nullptr);

    cl_uint family = 0xFFu;
    cl_uint index = 0xFFu;
    EXPECT_EQ(CL_SUCCESS, queue->getCmdQInfo(CL_QUEUE_FAMILY_INTEL, sizeof(family), &family, nullptr));
    EXPECT_EQ(CL_SUCCESS, queue->getCmdQInfo(CL_QUEUE_INDEX_INTEL, sizeof(index), &index, nullptr));
    EXPECT_EQ(0u, family);
    EXPECT_EQ(0u, index);
}

TEST_F(CommandQueueInfoFixture, givenQueueWhenQueryingImmediateCommandListHandleThenUnderlyingHandleIsReturned) {
    auto queue = createQueue(nullptr);

    ze_command_list_handle_t queried = nullptr;
    size_t retSize = 0;
    EXPECT_EQ(CL_SUCCESS, queue->getCmdQInfo(CL_L0_IMMEDIATE_CMD_LIST_HANDLE, sizeof(queried), &queried, &retSize));
    EXPECT_EQ(sizeof(ze_command_list_handle_t), retSize);
    EXPECT_EQ(capturingCmdList.toHandle(), queried);
    EXPECT_EQ(queue->getL0Handle(), queried);
}

TEST_F(CommandQueueInfoFixture, givenUnknownParamWhenQueryingCmdQueueInfoThenReturnsInvalidValue) {
    auto queue = createQueue(nullptr);

    size_t retSize = 99u;
    EXPECT_EQ(CL_INVALID_VALUE, queue->getCmdQInfo(0xDEAD0000u, 0, nullptr, &retSize));
    EXPECT_EQ(99u, retSize);
}

TEST_F(CommandQueueInfoFixture, givenTooSmallBufferWhenQueryingCmdQueueInfoThenReturnsInvalidValue) {
    auto queue = createQueue(nullptr);

    cl_context queried = nullptr;
    EXPECT_EQ(CL_INVALID_VALUE, queue->getCmdQInfo(CL_QUEUE_CONTEXT, sizeof(cl_context) - 1, &queried, nullptr));
}

TEST_F(CommandQueueInfoFixture, givenSizeOnlyQueryWhenQueryingScalarParamsThenSizesAreReported) {
    auto queue = createQueue(nullptr);

    const std::pair<cl_command_queue_info, size_t> scalarParams[] = {
        {CL_QUEUE_CONTEXT, sizeof(cl_context)},
        {CL_QUEUE_DEVICE, sizeof(cl_device_id)},
        {CL_QUEUE_REFERENCE_COUNT, sizeof(cl_int)},
        {CL_QUEUE_PROPERTIES, sizeof(cl_command_queue_properties)},
        {CL_QUEUE_DEVICE_DEFAULT, sizeof(cl_command_queue)},
        {CL_QUEUE_FAMILY_INTEL, sizeof(cl_uint)},
        {CL_QUEUE_INDEX_INTEL, sizeof(cl_uint)},
        {CL_L0_IMMEDIATE_CMD_LIST_HANDLE, sizeof(ze_command_list_handle_t)}};

    for (const auto &[paramName, expectedSize] : scalarParams) {
        size_t retSize = 0;
        EXPECT_EQ(CL_SUCCESS, queue->getCmdQInfo(paramName, 0, nullptr, &retSize)) << "param 0x" << std::hex << paramName;
        EXPECT_EQ(expectedSize, retSize) << "param 0x" << std::hex << paramName;
    }
}

TEST_F(CommandQueueInfoFixture, givenQueueWhenCreatedThenAccessorsMatchConstructionArguments) {
    auto queue = createQueue(nullptr);

    EXPECT_EQ(context.get(), queue->getContext());
    EXPECT_EQ(clDevice, queue->getDevice());
    EXPECT_EQ(capturingCmdList.toHandle(), queue->getL0Handle());
    EXPECT_FALSE(queue->isPerfCountersEnabled());
    EXPECT_FALSE(queue->hasDependencies());
}

TEST_F(CommandQueueInfoFixture, givenUnrelatedPropertiesWhenQueryingProfilingThenItStaysDisabled) {
    cl_queue_properties queueProperties[] = {CL_QUEUE_FAMILY_INTEL, 1, CL_QUEUE_INDEX_INTEL, 2, 0};
    auto queue = createQueue(queueProperties);

    EXPECT_FALSE(queue->isProfilingEnabled());

    cl_command_queue_properties properties = 0xFFu;
    EXPECT_EQ(CL_SUCCESS, queue->getCmdQInfo(CL_QUEUE_PROPERTIES, sizeof(properties), &properties, nullptr));
    EXPECT_EQ(0u, properties);
}

} // namespace ult
} // namespace LEO
} // namespace NEO
