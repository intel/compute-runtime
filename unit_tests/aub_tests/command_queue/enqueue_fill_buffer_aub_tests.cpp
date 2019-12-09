/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/helpers/options.h"
#include "core/helpers/ptr_math.h"
#include "core/unit_tests/helpers/debug_manager_state_restore.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/mem_obj/buffer.h"
#include "test.h"
#include "unit_tests/aub_tests/command_queue/command_enqueue_fixture.h"
#include "unit_tests/mocks/mock_context.h"

using namespace NEO;

struct FillBufferHw
    : public CommandEnqueueAUBFixture,
      public ::testing::WithParamInterface<size_t>,
      public ::testing::Test {

    void SetUp() override {
        CommandEnqueueAUBFixture::SetUp();
    }

    void TearDown() override {
        CommandEnqueueAUBFixture::TearDown();
    }
};

typedef FillBufferHw AUBFillBuffer;

HWTEST_P(AUBFillBuffer, simple) {
    cl_float destMemory[] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    auto pDestMemory = &destMemory[0];
    MockContext context(&this->pCmdQ->getDevice());
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

    pDestMemory = reinterpret_cast<decltype(pDestMemory)>((destBuffer->getGraphicsAllocation()->getGpuAddress()));

    // The memory under offset should be untouched
    if (offset) {
        cl_float *destMemoryRef = ptrOffset(&destMemory[0], offset);
        AUBCommandStreamFixture::expectMemory<FamilyType>(pDestMemory, destMemoryRef, offset);
        pDestMemory = ptrOffset(pDestMemory, offset);
    }

    // Compute our memory expecations based on kernel execution
    auto pEndMemory = ptrOffset(pDestMemory, size);
    while (pDestMemory < pEndMemory) {
        AUBCommandStreamFixture::expectMemory<FamilyType>(pDestMemory, pattern, patternSize);
        pDestMemory = ptrOffset(pDestMemory, patternSize);
    }

    // If the copykernel wasn't max sized, ensure we didn't overwrite existing memory
    size_t sizeUserMemory = sizeof(destMemory);
    if (offset + size < sizeUserMemory) {
        size_t sizeRemaining = sizeUserMemory - size - offset;
        cl_float *destMemoryRef = ptrOffset(&destMemory[0], offset + size);
        AUBCommandStreamFixture::expectMemory<FamilyType>(pDestMemory, destMemoryRef, sizeRemaining);
    }
    delete destBuffer;
}

HWTEST_F(AUBFillBuffer, givenFillBufferWhenSeveralSubmissionsWithoutPollForCompletionBetweenThenTestConcurrentCS) {
    DebugManagerStateRestore dbgRestorer;

    cl_float destMemory[] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    auto pDestMemory = &destMemory[0];
    MockContext context(&this->pCmdQ->getDevice());
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

    AUBCommandStreamFixture::pollForCompletion<FamilyType>();

    pDestMemory = reinterpret_cast<decltype(pDestMemory)>((destBuffer->getGraphicsAllocation()->getGpuAddress()));
    auto pEndMemory = ptrOffset(pDestMemory, numWrites * size);
    while (pDestMemory < pEndMemory) {
        AUBCommandStreamFixture::expectMemory<FamilyType>(pDestMemory, pattern, patternSize);
        pDestMemory = ptrOffset(pDestMemory, patternSize);
    }
}

INSTANTIATE_TEST_CASE_P(AUBFillBuffer_simple,
                        AUBFillBuffer,
                        ::testing::Values(
                            0 * sizeof(cl_float),
                            1 * sizeof(cl_float),
                            2 * sizeof(cl_float),
                            3 * sizeof(cl_float)));
