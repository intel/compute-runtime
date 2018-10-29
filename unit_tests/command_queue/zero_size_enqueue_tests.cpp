/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/event/event.h"
#include "runtime/memory_manager/svm_memory_manager.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/fixtures/image_fixture.h"
#include "unit_tests/mocks/mock_command_queue.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_kernel.h"
#include "unit_tests/mocks/mock_buffer.h"

#include "test.h"

using namespace OCLRT;

class ZeroSizeEnqueueHandlerTest : public DeviceFixture,
                                   public testing::Test {
  public:
    void SetUp() override {
        DeviceFixture::SetUp();
    }

    void TearDown() override {
        DeviceFixture::TearDown();
    }
    MockContext context;
    cl_int retVal;
};

HWTEST_F(ZeroSizeEnqueueHandlerTest, enqueueKernelWhenZeroSizeEnqueueIsDetectedThenCommandMarkerShouldBeEnqueued) {
    auto mockCmdQ = std::unique_ptr<MockCommandQueueHw<FamilyType>>(new MockCommandQueueHw<FamilyType>(&context, pDevice, 0));

    MockKernelWithInternals mockKernel(*pDevice);

    size_t *nullGWS1 = nullptr;
    mockCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, nullGWS1, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    size_t *nullGWS2 = nullptr;
    mockCmdQ->enqueueKernel(mockKernel.mockKernel, 2, nullptr, nullGWS2, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    size_t *nullGWS3 = nullptr;
    mockCmdQ->enqueueKernel(mockKernel.mockKernel, 3, nullptr, nullGWS3, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    size_t zeroGWS0 = 0;
    mockCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, &zeroGWS0, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    size_t zeroGWS00[] = {0, 0};
    mockCmdQ->enqueueKernel(mockKernel.mockKernel, 2, nullptr, zeroGWS00, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    size_t zeroGWS01[] = {0, 1};
    mockCmdQ->enqueueKernel(mockKernel.mockKernel, 2, nullptr, zeroGWS01, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    size_t zeroGWS10[] = {1, 0};
    mockCmdQ->enqueueKernel(mockKernel.mockKernel, 2, nullptr, zeroGWS10, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    size_t zeroGWS000[] = {0, 0, 0};
    mockCmdQ->enqueueKernel(mockKernel.mockKernel, 3, nullptr, zeroGWS000, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    size_t zeroGWS011[] = {0, 1, 1};
    mockCmdQ->enqueueKernel(mockKernel.mockKernel, 3, nullptr, zeroGWS011, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    size_t zeroGWS101[] = {1, 0, 1};
    mockCmdQ->enqueueKernel(mockKernel.mockKernel, 3, nullptr, zeroGWS101, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    size_t zeroGWS110[] = {1, 1, 0};
    mockCmdQ->enqueueKernel(mockKernel.mockKernel, 3, nullptr, zeroGWS110, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    size_t zeroGWS001[] = {0, 0, 1};
    mockCmdQ->enqueueKernel(mockKernel.mockKernel, 3, nullptr, zeroGWS001, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    size_t zeroGWS010[] = {0, 1, 0};
    mockCmdQ->enqueueKernel(mockKernel.mockKernel, 3, nullptr, zeroGWS010, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    size_t zeroGWS100[] = {1, 0, 0};
    mockCmdQ->enqueueKernel(mockKernel.mockKernel, 3, nullptr, zeroGWS100, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);
}

HWTEST_F(ZeroSizeEnqueueHandlerTest, enqueueKernelWhenZeroSizeEnqueueIsDetectedThenEventCommandTypeShoudBeUnchanged) {
    auto mockCmdQ = std::unique_ptr<MockCommandQueueHw<FamilyType>>(new MockCommandQueueHw<FamilyType>(&context, pDevice, 0));

    cl_event event;
    MockKernelWithInternals mockKernel(*pDevice);
    size_t zeroGWS[] = {0, 0, 0};
    mockCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, zeroGWS, nullptr, 0, nullptr, &event);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    auto pEvent = (Event *)event;
    cl_command_type cmdType = 0;
    size_t sizeReturned = 0;
    auto result = clGetEventInfo(pEvent, CL_EVENT_COMMAND_TYPE, sizeof(cmdType), &cmdType, &sizeReturned);
    ASSERT_EQ(CL_SUCCESS, result);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_NDRANGE_KERNEL), cmdType);
    EXPECT_EQ(sizeof(cl_command_type), sizeReturned);
    delete pEvent;
}

HWTEST_F(ZeroSizeEnqueueHandlerTest, enqueueReadBufferWhenZeroSizeEnqueueIsDetectedThenCommandMarkerShouldBeEnqueued) {
    auto mockCmdQ = std::unique_ptr<MockCommandQueueHw<FamilyType>>(new MockCommandQueueHw<FamilyType>(&context, pDevice, 0));

    MockBuffer buffer;
    size_t memory[1];
    size_t zeroSize = 0;
    mockCmdQ->enqueueReadBuffer(&buffer, CL_FALSE, 0, zeroSize, memory, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);
}

HWTEST_F(ZeroSizeEnqueueHandlerTest, enqueueReadBufferWhenZeroSizeEnqueueIsDetectedThenEventCommandTypeShouldBeUnchanged) {
    auto mockCmdQ = std::unique_ptr<MockCommandQueueHw<FamilyType>>(new MockCommandQueueHw<FamilyType>(&context, pDevice, 0));

    cl_event event;
    MockBuffer buffer;
    size_t memory[1];
    size_t zeroSize = 0;
    mockCmdQ->enqueueReadBuffer(&buffer, CL_FALSE, 0, zeroSize, memory, 0, nullptr, &event);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    auto pEvent = (Event *)event;
    cl_command_type cmdType = 0;
    size_t sizeReturned = 0;
    auto result = clGetEventInfo(pEvent, CL_EVENT_COMMAND_TYPE, sizeof(cmdType), &cmdType, &sizeReturned);
    ASSERT_EQ(CL_SUCCESS, result);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_READ_BUFFER), cmdType);
    EXPECT_EQ(sizeof(cl_command_type), sizeReturned);
    delete pEvent;
}

HWTEST_F(ZeroSizeEnqueueHandlerTest, enqueueReadBufferRectWhenZeroSizeEnqueueIsDetectedThenCommandMarkerShouldBeEnqueued) {
    auto mockCmdQ = std::unique_ptr<MockCommandQueueHw<FamilyType>>(new MockCommandQueueHw<FamilyType>(&context, pDevice, 0));

    MockBuffer buffer;
    size_t memory[1];
    size_t bufferOrigin[] = {0, 0, 0};
    size_t hostOrigin[] = {0, 0, 0};

    size_t zeroRegion000[] = {0, 0, 0};
    mockCmdQ->enqueueReadBufferRect(&buffer, CL_FALSE, bufferOrigin, hostOrigin, zeroRegion000, 0, 0, 0, 0, memory, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    size_t zeroRegion011[] = {0, 1, 1};
    mockCmdQ->enqueueReadBufferRect(&buffer, CL_FALSE, bufferOrigin, hostOrigin, zeroRegion011, 0, 0, 0, 0, memory, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    size_t zeroRegion101[] = {1, 0, 1};
    mockCmdQ->enqueueReadBufferRect(&buffer, CL_FALSE, bufferOrigin, hostOrigin, zeroRegion101, 0, 0, 0, 0, memory, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    size_t zeroRegion110[] = {1, 1, 0};
    mockCmdQ->enqueueReadBufferRect(&buffer, CL_FALSE, bufferOrigin, hostOrigin, zeroRegion110, 0, 0, 0, 0, memory, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    size_t zeroRegion001[] = {0, 0, 1};
    mockCmdQ->enqueueReadBufferRect(&buffer, CL_FALSE, bufferOrigin, hostOrigin, zeroRegion001, 0, 0, 0, 0, memory, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    size_t zeroRegion010[] = {0, 1, 0};
    mockCmdQ->enqueueReadBufferRect(&buffer, CL_FALSE, bufferOrigin, hostOrigin, zeroRegion010, 0, 0, 0, 0, memory, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    size_t zeroRegion100[] = {1, 0, 0};
    mockCmdQ->enqueueReadBufferRect(&buffer, CL_FALSE, bufferOrigin, hostOrigin, zeroRegion100, 0, 0, 0, 0, memory, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);
}

HWTEST_F(ZeroSizeEnqueueHandlerTest, enqueueReadBufferRectWhenZeroSizeEnqueueIsDetectedThenEventCommandTypeShouldBeUnchanged) {
    auto mockCmdQ = std::unique_ptr<MockCommandQueueHw<FamilyType>>(new MockCommandQueueHw<FamilyType>(&context, pDevice, 0));

    cl_event event;
    MockBuffer buffer;
    size_t memory[1];
    size_t bufferOrigin[] = {0, 0, 0};
    size_t hostOrigin[] = {0, 0, 0};
    size_t zeroRegion[] = {0, 0, 0};
    mockCmdQ->enqueueReadBufferRect(&buffer, CL_FALSE, bufferOrigin, hostOrigin, zeroRegion, 0, 0, 0, 0, memory, 0, nullptr, &event);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    auto pEvent = (Event *)event;
    cl_command_type cmdType = 0;
    size_t sizeReturned = 0;
    auto result = clGetEventInfo(pEvent, CL_EVENT_COMMAND_TYPE, sizeof(cmdType), &cmdType, &sizeReturned);
    ASSERT_EQ(CL_SUCCESS, result);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_READ_BUFFER_RECT), cmdType);
    EXPECT_EQ(sizeof(cl_command_type), sizeReturned);
    delete pEvent;
}

HWTEST_F(ZeroSizeEnqueueHandlerTest, enqueueWriteBufferWhenZeroSizeEnqueueIsDetectedThenCommandMarkerShouldBeEnqueued) {
    auto mockCmdQ = std::unique_ptr<MockCommandQueueHw<FamilyType>>(new MockCommandQueueHw<FamilyType>(&context, pDevice, 0));

    MockBuffer buffer;
    size_t memory[1];
    size_t zeroSize = 0;
    mockCmdQ->enqueueWriteBuffer(&buffer, CL_FALSE, 0, zeroSize, memory, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);
}

HWTEST_F(ZeroSizeEnqueueHandlerTest, enqueueWriteBufferWhenZeroSizeEnqueueIsDetectedThenEventCommandTypeShouldBeUnchanged) {
    auto mockCmdQ = std::unique_ptr<MockCommandQueueHw<FamilyType>>(new MockCommandQueueHw<FamilyType>(&context, pDevice, 0));

    cl_event event;
    MockBuffer buffer;
    size_t memory[1];
    size_t zeroSize = 0;
    mockCmdQ->enqueueWriteBuffer(&buffer, CL_FALSE, 0, zeroSize, memory, 0, nullptr, &event);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    auto pEvent = (Event *)event;
    cl_command_type cmdType = 0;
    size_t sizeReturned = 0;
    auto result = clGetEventInfo(pEvent, CL_EVENT_COMMAND_TYPE, sizeof(cmdType), &cmdType, &sizeReturned);
    ASSERT_EQ(CL_SUCCESS, result);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_WRITE_BUFFER), cmdType);
    EXPECT_EQ(sizeof(cl_command_type), sizeReturned);
    delete pEvent;
}

HWTEST_F(ZeroSizeEnqueueHandlerTest, enqueueWriteBufferRectWhenZeroSizeEnqueueIsDetectedThenCommandMarkerShouldBeEnqueued) {
    auto mockCmdQ = std::unique_ptr<MockCommandQueueHw<FamilyType>>(new MockCommandQueueHw<FamilyType>(&context, pDevice, 0));

    MockBuffer buffer;
    size_t memory[1];
    size_t bufferOrigin[] = {0, 0, 0};
    size_t hostOrigin[] = {0, 0, 0};

    size_t zeroRegion000[] = {0, 0, 0};
    mockCmdQ->enqueueWriteBufferRect(&buffer, CL_FALSE, bufferOrigin, hostOrigin, zeroRegion000, 0, 0, 0, 0, memory, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    size_t zeroRegion011[] = {0, 1, 1};
    mockCmdQ->enqueueWriteBufferRect(&buffer, CL_FALSE, bufferOrigin, hostOrigin, zeroRegion011, 0, 0, 0, 0, memory, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    size_t zeroRegion101[] = {1, 0, 1};
    mockCmdQ->enqueueWriteBufferRect(&buffer, CL_FALSE, bufferOrigin, hostOrigin, zeroRegion101, 0, 0, 0, 0, memory, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    size_t zeroRegion110[] = {1, 1, 0};
    mockCmdQ->enqueueWriteBufferRect(&buffer, CL_FALSE, bufferOrigin, hostOrigin, zeroRegion110, 0, 0, 0, 0, memory, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    size_t zeroRegion001[] = {0, 0, 1};
    mockCmdQ->enqueueWriteBufferRect(&buffer, CL_FALSE, bufferOrigin, hostOrigin, zeroRegion001, 0, 0, 0, 0, memory, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    size_t zeroRegion010[] = {0, 1, 0};
    mockCmdQ->enqueueWriteBufferRect(&buffer, CL_FALSE, bufferOrigin, hostOrigin, zeroRegion010, 0, 0, 0, 0, memory, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    size_t zeroRegion100[] = {1, 0, 0};
    mockCmdQ->enqueueWriteBufferRect(&buffer, CL_FALSE, bufferOrigin, hostOrigin, zeroRegion100, 0, 0, 0, 0, memory, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);
}

HWTEST_F(ZeroSizeEnqueueHandlerTest, enqueueWriteBufferRectWhenZeroSizeEnqueueIsDetectedThenEventCommandTypeShouldBeUnchanged) {
    auto mockCmdQ = std::unique_ptr<MockCommandQueueHw<FamilyType>>(new MockCommandQueueHw<FamilyType>(&context, pDevice, 0));

    cl_event event;
    MockBuffer buffer;
    size_t memory[1];
    size_t bufferOrigin[] = {0, 0, 0};
    size_t hostOrigin[] = {0, 0, 0};
    size_t zeroRegion[] = {0, 0, 0};
    mockCmdQ->enqueueWriteBufferRect(&buffer, CL_FALSE, bufferOrigin, hostOrigin, zeroRegion, 0, 0, 0, 0, memory, 0, nullptr, &event);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    auto pEvent = (Event *)event;
    cl_command_type cmdType = 0;
    size_t sizeReturned = 0;
    auto result = clGetEventInfo(pEvent, CL_EVENT_COMMAND_TYPE, sizeof(cmdType), &cmdType, &sizeReturned);
    ASSERT_EQ(CL_SUCCESS, result);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_WRITE_BUFFER_RECT), cmdType);
    EXPECT_EQ(sizeof(cl_command_type), sizeReturned);
    delete pEvent;
}

HWTEST_F(ZeroSizeEnqueueHandlerTest, enqueueCopyBufferWhenZeroSizeEnqueueIsDetectedThenCommandMarkerShouldBeEnqueued) {
    auto mockCmdQ = std::unique_ptr<MockCommandQueueHw<FamilyType>>(new MockCommandQueueHw<FamilyType>(&context, pDevice, 0));

    MockBuffer srcBuffer;
    MockBuffer dstBuffer;
    size_t zeroSize = 0;
    mockCmdQ->enqueueCopyBuffer(&srcBuffer, &dstBuffer, 0, 0, zeroSize, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);
}

HWTEST_F(ZeroSizeEnqueueHandlerTest, enqueueCopyBufferWhenZeroSizeEnqueueIsDetectedThenEventCommandTypeShouldBeUnchanged) {
    auto mockCmdQ = std::unique_ptr<MockCommandQueueHw<FamilyType>>(new MockCommandQueueHw<FamilyType>(&context, pDevice, 0));

    cl_event event;
    MockBuffer srcBuffer;
    MockBuffer dstBuffer;
    size_t zeroSize = 0;
    mockCmdQ->enqueueCopyBuffer(&srcBuffer, &dstBuffer, 0, 0, zeroSize, 0, nullptr, &event);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    auto pEvent = (Event *)event;
    cl_command_type cmdType = 0;
    size_t sizeReturned = 0;
    auto result = clGetEventInfo(pEvent, CL_EVENT_COMMAND_TYPE, sizeof(cmdType), &cmdType, &sizeReturned);
    ASSERT_EQ(CL_SUCCESS, result);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_COPY_BUFFER), cmdType);
    EXPECT_EQ(sizeof(cl_command_type), sizeReturned);
    delete pEvent;
}

HWTEST_F(ZeroSizeEnqueueHandlerTest, enqueueCopyBufferRectWhenZeroSizeEnqueueIsDetectedThenCommandMarkerShouldBeEnqueued) {
    auto mockCmdQ = std::unique_ptr<MockCommandQueueHw<FamilyType>>(new MockCommandQueueHw<FamilyType>(&context, pDevice, 0));

    MockBuffer srcBuffer;
    MockBuffer dstBuffer;
    size_t srcOrigin[3] = {1024u, 1, 0};
    size_t dstOrigin[3] = {1024u, 1, 0};

    size_t zeroRegion000[3] = {0, 0, 0};
    mockCmdQ->enqueueCopyBufferRect(&srcBuffer, &dstBuffer, srcOrigin, dstOrigin, zeroRegion000, 0, 0, 0, 0, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    size_t zeroRegion011[3] = {0, 1, 1};
    mockCmdQ->enqueueCopyBufferRect(&srcBuffer, &dstBuffer, srcOrigin, dstOrigin, zeroRegion011, 0, 0, 0, 0, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    size_t zeroRegion101[3] = {1, 0, 1};
    mockCmdQ->enqueueCopyBufferRect(&srcBuffer, &dstBuffer, srcOrigin, dstOrigin, zeroRegion101, 0, 0, 0, 0, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    size_t zeroRegion110[3] = {1, 1, 0};
    mockCmdQ->enqueueCopyBufferRect(&srcBuffer, &dstBuffer, srcOrigin, dstOrigin, zeroRegion110, 0, 0, 0, 0, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    size_t zeroRegion001[3] = {0, 0, 1};
    mockCmdQ->enqueueCopyBufferRect(&srcBuffer, &dstBuffer, srcOrigin, dstOrigin, zeroRegion001, 0, 0, 0, 0, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    size_t zeroRegion010[3] = {0, 1, 0};
    mockCmdQ->enqueueCopyBufferRect(&srcBuffer, &dstBuffer, srcOrigin, dstOrigin, zeroRegion010, 0, 0, 0, 0, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    size_t zeroRegion100[3] = {1, 0, 0};
    mockCmdQ->enqueueCopyBufferRect(&srcBuffer, &dstBuffer, srcOrigin, dstOrigin, zeroRegion100, 0, 0, 0, 0, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);
}

HWTEST_F(ZeroSizeEnqueueHandlerTest, enqueueCopyBufferRectWhenZeroSizeEnqueueIsDetectedThenEventCommandTypeShouldBeUnchanged) {
    auto mockCmdQ = std::unique_ptr<MockCommandQueueHw<FamilyType>>(new MockCommandQueueHw<FamilyType>(&context, pDevice, 0));

    cl_event event;
    MockBuffer srcBuffer;
    MockBuffer dstBuffer;
    size_t srcOrigin[3] = {1024u, 1, 0};
    size_t dstOrigin[3] = {1024u, 1, 0};
    size_t zeroRegion[3] = {0, 0, 0};
    mockCmdQ->enqueueCopyBufferRect(&srcBuffer, &dstBuffer, srcOrigin, dstOrigin, zeroRegion, 0, 0, 0, 0, 0, nullptr, &event);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    auto pEvent = (Event *)event;
    cl_command_type cmdType = 0;
    size_t sizeReturned = 0;
    auto result = clGetEventInfo(pEvent, CL_EVENT_COMMAND_TYPE, sizeof(cmdType), &cmdType, &sizeReturned);
    ASSERT_EQ(CL_SUCCESS, result);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_COPY_BUFFER_RECT), cmdType);
    EXPECT_EQ(sizeof(cl_command_type), sizeReturned);
    delete pEvent;
}

HWTEST_F(ZeroSizeEnqueueHandlerTest, enqueueFillBufferWhenZeroSizeEnqueueIsDetectedThenCommandMarkerShouldBeEnqueued) {
    auto mockCmdQ = std::unique_ptr<MockCommandQueueHw<FamilyType>>(new MockCommandQueueHw<FamilyType>(&context, pDevice, 0));

    MockBuffer buffer;
    cl_int pattern = 0xDEADBEEF;
    size_t zeroSize = 0;
    mockCmdQ->enqueueFillBuffer(&buffer, &pattern, sizeof(pattern), 0, zeroSize, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);
}

HWTEST_F(ZeroSizeEnqueueHandlerTest, enqueueFillBufferWhenZeroSizeEnqueueIsDetectedTheEventCommandTypeShouldBeUnchanged) {
    auto mockCmdQ = std::unique_ptr<MockCommandQueueHw<FamilyType>>(new MockCommandQueueHw<FamilyType>(&context, pDevice, 0));

    cl_event event;
    MockBuffer buffer;
    cl_int pattern = 0xDEADBEEF;
    size_t zeroSize = 0;
    mockCmdQ->enqueueFillBuffer(&buffer, &pattern, sizeof(pattern), 0, zeroSize, 0, nullptr, &event);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    auto pEvent = (Event *)event;
    cl_command_type cmdType = 0;
    size_t sizeReturned = 0;
    auto result = clGetEventInfo(pEvent, CL_EVENT_COMMAND_TYPE, sizeof(cmdType), &cmdType, &sizeReturned);
    ASSERT_EQ(CL_SUCCESS, result);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_FILL_BUFFER), cmdType);
    EXPECT_EQ(sizeof(cl_command_type), sizeReturned);
    delete pEvent;
}

HWTEST_F(ZeroSizeEnqueueHandlerTest, enqueueReadImageWhenZeroSizeEnqueueIsDetectedThenCommandMarkerShouldBeEnqueued) {
    auto mockCmdQ = std::unique_ptr<MockCommandQueueHw<FamilyType>>(new MockCommandQueueHw<FamilyType>(&context, pDevice, 0));

    std::unique_ptr<Image> image(Image2dHelper<>::create(&context));
    size_t memory[1];
    size_t origin[3] = {1024u, 1, 0};

    size_t zeroRegion000[3] = {0, 0, 0};
    mockCmdQ->enqueueReadImage(image.get(), CL_FALSE, origin, zeroRegion000, 0, 0, memory, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    size_t zeroRegion011[3] = {0, 1, 1};
    mockCmdQ->enqueueReadImage(image.get(), CL_FALSE, origin, zeroRegion011, 0, 0, memory, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    size_t zeroRegion101[3] = {1, 0, 1};
    mockCmdQ->enqueueReadImage(image.get(), CL_FALSE, origin, zeroRegion101, 0, 0, memory, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    size_t zeroRegion110[3] = {1, 1, 0};
    mockCmdQ->enqueueReadImage(image.get(), CL_FALSE, origin, zeroRegion110, 0, 0, memory, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    size_t zeroRegion001[3] = {0, 0, 1};
    mockCmdQ->enqueueReadImage(image.get(), CL_FALSE, origin, zeroRegion001, 0, 0, memory, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    size_t zeroRegion010[3] = {0, 1, 0};
    mockCmdQ->enqueueReadImage(image.get(), CL_FALSE, origin, zeroRegion010, 0, 0, memory, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    size_t zeroRegion100[3] = {1, 0, 0};
    mockCmdQ->enqueueReadImage(image.get(), CL_FALSE, origin, zeroRegion100, 0, 0, memory, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);
}

HWTEST_F(ZeroSizeEnqueueHandlerTest, enqueueReadImageWhenZeroSizeEnqueueIsDetectedThenEventCommandTypeShouldBeUnchanged) {
    auto mockCmdQ = std::unique_ptr<MockCommandQueueHw<FamilyType>>(new MockCommandQueueHw<FamilyType>(&context, pDevice, 0));

    cl_event event;
    std::unique_ptr<Image> image(Image2dHelper<>::create(&context));
    size_t memory[1];
    size_t origin[3] = {1024u, 1, 0};
    size_t zeroRegion[3] = {0, 0, 0};
    mockCmdQ->enqueueReadImage(image.get(), CL_FALSE, origin, zeroRegion, 0, 0, memory, 0, nullptr, &event);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    auto pEvent = (Event *)event;
    cl_command_type cmdType = 0;
    size_t sizeReturned = 0;
    auto result = clGetEventInfo(pEvent, CL_EVENT_COMMAND_TYPE, sizeof(cmdType), &cmdType, &sizeReturned);
    ASSERT_EQ(CL_SUCCESS, result);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_READ_IMAGE), cmdType);
    EXPECT_EQ(sizeof(cl_command_type), sizeReturned);
    delete pEvent;
}

HWTEST_F(ZeroSizeEnqueueHandlerTest, enqueueWriteImageWhenZeroSizeEnqueueIsDetectedThenCommandMarkerShouldBeEnqueued) {
    auto mockCmdQ = std::unique_ptr<MockCommandQueueHw<FamilyType>>(new MockCommandQueueHw<FamilyType>(&context, pDevice, 0));

    std::unique_ptr<Image> image(Image2dHelper<>::create(&context));
    size_t memory[1];
    size_t origin[3] = {1024u, 1, 0};

    size_t zeroRegion000[3] = {0, 0, 0};
    mockCmdQ->enqueueWriteImage(image.get(), CL_FALSE, origin, zeroRegion000, 0, 0, memory, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    size_t zeroRegion011[3] = {0, 1, 1};
    mockCmdQ->enqueueWriteImage(image.get(), CL_FALSE, origin, zeroRegion011, 0, 0, memory, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    size_t zeroRegion101[3] = {1, 0, 1};
    mockCmdQ->enqueueWriteImage(image.get(), CL_FALSE, origin, zeroRegion101, 0, 0, memory, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    size_t zeroRegion110[3] = {1, 1, 0};
    mockCmdQ->enqueueWriteImage(image.get(), CL_FALSE, origin, zeroRegion110, 0, 0, memory, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    size_t zeroRegion001[3] = {0, 0, 1};
    mockCmdQ->enqueueWriteImage(image.get(), CL_FALSE, origin, zeroRegion001, 0, 0, memory, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    size_t zeroRegion010[3] = {0, 1, 0};
    mockCmdQ->enqueueWriteImage(image.get(), CL_FALSE, origin, zeroRegion010, 0, 0, memory, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    size_t zeroRegion100[3] = {1, 0, 0};
    mockCmdQ->enqueueWriteImage(image.get(), CL_FALSE, origin, zeroRegion100, 0, 0, memory, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);
}

HWTEST_F(ZeroSizeEnqueueHandlerTest, enqueueWriteImageWhenZeroSizeEnqueueIsDetectedThenEventCommandTypeShouldBeUnchanged) {
    auto mockCmdQ = std::unique_ptr<MockCommandQueueHw<FamilyType>>(new MockCommandQueueHw<FamilyType>(&context, pDevice, 0));

    cl_event event;
    std::unique_ptr<Image> image(Image2dHelper<>::create(&context));
    size_t memory[1];
    size_t origin[3] = {1024u, 1, 0};
    size_t zeroRegion[3] = {0, 0, 0};
    mockCmdQ->enqueueWriteImage(image.get(), CL_FALSE, origin, zeroRegion, 0, 0, memory, 0, nullptr, &event);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    auto pEvent = (Event *)event;
    cl_command_type cmdType = 0;
    size_t sizeReturned = 0;
    auto result = clGetEventInfo(pEvent, CL_EVENT_COMMAND_TYPE, sizeof(cmdType), &cmdType, &sizeReturned);
    ASSERT_EQ(CL_SUCCESS, result);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_WRITE_IMAGE), cmdType);
    EXPECT_EQ(sizeof(cl_command_type), sizeReturned);
    delete pEvent;
}

HWTEST_F(ZeroSizeEnqueueHandlerTest, enqueueCopyImageWhenZeroSizeEnqueueIsDetectedThenCommandMarkerShouldBeEnqueued) {
    auto mockCmdQ = std::unique_ptr<MockCommandQueueHw<FamilyType>>(new MockCommandQueueHw<FamilyType>(&context, pDevice, 0));

    std::unique_ptr<Image> srcImage(Image2dHelper<>::create(&context));
    std::unique_ptr<Image> dstImage(Image2dHelper<>::create(&context));
    size_t srcOrigin[3] = {1024u, 1, 0};
    size_t dstOrigin[3] = {1024u, 1, 0};

    size_t zeroRegion000[3] = {0, 0, 0};
    mockCmdQ->enqueueCopyImage(srcImage.get(), dstImage.get(), srcOrigin, dstOrigin, zeroRegion000, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    size_t zeroRegion011[3] = {0, 1, 1};
    mockCmdQ->enqueueCopyImage(srcImage.get(), dstImage.get(), srcOrigin, dstOrigin, zeroRegion011, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    size_t zeroRegion101[3] = {1, 0, 1};
    mockCmdQ->enqueueCopyImage(srcImage.get(), dstImage.get(), srcOrigin, dstOrigin, zeroRegion101, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    size_t zeroRegion110[3] = {1, 1, 0};
    mockCmdQ->enqueueCopyImage(srcImage.get(), dstImage.get(), srcOrigin, dstOrigin, zeroRegion110, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    size_t zeroRegion001[3] = {0, 0, 1};
    mockCmdQ->enqueueCopyImage(srcImage.get(), dstImage.get(), srcOrigin, dstOrigin, zeroRegion001, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    size_t zeroRegion010[3] = {0, 1, 0};
    mockCmdQ->enqueueCopyImage(srcImage.get(), dstImage.get(), srcOrigin, dstOrigin, zeroRegion010, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    size_t zeroRegion100[3] = {1, 0, 0};
    mockCmdQ->enqueueCopyImage(srcImage.get(), dstImage.get(), srcOrigin, dstOrigin, zeroRegion100, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);
}

HWTEST_F(ZeroSizeEnqueueHandlerTest, enqueueCopyImageWhenZeroSizeEnqueueIsDetectedThenEventCommandTypeShouldBeUnchanged) {
    auto mockCmdQ = std::unique_ptr<MockCommandQueueHw<FamilyType>>(new MockCommandQueueHw<FamilyType>(&context, pDevice, 0));

    cl_event event;
    std::unique_ptr<Image> srcImage(Image2dHelper<>::create(&context));
    std::unique_ptr<Image> dstImage(Image2dHelper<>::create(&context));
    size_t srcOrigin[3] = {1024u, 1, 0};
    size_t dstOrigin[3] = {1024u, 1, 0};
    size_t zeroRegion[3] = {0, 0, 0};
    mockCmdQ->enqueueCopyImage(srcImage.get(), dstImage.get(), srcOrigin, dstOrigin, zeroRegion, 0, nullptr, &event);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    auto pEvent = (Event *)event;
    cl_command_type cmdType = 0;
    size_t sizeReturned = 0;
    auto result = clGetEventInfo(pEvent, CL_EVENT_COMMAND_TYPE, sizeof(cmdType), &cmdType, &sizeReturned);
    ASSERT_EQ(CL_SUCCESS, result);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_COPY_IMAGE), cmdType);
    EXPECT_EQ(sizeof(cl_command_type), sizeReturned);
    delete pEvent;
}

HWTEST_F(ZeroSizeEnqueueHandlerTest, enqueueCopyImageToBufferWhenZeroSizeEnqueueIsDetectedThenCommandMarkerShouldBeEnqueued) {
    auto mockCmdQ = std::unique_ptr<MockCommandQueueHw<FamilyType>>(new MockCommandQueueHw<FamilyType>(&context, pDevice, 0));

    std::unique_ptr<Image> srcImage(Image2dHelper<>::create(&context));
    std::unique_ptr<Buffer> dstBuffer(Buffer::create(&context, CL_MEM_READ_WRITE, 1024u, nullptr, retVal));
    size_t srcOrigin[3] = {1024u, 1, 0};

    size_t zeroRegion000[3] = {0, 0, 0};
    mockCmdQ->enqueueCopyImageToBuffer(srcImage.get(), dstBuffer.get(), srcOrigin, zeroRegion000, 0, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    size_t zeroRegion011[3] = {0, 1, 1};
    mockCmdQ->enqueueCopyImageToBuffer(srcImage.get(), dstBuffer.get(), srcOrigin, zeroRegion011, 0, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    size_t zeroRegion101[3] = {1, 0, 1};
    mockCmdQ->enqueueCopyImageToBuffer(srcImage.get(), dstBuffer.get(), srcOrigin, zeroRegion101, 0, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    size_t zeroRegion110[3] = {1, 1, 0};
    mockCmdQ->enqueueCopyImageToBuffer(srcImage.get(), dstBuffer.get(), srcOrigin, zeroRegion110, 0, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    size_t zeroRegion001[3] = {0, 0, 1};
    mockCmdQ->enqueueCopyImageToBuffer(srcImage.get(), dstBuffer.get(), srcOrigin, zeroRegion001, 0, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    size_t zeroRegion010[3] = {0, 1, 0};
    mockCmdQ->enqueueCopyImageToBuffer(srcImage.get(), dstBuffer.get(), srcOrigin, zeroRegion010, 0, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    size_t zeroRegion100[3] = {1, 0, 0};
    mockCmdQ->enqueueCopyImageToBuffer(srcImage.get(), dstBuffer.get(), srcOrigin, zeroRegion100, 0, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);
}

HWTEST_F(ZeroSizeEnqueueHandlerTest, enqueueCopyImageToBufferWhenZeroSizeEnqueueIsDetectedThenEventCommandTypeShouldBeUnchanged) {
    auto mockCmdQ = std::unique_ptr<MockCommandQueueHw<FamilyType>>(new MockCommandQueueHw<FamilyType>(&context, pDevice, 0));

    cl_event event;
    std::unique_ptr<Image> srcImage(Image2dHelper<>::create(&context));
    std::unique_ptr<Buffer> dstBuffer(Buffer::create(&context, CL_MEM_READ_WRITE, 1024u, nullptr, retVal));
    size_t srcOrigin[3] = {1024u, 1, 0};
    size_t zeroRegion[3] = {0, 0, 0};
    mockCmdQ->enqueueCopyImageToBuffer(srcImage.get(), dstBuffer.get(), srcOrigin, zeroRegion, 0, 0, nullptr, &event);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    auto pEvent = (Event *)event;
    cl_command_type cmdType = 0;
    size_t sizeReturned = 0;
    auto result = clGetEventInfo(pEvent, CL_EVENT_COMMAND_TYPE, sizeof(cmdType), &cmdType, &sizeReturned);
    ASSERT_EQ(CL_SUCCESS, result);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_COPY_IMAGE_TO_BUFFER), cmdType);
    EXPECT_EQ(sizeof(cl_command_type), sizeReturned);
    delete pEvent;
}

HWTEST_F(ZeroSizeEnqueueHandlerTest, enqueueCopyBufferToImageWhenZeroSizeEnqueueIsDetectedThenCommandMarkerShouldBeEnqueued) {
    auto mockCmdQ = std::unique_ptr<MockCommandQueueHw<FamilyType>>(new MockCommandQueueHw<FamilyType>(&context, pDevice, 0));

    std::unique_ptr<Buffer> srcBuffer(Buffer::create(&context, CL_MEM_READ_WRITE, 1024u, nullptr, retVal));
    std::unique_ptr<Image> dstImage(Image2dHelper<>::create(&context));
    size_t dstOrigin[3] = {1024u, 1, 0};

    size_t zeroRegion000[3] = {0, 0, 0};
    mockCmdQ->enqueueCopyBufferToImage(srcBuffer.get(), dstImage.get(), 0, dstOrigin, zeroRegion000, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    size_t zeroRegion011[3] = {0, 1, 1};
    mockCmdQ->enqueueCopyBufferToImage(srcBuffer.get(), dstImage.get(), 0, dstOrigin, zeroRegion011, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    size_t zeroRegion101[3] = {1, 0, 1};
    mockCmdQ->enqueueCopyBufferToImage(srcBuffer.get(), dstImage.get(), 0, dstOrigin, zeroRegion101, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    size_t zeroRegion110[3] = {1, 1, 0};
    mockCmdQ->enqueueCopyBufferToImage(srcBuffer.get(), dstImage.get(), 0, dstOrigin, zeroRegion110, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    size_t zeroRegion001[3] = {0, 0, 1};
    mockCmdQ->enqueueCopyBufferToImage(srcBuffer.get(), dstImage.get(), 0, dstOrigin, zeroRegion001, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    size_t zeroRegion010[3] = {0, 1, 0};
    mockCmdQ->enqueueCopyBufferToImage(srcBuffer.get(), dstImage.get(), 0, dstOrigin, zeroRegion010, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    size_t zeroRegion100[3] = {1, 0, 0};
    mockCmdQ->enqueueCopyBufferToImage(srcBuffer.get(), dstImage.get(), 0, dstOrigin, zeroRegion100, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);
}

HWTEST_F(ZeroSizeEnqueueHandlerTest, enqueueCopyBufferToImageWhenZeroSizeEnqueueIsDetectedThenEventCommandTypeShouldBeUnchanged) {
    auto mockCmdQ = std::unique_ptr<MockCommandQueueHw<FamilyType>>(new MockCommandQueueHw<FamilyType>(&context, pDevice, 0));

    cl_event event;
    std::unique_ptr<Buffer> srcBuffer(Buffer::create(&context, CL_MEM_READ_WRITE, 1024u, nullptr, retVal));
    std::unique_ptr<Image> dstImage(Image2dHelper<>::create(&context));
    size_t dstOrigin[3] = {1024u, 1, 0};
    size_t zeroRegion[3] = {0, 0, 0};
    mockCmdQ->enqueueCopyBufferToImage(srcBuffer.get(), dstImage.get(), 0, dstOrigin, zeroRegion, 0, nullptr, &event);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    auto pEvent = (Event *)event;
    cl_command_type cmdType = 0;
    size_t sizeReturned = 0;
    auto result = clGetEventInfo(pEvent, CL_EVENT_COMMAND_TYPE, sizeof(cmdType), &cmdType, &sizeReturned);
    ASSERT_EQ(CL_SUCCESS, result);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_COPY_BUFFER_TO_IMAGE), cmdType);
    EXPECT_EQ(sizeof(cl_command_type), sizeReturned);
    delete pEvent;
}

HWTEST_F(ZeroSizeEnqueueHandlerTest, enqueueFillImageWhenZeroSizeEnqueueIsDetectedThenCommandMarkerShouldBeEnqueued) {
    auto mockCmdQ = std::unique_ptr<MockCommandQueueHw<FamilyType>>(new MockCommandQueueHw<FamilyType>(&context, pDevice, 0));

    std::unique_ptr<Image> image(Image2dHelper<>::create(&context));
    size_t origin[3] = {1024u, 1, 1};
    int32_t fillColor[4] = {0xCC, 0xCC, 0xCC, 0xCC};

    size_t zeroRegion000[3] = {0, 0, 0};
    mockCmdQ->enqueueFillImage(image.get(), &fillColor, origin, zeroRegion000, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    size_t zeroRegion011[3] = {0, 1, 1};
    mockCmdQ->enqueueFillImage(image.get(), &fillColor, origin, zeroRegion011, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    size_t zeroRegion101[3] = {1, 0, 1};
    mockCmdQ->enqueueFillImage(image.get(), &fillColor, origin, zeroRegion101, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    size_t zeroRegion110[3] = {1, 1, 0};
    mockCmdQ->enqueueFillImage(image.get(), &fillColor, origin, zeroRegion110, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    size_t zeroRegion001[3] = {0, 0, 1};
    mockCmdQ->enqueueFillImage(image.get(), &fillColor, origin, zeroRegion001, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    size_t zeroRegion010[3] = {0, 1, 0};
    mockCmdQ->enqueueFillImage(image.get(), &fillColor, origin, zeroRegion010, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    size_t zeroRegion100[3] = {1, 0, 0};
    mockCmdQ->enqueueFillImage(image.get(), &fillColor, origin, zeroRegion100, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);
}

HWTEST_F(ZeroSizeEnqueueHandlerTest, enqueueFillImageWhenZeroSizeEnqueueIsDetectedThenEventCommandTypeShouldBeUnchanged) {
    auto mockCmdQ = std::unique_ptr<MockCommandQueueHw<FamilyType>>(new MockCommandQueueHw<FamilyType>(&context, pDevice, 0));

    cl_event event;
    std::unique_ptr<Image> image(Image2dHelper<>::create(&context));
    size_t origin[3] = {1024u, 1, 1};
    int32_t fillColor[4] = {0xCC, 0xCC, 0xCC, 0xCC};
    size_t zeroRegion[3] = {0, 0, 0};
    mockCmdQ->enqueueFillImage(image.get(), &fillColor, origin, zeroRegion, 0, nullptr, &event);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    auto pEvent = (Event *)event;
    cl_command_type cmdType = 0;
    size_t sizeReturned = 0;
    auto result = clGetEventInfo(pEvent, CL_EVENT_COMMAND_TYPE, sizeof(cmdType), &cmdType, &sizeReturned);
    ASSERT_EQ(CL_SUCCESS, result);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_FILL_IMAGE), cmdType);
    EXPECT_EQ(sizeof(cl_command_type), sizeReturned);
    delete pEvent;
}

HWTEST_F(ZeroSizeEnqueueHandlerTest, enqueueSVMMemcpyWhenZeroSizeEnqueueIsDetectedThenCommandMarkerShouldBeEnqueued) {
    auto mockCmdQ = std::unique_ptr<MockCommandQueueHw<FamilyType>>(new MockCommandQueueHw<FamilyType>(&context, pDevice, 0));

    void *pSrcSVM = context.getSVMAllocsManager()->createSVMAlloc(256);
    void *pDstSVM = context.getSVMAllocsManager()->createSVMAlloc(256);
    size_t zeroSize = 0;
    mockCmdQ->enqueueSVMMemcpy(false, pSrcSVM, pDstSVM, zeroSize, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    context.getSVMAllocsManager()->freeSVMAlloc(pSrcSVM);
    context.getSVMAllocsManager()->freeSVMAlloc(pDstSVM);
}

HWTEST_F(ZeroSizeEnqueueHandlerTest, enqueueSVMMemcpyWhenZeroSizeEnqueueIsDetectedThenEventCommandTypeShouldBeUnchanged) {
    auto mockCmdQ = std::unique_ptr<MockCommandQueueHw<FamilyType>>(new MockCommandQueueHw<FamilyType>(&context, pDevice, 0));

    cl_event event;
    void *pSrcSVM = context.getSVMAllocsManager()->createSVMAlloc(256);
    void *pDstSVM = context.getSVMAllocsManager()->createSVMAlloc(256);
    size_t zeroSize = 0;
    mockCmdQ->enqueueSVMMemcpy(false, pSrcSVM, pDstSVM, zeroSize, 0, nullptr, &event);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    auto pEvent = (Event *)event;
    cl_command_type cmdType = 0;
    size_t sizeReturned = 0;
    auto result = clGetEventInfo(pEvent, CL_EVENT_COMMAND_TYPE, sizeof(cmdType), &cmdType, &sizeReturned);
    ASSERT_EQ(CL_SUCCESS, result);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_SVM_MEMCPY), cmdType);
    EXPECT_EQ(sizeof(cl_command_type), sizeReturned);
    delete pEvent;

    context.getSVMAllocsManager()->freeSVMAlloc(pSrcSVM);
    context.getSVMAllocsManager()->freeSVMAlloc(pDstSVM);
}

HWTEST_F(ZeroSizeEnqueueHandlerTest, enqueueSVMMemFillWhenZeroSizeEnqueueIsDetectedThenCommandMarkerShouldBeEnqueued) {
    auto mockCmdQ = std::unique_ptr<MockCommandQueueHw<FamilyType>>(new MockCommandQueueHw<FamilyType>(&context, pDevice, 0));

    void *pSVM = context.getSVMAllocsManager()->createSVMAlloc(256);
    const float pattern[1] = {1.2345f};
    size_t zeroSize = 0;
    mockCmdQ->enqueueSVMMemFill(pSVM, &pattern, sizeof(pattern), zeroSize, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    context.getSVMAllocsManager()->freeSVMAlloc(pSVM);
}

HWTEST_F(ZeroSizeEnqueueHandlerTest, enqueueSVMMemFillWhenZeroSizeEnqueueIsDetectedThenEventCommandTypeShouldBeUnchanged) {
    auto mockCmdQ = std::unique_ptr<MockCommandQueueHw<FamilyType>>(new MockCommandQueueHw<FamilyType>(&context, pDevice, 0));

    cl_event event;
    void *pSVM = context.getSVMAllocsManager()->createSVMAlloc(256);
    const float pattern[1] = {1.2345f};
    size_t zeroSize = 0;
    mockCmdQ->enqueueSVMMemFill(pSVM, &pattern, sizeof(pattern), zeroSize, 0, nullptr, &event);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    auto pEvent = (Event *)event;
    cl_command_type cmdType = 0;
    size_t sizeReturned = 0;
    auto result = clGetEventInfo(pEvent, CL_EVENT_COMMAND_TYPE, sizeof(cmdType), &cmdType, &sizeReturned);
    ASSERT_EQ(CL_SUCCESS, result);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_SVM_MEMFILL), cmdType);
    EXPECT_EQ(sizeof(cl_command_type), sizeReturned);
    delete pEvent;

    context.getSVMAllocsManager()->freeSVMAlloc(pSVM);
}