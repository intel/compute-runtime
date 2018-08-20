/*
 * Copyright (c) 2017 - 2018, Intel Corporation
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

#include "gmock/gmock.h"
#include "runtime/gmm_helper/gmm.h"
#include "runtime/gmm_helper/gmm_helper.h"
#include "runtime/gmm_helper/resource_info.h"
#include "runtime/helpers/array_count.h"
#include "runtime/helpers/options.h"
#include "runtime/mem_obj/buffer.h"
#include "runtime/memory_manager/svm_memory_manager.h"
#include "test.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/fixtures/memory_management_fixture.h"
#include "unit_tests/gen_common/matchers.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/helpers/memory_management.h"
#include "unit_tests/libult/ult_command_stream_receiver.h"
#include "unit_tests/mocks/mock_buffer.h"
#include "unit_tests/mocks/mock_command_queue.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_gmm_resource_info.h"
#include "gtest/gtest.h"

using namespace OCLRT;

static const unsigned int g_scTestBufferSizeInBytes = 16;

TEST(Buffer, giveBufferWhenAskedForPtrOffsetForMappingThenReturnCorrectValue) {
    MockContext ctx;
    cl_int retVal;
    std::unique_ptr<Buffer> buffer(Buffer::create(&ctx, 0, 1, nullptr, retVal));

    MemObjOffsetArray offset = {{4, 5, 6}};

    auto retOffset = buffer->calculateOffsetForMapping(offset);
    EXPECT_EQ(offset[0], retOffset);
}

TEST(Buffer, givenBufferWhenAskedForPtrLengthThenReturnCorrectValue) {
    MockContext ctx;
    cl_int retVal;
    std::unique_ptr<Buffer> buffer(Buffer::create(&ctx, 0, 1, nullptr, retVal));

    MemObjSizeArray size = {{4, 5, 6}};

    auto retOffset = buffer->calculateMappedPtrLength(size);
    EXPECT_EQ(size[0], retOffset);
}

TEST(Buffer, givenReadOnlySetOfInputFlagsWhenPassedToisReadOnlyMemoryPermittedByFlagsThenTrueIsReturned) {
    class MockBuffer : public Buffer {
      public:
        using Buffer::isReadOnlyMemoryPermittedByFlags;
    };
    cl_mem_flags flags = CL_MEM_HOST_NO_ACCESS | CL_MEM_READ_ONLY;
    EXPECT_TRUE(MockBuffer::isReadOnlyMemoryPermittedByFlags(flags));

    flags = CL_MEM_HOST_READ_ONLY | CL_MEM_READ_ONLY;
    EXPECT_TRUE(MockBuffer::isReadOnlyMemoryPermittedByFlags(flags));
}

class BufferReadOnlyTest : public testing::TestWithParam<uint64_t> {
};

TEST_P(BufferReadOnlyTest, givenNonReadOnlySetOfInputFlagsWhenPassedToisReadOnlyMemoryPermittedByFlagsThenFalseIsReturned) {
    class MockBuffer : public Buffer {
      public:
        using Buffer::isReadOnlyMemoryPermittedByFlags;
    };

    cl_mem_flags flags = GetParam() | CL_MEM_USE_HOST_PTR;
    EXPECT_FALSE(MockBuffer::isReadOnlyMemoryPermittedByFlags(flags));
}
static cl_mem_flags nonReadOnlyFlags[] = {
    CL_MEM_READ_WRITE | CL_MEM_HOST_READ_ONLY,
    CL_MEM_WRITE_ONLY,
    CL_MEM_READ_ONLY | CL_MEM_HOST_WRITE_ONLY,
    CL_MEM_HOST_READ_ONLY,
    CL_MEM_HOST_WRITE_ONLY,
    CL_MEM_HOST_NO_ACCESS,
    CL_MEM_HOST_READ_ONLY | CL_MEM_WRITE_ONLY,
    CL_MEM_HOST_WRITE_ONLY | CL_MEM_WRITE_ONLY,
    0};

INSTANTIATE_TEST_CASE_P(
    nonReadOnlyFlags,
    BufferReadOnlyTest,
    testing::ValuesIn(nonReadOnlyFlags));

class GMockMemoryManagerFailFirstAllocation : public MockMemoryManager {
  public:
    MOCK_METHOD4(allocateGraphicsMemoryInPreferredPool, GraphicsAllocation *(bool allocateMemory, const void *hostPtr, size_t size, GraphicsAllocation::AllocationType type));
    GraphicsAllocation *baseallocateGraphicsMemoryInPreferredPool(bool allocateMemory, const void *hostPtr, size_t size, GraphicsAllocation::AllocationType type) {
        return MockMemoryManager::allocateGraphicsMemoryInPreferredPool(allocateMemory, hostPtr, size, type);
    }
};

TEST(Buffer, givenReadOnlyHostPtrMemoryWhenBufferIsCreatedWithReadOnlyFlagsThenBufferHasAllocatedNewMemoryStorageAndBufferIsNotZeroCopy) {
    void *memory = alignedMalloc(MemoryConstants::pageSize, MemoryConstants::pageSize);
    ASSERT_NE(nullptr, memory);

    memset(memory, 0xAA, MemoryConstants::pageSize);

    std::unique_ptr<MockDevice> device(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    ::testing::NiceMock<GMockMemoryManagerFailFirstAllocation> *memoryManager = new ::testing::NiceMock<GMockMemoryManagerFailFirstAllocation>;

    device->injectMemoryManager(memoryManager);
    MockContext ctx(device.get());

    // First fail simulates error for read only memory allocation
    EXPECT_CALL(*memoryManager, allocateGraphicsMemoryInPreferredPool(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(nullptr))
        .WillRepeatedly(::testing::Invoke(memoryManager, &GMockMemoryManagerFailFirstAllocation::baseallocateGraphicsMemoryInPreferredPool));

    cl_int retVal;
    cl_mem_flags flags = CL_MEM_HOST_READ_ONLY | CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR;

    std::unique_ptr<Buffer> buffer(Buffer::create(&ctx, flags, MemoryConstants::pageSize, (void *)memory, retVal));

    EXPECT_FALSE(buffer->isMemObjZeroCopy());
    void *memoryStorage = buffer->getCpuAddressForMemoryTransfer();
    EXPECT_NE((void *)memory, memoryStorage);
    EXPECT_THAT(buffer->getCpuAddressForMemoryTransfer(), MemCompare(memory, MemoryConstants::pageSize));

    alignedFree(memory);
}

TEST(Buffer, givenReadOnlyHostPtrMemoryWhenBufferIsCreatedWithReadOnlyFlagsAndSecondAllocationFailsThenNullptrIsReturned) {
    void *memory = alignedMalloc(MemoryConstants::pageSize, MemoryConstants::pageSize);
    ASSERT_NE(nullptr, memory);

    memset(memory, 0xAA, MemoryConstants::pageSize);

    std::unique_ptr<MockDevice> device(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    ::testing::NiceMock<GMockMemoryManagerFailFirstAllocation> *memoryManager = new ::testing::NiceMock<GMockMemoryManagerFailFirstAllocation>;

    device->injectMemoryManager(memoryManager);
    MockContext ctx(device.get());

    // First fail simulates error for read only memory allocation
    // Second fail returns nullptr
    EXPECT_CALL(*memoryManager, allocateGraphicsMemoryInPreferredPool(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Return(nullptr));

    cl_int retVal;
    cl_mem_flags flags = CL_MEM_HOST_READ_ONLY | CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR;

    std::unique_ptr<Buffer> buffer(Buffer::create(&ctx, flags, MemoryConstants::pageSize, (void *)memory, retVal));

    EXPECT_EQ(nullptr, buffer.get());
    alignedFree(memory);
}

TEST(Buffer, givenReadOnlyHostPtrMemoryWhenBufferIsCreatedWithKernelWriteFlagThenBufferAllocationFailsAndReturnsNullptr) {
    void *memory = alignedMalloc(MemoryConstants::pageSize, MemoryConstants::pageSize);
    ASSERT_NE(nullptr, memory);

    memset(memory, 0xAA, MemoryConstants::pageSize);

    std::unique_ptr<MockDevice> device(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    ::testing::NiceMock<GMockMemoryManagerFailFirstAllocation> *memoryManager = new ::testing::NiceMock<GMockMemoryManagerFailFirstAllocation>;

    device->injectMemoryManager(memoryManager);
    MockContext ctx(device.get());

    // First fail simulates error for read only memory allocation
    EXPECT_CALL(*memoryManager, allocateGraphicsMemoryInPreferredPool(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(nullptr));

    cl_int retVal;
    cl_mem_flags flags = CL_MEM_HOST_READ_ONLY | CL_MEM_WRITE_ONLY | CL_MEM_USE_HOST_PTR;

    std::unique_ptr<Buffer> buffer(Buffer::create(&ctx, flags, MemoryConstants::pageSize, (void *)memory, retVal));

    EXPECT_EQ(nullptr, buffer.get());
    alignedFree(memory);
}

TEST(Buffer, givenNullPtrWhenBufferIsCreatedWithKernelReadOnlyFlagsThenBufferAllocationFailsAndReturnsNullptr) {
    std::unique_ptr<MockDevice> device(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    ::testing::NiceMock<GMockMemoryManagerFailFirstAllocation> *memoryManager = new ::testing::NiceMock<GMockMemoryManagerFailFirstAllocation>;

    device->injectMemoryManager(memoryManager);
    MockContext ctx(device.get());

    // First fail simulates error for read only memory allocation
    EXPECT_CALL(*memoryManager, allocateGraphicsMemoryInPreferredPool(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(nullptr));

    cl_int retVal;
    cl_mem_flags flags = CL_MEM_HOST_READ_ONLY | CL_MEM_WRITE_ONLY;

    std::unique_ptr<Buffer> buffer(Buffer::create(&ctx, flags, MemoryConstants::pageSize, nullptr, retVal));

    EXPECT_EQ(nullptr, buffer.get());
}

TEST(Buffer, givenNullptrPassedToBufferCreateWhenAllocationIsNotSystemMemoryPoolThenBufferIsNotZeroCopy) {
    std::unique_ptr<MockDevice> device(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    ::testing::NiceMock<GMockMemoryManagerFailFirstAllocation> *memoryManager = new ::testing::NiceMock<GMockMemoryManagerFailFirstAllocation>;

    device->injectMemoryManager(memoryManager);
    MockContext ctx(device.get());

    auto allocateNonSystemGraphicsAllocation = [memoryManager](bool allocateMemory, const void *hostPtr, size_t size, GraphicsAllocation::AllocationType type) -> GraphicsAllocation * {
        auto allocation = memoryManager->allocateGraphicsMemory(size, MemoryConstants::pageSize, false, false);
        reinterpret_cast<MemoryAllocation *>(allocation)->overrideMemoryPool(MemoryPool::SystemCpuInaccessible);
        return allocation;
    };

    EXPECT_CALL(*memoryManager, allocateGraphicsMemoryInPreferredPool(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Invoke(allocateNonSystemGraphicsAllocation));

    cl_int retVal = 0;
    cl_mem_flags flags = CL_MEM_READ_WRITE;

    std::unique_ptr<Buffer> buffer(Buffer::create(&ctx, flags, MemoryConstants::pageSize, nullptr, retVal));

    ASSERT_NE(nullptr, buffer.get());
    EXPECT_FALSE(buffer->isMemObjZeroCopy());
}

TEST(Buffer, givenNullptrPassedToBufferCreateWhenNoSharedContextOrRenderCompressedBuffersThenBuffersAllocationTypeIsBufferOrBufferHostMemory) {
    std::unique_ptr<MockDevice> device(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MockContext ctx(device.get());

    cl_int retVal = 0;
    cl_mem_flags flags = CL_MEM_READ_WRITE;

    std::unique_ptr<Buffer> buffer(Buffer::create(&ctx, flags, MemoryConstants::pageSize, nullptr, retVal));

    ASSERT_NE(nullptr, buffer.get());
    if (MemoryPool::isSystemMemoryPool(buffer->getGraphicsAllocation()->getMemoryPool())) {
        EXPECT_EQ(GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY, buffer->getGraphicsAllocation()->getAllocationType());
    } else {
        EXPECT_EQ(GraphicsAllocation::AllocationType::BUFFER, buffer->getGraphicsAllocation()->getAllocationType());
    }
}

TEST(Buffer, givenAlignedHostPtrPassedToBufferCreateWhenNoSharedContextOrRenderCompressedBuffersThenBuffersAllocationTypeIsBufferHostMemory) {
    std::unique_ptr<MockDevice> device(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MockContext ctx(device.get());

    cl_int retVal = 0;
    cl_mem_flags flags = CL_MEM_USE_HOST_PTR;
    void *hostPtr = reinterpret_cast<void *>(0x3000);

    std::unique_ptr<Buffer> buffer(Buffer::create(&ctx, flags, MemoryConstants::pageSize, hostPtr, retVal));

    ASSERT_NE(nullptr, buffer.get());
    EXPECT_EQ(GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY, buffer->getGraphicsAllocation()->getAllocationType());
}

TEST(Buffer, givenAllocHostPtrFlagPassedToBufferCreateWhenNoSharedContextOrRenderCompressedBuffersThenBuffersAllocationTypeIsBufferHostMemory) {
    std::unique_ptr<MockDevice> device(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MockContext ctx(device.get());

    cl_int retVal = 0;
    cl_mem_flags flags = CL_MEM_ALLOC_HOST_PTR;
    size_t size = MemoryConstants::pageSize;
    void *hostPtr = alignedMalloc(size, MemoryConstants::pageSize);

    std::unique_ptr<Buffer> buffer(Buffer::create(&ctx, flags, MemoryConstants::pageSize, hostPtr, retVal));

    ASSERT_NE(nullptr, buffer.get());
    EXPECT_EQ(GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY, buffer->getGraphicsAllocation()->getAllocationType());
    alignedFree(hostPtr);
}

TEST(Buffer, givenRenderCompressedBuffersEnabledWhenAllocationTypeIsQueriedThenBufferCompressedTypeIsReturned) {
    cl_mem_flags flags = 0;
    auto type = MockPublicAccessBuffer::getGraphicsAllocationType(flags, false, true);
    EXPECT_EQ(GraphicsAllocation::AllocationType::BUFFER_COMPRESSED, type);
}

TEST(Buffer, givenSharedContextWhenAllocationTypeIsQueriedThenBufferHostMemoryTypeIsReturned) {
    cl_mem_flags flags = 0;
    auto type = MockPublicAccessBuffer::getGraphicsAllocationType(flags, true, false);
    EXPECT_EQ(GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY, type);
}

TEST(Buffer, givenSharedContextAndRenderCompressedBuffersEnabledWhenAllocationTypeIsQueriedThenBufferHostMemoryTypeIsReturned) {
    cl_mem_flags flags = 0;
    auto type = MockPublicAccessBuffer::getGraphicsAllocationType(flags, true, true);
    EXPECT_EQ(GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY, type);
}

TEST(Buffer, givenUseHostPtrFlagWhenAllocationTypeIsQueriedThenBufferHostMemoryTypeIsReturned) {
    cl_mem_flags flags = CL_MEM_USE_HOST_PTR;
    auto type = MockPublicAccessBuffer::getGraphicsAllocationType(flags, false, false);
    EXPECT_EQ(GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY, type);
}

TEST(Buffer, givenAllocHostPtrFlagWhenAllocationTypeIsQueriedThenBufferHostMemoryTypeIsReturned) {
    cl_mem_flags flags = CL_MEM_ALLOC_HOST_PTR;
    auto type = MockPublicAccessBuffer::getGraphicsAllocationType(flags, false, false);
    EXPECT_EQ(GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY, type);
}

TEST(Buffer, givenUseHostPtrFlagAndRenderCompressedBuffersEnabledWhenAllocationTypeIsQueriedThenBufferCompressedTypeIsReturned) {
    cl_mem_flags flags = CL_MEM_USE_HOST_PTR;
    auto type = MockPublicAccessBuffer::getGraphicsAllocationType(flags, false, true);
    EXPECT_EQ(GraphicsAllocation::AllocationType::BUFFER_COMPRESSED, type);
}

TEST(Buffer, givenAllocHostPtrFlagAndRenderCompressedBuffersEnabledWhenAllocationTypeIsQueriedThenBufferCompressedTypeIsReturned) {
    cl_mem_flags flags = CL_MEM_ALLOC_HOST_PTR;
    auto type = MockPublicAccessBuffer::getGraphicsAllocationType(flags, false, true);
    EXPECT_EQ(GraphicsAllocation::AllocationType::BUFFER_COMPRESSED, type);
}

TEST(Buffer, givenZeroFlagsNoSharedContextAndRenderCompressedBuffersDisabledWhenAllocationTypeIsQueriedThenBufferTypeIsReturned) {
    cl_mem_flags flags = 0;
    auto type = MockPublicAccessBuffer::getGraphicsAllocationType(flags, false, false);
    EXPECT_EQ(GraphicsAllocation::AllocationType::BUFFER, type);
}

struct RenderCompressedBuffersTests : public ::testing::Test {
    void SetUp() override {
        localHwInfo = *platformDevices[0];
        device.reset(Device::create<MockDevice>(&localHwInfo, new ExecutionEnvironment()));
        context = std::make_unique<MockContext>(device.get(), true);
    }

    cl_int retVal = CL_SUCCESS;
    HardwareInfo localHwInfo = {};
    std::unique_ptr<MockDevice> device;
    std::unique_ptr<MockContext> context;
    std::unique_ptr<Buffer> buffer;
};

TEST_F(RenderCompressedBuffersTests, givenBufferCompressedAllocationAndZeroCopyHostPtrWhenCheckingMemoryPropertiesThenForceDisableZeroCopyAndAllocateStorage) {
    localHwInfo.capabilityTable.ftrRenderCompressedBuffers = false;

    void *cacheAlignedHostPtr = alignedMalloc(MemoryConstants::cacheLineSize, MemoryConstants::cacheLineSize);

    buffer.reset(Buffer::create(context.get(), CL_MEM_USE_HOST_PTR, MemoryConstants::cacheLineSize, cacheAlignedHostPtr, retVal));
    EXPECT_EQ(cacheAlignedHostPtr, buffer->getGraphicsAllocation()->getUnderlyingBuffer());
    EXPECT_TRUE(buffer->isMemObjZeroCopy());
    EXPECT_EQ(buffer->getGraphicsAllocation()->getAllocationType(), GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY);

    uint32_t pattern[2] = {0, 0};
    pattern[0] = 0xdeadbeef;
    pattern[1] = 0xdeadbeef;

    static_assert(sizeof(pattern) <= MemoryConstants::cacheLineSize, "Incorrect pattern size");

    uint32_t *dest = reinterpret_cast<uint32_t *>(cacheAlignedHostPtr);

    for (size_t i = 0; i < arrayCount(pattern); i++) {
        dest[i] = pattern[i];
    }

    localHwInfo.capabilityTable.ftrRenderCompressedBuffers = true;
    buffer.reset(Buffer::create(context.get(), CL_MEM_USE_HOST_PTR, MemoryConstants::cacheLineSize, cacheAlignedHostPtr, retVal));
    EXPECT_NE(cacheAlignedHostPtr, buffer->getGraphicsAllocation()->getUnderlyingBuffer());
    EXPECT_FALSE(buffer->isMemObjZeroCopy());
    EXPECT_EQ(buffer->getGraphicsAllocation()->getAllocationType(), GraphicsAllocation::AllocationType::BUFFER_COMPRESSED);

    EXPECT_THAT(buffer->getGraphicsAllocation()->getUnderlyingBuffer(), MemCompare(&pattern[0], sizeof(pattern)));

    alignedFree(cacheAlignedHostPtr);
}

TEST_F(RenderCompressedBuffersTests, givenBufferCompressedAllocationAndNoHostPtrWhenCheckingMemoryPropertiesThenForceDisableZeroCopy) {
    localHwInfo.capabilityTable.ftrRenderCompressedBuffers = false;

    buffer.reset(Buffer::create(context.get(), 0, MemoryConstants::cacheLineSize, nullptr, retVal));
    EXPECT_TRUE(buffer->isMemObjZeroCopy());
    if (MemoryPool::isSystemMemoryPool(buffer->getGraphicsAllocation()->getMemoryPool())) {
        EXPECT_EQ(buffer->getGraphicsAllocation()->getAllocationType(), GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY);
    } else {
        EXPECT_EQ(buffer->getGraphicsAllocation()->getAllocationType(), GraphicsAllocation::AllocationType::BUFFER);
    }

    localHwInfo.capabilityTable.ftrRenderCompressedBuffers = true;
    buffer.reset(Buffer::create(context.get(), 0, MemoryConstants::cacheLineSize, nullptr, retVal));
    EXPECT_FALSE(buffer->isMemObjZeroCopy());
    EXPECT_EQ(buffer->getGraphicsAllocation()->getAllocationType(), GraphicsAllocation::AllocationType::BUFFER_COMPRESSED);
}

TEST_F(RenderCompressedBuffersTests, givenBufferCompressedAllocationWhenSharedContextIsUsedThenForceDisableCompression) {
    localHwInfo.capabilityTable.ftrRenderCompressedBuffers = true;
    context->isSharedContext = false;

    uint32_t hostPtr = 0;

    buffer.reset(Buffer::create(context.get(), 0, sizeof(uint32_t), &hostPtr, retVal));
    EXPECT_EQ(buffer->getGraphicsAllocation()->getAllocationType(), GraphicsAllocation::AllocationType::BUFFER_COMPRESSED);

    context->isSharedContext = true;
    buffer.reset(Buffer::create(context.get(), 0, sizeof(uint32_t), &hostPtr, retVal));
    EXPECT_EQ(buffer->getGraphicsAllocation()->getAllocationType(), GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY);
}

TEST_F(RenderCompressedBuffersTests, givenSvmAllocationWhenCreatingBufferThenForceDisableCompression) {
    localHwInfo.capabilityTable.ftrRenderCompressedBuffers = true;

    auto svmAlloc = context->getSVMAllocsManager()->createSVMAlloc(sizeof(uint32_t), false);

    buffer.reset(Buffer::create(context.get(), CL_MEM_USE_HOST_PTR, sizeof(uint32_t), svmAlloc, retVal));
    EXPECT_EQ(buffer->getGraphicsAllocation()->getAllocationType(), GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY);

    context->getSVMAllocsManager()->freeSVMAlloc(svmAlloc);
}

struct RenderCompressedBuffersCopyHostMemoryTests : public RenderCompressedBuffersTests {
    void SetUp() override {
        RenderCompressedBuffersTests::SetUp();
        device->injectMemoryManager(new MockMemoryManager(true));
        context->setMemoryManager(device->getMemoryManager());
        mockCmdQ = new MockCommandQueue();
        context->setSpecialQueue(mockCmdQ);
    }

    MockCommandQueue *mockCmdQ = nullptr;
    uint32_t hostPtr = 0;
};

TEST_F(RenderCompressedBuffersCopyHostMemoryTests, givenRenderCompressedBufferWhenCopyFromHostPtrIsRequiredThenCallWriteBuffer) {
    localHwInfo.capabilityTable.ftrRenderCompressedBuffers = true;

    buffer.reset(Buffer::create(context.get(), CL_MEM_COPY_HOST_PTR, sizeof(uint32_t), &hostPtr, retVal));
    EXPECT_EQ(buffer->getGraphicsAllocation()->getAllocationType(), GraphicsAllocation::AllocationType::BUFFER_COMPRESSED);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(1u, mockCmdQ->writeBufferCounter);
    EXPECT_TRUE(mockCmdQ->writeBufferBlocking);
    EXPECT_EQ(0u, mockCmdQ->writeBufferOffset);
    EXPECT_EQ(sizeof(uint32_t), mockCmdQ->writeBufferSize);
    EXPECT_EQ(&hostPtr, mockCmdQ->writeBufferPtr);
}

TEST_F(RenderCompressedBuffersCopyHostMemoryTests, givenNonRenderCompressedBufferWhenCopyFromHostPtrIsRequiredThenDontCallWriteBuffer) {
    localHwInfo.capabilityTable.ftrRenderCompressedBuffers = false;

    buffer.reset(Buffer::create(context.get(), CL_MEM_COPY_HOST_PTR, sizeof(uint32_t), &hostPtr, retVal));
    EXPECT_NE(buffer->getGraphicsAllocation()->getAllocationType(), GraphicsAllocation::AllocationType::BUFFER_COMPRESSED);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(0u, mockCmdQ->writeBufferCounter);
}

TEST_F(RenderCompressedBuffersCopyHostMemoryTests, givenRenderCompressedBufferWhenWriteBufferFailsThenReturnErrorCode) {
    localHwInfo.capabilityTable.ftrRenderCompressedBuffers = true;
    mockCmdQ->writeBufferRetValue = CL_INVALID_VALUE;

    buffer.reset(Buffer::create(context.get(), CL_MEM_COPY_HOST_PTR, sizeof(uint32_t), &hostPtr, retVal));
    EXPECT_EQ(CL_OUT_OF_RESOURCES, retVal);
    EXPECT_EQ(nullptr, buffer.get());
}

class BufferTest : public DeviceFixture,
                   public testing::TestWithParam<uint64_t /*cl_mem_flags*/> {
  public:
    BufferTest() {
    }

  protected:
    void SetUp() override {
        flags = GetParam();
        DeviceFixture::SetUp();
        context.reset(new MockContext(pDevice));
    }

    void TearDown() override {
        context.reset();
        DeviceFixture::TearDown();
    }

    cl_int retVal = CL_SUCCESS;
    std::unique_ptr<MockContext> context;
    MemoryManager *contextMemoryManager;
    cl_mem_flags flags = 0;
    unsigned char pHostPtr[g_scTestBufferSizeInBytes];
};

typedef BufferTest NoHostPtr;

TEST_P(NoHostPtr, ValidFlags) {
    auto buffer = Buffer::create(
        context.get(),
        flags,
        g_scTestBufferSizeInBytes,
        nullptr,
        retVal);

    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, buffer);

    auto address = buffer->getCpuAddress();
    EXPECT_NE(nullptr, address);

    delete buffer;
}

TEST_P(NoHostPtr, GivenNoHostPtrWhenHwBufferCreationFailsThenReturnNullptr) {
    BufferFuncs BufferFuncsBackup[IGFX_MAX_CORE];

    for (uint32_t i = 0; i < IGFX_MAX_CORE; i++) {
        BufferFuncsBackup[i] = bufferFactory[i];
        bufferFactory[i].createBufferFunction =
            [](Context *,
               cl_mem_flags,
               size_t,
               void *,
               void *,
               GraphicsAllocation *,
               bool,
               bool,
               bool)
            -> OCLRT::Buffer * { return nullptr; };
    }

    auto buffer = Buffer::create(
        context.get(),
        flags,
        g_scTestBufferSizeInBytes,
        nullptr,
        retVal);

    EXPECT_EQ(nullptr, buffer);

    for (uint32_t i = 0; i < IGFX_MAX_CORE; i++) {
        bufferFactory[i] = BufferFuncsBackup[i];
    }
}

TEST_P(NoHostPtr, WithUseHostPtr_returnsError) {
    auto buffer = Buffer::create(
        context.get(),
        flags | CL_MEM_USE_HOST_PTR,
        g_scTestBufferSizeInBytes,
        nullptr,
        retVal);
    EXPECT_EQ(CL_INVALID_HOST_PTR, retVal);
    EXPECT_EQ(nullptr, buffer);

    delete buffer;
}

TEST_P(NoHostPtr, WithCopyHostPtr_returnsError) {
    auto buffer = Buffer::create(
        context.get(),
        flags | CL_MEM_COPY_HOST_PTR,
        g_scTestBufferSizeInBytes,
        nullptr,
        retVal);
    EXPECT_EQ(CL_INVALID_HOST_PTR, retVal);
    EXPECT_EQ(nullptr, buffer);

    delete buffer;
}

TEST_P(NoHostPtr, withBufferGraphicsAllocationReportsBufferType) {
    auto buffer = Buffer::create(
        context.get(),
        flags,
        g_scTestBufferSizeInBytes,
        nullptr,
        retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, buffer);

    auto allocation = buffer->getGraphicsAllocation();
    if (MemoryPool::isSystemMemoryPool(allocation->getMemoryPool())) {
        EXPECT_EQ(allocation->getAllocationType(), GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY);
    } else {
        EXPECT_EQ(allocation->getAllocationType(), GraphicsAllocation::AllocationType::BUFFER);
    }

    auto isBufferWritable = !(flags & (CL_MEM_READ_ONLY | CL_MEM_HOST_READ_ONLY | CL_MEM_HOST_NO_ACCESS));
    EXPECT_EQ(isBufferWritable, allocation->isMemObjectsAllocationWithWritableFlags());

    delete buffer;
}

// Parameterized test that tests buffer creation with all flags
// that should be valid with a nullptr host ptr
cl_mem_flags NoHostPtrFlags[] = {
    CL_MEM_READ_WRITE,
    CL_MEM_WRITE_ONLY,
    CL_MEM_READ_ONLY,
    CL_MEM_HOST_READ_ONLY,
    CL_MEM_HOST_WRITE_ONLY,
    CL_MEM_HOST_NO_ACCESS};

INSTANTIATE_TEST_CASE_P(
    BufferTest_Create,
    NoHostPtr,
    testing::ValuesIn(NoHostPtrFlags));

struct ValidHostPtr
    : public BufferTest,
      public MemoryManagementFixture {
    typedef BufferTest BaseClass;

    using BufferTest::SetUp;
    using MemoryManagementFixture::SetUp;

    ValidHostPtr() {
    }

    void SetUp() override {
        MemoryManagementFixture::SetUp();
        BaseClass::SetUp();

        ASSERT_NE(nullptr, pDevice);
    }

    void TearDown() override {
        delete buffer;
        BaseClass::TearDown();
        MemoryManagementFixture::TearDown();
    }

    Buffer *createBuffer() {
        return Buffer::create(
            context.get(),
            flags,
            g_scTestBufferSizeInBytes,
            pHostPtr,
            retVal);
    }

    cl_int retVal = CL_INVALID_VALUE;
    Buffer *buffer = nullptr;
};

TEST_P(ValidHostPtr, isResident_defaultsToFalseAfterCreate) {
    buffer = createBuffer();
    ASSERT_NE(nullptr, buffer);

    EXPECT_FALSE(buffer->getGraphicsAllocation()->isResident());
}

TEST_P(ValidHostPtr, isResident_returnsValueFromSetResident) {
    buffer = createBuffer();
    ASSERT_NE(nullptr, buffer);

    buffer->getGraphicsAllocation()->residencyTaskCount = 1;
    EXPECT_TRUE(buffer->getGraphicsAllocation()->isResident());

    buffer->getGraphicsAllocation()->residencyTaskCount = ObjectNotResident;
    EXPECT_FALSE(buffer->getGraphicsAllocation()->isResident());
}

TEST_P(ValidHostPtr, getAddress) {
    buffer = createBuffer();
    ASSERT_NE(nullptr, buffer);

    auto address = buffer->getCpuAddress();
    EXPECT_NE(nullptr, address);
    if (flags & CL_MEM_USE_HOST_PTR && buffer->isMemObjZeroCopy()) {
        // Buffer should use host ptr
        EXPECT_EQ(pHostPtr, address);
    } else {
        // Buffer should have a different ptr
        EXPECT_NE(pHostPtr, address);
    }

    if (flags & CL_MEM_COPY_HOST_PTR) {
        // Buffer should contain a copy of host memory
        EXPECT_EQ(0, memcmp(pHostPtr, address, sizeof(g_scTestBufferSizeInBytes)));
    }
}

TEST_P(ValidHostPtr, getSize) {
    buffer = createBuffer();
    ASSERT_NE(nullptr, buffer);

    EXPECT_EQ(g_scTestBufferSizeInBytes, buffer->getSize());
}

TEST_P(ValidHostPtr, givenValidHostPtrParentFlagsWhenSubBufferIsCreatedWithZeroFlagsThenItCreatesSuccesfuly) {
    auto retVal = CL_SUCCESS;
    auto clBuffer = clCreateBuffer(context.get(),
                                   flags,
                                   g_scTestBufferSizeInBytes,
                                   pHostPtr,
                                   &retVal);

    ASSERT_NE(nullptr, clBuffer);

    cl_buffer_region region = {0, g_scTestBufferSizeInBytes};

    auto subBuffer = clCreateSubBuffer(clBuffer,
                                       0,
                                       CL_BUFFER_CREATE_TYPE_REGION,
                                       &region,
                                       &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clReleaseMemObject(subBuffer);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clReleaseMemObject(clBuffer);
    EXPECT_EQ(CL_SUCCESS, retVal);
}
TEST_P(ValidHostPtr, givenValidHostPtrParentFlagsWhenSubBufferIsCreatedWithParentFlagsThenItIsCreatedSuccesfuly) {
    auto retVal = CL_SUCCESS;
    auto clBuffer = clCreateBuffer(context.get(),
                                   flags,
                                   g_scTestBufferSizeInBytes,
                                   pHostPtr,
                                   &retVal);

    ASSERT_NE(nullptr, clBuffer);
    cl_buffer_region region = {0, g_scTestBufferSizeInBytes};

    const cl_mem_flags allValidFlags =
        CL_MEM_READ_WRITE | CL_MEM_WRITE_ONLY | CL_MEM_READ_ONLY |
        CL_MEM_HOST_WRITE_ONLY | CL_MEM_HOST_READ_ONLY | CL_MEM_HOST_NO_ACCESS;

    cl_mem_flags unionFlags = flags & allValidFlags;
    auto subBuffer = clCreateSubBuffer(clBuffer,
                                       unionFlags,
                                       CL_BUFFER_CREATE_TYPE_REGION,
                                       &region,
                                       &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, subBuffer);
    retVal = clReleaseMemObject(subBuffer);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clReleaseMemObject(clBuffer);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_P(ValidHostPtr, givenValidHostPtrParentFlagsWhenSubBufferIsCreatedWithInvalidParentFlagsThenCreationFails) {
    auto retVal = CL_SUCCESS;
    cl_mem_flags invalidFlags = 0;
    if (flags & CL_MEM_READ_ONLY) {
        invalidFlags |= CL_MEM_WRITE_ONLY;
    }
    if (flags & CL_MEM_WRITE_ONLY) {
        invalidFlags |= CL_MEM_READ_ONLY;
    }
    if (flags & CL_MEM_HOST_NO_ACCESS) {
        invalidFlags |= CL_MEM_HOST_READ_ONLY;
    }
    if (flags & CL_MEM_HOST_READ_ONLY) {
        invalidFlags |= CL_MEM_HOST_WRITE_ONLY;
    }
    if (flags & CL_MEM_HOST_WRITE_ONLY) {
        invalidFlags |= CL_MEM_HOST_READ_ONLY;
    }
    if (invalidFlags == 0) {
        return;
    }

    auto clBuffer = clCreateBuffer(context.get(),
                                   flags,
                                   g_scTestBufferSizeInBytes,
                                   pHostPtr,
                                   &retVal);

    ASSERT_NE(nullptr, clBuffer);
    cl_buffer_region region = {0, g_scTestBufferSizeInBytes};

    auto subBuffer = clCreateSubBuffer(clBuffer,
                                       invalidFlags,
                                       CL_BUFFER_CREATE_TYPE_REGION,
                                       &region,
                                       &retVal);
    EXPECT_NE(CL_SUCCESS, retVal);
    EXPECT_EQ(nullptr, subBuffer);
    retVal = clReleaseMemObject(clBuffer);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_P(ValidHostPtr, failedAllocationInjection) {
    InjectedFunction method = [this](size_t failureIndex) {
        delete buffer;
        buffer = nullptr;

        // System under test
        buffer = createBuffer();

        if (nonfailingAllocation == failureIndex) {
            EXPECT_EQ(CL_SUCCESS, retVal);
            EXPECT_NE(nullptr, buffer);
        }
    };
    injectFailures(method);
}

TEST_P(ValidHostPtr, SvmHostPtr) {
    const DeviceInfo &devInfo = pDevice->getDeviceInfo();
    if (devInfo.svmCapabilities != 0) {
        auto ptr = context->getSVMAllocsManager()->createSVMAlloc(64, false);

        auto bufferSvm = Buffer::create(context.get(), CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR, 64, ptr, retVal);
        EXPECT_NE(nullptr, bufferSvm);
        EXPECT_TRUE(bufferSvm->isMemObjWithHostPtrSVM());
        EXPECT_EQ(context->getSVMAllocsManager()->getSVMAlloc(ptr), bufferSvm->getGraphicsAllocation());
        EXPECT_EQ(CL_SUCCESS, retVal);

        context->getSVMAllocsManager()->freeSVMAlloc(ptr);
        delete bufferSvm;
    }
}

// Parameterized test that tests buffer creation with all flags that should be
// valid with a valid host ptr
cl_mem_flags ValidHostPtrFlags[] = {
    0 | CL_MEM_USE_HOST_PTR,
    CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR,
    CL_MEM_WRITE_ONLY | CL_MEM_USE_HOST_PTR,
    CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR,
    CL_MEM_HOST_READ_ONLY | CL_MEM_USE_HOST_PTR,
    CL_MEM_HOST_WRITE_ONLY | CL_MEM_USE_HOST_PTR,
    CL_MEM_HOST_NO_ACCESS | CL_MEM_USE_HOST_PTR,
    0 | CL_MEM_COPY_HOST_PTR,
    CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
    CL_MEM_WRITE_ONLY | CL_MEM_COPY_HOST_PTR,
    CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
    CL_MEM_HOST_READ_ONLY | CL_MEM_COPY_HOST_PTR,
    CL_MEM_HOST_WRITE_ONLY | CL_MEM_COPY_HOST_PTR,
    CL_MEM_HOST_NO_ACCESS | CL_MEM_COPY_HOST_PTR};

INSTANTIATE_TEST_CASE_P(
    BufferTest_Create,
    ValidHostPtr,
    testing::ValuesIn(ValidHostPtrFlags));

class BufferCalculateHostPtrSize : public testing::TestWithParam<std::tuple<size_t, size_t, size_t, size_t, size_t, size_t, size_t, size_t, size_t>> {
  public:
    BufferCalculateHostPtrSize(){};

  protected:
    void SetUp() override {
        std::tie(origin[0], origin[1], origin[2], region[0], region[1], region[2], rowPitch, slicePitch, hostPtrSize) = GetParam();
    }

    void TearDown() override {
    }

    size_t origin[3];
    size_t region[3];
    size_t rowPitch;
    size_t slicePitch;
    size_t hostPtrSize;
};
/* origin, region, rowPitch, slicePitch, hostPtrSize*/
static std::tuple<size_t, size_t, size_t, size_t, size_t, size_t, size_t, size_t, size_t> Inputs[] = {std::make_tuple(0, 0, 0, 1, 1, 1, 10, 1, 1),
                                                                                                      std::make_tuple(0, 0, 0, 7, 1, 1, 10, 1, 7),
                                                                                                      std::make_tuple(0, 0, 0, 7, 3, 1, 10, 1, 27),
                                                                                                      std::make_tuple(0, 0, 0, 7, 1, 3, 10, 10, 27),
                                                                                                      std::make_tuple(0, 0, 0, 7, 2, 3, 10, 20, 57),
                                                                                                      std::make_tuple(0, 0, 0, 7, 1, 3, 10, 30, 67),
                                                                                                      std::make_tuple(0, 0, 0, 7, 2, 3, 10, 30, 77),
                                                                                                      std::make_tuple(9, 0, 0, 1, 1, 1, 10, 1, 10),
                                                                                                      std::make_tuple(0, 2, 0, 7, 3, 1, 10, 1, 27 + 20),
                                                                                                      std::make_tuple(0, 0, 1, 7, 1, 3, 10, 10, 27 + 10),
                                                                                                      std::make_tuple(0, 2, 1, 7, 2, 3, 10, 20, 57 + 40),
                                                                                                      std::make_tuple(1, 1, 1, 7, 1, 3, 10, 30, 67 + 41),
                                                                                                      std::make_tuple(2, 0, 2, 7, 2, 3, 10, 30, 77 + 62)};

TEST_P(BufferCalculateHostPtrSize, CheckReturnedSize) {
    size_t calculatedSize = Buffer::calculateHostPtrSize(origin, region, rowPitch, slicePitch);
    EXPECT_EQ(hostPtrSize, calculatedSize);
}

INSTANTIATE_TEST_CASE_P(
    BufferCalculateHostPtrSizes,
    BufferCalculateHostPtrSize,
    testing::ValuesIn(Inputs));

TEST(Buffers64on32Tests, given32BitBufferCreatedWithUseHostPtrFlagThatIsZeroCopyWhenAskedForStorageThenHostPtrIsReturned) {
    DebugManagerStateRestore dbgRestorer;
    {
        DebugManager.flags.Force32bitAddressing.set(true);
        MockContext context;

        auto size = MemoryConstants::pageSize;
        void *ptr = (void *)0x1000;
        auto ptrOffset = MemoryConstants::cacheLineSize;
        uintptr_t offsetedPtr = (uintptr_t)ptr + ptrOffset;
        auto retVal = CL_SUCCESS;

        auto buffer = Buffer::create(
            &context,
            CL_MEM_USE_HOST_PTR,
            size,
            (void *)offsetedPtr,
            retVal);
        EXPECT_EQ(CL_SUCCESS, retVal);

        EXPECT_TRUE(buffer->isMemObjZeroCopy());
        EXPECT_EQ((void *)offsetedPtr, buffer->getCpuAddressForMapping());
        EXPECT_EQ((void *)offsetedPtr, buffer->getCpuAddressForMemoryTransfer());

        delete buffer;
        DebugManager.flags.Force32bitAddressing.set(false);
    }
}

TEST(Buffers64on32Tests, given32BitBufferCreatedWithAllocHostPtrFlagThatIsZeroCopyWhenAskedForStorageThenStorageIsEqualToMemoryStorage) {
    DebugManagerStateRestore dbgRestorer;
    {
        DebugManager.flags.Force32bitAddressing.set(true);
        MockContext context;
        auto size = MemoryConstants::pageSize;
        auto retVal = CL_SUCCESS;

        auto buffer = Buffer::create(
            &context,
            CL_MEM_ALLOC_HOST_PTR,
            size,
            nullptr,
            retVal);
        EXPECT_EQ(CL_SUCCESS, retVal);

        EXPECT_TRUE(buffer->isMemObjZeroCopy());
        EXPECT_EQ(buffer->getCpuAddress(), buffer->getCpuAddressForMapping());
        EXPECT_EQ(buffer->getCpuAddress(), buffer->getCpuAddressForMemoryTransfer());

        delete buffer;
        DebugManager.flags.Force32bitAddressing.set(false);
    }
}

TEST(Buffers64on32Tests, given32BitBufferThatIsCreatedWithUseHostPtrButIsNotZeroCopyThenProperPointersAreReturned) {
    DebugManagerStateRestore dbgRestorer;
    {
        DebugManager.flags.Force32bitAddressing.set(true);
        MockContext context;

        auto size = MemoryConstants::pageSize;
        void *ptr = (void *)alignedMalloc(size * 2, MemoryConstants::pageSize);
        auto ptrOffset = 1;
        uintptr_t offsetedPtr = (uintptr_t)ptr + ptrOffset;
        auto retVal = CL_SUCCESS;

        auto buffer = Buffer::create(
            &context,
            CL_MEM_USE_HOST_PTR,
            size,
            (void *)offsetedPtr,
            retVal);
        EXPECT_EQ(CL_SUCCESS, retVal);

        EXPECT_FALSE(buffer->isMemObjZeroCopy());
        EXPECT_EQ((void *)offsetedPtr, buffer->getCpuAddressForMapping());
        EXPECT_EQ(buffer->getCpuAddress(), buffer->getCpuAddressForMemoryTransfer());

        delete buffer;
        DebugManager.flags.Force32bitAddressing.set(false);
        alignedFree(ptr);
    }
}

TEST(SharedBuffersTest, whenBuffersIsCreatedWithSharingHandlerThenItIsSharedBuffer) {
    MockContext context;
    auto memoryManager = context.getDevice(0)->getMemoryManager();
    auto handler = new SharingHandler();
    auto graphicsAlloaction = memoryManager->allocateGraphicsMemory(4096);
    auto buffer = Buffer::createSharedBuffer(&context, CL_MEM_READ_ONLY, handler, graphicsAlloaction);
    ASSERT_NE(nullptr, buffer);
    EXPECT_EQ(handler, buffer->peekSharingHandler());
    buffer->release();
}

class BufferTests : public ::testing::Test {
  protected:
    void SetUp() override {
        device.reset(MockDevice::createWithNewExecutionEnvironment<MockDevice>(*platformDevices));
    }
    void TearDown() override {
    }
    std::unique_ptr<Device> device;
};

typedef BufferTests BufferSetSurfaceTests;

HWTEST_F(BufferSetSurfaceTests, givenBufferSetSurfaceThatMemoryPtrAndSizeIsAlignedToCachelineThenL3CacheShouldBeOn) {

    auto size = MemoryConstants::pageSize;
    auto ptr = (void *)alignedMalloc(size * 2, MemoryConstants::pageSize);

    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    RENDER_SURFACE_STATE surfaceState = {};

    Buffer::setSurfaceState(
        device.get(),
        &surfaceState,
        size,
        ptr);

    auto mocs = surfaceState.getMemoryObjectControlState();
    auto gmmHelper = device->getGmmHelper();
    EXPECT_EQ(gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER), mocs);

    alignedFree(ptr);
}

HWTEST_F(BufferSetSurfaceTests, givenBufferSetSurfaceThatMemoryPtrIsUnalignedToCachelineThenL3CacheShouldBeOff) {

    auto size = MemoryConstants::pageSize;
    auto ptr = alignedMalloc(size * 2, MemoryConstants::pageSize);
    auto ptrOffset = 1;
    auto offsetedPtr = (void *)((uintptr_t)ptr + ptrOffset);

    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    RENDER_SURFACE_STATE surfaceState = {};

    Buffer::setSurfaceState(
        device.get(),
        &surfaceState,
        size,
        offsetedPtr);

    auto mocs = surfaceState.getMemoryObjectControlState();
    auto gmmHelper = device->getGmmHelper();
    EXPECT_EQ(gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED), mocs);

    alignedFree(ptr);
}

HWTEST_F(BufferSetSurfaceTests, givenBufferSetSurfaceThatMemorySizeIsUnalignedToCachelineThenL3CacheShouldBeOff) {

    auto size = MemoryConstants::pageSize;
    auto ptr = alignedMalloc(size * 2, MemoryConstants::pageSize);
    auto sizeOffset = 1;
    auto offsetedSize = size + sizeOffset;

    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    RENDER_SURFACE_STATE surfaceState = {};

    Buffer::setSurfaceState(
        device.get(),
        &surfaceState,
        offsetedSize,
        ptr);

    auto mocs = surfaceState.getMemoryObjectControlState();
    auto gmmHelper = device->getGmmHelper();
    EXPECT_EQ(gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED), mocs);

    alignedFree(ptr);
}

HWTEST_F(BufferSetSurfaceTests, givenBufferSetSurfaceThatMemoryIsUnalignedToCachelineButReadOnlyThenL3CacheShouldBeStillOn) {

    auto size = MemoryConstants::pageSize;
    auto ptr = alignedMalloc(size * 2, MemoryConstants::pageSize);
    auto sizeOffset = 1;
    auto offsetedSize = size + sizeOffset;

    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    RENDER_SURFACE_STATE surfaceState = {};

    Buffer::setSurfaceState(
        device.get(),
        &surfaceState,
        offsetedSize,
        ptr,
        nullptr,
        CL_MEM_READ_ONLY);

    auto mocs = surfaceState.getMemoryObjectControlState();
    auto gmmHelper = device->getGmmHelper();
    EXPECT_EQ(gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER), mocs);

    alignedFree(ptr);
}

HWTEST_F(BufferSetSurfaceTests, givenBufferSetSurfaceThatMemorySizeIsUnalignedThenSurfaceSizeShouldBeAlignedToFour) {

    auto size = MemoryConstants::pageSize;
    auto ptr = alignedMalloc(size * 2, MemoryConstants::pageSize);
    auto sizeOffset = 1;
    auto offsetedSize = size + sizeOffset;

    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    RENDER_SURFACE_STATE surfaceState = {};

    Buffer::setSurfaceState(
        device.get(),
        &surfaceState,
        offsetedSize,
        ptr);

    auto width = surfaceState.getWidth();
    EXPECT_EQ(alignUp(width, 4), width);

    alignedFree(ptr);
}

HWTEST_F(BufferSetSurfaceTests, givenBufferSetSurfaceThatMemoryPtrIsNotNullThenBufferSurfaceShouldBeUsed) {

    auto size = MemoryConstants::pageSize;
    auto ptr = alignedMalloc(size * 2, MemoryConstants::pageSize);

    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    RENDER_SURFACE_STATE surfaceState = {};

    Buffer::setSurfaceState(
        device.get(),
        &surfaceState,
        size,
        ptr);

    auto surfType = surfaceState.getSurfaceType();
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_BUFFER, surfType);

    alignedFree(ptr);
}

HWTEST_F(BufferSetSurfaceTests, givenBufferSetSurfaceThatMemoryPtrIsNullThenNullSurfaceShouldBeUsed) {

    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    RENDER_SURFACE_STATE surfaceState = {};

    Buffer::setSurfaceState(
        device.get(),
        &surfaceState,
        0,
        nullptr);

    auto surfType = surfaceState.getSurfaceType();
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_NULL, surfType);
}

HWTEST_F(BufferSetSurfaceTests, givenBufferSetSurfaceThatAddressIsForcedTo32bitWhenSetArgStatefulIsCalledThenSurfaceBaseAddressIsPopulatedWithGpuAddress) {
    DebugManagerStateRestore dbgRestorer;
    {
        DebugManager.flags.Force32bitAddressing.set(true);
        MockContext context;
        auto size = MemoryConstants::pageSize;
        auto ptr = (void *)alignedMalloc(size * 2, MemoryConstants::pageSize);
        auto retVal = CL_SUCCESS;

        auto buffer = Buffer::create(
            &context,
            CL_MEM_USE_HOST_PTR,
            size,
            ptr,
            retVal);
        EXPECT_EQ(CL_SUCCESS, retVal);

        EXPECT_TRUE(is64bit ? buffer->getGraphicsAllocation()->is32BitAllocation : true);

        using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
        RENDER_SURFACE_STATE surfaceState = {};

        buffer->setArgStateful(&surfaceState, false);

        auto surfBaseAddress = surfaceState.getSurfaceBaseAddress();
        auto bufferAddress = buffer->getGraphicsAllocation()->getGpuAddress();
        EXPECT_EQ(bufferAddress, surfBaseAddress);

        delete buffer;
        alignedFree(ptr);
        DebugManager.flags.Force32bitAddressing.set(false);
    }
}

HWTEST_F(BufferSetSurfaceTests, givenBufferWithOffsetWhenSetArgStatefulIsCalledThenSurfaceBaseAddressIsProperlyOffseted) {
    MockContext context;
    auto size = MemoryConstants::pageSize;
    auto ptr = (void *)alignedMalloc(size * 2, MemoryConstants::pageSize);
    auto retVal = CL_SUCCESS;

    auto buffer = Buffer::create(
        &context,
        CL_MEM_USE_HOST_PTR,
        size,
        ptr,
        retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);

    cl_buffer_region region = {4, 8};
    retVal = -1;
    auto subBuffer = buffer->createSubBuffer(CL_MEM_READ_WRITE, &region, retVal);
    ASSERT_NE(nullptr, subBuffer);
    ASSERT_EQ(CL_SUCCESS, retVal);

    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    RENDER_SURFACE_STATE surfaceState = {};

    subBuffer->setArgStateful(&surfaceState, false);

    auto surfBaseAddress = surfaceState.getSurfaceBaseAddress();
    auto bufferAddress = buffer->getGraphicsAllocation()->getGpuAddress();
    EXPECT_EQ(bufferAddress + region.origin, surfBaseAddress);

    subBuffer->release();

    delete buffer;
    alignedFree(ptr);
    DebugManager.flags.Force32bitAddressing.set(false);
}

HWTEST_F(BufferSetSurfaceTests, givenRenderCompressedGmmResourceWhenSurfaceStateIsProgrammedThenSetAuxParams) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    using AUXILIARY_SURFACE_MODE = typename RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE;

    RENDER_SURFACE_STATE surfaceState = {};
    MockContext context;
    auto retVal = CL_SUCCESS;

    std::unique_ptr<Buffer> buffer(Buffer::create(&context, CL_MEM_READ_WRITE, 1, nullptr, retVal));
    buffer->getGraphicsAllocation()->setAllocationType(GraphicsAllocation::AllocationType::BUFFER_COMPRESSED);
    auto gmm = new Gmm(nullptr, 1, false);
    buffer->getGraphicsAllocation()->gmm = gmm;
    gmm->isRenderCompressed = true;

    buffer->setArgStateful(&surfaceState, false);

    EXPECT_EQ(0u, surfaceState.getAuxiliarySurfaceBaseAddress());
    EXPECT_TRUE(AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_CCS_E == surfaceState.getAuxiliarySurfaceMode());
    EXPECT_TRUE(RENDER_SURFACE_STATE::COHERENCY_TYPE_GPU_COHERENT == surfaceState.getCoherencyType());

    buffer->getGraphicsAllocation()->setAllocationType(GraphicsAllocation::AllocationType::BUFFER);
    buffer->setArgStateful(&surfaceState, false);
    EXPECT_TRUE(AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_NONE == surfaceState.getAuxiliarySurfaceMode());
}

HWTEST_F(BufferSetSurfaceTests, givenNonRenderCompressedGmmResourceWhenSurfaceStateIsProgrammedThenDontSetAuxParams) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    using AUXILIARY_SURFACE_MODE = typename RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE;

    RENDER_SURFACE_STATE surfaceState = {};
    MockContext context;
    auto retVal = CL_SUCCESS;

    std::unique_ptr<Buffer> buffer(Buffer::create(&context, CL_MEM_READ_WRITE, 1, nullptr, retVal));
    auto gmm = new Gmm(nullptr, 1, false);
    buffer->getGraphicsAllocation()->gmm = gmm;
    gmm->isRenderCompressed = false;

    buffer->setArgStateful(&surfaceState, false);

    EXPECT_EQ(0u, surfaceState.getAuxiliarySurfaceBaseAddress());
    EXPECT_TRUE(AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_NONE == surfaceState.getAuxiliarySurfaceMode());
    EXPECT_TRUE(RENDER_SURFACE_STATE::COHERENCY_TYPE_IA_COHERENT == surfaceState.getCoherencyType());
}

struct BufferUnmapTest : public DeviceFixture, public ::testing::Test {
    void SetUp() override {
        DeviceFixture::SetUp();
    }
    void TearDown() override {
        DeviceFixture::TearDown();
    }
};

HWTEST_F(BufferUnmapTest, givenBufferWithSharingHandlerWhenUnmappingThenUseEnqueueWriteBuffer) {
    MockContext context(pDevice);
    MockCommandQueueHw<FamilyType> cmdQ(&context, pDevice, nullptr);

    auto retVal = CL_SUCCESS;
    std::unique_ptr<Buffer> buffer(Buffer::create(&context, CL_MEM_ALLOC_HOST_PTR, 123, nullptr, retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);

    buffer->setSharingHandler(new SharingHandler());
    EXPECT_NE(nullptr, buffer->peekSharingHandler());

    auto mappedPtr = clEnqueueMapBuffer(&cmdQ, buffer.get(), CL_TRUE, CL_MAP_WRITE, 0, 1, 0, nullptr, nullptr, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(0u, cmdQ.EnqueueWriteBufferCounter);
    retVal = clEnqueueUnmapMemObject(&cmdQ, buffer.get(), mappedPtr, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(1u, cmdQ.EnqueueWriteBufferCounter);
    EXPECT_TRUE(cmdQ.blockingWriteBuffer);
}

HWTEST_F(BufferUnmapTest, givenBufferWithoutSharingHandlerWhenUnmappingThenDontUseEnqueueWriteBuffer) {
    MockContext context(pDevice);
    MockCommandQueueHw<FamilyType> cmdQ(&context, pDevice, nullptr);

    auto retVal = CL_SUCCESS;
    std::unique_ptr<Buffer> buffer(Buffer::create(&context, CL_MEM_ALLOC_HOST_PTR, 123, nullptr, retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(nullptr, buffer->peekSharingHandler());

    auto mappedPtr = clEnqueueMapBuffer(&cmdQ, buffer.get(), CL_TRUE, CL_MAP_READ, 0, 1, 0, nullptr, nullptr, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clEnqueueUnmapMemObject(&cmdQ, buffer.get(), mappedPtr, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(0u, cmdQ.EnqueueWriteBufferCounter);
}

using BufferTransferTests = BufferUnmapTest;

TEST_F(BufferTransferTests, givenBufferWhenTransferToHostPtrCalledThenCopyRequestedSizeAndOffsetOnly) {
    MockContext context(pDevice);
    auto retVal = CL_SUCCESS;
    const size_t bufferSize = 100;
    size_t ignoredParam = 123;
    MemObjOffsetArray copyOffset = {{20, ignoredParam, ignoredParam}};
    MemObjSizeArray copySize = {{10, ignoredParam, ignoredParam}};

    uint8_t hostPtr[bufferSize] = {};
    uint8_t expectedHostPtr[bufferSize] = {};
    std::unique_ptr<Buffer> buffer(Buffer::create(&context, CL_MEM_USE_HOST_PTR, bufferSize, hostPtr, retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto srcPtr = buffer->getCpuAddress();
    EXPECT_NE(srcPtr, hostPtr);
    memset(srcPtr, 123, bufferSize);
    memset(ptrOffset(expectedHostPtr, copyOffset[0]), 123, copySize[0]);

    buffer->transferDataToHostPtr(copySize, copyOffset);

    EXPECT_TRUE(memcmp(hostPtr, expectedHostPtr, copySize[0]) == 0);
}

TEST_F(BufferTransferTests, givenBufferWhenTransferFromHostPtrCalledThenCopyRequestedSizeAndOffsetOnly) {
    MockContext context(pDevice);
    auto retVal = CL_SUCCESS;
    const size_t bufferSize = 100;
    size_t ignoredParam = 123;
    MemObjOffsetArray copyOffset = {{20, ignoredParam, ignoredParam}};
    MemObjSizeArray copySize = {{10, ignoredParam, ignoredParam}};

    uint8_t hostPtr[bufferSize] = {};
    uint8_t expectedBufferMemory[bufferSize] = {};
    std::unique_ptr<Buffer> buffer(Buffer::create(&context, CL_MEM_USE_HOST_PTR, bufferSize, hostPtr, retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_NE(buffer->getCpuAddress(), hostPtr);
    memset(hostPtr, 123, bufferSize);
    memset(ptrOffset(expectedBufferMemory, copyOffset[0]), 123, copySize[0]);

    buffer->transferDataFromHostPtr(copySize, copyOffset);

    EXPECT_TRUE(memcmp(expectedBufferMemory, buffer->getCpuAddress(), copySize[0]) == 0);
}
