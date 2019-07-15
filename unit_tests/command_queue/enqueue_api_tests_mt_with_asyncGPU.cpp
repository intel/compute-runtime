/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/aligned_memory.h"
#include "runtime/memory_manager/graphics_allocation.h"
#include "runtime/os_interface/os_context.h"
#include "test.h"

#include "buffer_operations_withAsyncGPU_fixture.h"

#include <thread>

using namespace NEO;

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

    auto retPtr = EnqueueMapBufferHelper<>::enqueueMapBuffer(pCmdQ, srcBuffer.get());
    EXPECT_NE(nullptr, retPtr);

    char *outputPtr = static_cast<char *>(retPtr);

    //check if MapBuffer waited for GPU to feed the data
    for (int i = 0; i < sizeOfBuffer; i++) {
        EXPECT_EQ(outputPtr[i], ptrMemory[i]);
    }
    t.join();

    srcBuffer->getGraphicsAllocation()->updateTaskCount(0u, pCmdQ->getGpgpuCommandStreamReceiver().getOsContext().getContextId());

    alignedFree(ptrMemory);
}
