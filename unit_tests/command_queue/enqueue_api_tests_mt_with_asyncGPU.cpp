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

#include "buffer_operations_withAsyncGPU_fixture.h"
#include "runtime/memory_manager/graphics_allocation.h"
#include "runtime/helpers/aligned_memory.h"
#include "test.h"
#include <thread>

using namespace OCLRT;

HWTEST_F(AsyncGPUoperations, MapBufferAfterWriteBuffer) {

    auto sizeOfBuffer = static_cast<int>(BufferDefaults::sizeInBytes);
    auto ptrMemory = static_cast<char *>(alignedMalloc(sizeOfBuffer, MemoryConstants::preferredAlignment));

    //fill the inputs with data
    for (int i = 0; i < sizeOfBuffer; i++) {
        ptrMemory[i] = static_cast<char>(i);
    }

    //call WriteBuffer to have initial tagSetup
    enqueueWriteBuffer<FamilyType>(false, ptrMemory, sizeOfBuffer);

    auto bufferMemory = srcBuffer->getCpuAddress();
    EXPECT_NE(nullptr, bufferMemory);
    EXPECT_EQ(false, srcBuffer->isMemObjZeroCopy());
    int currentTag = pCmdQ->getHwTag();
    EXPECT_GT(currentTag, -1);
    auto tagAddress = pCmdQ->getHwTagAddress();
    EXPECT_NE(nullptr, const_cast<uint32_t *>(tagAddress));
    volatile int sleepTime = 10000;
    std::atomic<bool> ThreadStarted(false);

    std::thread t([&]() {
        ThreadStarted = true;
        while (sleepTime--)
            ;
        memcpy_s(bufferMemory, sizeOfBuffer, ptrMemory, sizeOfBuffer);
        *tagAddress += 2;
    });
    //wait for the thread to start
    while (!ThreadStarted)
        ;

    auto retPtr = EnqueueMapBufferHelper<>::enqueueMapBuffer(pCmdQ, srcBuffer);
    EXPECT_NE(nullptr, retPtr);

    char *outputPtr = static_cast<char *>(retPtr);

    //check if MapBuffer waited for GPU to feed the data
    for (int i = 0; i < sizeOfBuffer; i++) {
        EXPECT_EQ(outputPtr[i], ptrMemory[i]);
    }
    t.join();

    srcBuffer->getGraphicsAllocation()->taskCount = ObjectNotUsed;

    alignedFree(ptrMemory);
}
