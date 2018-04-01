/*
 * Copyright (c) 2018, Intel Corporation
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

#include "unit_tests/aub_tests/command_queue/command_enqueue_fixture.h"
#include "unit_tests/mocks/mock_context.h"
#include "runtime/mem_obj/buffer.h"
#include "test.h"

using namespace OCLRT;

struct AUBMapBuffer
    : public CommandEnqueueAUBFixture,
      public ::testing::Test {

    void SetUp() override {
        CommandEnqueueAUBFixture::SetUp();
    }

    void TearDown() override {
        CommandEnqueueAUBFixture::TearDown();
    }
};

HWTEST_F(AUBMapBuffer, MapUpdateUnmapVerify) {
    MockContext context(&this->pCmdQ->getDevice());
    auto retVal = CL_INVALID_VALUE;
    size_t bufferSize = 10;

    std::unique_ptr<Buffer> buffer(Buffer::create(
        &context,
        CL_MEM_READ_WRITE,
        bufferSize,
        nullptr,
        retVal));
    ASSERT_NE(nullptr, buffer);

    uint8_t pattern[] = {0xFF};
    size_t patternSize = sizeof(pattern);

    retVal = pCmdQ->enqueueFillBuffer(
        buffer.get(),
        pattern,
        patternSize,
        0,
        bufferSize,
        0,
        nullptr,
        nullptr);
    ASSERT_EQ(CL_SUCCESS, retVal);

    auto mappedPtr = pCmdQ->enqueueMapBuffer(buffer.get(), CL_TRUE, CL_MAP_WRITE | CL_MAP_READ,
                                             0, bufferSize, 0, nullptr, nullptr, retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);

    // write to mapped ptr
    auto mappedPtrStart = static_cast<uint8_t *>(mappedPtr);
    for (uint32_t i = 0; i < bufferSize; i++) {
        *(mappedPtrStart + i) = i;
    }

    pCmdQ->enqueueUnmapMemObject(buffer.get(), mappedPtr, 0, nullptr, nullptr);

    // verify unmap
    std::unique_ptr<uint8_t[]> readMemory(new uint8_t[bufferSize]);
    buffer->forceDisallowCPUCopy = true;

    retVal = pCmdQ->enqueueReadBuffer(buffer.get(), CL_TRUE, 0, bufferSize, readMemory.get(), 0, nullptr, nullptr);
    ASSERT_EQ(CL_SUCCESS, retVal);

    for (size_t i = 0; i < bufferSize; i++) {
        AUBCommandStreamFixture::expectMemory<FamilyType>(&readMemory[i], &i, sizeof(uint8_t));
    }
}
