/*
 * Copyright (c) 2017, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/helpers/options.h"
#include "runtime/helpers/ptr_math.h"
#include "runtime/mem_obj/buffer.h"
#include "unit_tests/aub_tests/command_queue/command_enqueue_fixture.h"
#include "unit_tests/mocks/mock_context.h"

#include "test.h"

using namespace OCLRT;

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
    MockContext context;
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

INSTANTIATE_TEST_CASE_P(AUBFillBuffer_simple,
                        AUBFillBuffer,
                        ::testing::Values(
                            0 * sizeof(cl_float),
                            1 * sizeof(cl_float),
                            2 * sizeof(cl_float),
                            3 * sizeof(cl_float)));
