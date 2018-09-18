/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/basic_math.h"
#include "runtime/gmm_helper/gmm.h"
#include "unit_tests/command_queue/enqueue_read_buffer_fixture.h"
#include "test.h"

using namespace OCLRT;

typedef EnqueueReadBufferTypeTest ReadWriteBufferCpuCopyTest;

HWTEST_F(ReadWriteBufferCpuCopyTest, givenRenderCompressedGmmWhenAskingForCpuOperationThenDisallow) {
    cl_int retVal;
    std::unique_ptr<Buffer> buffer(Buffer::create(context, CL_MEM_READ_WRITE, 1, nullptr, retVal));
    auto gmm = new Gmm(nullptr, 1, false);
    gmm->isRenderCompressed = false;
    buffer->getGraphicsAllocation()->gmm = gmm;

    auto alignedPtr = alignedMalloc(2, MemoryConstants::cacheLineSize);
    auto unalignedPtr = ptrOffset(alignedPtr, 1);
    EXPECT_TRUE(buffer->isReadWriteOnCpuAllowed(CL_TRUE, 0, unalignedPtr, 1));

    gmm->isRenderCompressed = true;
    EXPECT_FALSE(buffer->isReadWriteOnCpuAllowed(CL_TRUE, 0, unalignedPtr, 1));

    alignedFree(alignedPtr);
}

HWTEST_F(ReadWriteBufferCpuCopyTest, simpleRead) {
    cl_int retVal;
    size_t offset = 1;
    size_t size = 4;

    auto deviceInfo = context->getDevice(0)->getMutableDeviceInfo();
    deviceInfo->cpuCopyAllowed = true;

    auto alignedReadPtr = alignedMalloc(size + 1, MemoryConstants::cacheLineSize);
    memset(alignedReadPtr, 0x00, size + 1);
    auto unalignedReadPtr = ptrOffset(alignedReadPtr, 1);

    std::unique_ptr<uint8_t[]> bufferPtr(new uint8_t[size]);
    for (uint8_t i = 0; i < size; i++) {
        bufferPtr[i] = i + 1;
    }
    std::unique_ptr<Buffer> buffer(Buffer::create(context, CL_MEM_USE_HOST_PTR, size, bufferPtr.get(), retVal));
    EXPECT_EQ(retVal, CL_SUCCESS);

    bool aligned = (reinterpret_cast<uintptr_t>(unalignedReadPtr) & (MemoryConstants::cacheLineSize - 1)) == 0;
    EXPECT_TRUE(!aligned || buffer->isMemObjZeroCopy());
    ASSERT_TRUE(buffer->isReadWriteOnCpuAllowed(CL_TRUE, 0, unalignedReadPtr, size));

    retVal = EnqueueReadBufferHelper<>::enqueueReadBuffer(pCmdQ,
                                                          buffer.get(),
                                                          CL_TRUE,
                                                          offset,
                                                          size - offset,
                                                          unalignedReadPtr,
                                                          0,
                                                          nullptr,
                                                          nullptr);
    EXPECT_EQ(retVal, CL_SUCCESS);

    auto pBufferPtr = ptrOffset(bufferPtr.get(), offset);
    EXPECT_EQ(memcmp(unalignedReadPtr, pBufferPtr, size - offset), 0);

    alignedFree(alignedReadPtr);
}

HWTEST_F(ReadWriteBufferCpuCopyTest, givenDeviceThatDoesntSupportCpuCopiesWhenReadBufferIsExecutedThenCpuCopyIsNotDone) {
    cl_int retVal;
    size_t offset = 1;
    size_t size = 4;

    auto deviceInfo = context->getDevice(0)->getMutableDeviceInfo();
    deviceInfo->cpuCopyAllowed = false;

    auto alignedReadPtr = alignedMalloc(size + 1, MemoryConstants::cacheLineSize);
    memset(alignedReadPtr, 0x00, size + 1);
    auto unalignedReadPtr = ptrOffset(alignedReadPtr, 1);

    std::unique_ptr<uint8_t[]> bufferPtr(new uint8_t[size]);
    for (uint8_t i = 0; i < size; i++) {
        bufferPtr[i] = i + 1;
    }
    std::unique_ptr<Buffer> buffer(Buffer::create(context, CL_MEM_USE_HOST_PTR, size, bufferPtr.get(), retVal));
    EXPECT_EQ(retVal, CL_SUCCESS);

    bool aligned = (reinterpret_cast<uintptr_t>(unalignedReadPtr) & (MemoryConstants::cacheLineSize - 1)) == 0;
    EXPECT_TRUE(!aligned || buffer->isMemObjZeroCopy());
    ASSERT_TRUE(buffer->isReadWriteOnCpuAllowed(CL_TRUE, 0, unalignedReadPtr, size));

    retVal = EnqueueReadBufferHelper<>::enqueueReadBuffer(pCmdQ,
                                                          buffer.get(),
                                                          CL_TRUE,
                                                          offset,
                                                          size - offset,
                                                          unalignedReadPtr,
                                                          0,
                                                          nullptr,
                                                          nullptr);
    EXPECT_EQ(retVal, CL_SUCCESS);

    char *charPtr = (char *)unalignedReadPtr;
    for (uint8_t i = 0; i < size; i++) {
        EXPECT_EQ(charPtr[i], 0);
    }

    alignedFree(alignedReadPtr);
}

HWTEST_F(ReadWriteBufferCpuCopyTest, givenDeviceThatDoesntSupportCpuCopiesWhenWriteBufferIsExecutedThenCpuCopyIsNotDone) {
    cl_int retVal;
    size_t offset = 1;
    size_t size = 4;

    auto deviceInfo = context->getDevice(0)->getMutableDeviceInfo();
    deviceInfo->cpuCopyAllowed = false;

    auto alignedWritePtr = alignedMalloc(size + 1, MemoryConstants::cacheLineSize);
    memset(alignedWritePtr, 0x00, size + 1);
    auto unalignedWritePtr = ptrOffset(alignedWritePtr, 1);

    std::unique_ptr<uint8_t[]> bufferPtr(new uint8_t[size]);
    for (uint8_t i = 0; i < size; i++) {
        bufferPtr[i] = i + 1;
    }
    std::unique_ptr<Buffer> buffer(Buffer::create(context, CL_MEM_USE_HOST_PTR, size, bufferPtr.get(), retVal));
    EXPECT_EQ(retVal, CL_SUCCESS);

    bool aligned = (reinterpret_cast<uintptr_t>(unalignedWritePtr) & (MemoryConstants::cacheLineSize - 1)) == 0;
    EXPECT_TRUE(!aligned || buffer->isMemObjZeroCopy());
    ASSERT_TRUE(buffer->isReadWriteOnCpuAllowed(CL_TRUE, 0, unalignedWritePtr, size));

    retVal = EnqueueWriteBufferHelper<>::enqueueWriteBuffer(pCmdQ,
                                                            buffer.get(),
                                                            CL_TRUE,
                                                            offset,
                                                            size - offset,
                                                            unalignedWritePtr,
                                                            0,
                                                            nullptr,
                                                            nullptr);
    EXPECT_EQ(retVal, CL_SUCCESS);

    char *charPtr = (char *)buffer->getCpuAddressForMemoryTransfer() + offset;
    for (uint8_t i = 0; i < size - offset; i++) {
        EXPECT_EQ(charPtr[i], i + 2);
    }

    alignedFree(alignedWritePtr);
}

HWTEST_F(ReadWriteBufferCpuCopyTest, simpleWrite) {
    cl_int retVal;
    size_t offset = 1;
    size_t size = 4;

    auto deviceInfo = context->getDevice(0)->getMutableDeviceInfo();
    deviceInfo->cpuCopyAllowed = true;

    auto alignedWritePtr = alignedMalloc(size + 1, MemoryConstants::cacheLineSize);
    auto unalignedWritePtr = static_cast<uint8_t *>(ptrOffset(alignedWritePtr, 1));
    auto bufferPtrBase = new uint8_t[size];
    auto bufferPtr = new uint8_t[size];
    for (uint8_t i = 0; i < size; i++) {
        unalignedWritePtr[i] = i + 5;
        bufferPtrBase[i] = i + 1;
        bufferPtr[i] = i + 1;
    }

    std::unique_ptr<Buffer> buffer(Buffer::create(context, CL_MEM_USE_HOST_PTR, size, bufferPtr, retVal));
    EXPECT_EQ(retVal, CL_SUCCESS);

    bool aligned = (reinterpret_cast<uintptr_t>(unalignedWritePtr) & (MemoryConstants::cacheLineSize - 1)) == 0;
    EXPECT_TRUE(!aligned || buffer->isMemObjZeroCopy());
    ASSERT_TRUE(buffer->isReadWriteOnCpuAllowed(CL_TRUE, 0, unalignedWritePtr, size));

    retVal = EnqueueWriteBufferHelper<>::enqueueWriteBuffer(pCmdQ,
                                                            buffer.get(),
                                                            CL_TRUE,
                                                            offset,
                                                            size - offset,
                                                            unalignedWritePtr,
                                                            0,
                                                            nullptr,
                                                            nullptr);
    EXPECT_EQ(retVal, CL_SUCCESS);

    auto pBufferPtr = buffer->getCpuAddress();

    EXPECT_EQ(memcmp(pBufferPtr, bufferPtrBase, offset), 0); // untouched
    pBufferPtr = ptrOffset(pBufferPtr, offset);
    EXPECT_EQ(memcmp(pBufferPtr, unalignedWritePtr, size - offset), 0); // updated

    alignedFree(alignedWritePtr);
    delete[] bufferPtr;
    delete[] bufferPtrBase;
}

HWTEST_F(ReadWriteBufferCpuCopyTest, cpuCopyCriteriaMet) {
    cl_int retVal;
    size_t size = MemoryConstants::cacheLineSize;
    auto alignedBufferPtr = alignedMalloc(MemoryConstants::cacheLineSize + 1, MemoryConstants::cacheLineSize);
    auto unalignedBufferPtr = ptrOffset(alignedBufferPtr, 1);
    auto alignedHostPtr = alignedMalloc(MemoryConstants::cacheLineSize + 1, MemoryConstants::cacheLineSize);
    auto unalignedHostPtr = ptrOffset(alignedHostPtr, 1);
    auto smallBufferPtr = alignedMalloc(1 * MB, MemoryConstants::cacheLineSize);
    size_t largeBufferSize = 11u * MemoryConstants::megaByte;

    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    auto mockContext = std::unique_ptr<MockContext>(new MockContext(mockDevice.get()));
    auto memoryManager = static_cast<OsAgnosticMemoryManager *>(mockDevice->getMemoryManager());
    memoryManager->turnOnFakingBigAllocations();

    std::unique_ptr<Buffer> buffer(Buffer::create(context, CL_MEM_USE_HOST_PTR, size, alignedBufferPtr, retVal));
    EXPECT_EQ(retVal, CL_SUCCESS);
    EXPECT_TRUE(buffer->isMemObjZeroCopy());

    // zeroCopy == true && aligned/unaligned hostPtr
    EXPECT_TRUE(buffer->isReadWriteOnCpuAllowed(CL_TRUE, 0, alignedHostPtr, MemoryConstants::cacheLineSize + 1));
    EXPECT_TRUE(buffer->isReadWriteOnCpuAllowed(CL_TRUE, 0, unalignedHostPtr, MemoryConstants::cacheLineSize));

    buffer.reset(Buffer::create(context, CL_MEM_USE_HOST_PTR, size, unalignedBufferPtr, retVal));

    EXPECT_EQ(retVal, CL_SUCCESS);

    // zeroCopy == false && unaligned hostPtr
    EXPECT_TRUE(buffer->isReadWriteOnCpuAllowed(CL_TRUE, 0, unalignedHostPtr, MemoryConstants::cacheLineSize));

    buffer.reset(Buffer::create(mockContext.get(), CL_MEM_USE_HOST_PTR, 1 * MB, smallBufferPtr, retVal));

    // platform LP == true && size <= 10 MB
    mockDevice->getDeviceInfoToModify()->platformLP = true;
    EXPECT_TRUE(buffer->isReadWriteOnCpuAllowed(CL_TRUE, 0, smallBufferPtr, 1 * MB));

    // platform LP == false && size <= 10 MB
    mockDevice->getDeviceInfoToModify()->platformLP = false;
    EXPECT_TRUE(buffer->isReadWriteOnCpuAllowed(CL_TRUE, 0, smallBufferPtr, 1 * MB));

    buffer.reset(Buffer::create(mockContext.get(), CL_MEM_ALLOC_HOST_PTR, largeBufferSize, nullptr, retVal));

    // platform LP == false && size > 10 MB
    mockDevice->getDeviceInfoToModify()->platformLP = false;
    EXPECT_TRUE(buffer->isReadWriteOnCpuAllowed(CL_TRUE, 0, buffer->getCpuAddress(), largeBufferSize));

    alignedFree(smallBufferPtr);
    alignedFree(alignedHostPtr);
    alignedFree(alignedBufferPtr);
}

HWTEST_F(ReadWriteBufferCpuCopyTest, cpuCopyCriteriaNotMet) {
    cl_int retVal;
    size_t size = MemoryConstants::cacheLineSize;
    auto alignedBufferPtr = alignedMalloc(MemoryConstants::cacheLineSize + 1, MemoryConstants::cacheLineSize);
    auto unalignedBufferPtr = ptrOffset(alignedBufferPtr, 1);
    auto alignedHostPtr = alignedMalloc(MemoryConstants::cacheLineSize + 1, MemoryConstants::cacheLineSize);
    auto unalignedHostPtr = ptrOffset(alignedHostPtr, 1);
    size_t largeBufferSize = 11u * MemoryConstants::megaByte;

    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    auto mockContext = std::unique_ptr<MockContext>(new MockContext(mockDevice.get()));
    auto memoryManager = static_cast<OsAgnosticMemoryManager *>(mockDevice->getMemoryManager());
    memoryManager->turnOnFakingBigAllocations();

    std::unique_ptr<Buffer> buffer(Buffer::create(context, CL_MEM_USE_HOST_PTR, size, alignedBufferPtr, retVal));
    EXPECT_EQ(retVal, CL_SUCCESS);
    EXPECT_TRUE(buffer->isMemObjZeroCopy());

    // non blocking
    EXPECT_FALSE(buffer->isReadWriteOnCpuAllowed(CL_FALSE, 0, unalignedHostPtr, MemoryConstants::cacheLineSize));
    // numEventWaitlist > 0
    EXPECT_FALSE(buffer->isReadWriteOnCpuAllowed(CL_TRUE, 1, unalignedHostPtr, MemoryConstants::cacheLineSize));

    buffer.reset(Buffer::create(context, CL_MEM_USE_HOST_PTR, size, unalignedBufferPtr, retVal));

    EXPECT_EQ(retVal, CL_SUCCESS);

    // zeroCopy == false && aligned hostPtr
    EXPECT_FALSE(buffer->isReadWriteOnCpuAllowed(CL_TRUE, 0, alignedHostPtr, MemoryConstants::cacheLineSize + 1));

    buffer.reset(Buffer::create(mockContext.get(), CL_MEM_ALLOC_HOST_PTR, largeBufferSize, nullptr, retVal));

    // platform LP == true && size > 10 MB
    mockDevice->getDeviceInfoToModify()->platformLP = true;
    EXPECT_FALSE(buffer->isReadWriteOnCpuAllowed(CL_TRUE, 0, buffer->getCpuAddress(), largeBufferSize));

    alignedFree(alignedHostPtr);
    alignedFree(alignedBufferPtr);
}
