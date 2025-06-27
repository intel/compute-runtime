/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/helpers/local_memory_access_modes.h"
#include "shared/source/memory_manager/memory_allocation.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"

#include "opencl/test/unit_test/command_queue/enqueue_read_buffer_fixture.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"

using namespace NEO;

typedef EnqueueReadBufferTypeTest ReadWriteBufferCpuCopyTest;

HWTEST_F(ReadWriteBufferCpuCopyTest, givenCompressedGmmWhenAskingForCpuOperationThenDisallow) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableCpuCacheForResources.set(true);
    debugManager.flags.ForceLocalMemoryAccessMode.set(static_cast<int32_t>(LocalMemoryAccessMode::defaultMode));
    cl_int retVal;
    auto rootDeviceIndex = context->getDevice(0)->getRootDeviceIndex();
    std::unique_ptr<Buffer> buffer(Buffer::create(context, CL_MEM_READ_WRITE, 1, nullptr, retVal));
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    auto gmm = new Gmm(pDevice->getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements);
    gmm->setCompressionEnabled(false);
    auto allocation = buffer->getGraphicsAllocation(rootDeviceIndex);
    allocation->setDefaultGmm(gmm);

    auto alignedPtr = alignedMalloc(2, MemoryConstants::cacheLineSize);
    auto unalignedPtr = ptrOffset(alignedPtr, 1);
    EXPECT_EQ(1u, allocation->storageInfo.getNumBanks());
    EXPECT_TRUE(buffer->isReadWriteOnCpuAllowed(*pDevice));
    EXPECT_TRUE(buffer->isReadWriteOnCpuPreferred(unalignedPtr, 1, *pDevice));

    gmm->setCompressionEnabled(true);
    EXPECT_FALSE(buffer->isReadWriteOnCpuAllowed(*pDevice));
    EXPECT_TRUE(buffer->isReadWriteOnCpuPreferred(unalignedPtr, 1, *pDevice));

    alignedFree(alignedPtr);
}

HWTEST_F(ReadWriteBufferCpuCopyTest, GivenUnalignedReadPtrWhenReadingBufferThenMemoryIsReadCorrectly) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ForceLocalMemoryAccessMode.set(static_cast<int32_t>(LocalMemoryAccessMode::defaultMode));
    cl_int retVal;
    size_t offset = 1;
    size_t size = 4;

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
    ASSERT_TRUE(buffer->isReadWriteOnCpuAllowed(pCmdQ->getDevice()));
    ASSERT_TRUE(buffer->isReadWriteOnCpuPreferred(unalignedReadPtr, size, context->getDevice(0)->getDevice()));

    retVal = EnqueueReadBufferHelper<>::enqueueReadBuffer(pCmdQ,
                                                          buffer.get(),
                                                          CL_TRUE,
                                                          offset,
                                                          size - offset,
                                                          unalignedReadPtr,
                                                          nullptr,
                                                          0,
                                                          nullptr,
                                                          nullptr);
    EXPECT_EQ(retVal, CL_SUCCESS);

    auto pBufferPtr = ptrOffset(bufferPtr.get(), offset);
    EXPECT_EQ(memcmp(unalignedReadPtr, pBufferPtr, size - offset), 0);

    alignedFree(alignedReadPtr);
}

HWTEST_F(ReadWriteBufferCpuCopyTest, GivenUnalignedSrcPtrWhenWritingBufferThenMemoryIsWrittenCorrectly) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ForceLocalMemoryAccessMode.set(static_cast<int32_t>(LocalMemoryAccessMode::defaultMode));
    cl_int retVal;
    size_t offset = 1;
    size_t size = 4;

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
    ASSERT_TRUE(buffer->isReadWriteOnCpuAllowed(pCmdQ->getDevice()));
    ASSERT_TRUE(buffer->isReadWriteOnCpuPreferred(unalignedWritePtr, size, context->getDevice(0)->getDevice()));

    retVal = EnqueueWriteBufferHelper<>::enqueueWriteBuffer(pCmdQ,
                                                            buffer.get(),
                                                            CL_TRUE,
                                                            offset,
                                                            size - offset,
                                                            unalignedWritePtr,
                                                            nullptr,
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

HWTEST_F(ReadWriteBufferCpuCopyTest, GivenSpecificMemoryStructuresWhenReadingWritingMemoryThenCpuReadWriteIsAllowed) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ForceLocalMemoryAccessMode.set(static_cast<int32_t>(LocalMemoryAccessMode::defaultMode));
    cl_int retVal;
    size_t size = MemoryConstants::cacheLineSize;
    auto alignedBufferPtr = alignedMalloc(MemoryConstants::cacheLineSize + 1, MemoryConstants::cacheLineSize);
    auto unalignedBufferPtr = ptrOffset(alignedBufferPtr, 1);
    auto alignedHostPtr = alignedMalloc(MemoryConstants::cacheLineSize + 1, MemoryConstants::cacheLineSize);
    auto unalignedHostPtr = ptrOffset(alignedHostPtr, 1);
    auto smallBufferPtr = alignedMalloc(1 * MemoryConstants::megaByte, MemoryConstants::cacheLineSize);
    size_t largeBufferSize = 11u * MemoryConstants::megaByte;

    auto mockDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    auto mockContext = std::unique_ptr<MockContext>(new MockContext(mockDevice.get()));
    auto memoryManager = static_cast<OsAgnosticMemoryManager *>(mockDevice->getMemoryManager());
    memoryManager->turnOnFakingBigAllocations();

    std::unique_ptr<Buffer> buffer(Buffer::create(context, CL_MEM_USE_HOST_PTR, size, alignedBufferPtr, retVal));
    EXPECT_EQ(retVal, CL_SUCCESS);
    EXPECT_TRUE(buffer->isMemObjZeroCopy());

    // zeroCopy == true && aligned/unaligned hostPtr
    EXPECT_TRUE(buffer->isReadWriteOnCpuPreferred(alignedHostPtr, MemoryConstants::cacheLineSize + 1, mockDevice->getDevice()));
    EXPECT_TRUE(buffer->isReadWriteOnCpuPreferred(unalignedHostPtr, MemoryConstants::cacheLineSize, mockDevice->getDevice()));

    buffer.reset(Buffer::create(context, CL_MEM_USE_HOST_PTR, size, unalignedBufferPtr, retVal));

    EXPECT_EQ(retVal, CL_SUCCESS);

    // zeroCopy == false && unaligned hostPtr
    EXPECT_TRUE(buffer->isReadWriteOnCpuPreferred(unalignedHostPtr, MemoryConstants::cacheLineSize, mockDevice->getDevice()));

    buffer.reset(Buffer::create(mockContext.get(), CL_MEM_USE_HOST_PTR, 1 * MemoryConstants::megaByte, smallBufferPtr, retVal));

    // platform LP == true && size <= 10 MB
    mockDevice->deviceInfo.platformLP = true;
    EXPECT_TRUE(buffer->isReadWriteOnCpuPreferred(smallBufferPtr, 1 * MemoryConstants::megaByte, mockDevice->getDevice()));

    // platform LP == false && size <= 10 MB
    mockDevice->deviceInfo.platformLP = false;
    EXPECT_TRUE(buffer->isReadWriteOnCpuPreferred(smallBufferPtr, 1 * MemoryConstants::megaByte, mockDevice->getDevice()));

    buffer.reset(Buffer::create(mockContext.get(), CL_MEM_ALLOC_HOST_PTR, largeBufferSize, nullptr, retVal));

    // platform LP == false && size > 10 MB
    mockDevice->deviceInfo.platformLP = false;
    EXPECT_TRUE(buffer->isReadWriteOnCpuPreferred(buffer->getCpuAddress(), largeBufferSize, mockDevice->getDevice()));

    alignedFree(smallBufferPtr);
    alignedFree(alignedHostPtr);
    alignedFree(alignedBufferPtr);
}

HWTEST_F(ReadWriteBufferCpuCopyTest, GivenSpecificMemoryStructuresWhenReadingWritingMemoryThenCpuReadWriteIsNotAllowed) {
    cl_int retVal;
    size_t size = MemoryConstants::cacheLineSize;
    auto alignedBufferPtr = alignedMalloc(MemoryConstants::cacheLineSize + 1, MemoryConstants::cacheLineSize);
    auto unalignedBufferPtr = ptrOffset(alignedBufferPtr, 1);
    auto alignedHostPtr = alignedMalloc(MemoryConstants::cacheLineSize + 1, MemoryConstants::cacheLineSize);
    auto unalignedHostPtr = ptrOffset(alignedHostPtr, 1);
    size_t largeBufferSize = 11u * MemoryConstants::megaByte;

    auto mockDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    auto mockContext = std::make_unique<MockContext>(mockDevice.get());
    auto mockCommandQueue = std::make_unique<MockCommandQueue>(*mockContext);

    auto memoryManager = static_cast<OsAgnosticMemoryManager *>(mockDevice->getMemoryManager());
    memoryManager->turnOnFakingBigAllocations();

    std::unique_ptr<Buffer> buffer(Buffer::create(context, CL_MEM_USE_HOST_PTR, size, alignedBufferPtr, retVal));
    EXPECT_EQ(retVal, CL_SUCCESS);
    EXPECT_TRUE(buffer->isMemObjZeroCopy());

    // non blocking
    EXPECT_FALSE(mockCommandQueue->bufferCpuCopyAllowed(buffer.get(), CL_COMMAND_NDRANGE_KERNEL, false, size, unalignedHostPtr, 0u, nullptr));

    buffer.reset(Buffer::create(context, CL_MEM_USE_HOST_PTR, size, unalignedBufferPtr, retVal));

    EXPECT_EQ(retVal, CL_SUCCESS);

    // zeroCopy == false && aligned hostPtr
    EXPECT_FALSE(buffer->isReadWriteOnCpuPreferred(alignedHostPtr, MemoryConstants::cacheLineSize + 1, mockDevice->getDevice()));

    buffer.reset(Buffer::create(mockContext.get(), CL_MEM_ALLOC_HOST_PTR, largeBufferSize, nullptr, retVal));

    // platform LP == true && size > 10 MB
    mockDevice->deviceInfo.platformLP = true;
    EXPECT_FALSE(buffer->isReadWriteOnCpuPreferred(buffer->getCpuAddress(), largeBufferSize, mockDevice->getDevice()));

    alignedFree(alignedHostPtr);
    alignedFree(alignedBufferPtr);
}

HWTEST_F(ReadWriteBufferCpuCopyTest, givenDebugVariableToDisableCpuCopiesWhenBufferCpuCopyAllowedIsCalledThenItReturnsFalse) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableLocalMemory.set(false);
    debugManager.flags.ForceLocalMemoryAccessMode.set(static_cast<int32_t>(LocalMemoryAccessMode::defaultMode));

    cl_int retVal;

    auto mockDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    auto mockContext = std::make_unique<MockContext>(mockDevice.get());
    auto mockCommandQueue = std::make_unique<MockCommandQueue>(*mockContext);

    std::unique_ptr<Buffer> buffer(Buffer::create(context, CL_MEM_ALLOC_HOST_PTR, MemoryConstants::pageSize, nullptr, retVal));
    EXPECT_EQ(retVal, CL_SUCCESS);
    EXPECT_TRUE(buffer->isMemObjZeroCopy());

    EXPECT_TRUE(mockCommandQueue->bufferCpuCopyAllowed(buffer.get(), CL_COMMAND_READ_BUFFER, true, MemoryConstants::pageSize, reinterpret_cast<void *>(0x1000), 0u, nullptr));
    EXPECT_TRUE(mockCommandQueue->bufferCpuCopyAllowed(buffer.get(), CL_COMMAND_WRITE_BUFFER, true, MemoryConstants::pageSize, reinterpret_cast<void *>(0x1000), 0u, nullptr));

    debugManager.flags.DoCpuCopyOnReadBuffer.set(0);
    EXPECT_FALSE(mockCommandQueue->bufferCpuCopyAllowed(buffer.get(), CL_COMMAND_READ_BUFFER, true, MemoryConstants::pageSize, reinterpret_cast<void *>(0x1000), 0u, nullptr));
    EXPECT_TRUE(mockCommandQueue->bufferCpuCopyAllowed(buffer.get(), CL_COMMAND_WRITE_BUFFER, true, MemoryConstants::pageSize, reinterpret_cast<void *>(0x1000), 0u, nullptr));

    debugManager.flags.DoCpuCopyOnWriteBuffer.set(0);
    EXPECT_FALSE(mockCommandQueue->bufferCpuCopyAllowed(buffer.get(), CL_COMMAND_READ_BUFFER, true, MemoryConstants::pageSize, reinterpret_cast<void *>(0x1000), 0u, nullptr));
    EXPECT_FALSE(mockCommandQueue->bufferCpuCopyAllowed(buffer.get(), CL_COMMAND_WRITE_BUFFER, true, MemoryConstants::pageSize, reinterpret_cast<void *>(0x1000), 0u, nullptr));
}

TEST(ReadWriteBufferOnCpu, givenNoHostPtrAndAlignedSizeWhenMemoryAllocationIsInNonSystemMemoryPoolThenIsReadWriteOnCpuAllowedReturnsFalse) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ForceLocalMemoryAccessMode.set(static_cast<int32_t>(LocalMemoryAccessMode::defaultMode));
    MockExecutionEnvironment *executionEnvironment = new MockExecutionEnvironment(nullptr, false, 1u);
    MockMemoryManager *memoryManager = new MockMemoryManager(*executionEnvironment);
    executionEnvironment->memoryManager.reset(memoryManager);
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithExecutionEnvironment<MockDevice>(nullptr, executionEnvironment, 0u));

    MockContext ctx(device.get());

    cl_int retVal = 0;
    cl_mem_flags flags = CL_MEM_READ_WRITE;

    std::unique_ptr<Buffer> buffer(Buffer::create(&ctx, flags, MemoryConstants::pageSize, nullptr, retVal));
    ASSERT_NE(nullptr, buffer.get());

    EXPECT_TRUE(buffer->isReadWriteOnCpuAllowed(device->getDevice()));
    EXPECT_TRUE(buffer->isReadWriteOnCpuPreferred(reinterpret_cast<void *>(0x1000), MemoryConstants::pageSize, device->getDevice()));
    reinterpret_cast<MemoryAllocation *>(buffer->getGraphicsAllocation(device->getRootDeviceIndex()))->overrideMemoryPool(MemoryPool::systemCpuInaccessible);
    // read write on CPU is allowed, but not preferred. We can access this memory via Lock.
    EXPECT_TRUE(buffer->isReadWriteOnCpuAllowed(device->getDevice()));
    EXPECT_FALSE(buffer->isReadWriteOnCpuPreferred(reinterpret_cast<void *>(0x1000), MemoryConstants::pageSize, device->getDevice()));
}

TEST(ReadWriteBufferOnCpu, givenPointerThatRequiresCpuCopyWhenCpuCopyIsEvaluatedThenTrueIsReturned) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ForceLocalMemoryAccessMode.set(static_cast<int32_t>(LocalMemoryAccessMode::defaultMode));
    MockExecutionEnvironment *executionEnvironment = new MockExecutionEnvironment(nullptr, false, 1u);
    MockMemoryManager *memoryManager = new MockMemoryManager(*executionEnvironment);
    executionEnvironment->memoryManager.reset(memoryManager);
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithExecutionEnvironment<MockDevice>(nullptr, executionEnvironment, 0u));

    MockContext context(device.get());

    cl_int retVal = 0;
    cl_mem_flags flags = CL_MEM_READ_WRITE;

    std::unique_ptr<Buffer> buffer(Buffer::create(&context, flags, MemoryConstants::pageSize, nullptr, retVal));

    ASSERT_NE(nullptr, buffer.get());
    auto mockCommandQueue = std::make_unique<MockCommandQueue>(context);

    EXPECT_FALSE(mockCommandQueue->bufferCpuCopyAllowed(buffer.get(), CL_COMMAND_READ_BUFFER, false, MemoryConstants::pageSize, nullptr, 0u, nullptr));
    memoryManager->cpuCopyRequired = true;
    EXPECT_TRUE(mockCommandQueue->bufferCpuCopyAllowed(buffer.get(), CL_COMMAND_READ_BUFFER, false, MemoryConstants::pageSize, nullptr, 0u, nullptr));
}

TEST(ReadWriteBufferOnCpu, givenPointerThatRequiresCpuCopyButItIsNotPossibleWhenCpuCopyIsEvaluatedThenFalseIsReturned) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ForceLocalMemoryAccessMode.set(static_cast<int32_t>(LocalMemoryAccessMode::defaultMode));
    MockExecutionEnvironment *executionEnvironment = new MockExecutionEnvironment(nullptr, false, 1u);
    MockMemoryManager *memoryManager = new MockMemoryManager(*executionEnvironment);
    executionEnvironment->memoryManager.reset(memoryManager);
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithExecutionEnvironment<MockDevice>(nullptr, executionEnvironment, 0u));

    MockContext context(device.get());

    cl_int retVal = 0;
    cl_mem_flags flags = CL_MEM_READ_WRITE;

    std::unique_ptr<Buffer> buffer(Buffer::create(&context, flags, MemoryConstants::pageSize, nullptr, retVal));

    ASSERT_NE(nullptr, buffer.get());
    auto mockCommandQueue = std::make_unique<MockCommandQueue>(context);

    buffer->forceDisallowCPUCopy = true;
    EXPECT_FALSE(mockCommandQueue->bufferCpuCopyAllowed(buffer.get(), CL_COMMAND_READ_BUFFER, true, MemoryConstants::pageSize, nullptr, 0u, nullptr));
    memoryManager->cpuCopyRequired = true;
    EXPECT_FALSE(mockCommandQueue->bufferCpuCopyAllowed(buffer.get(), CL_COMMAND_READ_BUFFER, true, MemoryConstants::pageSize, nullptr, 0u, nullptr));
}

TEST(ReadWriteBufferOnCpu, whenLocalMemoryPoolAllocationIsAskedForPreferenceThenCpuIsNotChoosen) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ForceLocalMemoryAccessMode.set(static_cast<int32_t>(LocalMemoryAccessMode::defaultMode));
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MockContext ctx(device.get());

    cl_int retVal = 0;
    std::unique_ptr<Buffer> buffer(Buffer::create(&ctx, CL_MEM_READ_WRITE, MemoryConstants::pageSize, nullptr, retVal));
    ASSERT_NE(nullptr, buffer.get());
    reinterpret_cast<MemoryAllocation *>(buffer->getGraphicsAllocation(device->getRootDeviceIndex()))->overrideMemoryPool(MemoryPool::localMemory);

    EXPECT_FALSE(buffer->isReadWriteOnCpuAllowed(device->getDevice()));
    EXPECT_FALSE(buffer->isReadWriteOnCpuPreferred(reinterpret_cast<void *>(0x1000), MemoryConstants::pageSize, device->getDevice()));
}
