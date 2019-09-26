/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/memory_manager/unified_memory_manager.h"
#include "runtime/event/event.h"
#include "test.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/fixtures/image_fixture.h"
#include "unit_tests/mocks/mock_buffer.h"
#include "unit_tests/mocks/mock_command_queue.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_kernel.h"

using namespace NEO;

class ZeroSizeEnqueueHandlerTest : public Test<DeviceFixture> {
  public:
    MockContext context;
    cl_int retVal;
};

class ZeroSizeEnqueueHandlerTestZeroGws : public ZeroSizeEnqueueHandlerTest {
  public:
    void SetUp() override {
        ZeroSizeEnqueueHandlerTest::SetUp();

        testGwsInputs[0] = std::make_tuple(1, nullptr);
        testGwsInputs[1] = std::make_tuple(2, nullptr);
        testGwsInputs[2] = std::make_tuple(3, nullptr);
        testGwsInputs[3] = std::make_tuple(1, zeroGWS0);
        testGwsInputs[4] = std::make_tuple(2, zeroGWS00);
        testGwsInputs[5] = std::make_tuple(2, zeroGWS01);
        testGwsInputs[6] = std::make_tuple(2, zeroGWS10);
        testGwsInputs[7] = std::make_tuple(3, zeroGWS000);
        testGwsInputs[8] = std::make_tuple(3, zeroGWS011);
        testGwsInputs[9] = std::make_tuple(3, zeroGWS101);
        testGwsInputs[10] = std::make_tuple(3, zeroGWS110);
        testGwsInputs[11] = std::make_tuple(3, zeroGWS001);
        testGwsInputs[12] = std::make_tuple(3, zeroGWS010);
        testGwsInputs[13] = std::make_tuple(3, zeroGWS100);
    }

    size_t zeroGWS0[1] = {0};
    size_t zeroGWS00[2] = {0, 0};
    size_t zeroGWS01[2] = {0, 1};
    size_t zeroGWS10[2] = {1, 0};
    size_t zeroGWS000[3] = {0, 0, 0};
    size_t zeroGWS011[3] = {0, 1, 1};
    size_t zeroGWS101[3] = {1, 0, 1};
    size_t zeroGWS110[3] = {1, 1, 0};
    size_t zeroGWS001[3] = {0, 0, 1};
    size_t zeroGWS010[3] = {0, 1, 0};
    size_t zeroGWS100[3] = {1, 0, 0};

    std::tuple<unsigned int, size_t *> testGwsInputs[14];
};

HWTEST_F(ZeroSizeEnqueueHandlerTestZeroGws, enqueueKernelWhenZeroSizeEnqueueIsDetectedAndOpenCLAtLeast21ThenCommandMarkerShouldBeEnqueued) {
    pDevice->enabledClVersion = 21;

    auto mockCmdQ = std::unique_ptr<MockCommandQueueHw<FamilyType>>(new MockCommandQueueHw<FamilyType>(&context, pDevice, 0));

    MockKernelWithInternals mockKernel(*pDevice);

    for (auto testInput : testGwsInputs) {
        auto workDim = std::get<0>(testInput);
        auto gws = std::get<1>(testInput);
        mockCmdQ->lastCommandType = static_cast<cl_command_type>(CL_COMMAND_COPY_BUFFER);

        retVal = mockCmdQ->enqueueKernel(mockKernel.mockKernel, workDim, nullptr, gws, nullptr, 0, nullptr, nullptr);
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);
    }
}

HWTEST_F(ZeroSizeEnqueueHandlerTestZeroGws, enqueueKernelWhenZeroSizeEnqueueIsDetectedAndOpenCLIs20OrOlderThenErrorIsReturned) {
    int oclVersionsToTest[] = {12, 20};
    for (auto oclVersion : oclVersionsToTest) {
        pDevice->enabledClVersion = oclVersion;

        auto mockCmdQ = std::unique_ptr<MockCommandQueueHw<FamilyType>>(new MockCommandQueueHw<FamilyType>(&context, pDevice, 0));

        MockKernelWithInternals mockKernel(*pDevice);

        for (auto testInput : testGwsInputs) {
            auto workDim = std::get<0>(testInput);
            auto gws = std::get<1>(testInput);
            mockCmdQ->lastCommandType = static_cast<cl_command_type>(CL_COMMAND_COPY_BUFFER);

            retVal = mockCmdQ->enqueueKernel(mockKernel.mockKernel, workDim, nullptr, gws, nullptr, 0, nullptr, nullptr);
            EXPECT_EQ(CL_INVALID_GLOBAL_WORK_SIZE, retVal);
            EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_COPY_BUFFER), mockCmdQ->lastCommandType);
        }
    }
}

HWTEST_F(ZeroSizeEnqueueHandlerTestZeroGws, enqueueKernelWhenZeroSizeEnqueueIsDetectedAndLocalWorkSizeIsSetThenNoExceptionIsThrown) {
    auto mockCmdQ = std::unique_ptr<MockCommandQueueHw<FamilyType>>(new MockCommandQueueHw<FamilyType>(&context, pDevice, 0));

    MockKernelWithInternals mockKernel(*pDevice);
    mockKernel.mockProgram->setAllowNonUniform(true);

    auto workDim = 1;
    auto gws = zeroGWS0;
    size_t lws[1] = {1};

    EXPECT_NO_THROW(retVal = mockCmdQ->enqueueKernel(mockKernel.mockKernel, workDim, nullptr, gws, lws, 0, nullptr, nullptr));
    auto expected = (pDevice->getEnabledClVersion() < 21 ? CL_INVALID_GLOBAL_WORK_SIZE : CL_SUCCESS);
    EXPECT_EQ(expected, retVal);
}

HWTEST_F(ZeroSizeEnqueueHandlerTest, enqueueKernelWhenZeroSizeEnqueueIsDetectedThenEventCommandTypeShoudBeUnchanged) {
    if (pDevice->getEnabledClVersion() < 21) {
        return;
    }
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
    mockCmdQ->enqueueReadBuffer(&buffer, CL_FALSE, 0, zeroSize, memory, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);
}

HWTEST_F(ZeroSizeEnqueueHandlerTest, enqueueReadBufferWhenZeroSizeEnqueueIsDetectedThenEventCommandTypeShouldBeUnchanged) {
    auto mockCmdQ = std::unique_ptr<MockCommandQueueHw<FamilyType>>(new MockCommandQueueHw<FamilyType>(&context, pDevice, 0));

    cl_event event;
    MockBuffer buffer;
    size_t memory[1];
    size_t zeroSize = 0;
    mockCmdQ->enqueueReadBuffer(&buffer, CL_FALSE, 0, zeroSize, memory, nullptr, 0, nullptr, &event);
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
    mockCmdQ->enqueueWriteBuffer(&buffer, CL_FALSE, 0, zeroSize, memory, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);
}

HWTEST_F(ZeroSizeEnqueueHandlerTest, enqueueWriteBufferWhenZeroSizeEnqueueIsDetectedThenEventCommandTypeShouldBeUnchanged) {
    auto mockCmdQ = std::unique_ptr<MockCommandQueueHw<FamilyType>>(new MockCommandQueueHw<FamilyType>(&context, pDevice, 0));

    cl_event event;
    MockBuffer buffer;
    size_t memory[1];
    size_t zeroSize = 0;
    mockCmdQ->enqueueWriteBuffer(&buffer, CL_FALSE, 0, zeroSize, memory, nullptr, 0, nullptr, &event);
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
    mockCmdQ->enqueueReadImage(image.get(), CL_FALSE, origin, zeroRegion000, 0, 0, memory, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    size_t zeroRegion011[3] = {0, 1, 1};
    mockCmdQ->enqueueReadImage(image.get(), CL_FALSE, origin, zeroRegion011, 0, 0, memory, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    size_t zeroRegion101[3] = {1, 0, 1};
    mockCmdQ->enqueueReadImage(image.get(), CL_FALSE, origin, zeroRegion101, 0, 0, memory, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    size_t zeroRegion110[3] = {1, 1, 0};
    mockCmdQ->enqueueReadImage(image.get(), CL_FALSE, origin, zeroRegion110, 0, 0, memory, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    size_t zeroRegion001[3] = {0, 0, 1};
    mockCmdQ->enqueueReadImage(image.get(), CL_FALSE, origin, zeroRegion001, 0, 0, memory, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    size_t zeroRegion010[3] = {0, 1, 0};
    mockCmdQ->enqueueReadImage(image.get(), CL_FALSE, origin, zeroRegion010, 0, 0, memory, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    size_t zeroRegion100[3] = {1, 0, 0};
    mockCmdQ->enqueueReadImage(image.get(), CL_FALSE, origin, zeroRegion100, 0, 0, memory, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);
}

HWTEST_F(ZeroSizeEnqueueHandlerTest, enqueueReadImageWhenZeroSizeEnqueueIsDetectedThenEventCommandTypeShouldBeUnchanged) {
    auto mockCmdQ = std::unique_ptr<MockCommandQueueHw<FamilyType>>(new MockCommandQueueHw<FamilyType>(&context, pDevice, 0));

    cl_event event;
    std::unique_ptr<Image> image(Image2dHelper<>::create(&context));
    size_t memory[1];
    size_t origin[3] = {1024u, 1, 0};
    size_t zeroRegion[3] = {0, 0, 0};
    mockCmdQ->enqueueReadImage(image.get(), CL_FALSE, origin, zeroRegion, 0, 0, memory, nullptr, 0, nullptr, &event);
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
    mockCmdQ->enqueueWriteImage(image.get(), CL_FALSE, origin, zeroRegion000, 0, 0, memory, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    size_t zeroRegion011[3] = {0, 1, 1};
    mockCmdQ->enqueueWriteImage(image.get(), CL_FALSE, origin, zeroRegion011, 0, 0, memory, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    size_t zeroRegion101[3] = {1, 0, 1};
    mockCmdQ->enqueueWriteImage(image.get(), CL_FALSE, origin, zeroRegion101, 0, 0, memory, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    size_t zeroRegion110[3] = {1, 1, 0};
    mockCmdQ->enqueueWriteImage(image.get(), CL_FALSE, origin, zeroRegion110, 0, 0, memory, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    size_t zeroRegion001[3] = {0, 0, 1};
    mockCmdQ->enqueueWriteImage(image.get(), CL_FALSE, origin, zeroRegion001, 0, 0, memory, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    size_t zeroRegion010[3] = {0, 1, 0};
    mockCmdQ->enqueueWriteImage(image.get(), CL_FALSE, origin, zeroRegion010, 0, 0, memory, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    size_t zeroRegion100[3] = {1, 0, 0};
    mockCmdQ->enqueueWriteImage(image.get(), CL_FALSE, origin, zeroRegion100, 0, 0, memory, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);
}

HWTEST_F(ZeroSizeEnqueueHandlerTest, enqueueWriteImageWhenZeroSizeEnqueueIsDetectedThenEventCommandTypeShouldBeUnchanged) {
    auto mockCmdQ = std::unique_ptr<MockCommandQueueHw<FamilyType>>(new MockCommandQueueHw<FamilyType>(&context, pDevice, 0));

    cl_event event;
    std::unique_ptr<Image> image(Image2dHelper<>::create(&context));
    size_t memory[1];
    size_t origin[3] = {1024u, 1, 0};
    size_t zeroRegion[3] = {0, 0, 0};
    mockCmdQ->enqueueWriteImage(image.get(), CL_FALSE, origin, zeroRegion, 0, 0, memory, nullptr, 0, nullptr, &event);
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
    if (pDevice->getHardwareInfo().capabilityTable.ftrSvm == false) {
        GTEST_SKIP();
    }
    auto mockCmdQ = std::unique_ptr<MockCommandQueueHw<FamilyType>>(new MockCommandQueueHw<FamilyType>(&context, pDevice, 0));

    void *pSrcSVM = context.getSVMAllocsManager()->createSVMAlloc(256, {});
    void *pDstSVM = context.getSVMAllocsManager()->createSVMAlloc(256, {});
    size_t zeroSize = 0;
    mockCmdQ->enqueueSVMMemcpy(false, pSrcSVM, pDstSVM, zeroSize, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    context.getSVMAllocsManager()->freeSVMAlloc(pSrcSVM);
    context.getSVMAllocsManager()->freeSVMAlloc(pDstSVM);
}

HWTEST_F(ZeroSizeEnqueueHandlerTest, enqueueSVMMemcpyWhenZeroSizeEnqueueIsDetectedThenEventCommandTypeShouldBeUnchanged) {
    if (pDevice->getHardwareInfo().capabilityTable.ftrSvm == false) {
        GTEST_SKIP();
    }
    auto mockCmdQ = std::unique_ptr<MockCommandQueueHw<FamilyType>>(new MockCommandQueueHw<FamilyType>(&context, pDevice, 0));

    cl_event event;
    void *pSrcSVM = context.getSVMAllocsManager()->createSVMAlloc(256, {});
    void *pDstSVM = context.getSVMAllocsManager()->createSVMAlloc(256, {});
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
    if (pDevice->getHardwareInfo().capabilityTable.ftrSvm == false) {
        GTEST_SKIP();
    }
    auto mockCmdQ = std::unique_ptr<MockCommandQueueHw<FamilyType>>(new MockCommandQueueHw<FamilyType>(&context, pDevice, 0));

    void *pSVM = context.getSVMAllocsManager()->createSVMAlloc(256, {});
    const float pattern[1] = {1.2345f};
    size_t zeroSize = 0;
    mockCmdQ->enqueueSVMMemFill(pSVM, &pattern, sizeof(pattern), zeroSize, 0, nullptr, nullptr);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), mockCmdQ->lastCommandType);

    context.getSVMAllocsManager()->freeSVMAlloc(pSVM);
}

HWTEST_F(ZeroSizeEnqueueHandlerTest, enqueueSVMMemFillWhenZeroSizeEnqueueIsDetectedThenEventCommandTypeShouldBeUnchanged) {
    if (pDevice->getHardwareInfo().capabilityTable.ftrSvm == false) {
        GTEST_SKIP();
    }
    auto mockCmdQ = std::unique_ptr<MockCommandQueueHw<FamilyType>>(new MockCommandQueueHw<FamilyType>(&context, pDevice, 0));

    cl_event event;
    void *pSVM = context.getSVMAllocsManager()->createSVMAlloc(256, {});
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
