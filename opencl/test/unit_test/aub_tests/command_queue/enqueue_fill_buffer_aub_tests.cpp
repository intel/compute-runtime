/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/source/mem_obj/buffer.h"
#include "opencl/test/unit_test/aub_tests/command_queue/command_enqueue_fixture.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

using namespace NEO;

struct FillBufferHw
    : public CommandEnqueueAUBFixture,
      public ::testing::WithParamInterface<size_t>,
      public ::testing::Test {

    void SetUp() override {
        CommandEnqueueAUBFixture::setUp();
    }

    void TearDown() override {
        CommandEnqueueAUBFixture::tearDown();
    }
};

typedef FillBufferHw AUBFillBuffer;

HWTEST_P(AUBFillBuffer, WhenFillingThenExpectationsMet) {
    cl_float destMemory[] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    auto pDestMemory = &destMemory[0];
    MockContext context(this->pCmdQ->getDevice().getSpecializedDevice<ClDevice>());
    auto retVal = CL_INVALID_VALUE;
    auto destBuffer = Buffer::create(
        &context,
        CL_MEM_USE_HOST_PTR,
        sizeof(destMemory),
        pDestMemory,
        retVal);
    ASSERT_NE(nullptr, destBuffer);

    float pattern[] = {1.0f};
    size_t patternSize = sizeof(pattern);
    size_t offset = GetParam();
    size_t size = 2 * patternSize;
    cl_uint numEventsInWaitList = 0;
    cl_event *eventWaitList = nullptr;
    cl_event *event = nullptr;

    retVal = pCmdQ->enqueueFillBuffer(
        destBuffer,
        pattern,
        patternSize,
        offset,
        size,
        numEventsInWaitList,
        eventWaitList,
        event);
    ASSERT_EQ(CL_SUCCESS, retVal);

    pCmdQ->flush();

    pDestMemory = reinterpret_cast<decltype(pDestMemory)>(ptrOffset(destBuffer->getGraphicsAllocation(pClDevice->getRootDeviceIndex())->getGpuAddress(), destBuffer->getOffset()));

    // The memory under offset should be untouched
    if (offset) {
        cl_float *destMemoryRef = ptrOffset(&destMemory[0], offset);
        expectMemory<FamilyType>(pDestMemory, destMemoryRef, offset);
        pDestMemory = ptrOffset(pDestMemory, offset);
    }

    // Compute our memory expectations based on kernel execution
    auto pEndMemory = ptrOffset(pDestMemory, size);
    while (pDestMemory < pEndMemory) {
        expectMemory<FamilyType>(pDestMemory, pattern, patternSize);
        pDestMemory = ptrOffset(pDestMemory, patternSize);
    }

    // If the copykernel wasn't max sized, ensure we didn't overwrite existing memory
    size_t sizeUserMemory = sizeof(destMemory);
    if (offset + size < sizeUserMemory) {
        size_t sizeRemaining = sizeUserMemory - size - offset;
        cl_float *destMemoryRef = ptrOffset(&destMemory[0], offset + size);
        expectMemory<FamilyType>(pDestMemory, destMemoryRef, sizeRemaining);
    }
    delete destBuffer;
}

HWTEST_F(AUBFillBuffer, givenFillBufferWhenSeveralSubmissionsWithoutPollForCompletionBetweenThenTestConcurrentCS) {
    DebugManagerStateRestore dbgRestorer;

    cl_float destMemory[] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    auto pDestMemory = &destMemory[0];
    MockContext context(this->pCmdQ->getDevice().getSpecializedDevice<ClDevice>());
    auto retVal = CL_INVALID_VALUE;
    std::unique_ptr<Buffer> destBuffer(Buffer::create(
        &context,
        CL_MEM_USE_HOST_PTR,
        sizeof(destMemory),
        pDestMemory,
        retVal));
    ASSERT_NE(nullptr, destBuffer);

    float pattern[] = {1.0f};
    size_t patternSize = sizeof(pattern);
    size_t offset = 0;
    size_t size = 2 * patternSize;
    cl_uint numEventsInWaitList = 0;
    cl_event *eventWaitList = nullptr;
    cl_event *event = nullptr;
    uint32_t numWrites = 4;

    for (uint32_t id = 0; id < numWrites; id++) {
        offset = id * size;
        retVal = pCmdQ->enqueueFillBuffer(
            destBuffer.get(),
            pattern,
            patternSize,
            offset,
            size,
            numEventsInWaitList,
            eventWaitList,
            event);
        ASSERT_EQ(CL_SUCCESS, retVal);

        pCmdQ->flush();
    }

    pollForCompletion<FamilyType>();

    pDestMemory = reinterpret_cast<decltype(pDestMemory)>(ptrOffset(destBuffer->getGraphicsAllocation(pClDevice->getRootDeviceIndex())->getGpuAddress(), destBuffer->getOffset()));
    auto pEndMemory = ptrOffset(pDestMemory, numWrites * size);
    while (pDestMemory < pEndMemory) {
        expectMemory<FamilyType>(pDestMemory, pattern, patternSize);
        pDestMemory = ptrOffset(pDestMemory, patternSize);
    }
}

INSTANTIATE_TEST_SUITE_P(AUBFillBuffer_simple,
                         AUBFillBuffer,
                         ::testing::Values(
                             0 * sizeof(cl_float),
                             1 * sizeof(cl_float),
                             2 * sizeof(cl_float),
                             3 * sizeof(cl_float)));
