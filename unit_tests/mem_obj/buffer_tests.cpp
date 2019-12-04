/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/gmm_helper/gmm_helper.h"
#include "core/helpers/hw_helper.h"
#include "core/memory_manager/unified_memory_manager.h"
#include "core/unit_tests/helpers/debug_manager_state_restore.h"
#include "core/unit_tests/utilities/base_object_utils.h"
#include "public/cl_ext_private.h"
#include "runtime/command_queue/command_queue_hw.h"
#include "runtime/command_queue/gpgpu_walker.h"
#include "runtime/event/user_event.h"
#include "runtime/gmm_helper/gmm.h"
#include "runtime/gmm_helper/resource_info.h"
#include "runtime/helpers/array_count.h"
#include "runtime/helpers/memory_properties_flags_helpers.h"
#include "runtime/helpers/options.h"
#include "runtime/mem_obj/buffer.h"
#include "runtime/memory_manager/allocations_list.h"
#include "runtime/os_interface/os_context.h"
#include "runtime/platform/platform.h"
#include "test.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/fixtures/memory_management_fixture.h"
#include "unit_tests/gen_common/matchers.h"
#include "unit_tests/helpers/hw_parse.h"
#include "unit_tests/helpers/unit_test_helper.h"
#include "unit_tests/mocks/mock_buffer.h"
#include "unit_tests/mocks/mock_command_queue.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_execution_environment.h"
#include "unit_tests/mocks/mock_gmm_resource_info.h"
#include "unit_tests/mocks/mock_memory_manager.h"
#include "unit_tests/mocks/mock_timestamp_container.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

using namespace NEO;

static const unsigned int g_scTestBufferSizeInBytes = 16;

TEST(Buffer, giveBufferWhenAskedForPtrOffsetForMappingThenReturnCorrectValue) {
    MockContext ctx;
    cl_int retVal;
    std::unique_ptr<Buffer> buffer(Buffer::create(&ctx, 0, 1, nullptr, retVal));

    MemObjOffsetArray offset = {{4, 5, 6}};

    auto retOffset = buffer->calculateOffsetForMapping(offset);
    EXPECT_EQ(offset[0], retOffset);
}

TEST(Buffer, giveBufferCreateWithHostPtrButWithoutProperFlagsWhenCreatedThenErrorIsReturned) {
    MockContext ctx;
    cl_int retVal;
    auto hostPtr = reinterpret_cast<void *>(0x1774);
    std::unique_ptr<Buffer> buffer(Buffer::create(&ctx, CL_MEM_READ_WRITE, 1, hostPtr, retVal));
    EXPECT_EQ(retVal, CL_INVALID_HOST_PTR);
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
    MemoryPropertiesFlags memoryProperties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(flags, 0);
    EXPECT_TRUE(MockBuffer::isReadOnlyMemoryPermittedByFlags(memoryProperties));

    flags = CL_MEM_HOST_READ_ONLY | CL_MEM_READ_ONLY;
    memoryProperties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(flags, 0);
    EXPECT_TRUE(MockBuffer::isReadOnlyMemoryPermittedByFlags(memoryProperties));
}

class BufferReadOnlyTest : public testing::TestWithParam<uint64_t> {
};

TEST_P(BufferReadOnlyTest, givenNonReadOnlySetOfInputFlagsWhenPassedToisReadOnlyMemoryPermittedByFlagsThenFalseIsReturned) {
    class MockBuffer : public Buffer {
      public:
        using Buffer::isReadOnlyMemoryPermittedByFlags;
    };

    cl_mem_flags flags = GetParam() | CL_MEM_USE_HOST_PTR;
    MemoryPropertiesFlags memoryProperties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(flags, 0);
    EXPECT_FALSE(MockBuffer::isReadOnlyMemoryPermittedByFlags(memoryProperties));
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

TEST(Buffer, givenReadOnlyHostPtrMemoryWhenBufferIsCreatedWithReadOnlyFlagsThenBufferHasAllocatedNewMemoryStorageAndBufferIsNotZeroCopy) {
    void *memory = alignedMalloc(MemoryConstants::pageSize, MemoryConstants::pageSize);
    ASSERT_NE(nullptr, memory);

    memset(memory, 0xAA, MemoryConstants::pageSize);

    std::unique_ptr<MockDevice> device(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    ::testing::NiceMock<GMockMemoryManagerFailFirstAllocation> *memoryManager = new ::testing::NiceMock<GMockMemoryManagerFailFirstAllocation>(*device->getExecutionEnvironment());

    device->injectMemoryManager(memoryManager);
    MockContext ctx(device.get());

    // First fail simulates error for read only memory allocation
    EXPECT_CALL(*memoryManager, allocateGraphicsMemoryInDevicePool(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(nullptr))
        .WillRepeatedly(::testing::Invoke(memoryManager, &GMockMemoryManagerFailFirstAllocation::baseAllocateGraphicsMemoryInDevicePool));

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
    ::testing::NiceMock<GMockMemoryManagerFailFirstAllocation> *memoryManager = new ::testing::NiceMock<GMockMemoryManagerFailFirstAllocation>(*device->getExecutionEnvironment());

    device->injectMemoryManager(memoryManager);
    MockContext ctx(device.get());

    // First fail simulates error for read only memory allocation
    // Second fail returns nullptr
    EXPECT_CALL(*memoryManager, allocateGraphicsMemoryInDevicePool(::testing::_, ::testing::_))
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
    ::testing::NiceMock<GMockMemoryManagerFailFirstAllocation> *memoryManager = new ::testing::NiceMock<GMockMemoryManagerFailFirstAllocation>(*device->getExecutionEnvironment());

    device->injectMemoryManager(memoryManager);
    MockContext ctx(device.get());

    // First fail simulates error for read only memory allocation
    EXPECT_CALL(*memoryManager, allocateGraphicsMemoryInDevicePool(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(nullptr));

    cl_int retVal;
    cl_mem_flags flags = CL_MEM_HOST_READ_ONLY | CL_MEM_WRITE_ONLY | CL_MEM_USE_HOST_PTR;

    std::unique_ptr<Buffer> buffer(Buffer::create(&ctx, flags, MemoryConstants::pageSize, (void *)memory, retVal));

    EXPECT_EQ(nullptr, buffer.get());
    alignedFree(memory);
}

TEST(Buffer, givenNullPtrWhenBufferIsCreatedWithKernelReadOnlyFlagsThenBufferAllocationFailsAndReturnsNullptr) {
    std::unique_ptr<MockDevice> device(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    ::testing::NiceMock<GMockMemoryManagerFailFirstAllocation> *memoryManager = new ::testing::NiceMock<GMockMemoryManagerFailFirstAllocation>(*device->getExecutionEnvironment());

    device->injectMemoryManager(memoryManager);
    MockContext ctx(device.get());

    // First fail simulates error for read only memory allocation
    EXPECT_CALL(*memoryManager, allocateGraphicsMemoryInDevicePool(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(nullptr));

    cl_int retVal;
    cl_mem_flags flags = CL_MEM_HOST_READ_ONLY | CL_MEM_WRITE_ONLY;

    std::unique_ptr<Buffer> buffer(Buffer::create(&ctx, flags, MemoryConstants::pageSize, nullptr, retVal));

    EXPECT_EQ(nullptr, buffer.get());
}

TEST(Buffer, givenNullptrPassedToBufferCreateWhenAllocationIsNotSystemMemoryPoolThenBufferIsNotZeroCopy) {
    std::unique_ptr<MockDevice> device(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    ::testing::NiceMock<GMockMemoryManagerFailFirstAllocation> *memoryManager = new ::testing::NiceMock<GMockMemoryManagerFailFirstAllocation>(*device->getExecutionEnvironment());

    device->injectMemoryManager(memoryManager);
    MockContext ctx(device.get());

    EXPECT_CALL(*memoryManager, allocateGraphicsMemoryInDevicePool(::testing::_, ::testing::_))
        .WillOnce(::testing::Invoke(memoryManager, &GMockMemoryManagerFailFirstAllocation::allocateNonSystemGraphicsMemoryInDevicePool));

    cl_int retVal = 0;
    cl_mem_flags flags = CL_MEM_READ_WRITE;

    std::unique_ptr<Buffer> buffer(Buffer::create(&ctx, flags, MemoryConstants::pageSize, nullptr, retVal));

    ASSERT_NE(nullptr, buffer.get());
    EXPECT_FALSE(buffer->isMemObjZeroCopy());
}

TEST(Buffer, givenNullptrPassedToBufferCreateWhenAllocationIsNotSystemMemoryPoolThenAllocationIsNotAddedToHostPtrManager) {
    std::unique_ptr<MockDevice> device(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    ::testing::NiceMock<GMockMemoryManagerFailFirstAllocation> *memoryManager = new ::testing::NiceMock<GMockMemoryManagerFailFirstAllocation>(*device->getExecutionEnvironment());

    device->injectMemoryManager(memoryManager);
    MockContext ctx(device.get());

    EXPECT_CALL(*memoryManager, allocateGraphicsMemoryInDevicePool(::testing::_, ::testing::_))
        .WillOnce(::testing::Invoke(memoryManager, &GMockMemoryManagerFailFirstAllocation::allocateNonSystemGraphicsMemoryInDevicePool));

    cl_int retVal = 0;
    cl_mem_flags flags = CL_MEM_READ_WRITE;

    auto hostPtrManager = static_cast<MockHostPtrManager *>(memoryManager->getHostPtrManager());
    auto hostPtrAllocationCountBefore = hostPtrManager->getFragmentCount();
    std::unique_ptr<Buffer> buffer(Buffer::create(&ctx, flags, MemoryConstants::pageSize, nullptr, retVal));

    ASSERT_NE(nullptr, buffer.get());
    auto hostPtrAllocationCountAfter = hostPtrManager->getFragmentCount();

    EXPECT_EQ(hostPtrAllocationCountBefore, hostPtrAllocationCountAfter);
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

TEST(Buffer, givenHostPtrPassedToBufferCreateWhenMemUseHostPtrFlagisSetAndBufferIsNotZeroCopyThenCreateMapAllocationWithHostPtr) {
    std::unique_ptr<MockDevice> device(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MockContext ctx(device.get());

    cl_int retVal = 0;
    cl_mem_flags flags = CL_MEM_USE_HOST_PTR;
    auto size = MemoryConstants::pageSize;
    void *ptr = (void *)alignedMalloc(size * 2, MemoryConstants::pageSize);
    auto ptrOffset = 1;
    void *offsetedPtr = (void *)((uintptr_t)ptr + ptrOffset);

    std::unique_ptr<Buffer> buffer(Buffer::create(&ctx, flags, MemoryConstants::pageSize, offsetedPtr, retVal));
    ASSERT_NE(nullptr, buffer.get());

    auto mapAllocation = buffer->getMapAllocation();
    EXPECT_NE(nullptr, mapAllocation);
    EXPECT_EQ(offsetedPtr, mapAllocation->getUnderlyingBuffer());
    EXPECT_EQ(GraphicsAllocation::AllocationType::MAP_ALLOCATION, mapAllocation->getAllocationType());

    alignedFree(ptr);
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

    std::unique_ptr<Buffer> buffer(Buffer::create(&ctx, flags, MemoryConstants::pageSize, nullptr, retVal));

    ASSERT_NE(nullptr, buffer.get());
    EXPECT_EQ(GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY, buffer->getGraphicsAllocation()->getAllocationType());
}

TEST(Buffer, givenRenderCompressedBuffersEnabledWhenAllocationTypeIsQueriedThenBufferCompressedTypeIsReturnedIn64Bit) {
    MemoryPropertiesFlags memoryProperties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(0, 0);
    MockContext context;
    context.contextType = ContextType::CONTEXT_TYPE_UNRESTRICTIVE;
    context.isSharedContext = false;
    auto type = MockPublicAccessBuffer::getGraphicsAllocationType(memoryProperties, context, true, false, true);
    EXPECT_EQ(GraphicsAllocation::AllocationType::BUFFER_COMPRESSED, type);
}

TEST(Buffer, givenRenderCompressedBuffersDisabledLocalMemoryEnabledWhenAllocationTypeIsQueriedThenBufferTypeIsReturnedIn64Bit) {
    MemoryPropertiesFlags memoryProperties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(0, 0);
    MockContext context;
    context.contextType = ContextType::CONTEXT_TYPE_UNRESTRICTIVE;
    context.isSharedContext = false;
    auto type = MockPublicAccessBuffer::getGraphicsAllocationType(memoryProperties, context, false, true, true);
    EXPECT_EQ(GraphicsAllocation::AllocationType::BUFFER, type);
}

TEST(Buffer, givenSharedContextWhenAllocationTypeIsQueriedThenBufferHostMemoryTypeIsReturned) {
    MemoryPropertiesFlags memoryProperties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(0, 0);
    MockContext context;
    context.contextType = ContextType::CONTEXT_TYPE_UNRESTRICTIVE;
    context.isSharedContext = true;
    auto type = MockPublicAccessBuffer::getGraphicsAllocationType(memoryProperties, context, false, false, true);
    EXPECT_EQ(GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY, type);
}

TEST(Buffer, givenSharedContextAndRenderCompressedBuffersEnabledWhenAllocationTypeIsQueriedThenBufferHostMemoryTypeIsReturned) {
    MemoryPropertiesFlags memoryProperties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(0, 0);
    MockContext context;
    context.contextType = ContextType::CONTEXT_TYPE_UNRESTRICTIVE;
    context.isSharedContext = true;
    auto type = MockPublicAccessBuffer::getGraphicsAllocationType(memoryProperties, context, true, false, true);
    EXPECT_EQ(GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY, type);
}

TEST(Buffer, givenUseHostPtrFlagAndLocalMemoryDisabledWhenAllocationTypeIsQueriedThenBufferHostMemoryTypeIsReturned) {
    cl_mem_flags flags = CL_MEM_USE_HOST_PTR;
    MemoryPropertiesFlags memoryProperties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(flags, 0);
    MockContext context;
    context.contextType = ContextType::CONTEXT_TYPE_UNRESTRICTIVE;
    context.isSharedContext = false;
    auto type = MockPublicAccessBuffer::getGraphicsAllocationType(memoryProperties, context, false, false, true);
    EXPECT_EQ(GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY, type);
}

TEST(Buffer, givenUseHostPtrFlagAndLocalMemoryEnabledWhenAllocationTypeIsQueriedThenBufferTypeIsReturned) {
    cl_mem_flags flags = CL_MEM_USE_HOST_PTR;
    MemoryPropertiesFlags memoryProperties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(flags, 0);
    MockContext context;
    context.contextType = ContextType::CONTEXT_TYPE_UNRESTRICTIVE;
    context.isSharedContext = false;
    auto type = MockPublicAccessBuffer::getGraphicsAllocationType(memoryProperties, context, false, true, true);
    EXPECT_EQ(GraphicsAllocation::AllocationType::BUFFER, type);
}

TEST(Buffer, givenAllocHostPtrFlagWhenAllocationTypeIsQueriedThenBufferTypeIsReturned) {
    cl_mem_flags flags = CL_MEM_ALLOC_HOST_PTR;
    MemoryPropertiesFlags memoryProperties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(flags, 0);
    MockContext context;
    context.contextType = ContextType::CONTEXT_TYPE_UNRESTRICTIVE;
    context.isSharedContext = false;
    auto type = MockPublicAccessBuffer::getGraphicsAllocationType(memoryProperties, context, false, false, true);
    EXPECT_EQ(GraphicsAllocation::AllocationType::BUFFER, type);
}

TEST(Buffer, givenUseHostPtrFlagAndLocalMemoryDisabledAndRenderCompressedBuffersEnabledWhenAllocationTypeIsQueriedThenBufferMemoryTypeIsReturned) {
    cl_mem_flags flags = CL_MEM_USE_HOST_PTR;
    MemoryPropertiesFlags memoryProperties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(flags, 0);
    MockContext context;
    context.contextType = ContextType::CONTEXT_TYPE_UNRESTRICTIVE;
    context.isSharedContext = false;
    auto type = MockPublicAccessBuffer::getGraphicsAllocationType(memoryProperties, context, true, false, true);
    EXPECT_EQ(GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY, type);
}

TEST(Buffer, givenUseHostPtrFlagAndLocalMemoryEnabledAndRenderCompressedBuffersEnabledWhenAllocationTypeIsQueriedThenBufferMemoryTypeIsReturned) {
    cl_mem_flags flags = CL_MEM_USE_HOST_PTR;
    MemoryPropertiesFlags memoryProperties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(flags, 0);
    MockContext context;
    context.contextType = ContextType::CONTEXT_TYPE_UNRESTRICTIVE;
    context.isSharedContext = false;
    auto type = MockPublicAccessBuffer::getGraphicsAllocationType(memoryProperties, context, true, true, true);
    EXPECT_EQ(GraphicsAllocation::AllocationType::BUFFER_COMPRESSED, type);
}

TEST(Buffer, givenUseHostPointerFlagAndForceSharedPhysicalStorageWhenLocalMemoryIsEnabledThenBufferHostMemoryTypeIsReturned) {
    cl_mem_flags flags = CL_MEM_USE_HOST_PTR | CL_MEM_FORCE_SHARED_PHYSICAL_MEMORY_INTEL;
    MemoryPropertiesFlags memoryProperties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(flags, 0);
    MockContext context;
    context.contextType = ContextType::CONTEXT_TYPE_UNRESTRICTIVE;
    context.isSharedContext = false;
    auto type = MockPublicAccessBuffer::getGraphicsAllocationType(memoryProperties, context, true, true, true);
    EXPECT_EQ(GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY, type);
}

TEST(Buffer, givenAllocHostPtrFlagAndRenderCompressedBuffersEnabledWhenAllocationTypeIsQueriedThenBufferCompressedTypeIsReturned) {
    cl_mem_flags flags = CL_MEM_ALLOC_HOST_PTR;
    MemoryPropertiesFlags memoryProperties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(flags, 0);
    MockContext context;
    context.contextType = ContextType::CONTEXT_TYPE_UNRESTRICTIVE;
    context.isSharedContext = false;
    auto type = MockPublicAccessBuffer::getGraphicsAllocationType(memoryProperties, context, true, false, true);
    EXPECT_EQ(GraphicsAllocation::AllocationType::BUFFER_COMPRESSED, type);
}

TEST(Buffer, givenZeroFlagsNoSharedContextAndRenderCompressedBuffersDisabledWhenAllocationTypeIsQueriedThenBufferTypeIsReturned) {
    MemoryPropertiesFlags memoryProperties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(0, 0);
    MockContext context;
    context.contextType = ContextType::CONTEXT_TYPE_UNRESTRICTIVE;
    context.isSharedContext = false;
    auto type = MockPublicAccessBuffer::getGraphicsAllocationType(memoryProperties, context, false, false, true);
    EXPECT_EQ(GraphicsAllocation::AllocationType::BUFFER, type);
}

TEST(Buffer, givenClMemCopyHostPointerPassedToBufferCreateWhenAllocationIsNotInSystemMemoryPoolThenAllocationIsWrittenByEnqueueWriteBuffer) {
    ExecutionEnvironment *executionEnvironment = platformImpl->peekExecutionEnvironment();

    auto *memoryManager = new ::testing::NiceMock<GMockMemoryManagerFailFirstAllocation>(*executionEnvironment);
    executionEnvironment->memoryManager.reset(memoryManager);
    EXPECT_CALL(*memoryManager, allocateGraphicsMemoryInDevicePool(::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Invoke(memoryManager, &GMockMemoryManagerFailFirstAllocation::baseAllocateGraphicsMemoryInDevicePool));

    std::unique_ptr<MockDevice> device(MockDevice::create<MockDevice>(executionEnvironment, 0));

    MockContext ctx(device.get());
    EXPECT_CALL(*memoryManager, allocateGraphicsMemoryInDevicePool(::testing::_, ::testing::_))
        .WillOnce(::testing::Invoke(memoryManager, &GMockMemoryManagerFailFirstAllocation::allocateNonSystemGraphicsMemoryInDevicePool))
        .WillRepeatedly(::testing::Invoke(memoryManager, &GMockMemoryManagerFailFirstAllocation::baseAllocateGraphicsMemoryInDevicePool));

    cl_int retVal = 0;
    cl_mem_flags flags = CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR;
    char memory[] = {1, 2, 3, 4, 5, 6, 7, 8};
    auto taskCount = device->getGpgpuCommandStreamReceiver().peekLatestFlushedTaskCount();

    std::unique_ptr<Buffer> buffer(Buffer::create(&ctx, flags, sizeof(memory), memory, retVal));
    ASSERT_NE(nullptr, buffer.get());
    auto taskCountSent = device->getGpgpuCommandStreamReceiver().peekLatestFlushedTaskCount();
    EXPECT_LT(taskCount, taskCountSent);
}
struct RenderCompressedBuffersTests : public ::testing::Test {
    void SetUp() override {
        ExecutionEnvironment *executionEnvironment = platformImpl->peekExecutionEnvironment();
        hwInfo = executionEnvironment->getMutableHardwareInfo();
        device.reset(Device::create<MockDevice>(executionEnvironment, 0u));
        context = std::make_unique<MockContext>(device.get(), true);
        context->contextType = ContextType::CONTEXT_TYPE_UNRESTRICTIVE;
    }

    cl_int retVal = CL_SUCCESS;
    HardwareInfo *hwInfo = nullptr;
    std::unique_ptr<MockDevice> device;
    std::unique_ptr<MockContext> context;
    std::unique_ptr<Buffer> buffer;

    uint32_t hostPtr[2048];
    size_t bufferSize = sizeof(hostPtr);
};

TEST_F(RenderCompressedBuffersTests, givenBufferCompressedAllocationAndZeroCopyHostPtrWhenCheckingMemoryPropertiesThenUseHostPtrAndDontAllocateStorage) {
    hwInfo->capabilityTable.ftrRenderCompressedBuffers = false;

    void *cacheAlignedHostPtr = alignedMalloc(MemoryConstants::cacheLineSize, MemoryConstants::cacheLineSize);

    buffer.reset(Buffer::create(context.get(), CL_MEM_FORCE_SHARED_PHYSICAL_MEMORY_INTEL | CL_MEM_USE_HOST_PTR, MemoryConstants::cacheLineSize, cacheAlignedHostPtr, retVal));
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

    hwInfo->capabilityTable.ftrRenderCompressedBuffers = true;
    buffer.reset(Buffer::create(context.get(), CL_MEM_FORCE_SHARED_PHYSICAL_MEMORY_INTEL | CL_MEM_USE_HOST_PTR, MemoryConstants::cacheLineSize, cacheAlignedHostPtr, retVal));
    EXPECT_EQ(cacheAlignedHostPtr, buffer->getGraphicsAllocation()->getUnderlyingBuffer());
    EXPECT_TRUE(buffer->isMemObjZeroCopy());
    EXPECT_EQ(buffer->getGraphicsAllocation()->getAllocationType(), GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY);

    EXPECT_THAT(buffer->getGraphicsAllocation()->getUnderlyingBuffer(), MemCompare(&pattern[0], sizeof(pattern)));

    alignedFree(cacheAlignedHostPtr);
}

TEST_F(RenderCompressedBuffersTests, givenAllocationCreatedWithForceSharedPhysicalMemoryWhenItIsCreatedItIsZeroCopy) {
    buffer.reset(Buffer::create(context.get(), CL_MEM_FORCE_SHARED_PHYSICAL_MEMORY_INTEL, 1u, nullptr, retVal));
    EXPECT_EQ(buffer->getGraphicsAllocation()->getAllocationType(), GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY);
    EXPECT_TRUE(buffer->isMemObjZeroCopy());
    EXPECT_EQ(1u, buffer->getSize());
}

TEST_F(RenderCompressedBuffersTests, givenRenderCompressedBuffersAndAllocationCreatedWithForceSharedPhysicalMemoryWhenItIsCreatedItIsZeroCopy) {
    hwInfo->capabilityTable.ftrRenderCompressedBuffers = true;
    buffer.reset(Buffer::create(context.get(), CL_MEM_FORCE_SHARED_PHYSICAL_MEMORY_INTEL, 1u, nullptr, retVal));
    EXPECT_EQ(buffer->getGraphicsAllocation()->getAllocationType(), GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY);
    EXPECT_TRUE(buffer->isMemObjZeroCopy());
    EXPECT_EQ(1u, buffer->getSize());
}

TEST_F(RenderCompressedBuffersTests, givenBufferNotCompressedAllocationAndNoHostPtrWhenCheckingMemoryPropertiesThenForceDisableZeroCopy) {
    hwInfo->capabilityTable.ftrRenderCompressedBuffers = false;

    buffer.reset(Buffer::create(context.get(), 0, bufferSize, nullptr, retVal));
    EXPECT_TRUE(buffer->isMemObjZeroCopy());
    if (MemoryPool::isSystemMemoryPool(buffer->getGraphicsAllocation()->getMemoryPool())) {
        EXPECT_EQ(buffer->getGraphicsAllocation()->getAllocationType(), GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY);
    } else {
        EXPECT_EQ(buffer->getGraphicsAllocation()->getAllocationType(), GraphicsAllocation::AllocationType::BUFFER);
    }

    hwInfo->capabilityTable.ftrRenderCompressedBuffers = true;
    buffer.reset(Buffer::create(context.get(), 0, bufferSize, nullptr, retVal));
    if (HwHelper::get(context->getDevice(0)->getHardwareInfo().platform.eRenderCoreFamily).obtainRenderBufferCompressionPreference(context->getDevice(0)->getHardwareInfo(), bufferSize)) {
        EXPECT_FALSE(buffer->isMemObjZeroCopy());
        EXPECT_EQ(buffer->getGraphicsAllocation()->getAllocationType(), GraphicsAllocation::AllocationType::BUFFER_COMPRESSED);
    } else {
        EXPECT_TRUE(buffer->isMemObjZeroCopy());
        EXPECT_EQ(buffer->getGraphicsAllocation()->getAllocationType(), GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY);
    }
}

TEST_F(RenderCompressedBuffersTests, givenBufferCompressedAllocationWhenSharedContextIsUsedThenForceDisableCompression) {
    hwInfo->capabilityTable.ftrRenderCompressedBuffers = true;
    context->isSharedContext = false;

    buffer.reset(Buffer::create(context.get(), CL_MEM_READ_WRITE, bufferSize, nullptr, retVal));
    if (HwHelper::get(context->getDevice(0)->getHardwareInfo().platform.eRenderCoreFamily).obtainRenderBufferCompressionPreference(context->getDevice(0)->getHardwareInfo(), bufferSize)) {
        EXPECT_EQ(buffer->getGraphicsAllocation()->getAllocationType(), GraphicsAllocation::AllocationType::BUFFER_COMPRESSED);
    } else {
        EXPECT_EQ(buffer->getGraphicsAllocation()->getAllocationType(), GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY);
    }
    context->isSharedContext = true;
    buffer.reset(Buffer::create(context.get(), CL_MEM_USE_HOST_PTR, bufferSize, hostPtr, retVal));
    EXPECT_EQ(buffer->getGraphicsAllocation()->getAllocationType(), GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY);
}

TEST_F(RenderCompressedBuffersTests, givenDebugVariableSetWhenHwFlagIsNotSetThenSelectOptionFromDebugFlag) {
    DebugManagerStateRestore restore;

    hwInfo->capabilityTable.ftrRenderCompressedBuffers = false;

    DebugManager.flags.RenderCompressedBuffersEnabled.set(1);
    buffer.reset(Buffer::create(context.get(), 0, bufferSize, nullptr, retVal));
    if (HwHelper::get(context->getDevice(0)->getHardwareInfo().platform.eRenderCoreFamily).obtainRenderBufferCompressionPreference(context->getDevice(0)->getHardwareInfo(), bufferSize)) {
        EXPECT_EQ(buffer->getGraphicsAllocation()->getAllocationType(), GraphicsAllocation::AllocationType::BUFFER_COMPRESSED);
    } else {
        EXPECT_EQ(buffer->getGraphicsAllocation()->getAllocationType(), GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY);
    }

    DebugManager.flags.RenderCompressedBuffersEnabled.set(0);
    buffer.reset(Buffer::create(context.get(), 0, bufferSize, nullptr, retVal));
    EXPECT_NE(buffer->getGraphicsAllocation()->getAllocationType(), GraphicsAllocation::AllocationType::BUFFER_COMPRESSED);
}

struct RenderCompressedBuffersSvmTests : public RenderCompressedBuffersTests {
    void SetUp() override {
        ExecutionEnvironment *executionEnvironment = platformImpl->peekExecutionEnvironment();
        hwInfo = executionEnvironment->getMutableHardwareInfo();
        hwInfo->capabilityTable.gpuAddressSpace = MemoryConstants::max48BitAddress;
        RenderCompressedBuffersTests::SetUp();
    }
};

TEST_F(RenderCompressedBuffersSvmTests, givenSvmAllocationWhenCreatingBufferThenForceDisableCompression) {
    hwInfo->capabilityTable.ftrRenderCompressedBuffers = true;

    auto svmPtr = context->getSVMAllocsManager()->createSVMAlloc(device->getRootDeviceIndex(), sizeof(uint32_t), {});
    auto expectedAllocationType = context->getSVMAllocsManager()->getSVMAlloc(svmPtr)->gpuAllocation->getAllocationType();
    buffer.reset(Buffer::create(context.get(), CL_MEM_USE_HOST_PTR, sizeof(uint32_t), svmPtr, retVal));
    EXPECT_EQ(expectedAllocationType, buffer->getGraphicsAllocation()->getAllocationType());

    context->getSVMAllocsManager()->freeSVMAlloc(svmPtr);
}

struct RenderCompressedBuffersCopyHostMemoryTests : public RenderCompressedBuffersTests {
    void SetUp() override {
        RenderCompressedBuffersTests::SetUp();
        device->injectMemoryManager(new MockMemoryManager(true, false, *platformImpl->peekExecutionEnvironment()));
        context->memoryManager = device->getMemoryManager();
        mockCmdQ = new MockCommandQueue();
        context->setSpecialQueue(mockCmdQ);
    }

    MockCommandQueue *mockCmdQ = nullptr;
};

TEST_F(RenderCompressedBuffersCopyHostMemoryTests, givenRenderCompressedBufferWhenCopyFromHostPtrIsRequiredThenCallWriteBuffer) {
    if (is32bit) {
        return;
    }
    hwInfo->capabilityTable.ftrRenderCompressedBuffers = true;

    buffer.reset(Buffer::create(context.get(), CL_MEM_COPY_HOST_PTR, bufferSize, hostPtr, retVal));
    if (HwHelper::get(context->getDevice(0)->getHardwareInfo().platform.eRenderCoreFamily).obtainRenderBufferCompressionPreference(context->getDevice(0)->getHardwareInfo(), bufferSize)) {
        EXPECT_EQ(buffer->getGraphicsAllocation()->getAllocationType(), GraphicsAllocation::AllocationType::BUFFER_COMPRESSED);
        EXPECT_EQ(1u, mockCmdQ->writeBufferCounter);
        EXPECT_TRUE(mockCmdQ->writeBufferBlocking);
        EXPECT_EQ(0u, mockCmdQ->writeBufferOffset);
        EXPECT_EQ(bufferSize, mockCmdQ->writeBufferSize);
        EXPECT_EQ(hostPtr, mockCmdQ->writeBufferPtr);
    } else {
        EXPECT_EQ(buffer->getGraphicsAllocation()->getAllocationType(), GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY);
        EXPECT_EQ(0u, mockCmdQ->writeBufferCounter);
        EXPECT_FALSE(mockCmdQ->writeBufferBlocking);
        EXPECT_EQ(0u, mockCmdQ->writeBufferOffset);
        EXPECT_EQ(0u, mockCmdQ->writeBufferSize);
        EXPECT_EQ(nullptr, mockCmdQ->writeBufferPtr);
    }
    EXPECT_EQ(CL_SUCCESS, retVal);
}

struct BcsBufferTests : public ::testing::Test {
    class BcsMockContext : public MockContext {
      public:
        BcsMockContext(Device *device) : MockContext(device) {
            bcsOsContext.reset(OsContext::create(nullptr, 0, 0, aub_stream::ENGINE_BCS, PreemptionMode::Disabled, false));
            bcsCsr.reset(createCommandStream(*device->getExecutionEnvironment(), device->getRootDeviceIndex()));
            bcsCsr->setupContext(*bcsOsContext);
            bcsCsr->initializeTagAllocation();
        }
        CommandStreamReceiver *getCommandStreamReceiverForBlitOperation(MemObj &memObj) const override {
            return bcsCsr.get();
        }
        BlitOperationResult blitMemoryToAllocation(MemObj &memObj, GraphicsAllocation *memory, void *hostPtr, size_t size) const override {
            auto blitProperties = BlitProperties::constructPropertiesForReadWriteBuffer(BlitterConstants::BlitDirection::HostPtrToBuffer,
                                                                                        *bcsCsr, memory, 0, nullptr,
                                                                                        hostPtr, 0, 0, size);

            BlitPropertiesContainer container;
            container.push_back(blitProperties);
            bcsCsr->blitBuffer(container, true);

            return BlitOperationResult::Success;
        }
        std::unique_ptr<OsContext> bcsOsContext;
        std::unique_ptr<CommandStreamReceiver> bcsCsr;
    };

    template <typename FamilyType>
    class MyMockCsr : public UltCommandStreamReceiver<FamilyType> {
      public:
        using UltCommandStreamReceiver<FamilyType>::UltCommandStreamReceiver;

        void waitForTaskCountWithKmdNotifyFallback(uint32_t taskCountToWait, FlushStamp flushStampToWait,
                                                   bool useQuickKmdSleep, bool forcePowerSavingMode) override {
            EXPECT_EQ(this->latestFlushedTaskCount, taskCountToWait);
            EXPECT_EQ(0u, flushStampToWait);
            EXPECT_FALSE(useQuickKmdSleep);
            EXPECT_FALSE(forcePowerSavingMode);
            waitForTaskCountWithKmdNotifyFallbackCalled++;
        }

        void waitForTaskCountAndCleanAllocationList(uint32_t requiredTaskCount, uint32_t allocationUsage) override {
            EXPECT_EQ(1u, waitForTaskCountWithKmdNotifyFallbackCalled);
            EXPECT_EQ(this->latestFlushedTaskCount, requiredTaskCount);
            waitForTaskCountAndCleanAllocationListCalled++;
        }

        uint32_t waitForTaskCountAndCleanAllocationListCalled = 0;
        uint32_t waitForTaskCountWithKmdNotifyFallbackCalled = 0;
        CommandStreamReceiver *gpgpuCsr = nullptr;
    };

    template <typename FamilyType>
    void SetUpT() {
        if (is32bit) {
            GTEST_SKIP();
        }
        DebugManager.flags.EnableTimestampPacket.set(1);
        DebugManager.flags.EnableBlitterOperationsForReadWriteBuffers.set(1);
        device.reset(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
        auto &capabilityTable = device->getExecutionEnvironment()->getMutableHardwareInfo()->capabilityTable;
        bool createBcsEngine = !capabilityTable.blitterOperationsSupported;
        capabilityTable.blitterOperationsSupported = true;

        if (createBcsEngine) {
            auto &engine = device->getEngine(HwHelperHw<FamilyType>::lowPriorityEngineType, true);
            bcsOsContext.reset(OsContext::create(nullptr, 1, 0, aub_stream::ENGINE_BCS, PreemptionMode::Disabled, false));
            engine.osContext = bcsOsContext.get();
            engine.commandStreamReceiver->setupContext(*bcsOsContext);
        }

        bcsMockContext = std::make_unique<BcsMockContext>(device.get());
        commandQueue.reset(new MockCommandQueueHw<FamilyType>(bcsMockContext.get(), device.get(), nullptr));
    }

    template <typename FamilyType>
    void TearDownT() {}

    DebugManagerStateRestore restore;

    std::unique_ptr<OsContext> bcsOsContext;
    std::unique_ptr<MockDevice> device;
    std::unique_ptr<BcsMockContext> bcsMockContext;
    std::unique_ptr<CommandQueue> commandQueue;
    uint32_t hostPtr = 0;
    cl_int retVal = CL_SUCCESS;
};

HWTEST_TEMPLATED_F(BcsBufferTests, givenBufferWithInitializationDataAndBcsCsrWhenCreatingThenUseBlitOperation) {
    auto bcsCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(bcsMockContext->bcsCsr.get());
    auto newMemoryManager = new MockMemoryManager(true, true, *device->getExecutionEnvironment());
    device->getExecutionEnvironment()->memoryManager.reset(newMemoryManager);
    bcsMockContext->memoryManager = newMemoryManager;

    EXPECT_EQ(0u, bcsCsr->blitBufferCalled);
    auto bufferForBlt = clUniquePtr(Buffer::create(bcsMockContext.get(), CL_MEM_COPY_HOST_PTR, 2000, &hostPtr, retVal));
    EXPECT_EQ(1u, bcsCsr->blitBufferCalled);
}

HWTEST_TEMPLATED_F(BcsBufferTests, givenBcsSupportedWhenEnqueueBufferOperationIsCalledThenUseBcsCsr) {
    DebugManager.flags.EnableBlitterOperationsForReadWriteBuffers.set(0);
    auto bcsCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(commandQueue->getBcsCommandStreamReceiver());

    auto bufferForBlt0 = clUniquePtr(Buffer::create(bcsMockContext.get(), CL_MEM_READ_WRITE, 1, nullptr, retVal));
    auto bufferForBlt1 = clUniquePtr(Buffer::create(bcsMockContext.get(), CL_MEM_READ_WRITE, 1, nullptr, retVal));
    bufferForBlt0->forceDisallowCPUCopy = true;
    bufferForBlt1->forceDisallowCPUCopy = true;
    auto *hwInfo = device->getExecutionEnvironment()->getMutableHardwareInfo();

    DebugManager.flags.EnableBlitterOperationsForReadWriteBuffers.set(0);
    hwInfo->capabilityTable.blitterOperationsSupported = false;
    commandQueue->enqueueWriteBuffer(bufferForBlt0.get(), CL_TRUE, 0, 1, &hostPtr, nullptr, 0, nullptr, nullptr);
    commandQueue->enqueueReadBuffer(bufferForBlt0.get(), CL_TRUE, 0, 1, &hostPtr, nullptr, 0, nullptr, nullptr);
    commandQueue->enqueueCopyBuffer(bufferForBlt0.get(), bufferForBlt1.get(), 0, 1, 1, 0, nullptr, nullptr);

    DebugManager.flags.EnableBlitterOperationsForReadWriteBuffers.set(1);
    hwInfo->capabilityTable.blitterOperationsSupported = false;
    commandQueue->enqueueWriteBuffer(bufferForBlt0.get(), CL_TRUE, 0, 1, &hostPtr, nullptr, 0, nullptr, nullptr);
    commandQueue->enqueueReadBuffer(bufferForBlt0.get(), CL_TRUE, 0, 1, &hostPtr, nullptr, 0, nullptr, nullptr);
    commandQueue->enqueueCopyBuffer(bufferForBlt0.get(), bufferForBlt1.get(), 0, 1, 1, 0, nullptr, nullptr);

    DebugManager.flags.EnableBlitterOperationsForReadWriteBuffers.set(0);
    hwInfo->capabilityTable.blitterOperationsSupported = true;
    commandQueue->enqueueWriteBuffer(bufferForBlt0.get(), CL_TRUE, 0, 1, &hostPtr, nullptr, 0, nullptr, nullptr);
    commandQueue->enqueueReadBuffer(bufferForBlt0.get(), CL_TRUE, 0, 1, &hostPtr, nullptr, 0, nullptr, nullptr);
    commandQueue->enqueueCopyBuffer(bufferForBlt0.get(), bufferForBlt1.get(), 0, 1, 1, 0, nullptr, nullptr);

    DebugManager.flags.EnableBlitterOperationsForReadWriteBuffers.set(-1);
    hwInfo->capabilityTable.blitterOperationsSupported = true;
    commandQueue->enqueueWriteBuffer(bufferForBlt0.get(), CL_TRUE, 0, 1, &hostPtr, nullptr, 0, nullptr, nullptr);
    commandQueue->enqueueReadBuffer(bufferForBlt0.get(), CL_TRUE, 0, 1, &hostPtr, nullptr, 0, nullptr, nullptr);
    commandQueue->enqueueCopyBuffer(bufferForBlt0.get(), bufferForBlt1.get(), 0, 1, 1, 0, nullptr, nullptr);

    EXPECT_EQ(3u, bcsCsr->blitBufferCalled);

    DebugManager.flags.EnableBlitterOperationsForReadWriteBuffers.set(1);
    hwInfo->capabilityTable.blitterOperationsSupported = true;
    commandQueue->enqueueWriteBuffer(bufferForBlt0.get(), CL_TRUE, 0, 1, &hostPtr, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(4u, bcsCsr->blitBufferCalled);
    commandQueue->enqueueReadBuffer(bufferForBlt0.get(), CL_TRUE, 0, 1, &hostPtr, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(5u, bcsCsr->blitBufferCalled);
    commandQueue->enqueueCopyBuffer(bufferForBlt0.get(), bufferForBlt1.get(), 0, 1, 1, 0, nullptr, nullptr);
    EXPECT_EQ(6u, bcsCsr->blitBufferCalled);
}

HWTEST_TEMPLATED_F(BcsBufferTests, givenBcsSupportedWhenQueueIsBlockedThenDispatchBlitWhenUnblocked) {
    auto bcsCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(commandQueue->getBcsCommandStreamReceiver());

    auto bufferForBlt0 = clUniquePtr(Buffer::create(bcsMockContext.get(), CL_MEM_READ_WRITE, 1, nullptr, retVal));
    auto bufferForBlt1 = clUniquePtr(Buffer::create(bcsMockContext.get(), CL_MEM_READ_WRITE, 1, nullptr, retVal));
    bufferForBlt0->forceDisallowCPUCopy = true;
    bufferForBlt1->forceDisallowCPUCopy = true;
    UserEvent userEvent(bcsMockContext.get());
    cl_event waitlist = &userEvent;

    commandQueue->enqueueWriteBuffer(bufferForBlt0.get(), CL_FALSE, 0, 1, &hostPtr, nullptr, 1, &waitlist, nullptr);
    commandQueue->enqueueReadBuffer(bufferForBlt1.get(), CL_FALSE, 0, 1, &hostPtr, nullptr, 0, nullptr, nullptr);
    commandQueue->enqueueCopyBuffer(bufferForBlt0.get(), bufferForBlt1.get(), 0, 1, 1, 0, nullptr, nullptr);

    EXPECT_EQ(0u, bcsCsr->blitBufferCalled);

    userEvent.setStatus(CL_COMPLETE);

    EXPECT_EQ(3u, bcsCsr->blitBufferCalled);

    commandQueue->enqueueWriteBuffer(bufferForBlt0.get(), CL_FALSE, 0, 1, &hostPtr, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(4u, bcsCsr->blitBufferCalled);
    commandQueue->enqueueReadBuffer(bufferForBlt0.get(), CL_FALSE, 0, 1, &hostPtr, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(5u, bcsCsr->blitBufferCalled);
    commandQueue->enqueueCopyBuffer(bufferForBlt0.get(), bufferForBlt1.get(), 0, 1, 1, 0, nullptr, nullptr);
}

HWTEST_TEMPLATED_F(BcsBufferTests, givenBuffersWhenCopyBufferCalledThenUseBcs) {
    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;
    auto cmdQ = clUniquePtr(new MockCommandQueueHw<FamilyType>(bcsMockContext.get(), device.get(), nullptr));

    auto bufferForBlt0 = clUniquePtr(Buffer::create(bcsMockContext.get(), CL_MEM_READ_WRITE, 1, nullptr, retVal));
    auto bufferForBlt1 = clUniquePtr(Buffer::create(bcsMockContext.get(), CL_MEM_READ_WRITE, 1, nullptr, retVal));
    bufferForBlt0->forceDisallowCPUCopy = true;
    bufferForBlt1->forceDisallowCPUCopy = true;

    cmdQ->enqueueCopyBuffer(bufferForBlt0.get(), bufferForBlt1.get(), 0, 0, 1, 0, nullptr, nullptr);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(commandQueue->getBcsCommandStreamReceiver()->getCS(0));
    auto commandItor = find<XY_COPY_BLT *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    EXPECT_NE(hwParser.cmdList.end(), commandItor);
    auto copyBltCmd = genCmdCast<XY_COPY_BLT *>(*commandItor);

    EXPECT_EQ(bufferForBlt0->getGraphicsAllocation()->getGpuAddress(), copyBltCmd->getSourceBaseAddress());
    EXPECT_EQ(bufferForBlt1->getGraphicsAllocation()->getGpuAddress(), copyBltCmd->getDestinationBaseAddress());
}

HWTEST_TEMPLATED_F(BcsBufferTests, givenBlockedBlitEnqueueWhenUnblockingThenMakeResidentAllTimestampPackets) {
    auto bcsCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(commandQueue->getBcsCommandStreamReceiver());
    bcsCsr->storeMakeResidentAllocations = true;

    auto mockCmdQ = static_cast<MockCommandQueueHw<FamilyType> *>(commandQueue.get());

    auto bufferForBlt = clUniquePtr(Buffer::create(bcsMockContext.get(), CL_MEM_READ_WRITE, 1, nullptr, retVal));
    bufferForBlt->forceDisallowCPUCopy = true;

    TimestampPacketContainer previousTimestampPackets;
    mockCmdQ->obtainNewTimestampPacketNodes(1, previousTimestampPackets, false);
    auto dependencyFromPreviousEnqueue = mockCmdQ->timestampPacketContainer->peekNodes()[0];

    auto event = make_releaseable<Event>(mockCmdQ, CL_COMMAND_READ_BUFFER, 0, 0);
    MockTimestampPacketContainer eventDependencyContainer(*bcsCsr->getTimestampPacketAllocator(), 1);
    auto eventDependency = eventDependencyContainer.getNode(0);
    event->addTimestampPacketNodes(eventDependencyContainer);

    auto userEvent = make_releaseable<UserEvent>(bcsMockContext.get());
    cl_event waitlist[] = {userEvent.get(), event.get()};

    commandQueue->enqueueReadBuffer(bufferForBlt.get(), CL_FALSE, 0, 1, &hostPtr, nullptr, 2, waitlist, nullptr);

    auto outputDependency = mockCmdQ->timestampPacketContainer->peekNodes()[0];
    EXPECT_NE(outputDependency, dependencyFromPreviousEnqueue);

    EXPECT_FALSE(bcsCsr->isMadeResident(dependencyFromPreviousEnqueue->getBaseGraphicsAllocation()));
    EXPECT_FALSE(bcsCsr->isMadeResident(outputDependency->getBaseGraphicsAllocation()));
    EXPECT_FALSE(bcsCsr->isMadeResident(eventDependency->getBaseGraphicsAllocation()));

    userEvent->setStatus(CL_COMPLETE);

    EXPECT_TRUE(bcsCsr->isMadeResident(dependencyFromPreviousEnqueue->getBaseGraphicsAllocation(), bcsCsr->taskCount));
    EXPECT_TRUE(bcsCsr->isMadeResident(outputDependency->getBaseGraphicsAllocation(), bcsCsr->taskCount));
    EXPECT_TRUE(bcsCsr->isMadeResident(eventDependency->getBaseGraphicsAllocation(), bcsCsr->taskCount));
}

HWTEST_TEMPLATED_F(BcsBufferTests, givenMapAllocationWhenEnqueueingReadOrWriteBufferThenStoreMapAllocationInDispatchParameters) {
    DebugManager.flags.DisableZeroCopyForBuffers.set(true);
    auto mockCmdQ = static_cast<MockCommandQueueHw<FamilyType> *>(commandQueue.get());
    uint8_t hostPtr[64] = {};

    auto bufferForBlt = clUniquePtr(Buffer::create(bcsMockContext.get(), CL_MEM_USE_HOST_PTR, 1, hostPtr, retVal));
    bufferForBlt->forceDisallowCPUCopy = true;
    auto mapAllocation = bufferForBlt->getMapAllocation();
    EXPECT_NE(nullptr, mapAllocation);

    mockCmdQ->kernelParams.transferAllocation = nullptr;
    auto mapPtr = clEnqueueMapBuffer(mockCmdQ, bufferForBlt.get(), true, 0, 0, 1, 0, nullptr, nullptr, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(mapAllocation, mockCmdQ->kernelParams.transferAllocation);

    mockCmdQ->kernelParams.transferAllocation = nullptr;
    retVal = clEnqueueUnmapMemObject(mockCmdQ, bufferForBlt.get(), mapPtr, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(mapAllocation, mockCmdQ->kernelParams.transferAllocation);
}

HWTEST_TEMPLATED_F(BcsBufferTests, givenWriteBufferEnqueueWhenProgrammingCommandStreamThenAddSemaphoreWait) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using MI_ATOMIC = typename FamilyType::MI_ATOMIC;

    auto cmdQ = clUniquePtr(new MockCommandQueueHw<FamilyType>(bcsMockContext.get(), device.get(), nullptr));

    auto queueCsr = cmdQ->gpgpuEngine->commandStreamReceiver;
    auto initialTaskCount = queueCsr->peekTaskCount();

    cl_int retVal = CL_SUCCESS;
    auto buffer = clUniquePtr<Buffer>(Buffer::create(bcsMockContext.get(), CL_MEM_READ_WRITE, 1, nullptr, retVal));
    buffer->forceDisallowCPUCopy = true;
    void *hostPtr = reinterpret_cast<void *>(0x12340000);

    cmdQ->enqueueWriteBuffer(buffer.get(), true, 0, 1, hostPtr, nullptr, 0, nullptr, nullptr);
    auto timestampPacketNode = cmdQ->timestampPacketContainer->peekNodes().at(0);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(*cmdQ->peekCommandStream());

    uint32_t semaphoresCount = 0;
    uint32_t miAtomicsCount = 0;
    for (auto &cmd : hwParser.cmdList) {
        if (auto semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(cmd)) {
            semaphoresCount++;
            auto dataAddress = timestampPacketNode->getGpuAddress() + offsetof(TimestampPacketStorage, packets[0].contextEnd);
            EXPECT_EQ(dataAddress, semaphoreCmd->getSemaphoreGraphicsAddress());
            EXPECT_EQ(0u, miAtomicsCount);

        } else if (auto miAtomicCmd = genCmdCast<MI_ATOMIC *>(cmd)) {
            miAtomicsCount++;
            auto dataAddress = timestampPacketNode->getGpuAddress() + offsetof(TimestampPacketStorage, implicitDependenciesCount);
            EXPECT_EQ(MI_ATOMIC::ATOMIC_OPCODES::ATOMIC_4B_DECREMENT, miAtomicCmd->getAtomicOpcode());
            EXPECT_EQ(dataAddress, UnitTestHelper<FamilyType>::getMemoryAddress(*miAtomicCmd));
            EXPECT_EQ(1u, semaphoresCount);
        }
    }
    EXPECT_EQ(1u, semaphoresCount);
    EXPECT_EQ(1u, miAtomicsCount);
    EXPECT_EQ(initialTaskCount + 1, queueCsr->peekTaskCount());
}

HWTEST_TEMPLATED_F(BcsBufferTests, givenReadBufferEnqueueWhenProgrammingCommandStreamThenAddSemaphoreWait) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using MI_ATOMIC = typename FamilyType::MI_ATOMIC;

    auto cmdQ = clUniquePtr(new MockCommandQueueHw<FamilyType>(bcsMockContext.get(), device.get(), nullptr));

    auto queueCsr = cmdQ->gpgpuEngine->commandStreamReceiver;
    auto initialTaskCount = queueCsr->peekTaskCount();

    cl_int retVal = CL_SUCCESS;
    auto buffer = clUniquePtr<Buffer>(Buffer::create(bcsMockContext.get(), CL_MEM_READ_WRITE, 1, nullptr, retVal));
    buffer->forceDisallowCPUCopy = true;
    void *hostPtr = reinterpret_cast<void *>(0x12340000);

    cmdQ->enqueueWriteBuffer(buffer.get(), true, 0, 1, hostPtr, nullptr, 0, nullptr, nullptr);
    auto timestampPacketNode = cmdQ->timestampPacketContainer->peekNodes().at(0);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(*cmdQ->peekCommandStream());

    uint32_t semaphoresCount = 0;
    uint32_t miAtomicsCount = 0;
    for (auto &cmd : hwParser.cmdList) {
        if (auto semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(cmd)) {
            semaphoresCount++;
            auto dataAddress = timestampPacketNode->getGpuAddress() + offsetof(TimestampPacketStorage, packets[0].contextEnd);
            EXPECT_EQ(dataAddress, semaphoreCmd->getSemaphoreGraphicsAddress());
            EXPECT_EQ(0u, miAtomicsCount);

        } else if (auto miAtomicCmd = genCmdCast<MI_ATOMIC *>(cmd)) {
            miAtomicsCount++;
            auto dataAddress = timestampPacketNode->getGpuAddress() + offsetof(TimestampPacketStorage, implicitDependenciesCount);
            EXPECT_EQ(MI_ATOMIC::ATOMIC_OPCODES::ATOMIC_4B_DECREMENT, miAtomicCmd->getAtomicOpcode());
            EXPECT_EQ(dataAddress, UnitTestHelper<FamilyType>::getMemoryAddress(*miAtomicCmd));
            EXPECT_EQ(1u, semaphoresCount);
        }
    }
    EXPECT_EQ(1u, semaphoresCount);
    EXPECT_EQ(1u, miAtomicsCount);
    EXPECT_EQ(initialTaskCount + 1, queueCsr->peekTaskCount());
}

HWTEST_TEMPLATED_F(BcsBufferTests, givenPipeControlRequestWhenDispatchingBlitEnqueueThenWaitPipeControlOnBcsEngine) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto cmdQ = clUniquePtr(new MockCommandQueueHw<FamilyType>(bcsMockContext.get(), device.get(), nullptr));
    auto bcsCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(cmdQ->getBcsCommandStreamReceiver());

    auto queueCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(cmdQ->gpgpuEngine->commandStreamReceiver);
    queueCsr->stallingPipeControlOnNextFlushRequired = true;

    cl_int retVal = CL_SUCCESS;
    auto buffer = clUniquePtr<Buffer>(Buffer::create(bcsMockContext.get(), CL_MEM_READ_WRITE, 1, nullptr, retVal));
    buffer->forceDisallowCPUCopy = true;
    void *hostPtr = reinterpret_cast<void *>(0x12340000);

    cmdQ->enqueueWriteBuffer(buffer.get(), true, 0, 1, hostPtr, nullptr, 0, nullptr, nullptr);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(queueCsr->commandStream);

    uint64_t pipeControlWriteAddress = 0;
    for (auto &cmd : hwParser.cmdList) {
        if (auto pipeControlCmd = genCmdCast<PIPE_CONTROL *>(cmd)) {
            if (pipeControlCmd->getPostSyncOperation() != PIPE_CONTROL::POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA) {
                continue;
            }

            EXPECT_TRUE(pipeControlCmd->getCommandStreamerStallEnable());
            auto addressLow = static_cast<uint64_t>(pipeControlCmd->getAddress());
            auto addressHigh = static_cast<uint64_t>(pipeControlCmd->getAddressHigh());
            pipeControlWriteAddress = (addressHigh << 32) | addressLow;
            break;
        }
    }

    EXPECT_NE(0u, pipeControlWriteAddress);

    HardwareParse bcsHwParser;
    bcsHwParser.parseCommands<FamilyType>(bcsCsr->commandStream);

    auto semaphores = findAll<MI_SEMAPHORE_WAIT *>(bcsHwParser.cmdList.begin(), bcsHwParser.cmdList.end());
    EXPECT_EQ(1u, semaphores.size());
    EXPECT_EQ(pipeControlWriteAddress, genCmdCast<MI_SEMAPHORE_WAIT *>(*(semaphores[0]))->getSemaphoreGraphicsAddress());
}

HWTEST_TEMPLATED_F(BcsBufferTests, givenBarrierWhenReleasingMultipleBlockedEnqueuesThenProgramBarrierOnce) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    auto cmdQ = clUniquePtr(new MockCommandQueueHw<FamilyType>(bcsMockContext.get(), device.get(), nullptr));

    cl_int retVal = CL_SUCCESS;
    auto buffer = clUniquePtr<Buffer>(Buffer::create(bcsMockContext.get(), CL_MEM_READ_WRITE, 1, nullptr, retVal));
    buffer->forceDisallowCPUCopy = true;
    void *hostPtr = reinterpret_cast<void *>(0x12340000);

    UserEvent userEvent0, userEvent1;
    cl_event waitlist0[] = {&userEvent0};
    cl_event waitlist1[] = {&userEvent1};

    cmdQ->enqueueBarrierWithWaitList(0, nullptr, nullptr);
    cmdQ->enqueueWriteBuffer(buffer.get(), false, 0, 1, hostPtr, nullptr, 1, waitlist0, nullptr);
    cmdQ->enqueueWriteBuffer(buffer.get(), false, 0, 1, hostPtr, nullptr, 1, waitlist1, nullptr);

    auto pipeControlLookup = [](LinearStream &stream, size_t offset) {
        HardwareParse hwParser;
        hwParser.parseCommands<FamilyType>(stream, offset);

        bool stallingPipeControlFound = false;
        for (auto &cmd : hwParser.cmdList) {
            if (auto pipeControlCmd = genCmdCast<PIPE_CONTROL *>(cmd)) {
                if (pipeControlCmd->getPostSyncOperation() != PIPE_CONTROL::POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA) {
                    continue;
                }

                stallingPipeControlFound = true;
                EXPECT_TRUE(pipeControlCmd->getCommandStreamerStallEnable());
                break;
            }
        }

        return stallingPipeControlFound;
    };

    auto &csrStream = cmdQ->getGpgpuCommandStreamReceiver().getCS(0);
    EXPECT_TRUE(cmdQ->getGpgpuCommandStreamReceiver().isStallingPipeControlOnNextFlushRequired());
    userEvent0.setStatus(CL_COMPLETE);
    EXPECT_FALSE(cmdQ->getGpgpuCommandStreamReceiver().isStallingPipeControlOnNextFlushRequired());
    EXPECT_TRUE(pipeControlLookup(csrStream, 0));

    auto csrOffset = csrStream.getUsed();
    userEvent1.setStatus(CL_COMPLETE);
    EXPECT_FALSE(pipeControlLookup(csrStream, csrOffset));
    cmdQ->isQueueBlocked();
}

HWTEST_TEMPLATED_F(BcsBufferTests, givenPipeControlRequestWhenDispatchingBlockedBlitEnqueueThenWaitPipeControlOnBcsEngine) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto cmdQ = clUniquePtr(new MockCommandQueueHw<FamilyType>(bcsMockContext.get(), device.get(), nullptr));
    auto bcsCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(cmdQ->getBcsCommandStreamReceiver());

    auto queueCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(cmdQ->gpgpuEngine->commandStreamReceiver);
    queueCsr->stallingPipeControlOnNextFlushRequired = true;

    cl_int retVal = CL_SUCCESS;
    auto buffer = clUniquePtr<Buffer>(Buffer::create(bcsMockContext.get(), CL_MEM_READ_WRITE, 1, nullptr, retVal));
    buffer->forceDisallowCPUCopy = true;
    void *hostPtr = reinterpret_cast<void *>(0x12340000);

    UserEvent userEvent;
    cl_event waitlist = &userEvent;
    cmdQ->enqueueWriteBuffer(buffer.get(), false, 0, 1, hostPtr, nullptr, 1, &waitlist, nullptr);
    userEvent.setStatus(CL_COMPLETE);

    HardwareParse bcsHwParser;
    bcsHwParser.parseCommands<FamilyType>(bcsCsr->commandStream);

    auto semaphores = findAll<MI_SEMAPHORE_WAIT *>(bcsHwParser.cmdList.begin(), bcsHwParser.cmdList.end());
    EXPECT_EQ(1u, semaphores.size());

    cmdQ->isQueueBlocked();
}

HWTEST_TEMPLATED_F(BcsBufferTests, givenBufferOperationWithoutKernelWhenEstimatingCommandsSizeThenReturnCorrectValue) {
    auto cmdQ = clUniquePtr(new MockCommandQueueHw<FamilyType>(bcsMockContext.get(), device.get(), nullptr));
    CsrDependencies csrDependencies;
    MultiDispatchInfo multiDispatchInfo;

    auto readBufferCmdsSize = EnqueueOperation<FamilyType>::getTotalSizeRequiredCS(CL_COMMAND_READ_BUFFER, csrDependencies, false, false,
                                                                                   true, *cmdQ, multiDispatchInfo);
    auto writeBufferCmdsSize = EnqueueOperation<FamilyType>::getTotalSizeRequiredCS(CL_COMMAND_WRITE_BUFFER, csrDependencies, false, false,
                                                                                    true, *cmdQ, multiDispatchInfo);
    auto copyBufferCmdsSize = EnqueueOperation<FamilyType>::getTotalSizeRequiredCS(CL_COMMAND_COPY_BUFFER, csrDependencies, false, false,
                                                                                   true, *cmdQ, multiDispatchInfo);
    auto expectedSize = TimestampPacketHelper::getRequiredCmdStreamSizeForNodeDependencyWithBlitEnqueue<FamilyType>();

    EXPECT_EQ(expectedSize, readBufferCmdsSize);
    EXPECT_EQ(expectedSize, writeBufferCmdsSize);
    EXPECT_EQ(expectedSize, copyBufferCmdsSize);
}

HWTEST_TEMPLATED_F(BcsBufferTests, givenOutputTimestampPacketWhenBlitCalledThenProgramMiFlushDwWithDataWrite) {
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;

    auto csr = static_cast<UltCommandStreamReceiver<FamilyType> *>(commandQueue->getBcsCommandStreamReceiver());
    auto cmdQ = clUniquePtr(new MockCommandQueueHw<FamilyType>(bcsMockContext.get(), device.get(), nullptr));
    cl_int retVal = CL_SUCCESS;

    auto buffer = clUniquePtr<Buffer>(Buffer::create(bcsMockContext.get(), CL_MEM_READ_WRITE, 1, nullptr, retVal));
    buffer->forceDisallowCPUCopy = true;
    void *hostPtr = reinterpret_cast<void *>(0x12340000);

    cmdQ->enqueueWriteBuffer(buffer.get(), true, 0, 1, hostPtr, nullptr, 0, nullptr, nullptr);
    auto outputTimestampPacket = cmdQ->timestampPacketContainer->peekNodes().at(0);
    auto timestampPacketGpuWriteAddress = outputTimestampPacket->getGpuAddress() + offsetof(TimestampPacketStorage, packets[0].contextEnd);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(csr->commandStream);

    uint32_t miFlushDwCmdsCount = 0;
    bool blitCmdFound = false;
    for (auto &cmd : hwParser.cmdList) {
        if (auto miFlushDwCmd = genCmdCast<MI_FLUSH_DW *>(cmd)) {
            EXPECT_TRUE(blitCmdFound);
            EXPECT_EQ(miFlushDwCmdsCount == 0,
                      timestampPacketGpuWriteAddress == miFlushDwCmd->getDestinationAddress());
            EXPECT_EQ(miFlushDwCmdsCount == 0,
                      0u == miFlushDwCmd->getImmediateData());
            miFlushDwCmdsCount++;
        } else if (genCmdCast<typename FamilyType::XY_COPY_BLT *>(cmd)) {
            blitCmdFound = true;
            EXPECT_EQ(0u, miFlushDwCmdsCount);
        }
    }
    EXPECT_EQ(2u, miFlushDwCmdsCount);
    EXPECT_TRUE(blitCmdFound);
}

HWTEST_TEMPLATED_F(BcsBufferTests, givenInputAndOutputTimestampPacketWhenBlitCalledThenMakeThemResident) {
    auto bcsCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(commandQueue->getBcsCommandStreamReceiver());

    auto cmdQ = clUniquePtr(new MockCommandQueueHw<FamilyType>(bcsMockContext.get(), device.get(), nullptr));
    cl_int retVal = CL_SUCCESS;

    auto &cmdQueueCsr = static_cast<UltCommandStreamReceiver<FamilyType> &>(cmdQ->getGpgpuCommandStreamReceiver());
    auto memoryManager = cmdQueueCsr.getMemoryManager();
    cmdQueueCsr.timestampPacketAllocator = std::make_unique<TagAllocator<TimestampPacketStorage>>(device->getRootDeviceIndex(), memoryManager, 1,
                                                                                                  MemoryConstants::cacheLineSize,
                                                                                                  sizeof(TimestampPacketStorage),
                                                                                                  false);

    auto buffer = clUniquePtr<Buffer>(Buffer::create(bcsMockContext.get(), CL_MEM_READ_WRITE, 1, nullptr, retVal));
    buffer->forceDisallowCPUCopy = true;
    void *hostPtr = reinterpret_cast<void *>(0x12340000);

    // first enqueue to create IOQ dependency
    cmdQ->enqueueWriteBuffer(buffer.get(), true, 0, 1, hostPtr, nullptr, 0, nullptr, nullptr);
    auto inputTimestampPacketAllocation = cmdQ->timestampPacketContainer->peekNodes().at(0)->getBaseGraphicsAllocation();

    cmdQ->enqueueWriteBuffer(buffer.get(), true, 0, 1, hostPtr, nullptr, 0, nullptr, nullptr);
    auto outputTimestampPacketAllocation = cmdQ->timestampPacketContainer->peekNodes().at(0)->getBaseGraphicsAllocation();

    EXPECT_NE(outputTimestampPacketAllocation, inputTimestampPacketAllocation);

    EXPECT_EQ(cmdQ->taskCount, inputTimestampPacketAllocation->getTaskCount(bcsCsr->getOsContext().getContextId()));
    EXPECT_EQ(cmdQ->taskCount, outputTimestampPacketAllocation->getTaskCount(bcsCsr->getOsContext().getContextId()));
}

HWTEST_TEMPLATED_F(BcsBufferTests, givenBlockingWriteBufferWhenUsingBcsThenCallWait) {
    auto myMockCsr = new MyMockCsr<FamilyType>(*device->getExecutionEnvironment(), device->getRootDeviceIndex());
    myMockCsr->taskCount = 1234;
    myMockCsr->initializeTagAllocation();
    myMockCsr->setupContext(*bcsMockContext->bcsOsContext);
    bcsMockContext->bcsCsr.reset(myMockCsr);

    EngineControl bcsEngineControl = {myMockCsr, bcsMockContext->bcsOsContext.get()};

    auto cmdQ = clUniquePtr(new MockCommandQueueHw<FamilyType>(bcsMockContext.get(), device.get(), nullptr));
    cmdQ->bcsEngine = &bcsEngineControl;
    auto &gpgpuCsr = cmdQ->getGpgpuCommandStreamReceiver();
    myMockCsr->gpgpuCsr = &gpgpuCsr;

    cl_int retVal = CL_SUCCESS;
    auto buffer = clUniquePtr<Buffer>(Buffer::create(bcsMockContext.get(), CL_MEM_READ_WRITE, 1, nullptr, retVal));
    buffer->forceDisallowCPUCopy = true;
    void *hostPtr = reinterpret_cast<void *>(0x12340000);

    cmdQ->enqueueWriteBuffer(buffer.get(), false, 0, 1, hostPtr, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(0u, myMockCsr->waitForTaskCountAndCleanAllocationListCalled);
    EXPECT_TRUE(gpgpuCsr.getTemporaryAllocations().peekIsEmpty());
    EXPECT_FALSE(myMockCsr->getTemporaryAllocations().peekIsEmpty());

    bool tempAllocationFound = false;
    auto tempAllocation = myMockCsr->getTemporaryAllocations().peekHead();
    while (tempAllocation) {
        if (tempAllocation->getUnderlyingBuffer() == hostPtr) {
            tempAllocationFound = true;
            break;
        }
        tempAllocation = tempAllocation->next;
    }
    EXPECT_TRUE(tempAllocationFound);

    cmdQ->enqueueWriteBuffer(buffer.get(), true, 0, 1, hostPtr, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(1u, myMockCsr->waitForTaskCountAndCleanAllocationListCalled);
}

HWTEST_TEMPLATED_F(BcsBufferTests, givenBlockingReadBufferWhenUsingBcsThenCallWait) {
    auto myMockCsr = new MyMockCsr<FamilyType>(*device->getExecutionEnvironment(), device->getRootDeviceIndex());
    myMockCsr->taskCount = 1234;
    myMockCsr->initializeTagAllocation();
    myMockCsr->setupContext(*bcsMockContext->bcsOsContext);
    bcsMockContext->bcsCsr.reset(myMockCsr);

    EngineControl bcsEngineControl = {myMockCsr, bcsMockContext->bcsOsContext.get()};

    auto cmdQ = clUniquePtr(new MockCommandQueueHw<FamilyType>(bcsMockContext.get(), device.get(), nullptr));
    cmdQ->bcsEngine = &bcsEngineControl;
    auto &gpgpuCsr = cmdQ->getGpgpuCommandStreamReceiver();
    myMockCsr->gpgpuCsr = &gpgpuCsr;

    cl_int retVal = CL_SUCCESS;
    auto buffer = clUniquePtr<Buffer>(Buffer::create(bcsMockContext.get(), CL_MEM_READ_WRITE, 1, nullptr, retVal));
    buffer->forceDisallowCPUCopy = true;
    void *hostPtr = reinterpret_cast<void *>(0x12340000);

    cmdQ->enqueueReadBuffer(buffer.get(), false, 0, 1, hostPtr, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(0u, myMockCsr->waitForTaskCountAndCleanAllocationListCalled);
    EXPECT_TRUE(gpgpuCsr.getTemporaryAllocations().peekIsEmpty());
    EXPECT_FALSE(myMockCsr->getTemporaryAllocations().peekIsEmpty());

    bool tempAllocationFound = false;
    auto tempAllocation = myMockCsr->getTemporaryAllocations().peekHead();
    while (tempAllocation) {
        if (tempAllocation->getUnderlyingBuffer() == hostPtr) {
            tempAllocationFound = true;
            break;
        }
        tempAllocation = tempAllocation->next;
    }
    EXPECT_TRUE(tempAllocationFound);

    cmdQ->enqueueReadBuffer(buffer.get(), true, 0, 1, hostPtr, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(1u, myMockCsr->waitForTaskCountAndCleanAllocationListCalled);
}

HWTEST_TEMPLATED_F(BcsBufferTests, givenBlockedEnqueueWhenUsingBcsThenWaitForValidTaskCountOnBlockingCall) {
    auto myMockCsr = new MyMockCsr<FamilyType>(*device->getExecutionEnvironment(), device->getRootDeviceIndex());
    myMockCsr->taskCount = 1234;
    myMockCsr->initializeTagAllocation();
    myMockCsr->setupContext(*bcsMockContext->bcsOsContext);
    bcsMockContext->bcsCsr.reset(myMockCsr);

    EngineControl bcsEngineControl = {myMockCsr, bcsMockContext->bcsOsContext.get()};

    auto cmdQ = clUniquePtr(new MockCommandQueueHw<FamilyType>(bcsMockContext.get(), device.get(), nullptr));
    cmdQ->bcsEngine = &bcsEngineControl;
    auto &gpgpuCsr = cmdQ->getGpgpuCommandStreamReceiver();
    myMockCsr->gpgpuCsr = &gpgpuCsr;

    cl_int retVal = CL_SUCCESS;
    auto buffer = clUniquePtr<Buffer>(Buffer::create(bcsMockContext.get(), CL_MEM_READ_WRITE, 1, nullptr, retVal));
    buffer->forceDisallowCPUCopy = true;
    void *hostPtr = reinterpret_cast<void *>(0x12340000);

    UserEvent userEvent;
    cl_event waitlist = &userEvent;

    cmdQ->enqueueWriteBuffer(buffer.get(), false, 0, 1, hostPtr, nullptr, 1, &waitlist, nullptr);

    userEvent.setStatus(CL_COMPLETE);
    EXPECT_EQ(0u, myMockCsr->waitForTaskCountAndCleanAllocationListCalled);

    cmdQ->finish();
    EXPECT_EQ(1u, myMockCsr->waitForTaskCountAndCleanAllocationListCalled);
}

TEST_F(RenderCompressedBuffersCopyHostMemoryTests, givenNonRenderCompressedBufferWhenCopyFromHostPtrIsRequiredThenDontCallWriteBuffer) {
    hwInfo->capabilityTable.ftrRenderCompressedBuffers = false;

    buffer.reset(Buffer::create(context.get(), CL_MEM_COPY_HOST_PTR, sizeof(uint32_t), &hostPtr, retVal));
    EXPECT_NE(buffer->getGraphicsAllocation()->getAllocationType(), GraphicsAllocation::AllocationType::BUFFER_COMPRESSED);

    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(0u, mockCmdQ->writeBufferCounter);
}

TEST_F(RenderCompressedBuffersCopyHostMemoryTests, givenRenderCompressedBufferWhenWriteBufferFailsThenReturnErrorCode) {
    if (is32bit || !HwHelper::get(context->getDevice(0)->getHardwareInfo().platform.eRenderCoreFamily).obtainRenderBufferCompressionPreference(context->getDevice(0)->getHardwareInfo(), bufferSize)) {
        return;
    }
    hwInfo->capabilityTable.ftrRenderCompressedBuffers = true;
    mockCmdQ->writeBufferRetValue = CL_INVALID_VALUE;

    buffer.reset(Buffer::create(context.get(), CL_MEM_COPY_HOST_PTR, bufferSize, hostPtr, retVal));
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
               MemoryPropertiesFlags,
               cl_mem_flags,
               cl_mem_flags_intel,
               size_t,
               void *,
               void *,
               GraphicsAllocation *,
               bool,
               bool,
               bool)
            -> NEO::Buffer * { return nullptr; };
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

    EXPECT_FALSE(buffer->getGraphicsAllocation()->isResident(pDevice->getDefaultEngine().osContext->getContextId()));
}

TEST_P(ValidHostPtr, getAddress) {
    buffer = createBuffer();
    ASSERT_NE(nullptr, buffer);

    auto address = buffer->getCpuAddress();
    EXPECT_NE(nullptr, address);
    if (flags & CL_MEM_USE_HOST_PTR && buffer->isMemObjZeroCopy()) {
        // Buffer should use host ptr
        EXPECT_EQ(pHostPtr, address);
        EXPECT_EQ(pHostPtr, buffer->getHostPtr());
    } else {
        // Buffer should have a different ptr
        EXPECT_NE(pHostPtr, address);
    }

    if (flags & CL_MEM_COPY_HOST_PTR) {
        // Buffer should contain a copy of host memory
        EXPECT_EQ(0, memcmp(pHostPtr, address, sizeof(g_scTestBufferSizeInBytes)));
        EXPECT_EQ(nullptr, buffer->getHostPtr());
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

        if (MemoryManagement::nonfailingAllocation == failureIndex) {
            EXPECT_EQ(CL_SUCCESS, retVal);
            EXPECT_NE(nullptr, buffer);
        }
    };
    injectFailures(method);
}

TEST_P(ValidHostPtr, SvmHostPtr) {
    const DeviceInfo &devInfo = pDevice->getDeviceInfo();
    if (devInfo.svmCapabilities != 0) {
        auto ptr = context->getSVMAllocsManager()->createSVMAlloc(pDevice->getRootDeviceIndex(), 64, {});

        auto bufferSvm = Buffer::create(context.get(), CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR, 64, ptr, retVal);
        EXPECT_NE(nullptr, bufferSvm);
        EXPECT_TRUE(bufferSvm->isMemObjWithHostPtrSVM());
        auto svmData = context->getSVMAllocsManager()->getSVMAlloc(ptr);
        ASSERT_NE(nullptr, svmData);
        EXPECT_EQ(svmData->gpuAllocation, bufferSvm->getGraphicsAllocation());
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
    auto graphicsAlloaction = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});
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

        EXPECT_TRUE(is64bit ? buffer->getGraphicsAllocation()->is32BitAllocation() : true);

        using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
        RENDER_SURFACE_STATE surfaceState = {};

        buffer->setArgStateful(&surfaceState, false, false, false, false);

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
    auto subBuffer = buffer->createSubBuffer(CL_MEM_READ_WRITE, 0, &region, retVal);
    ASSERT_NE(nullptr, subBuffer);
    ASSERT_EQ(CL_SUCCESS, retVal);

    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    RENDER_SURFACE_STATE surfaceState = {};

    subBuffer->setArgStateful(&surfaceState, false, false, false, false);

    auto surfBaseAddress = surfaceState.getSurfaceBaseAddress();
    auto bufferAddress = buffer->getGraphicsAllocation()->getGpuAddress();
    EXPECT_EQ(bufferAddress + region.origin, surfBaseAddress);

    subBuffer->release();

    delete buffer;
    alignedFree(ptr);
    DebugManager.flags.Force32bitAddressing.set(false);
}

HWTEST_F(BufferSetSurfaceTests, givenBufferWhenSetArgStatefulWithL3ChacheDisabledIsCalledThenL3CacheShouldBeOffAndSizeIsAlignedTo512) {
    MockContext context;
    auto size = 128;
    auto retVal = CL_SUCCESS;

    auto buffer = std::unique_ptr<Buffer>(Buffer::create(
        &context,
        CL_MEM_READ_WRITE,
        size,
        nullptr,
        retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);

    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    RENDER_SURFACE_STATE surfaceState = {};

    buffer->setArgStateful(&surfaceState, false, true, true, false);

    auto mocs = surfaceState.getMemoryObjectControlState();
    auto gmmHelper = device->getGmmHelper();
    EXPECT_EQ(gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED), mocs);
    EXPECT_EQ(128u, surfaceState.getWidth());
    EXPECT_EQ(4u, surfaceState.getHeight());
}

HWTEST_F(BufferSetSurfaceTests, givenBufferThatIsMisalignedButIsAReadOnlyArgumentWhenSurfaceStateIsSetThenL3IsOn) {
    MockContext context;
    auto size = 128;
    auto retVal = CL_SUCCESS;

    auto buffer = std::unique_ptr<Buffer>(Buffer::create(
        &context,
        CL_MEM_READ_WRITE,
        size,
        nullptr,
        retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);

    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    RENDER_SURFACE_STATE surfaceState = {};

    buffer->getGraphicsAllocation()->setSize(127);

    buffer->setArgStateful(&surfaceState, false, false, false, true);

    auto mocs = surfaceState.getMemoryObjectControlState();
    auto gmmHelper = device->getGmmHelper();
    auto expectedMocs = gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER);
    auto expectedMocs2 = gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CONST);
    EXPECT_TRUE(expectedMocs == mocs || expectedMocs2 == mocs);
}

HWTEST_F(BufferSetSurfaceTests, givenAlignedCacheableReadOnlyBufferThenChoseOclBufferPolicy) {
    MockContext context;
    const auto size = MemoryConstants::pageSize;
    const auto ptr = (void *)alignedMalloc(size * 2, MemoryConstants::pageSize);
    const auto flags = CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY;

    auto retVal = CL_SUCCESS;
    auto buffer = std::unique_ptr<Buffer>(Buffer::create(
        &context,
        flags,
        size,
        ptr,
        retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);

    typename FamilyType::RENDER_SURFACE_STATE surfaceState = {};
    buffer->setArgStateful(&surfaceState, false, false, false, false);

    const auto expectedMocs = device->getGmmHelper()->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER);
    const auto actualMocs = surfaceState.getMemoryObjectControlState();
    EXPECT_EQ(expectedMocs, actualMocs);

    alignedFree(ptr);
}

HWTEST_F(BufferSetSurfaceTests, givenAlignedCacheableNonReadOnlyBufferThenChooseOclBufferPolicy) {
    MockContext context;
    const auto size = MemoryConstants::pageSize;
    const auto ptr = (void *)alignedMalloc(size * 2, MemoryConstants::pageSize);
    const auto flags = CL_MEM_USE_HOST_PTR;

    auto retVal = CL_SUCCESS;
    auto buffer = std::unique_ptr<Buffer>(Buffer::create(
        &context,
        flags,
        size,
        ptr,
        retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);

    typename FamilyType::RENDER_SURFACE_STATE surfaceState = {};
    buffer->setArgStateful(&surfaceState, false, false, false, false);

    const auto expectedMocs = device->getGmmHelper()->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER);
    const auto actualMocs = surfaceState.getMemoryObjectControlState();
    EXPECT_EQ(expectedMocs, actualMocs);

    alignedFree(ptr);
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
    buffer->getGraphicsAllocation()->setDefaultGmm(gmm);
    gmm->isRenderCompressed = true;

    buffer->setArgStateful(&surfaceState, false, false, false, false);

    EXPECT_EQ(0u, surfaceState.getAuxiliarySurfaceBaseAddress());
    EXPECT_TRUE(AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_CCS_E == surfaceState.getAuxiliarySurfaceMode());
    EXPECT_TRUE(RENDER_SURFACE_STATE::COHERENCY_TYPE_GPU_COHERENT == surfaceState.getCoherencyType());

    buffer->getGraphicsAllocation()->setAllocationType(GraphicsAllocation::AllocationType::BUFFER);
    buffer->setArgStateful(&surfaceState, false, false, false, false);
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
    buffer->getGraphicsAllocation()->setDefaultGmm(gmm);
    gmm->isRenderCompressed = false;

    buffer->setArgStateful(&surfaceState, false, false, false, false);

    EXPECT_EQ(0u, surfaceState.getAuxiliarySurfaceBaseAddress());
    EXPECT_TRUE(AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_NONE == surfaceState.getAuxiliarySurfaceMode());
    EXPECT_TRUE(RENDER_SURFACE_STATE::COHERENCY_TYPE_IA_COHERENT == surfaceState.getCoherencyType());
}

HWTEST_F(BufferSetSurfaceTests, givenMisalignedPointerWhenSurfaceStateIsProgrammedThenBaseAddressAndLengthAreAlignedToDword) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    using AUXILIARY_SURFACE_MODE = typename RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE;

    RENDER_SURFACE_STATE surfaceState = {};
    MockContext context;
    uintptr_t ptr = 0xfffff000;
    void *svmPtr = reinterpret_cast<void *>(ptr);

    Buffer::setSurfaceState(device.get(),
                            &surfaceState,
                            5,
                            svmPtr,
                            nullptr,
                            0);

    EXPECT_EQ(castToUint64(svmPtr), surfaceState.getSurfaceBaseAddress());
    SURFACE_STATE_BUFFER_LENGTH length = {};
    length.SurfaceState.Width = surfaceState.getWidth() - 1;
    length.SurfaceState.Height = surfaceState.getHeight() - 1;
    length.SurfaceState.Depth = surfaceState.getDepth() - 1;
    EXPECT_EQ(alignUp(5u, 4u), length.Length + 1);
}

HWTEST_F(BufferSetSurfaceTests, givenBufferThatIsMisalignedWhenSurfaceStateIsBeingProgrammedThenL3CacheIsOff) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;

    RENDER_SURFACE_STATE surfaceState = {};
    MockContext context;
    void *svmPtr = reinterpret_cast<void *>(0x1005);

    Buffer::setSurfaceState(device.get(),
                            &surfaceState,
                            5,
                            svmPtr,
                            nullptr,
                            0);

    EXPECT_EQ(0u, surfaceState.getMemoryObjectControlState());
}

class BufferL3CacheTests : public ::testing::TestWithParam<uint64_t> {
  public:
    void SetUp() override {
        hostPtr = reinterpret_cast<void *>(GetParam());
    }
    MockContext ctx;
    const size_t region[3] = {3, 3, 1};
    const size_t origin[3] = {0, 0, 0};

    void *hostPtr;
};

HWTEST_P(BufferL3CacheTests, givenMisalignedAndAlignedBufferWhenClEnqueueWriteImageThenL3CacheIsOn) {
    if (ctx.getDevice(0)->areSharedSystemAllocationsAllowed()) {
        GTEST_SKIP();
    }
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;

    CommandQueueHw<FamilyType> cmdQ(&ctx, ctx.getDevice(0), nullptr);
    auto surfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(cmdQ.getGpgpuCommandStreamReceiver().getIndirectHeap(IndirectHeap::Type::SURFACE_STATE, 0).getSpace(0));

    cl_image_format imageFormat;
    cl_image_desc imageDesc;
    imageFormat.image_channel_order = CL_RGBA;
    imageFormat.image_channel_data_type = CL_UNORM_INT8;
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    imageDesc.image_width = 3;
    imageDesc.image_height = 3;
    imageDesc.image_depth = 1;
    imageDesc.image_array_size = 1;
    imageDesc.image_row_pitch = 0;
    imageDesc.image_slice_pitch = 0;
    imageDesc.num_mip_levels = 0;
    imageDesc.num_samples = 0;
    imageDesc.mem_object = nullptr;
    auto image = clCreateImage(&ctx, CL_MEM_READ_WRITE, &imageFormat, &imageDesc, nullptr, nullptr);

    clEnqueueWriteImage(&cmdQ, image, false, origin, region, 0, 0, hostPtr, 0, nullptr, nullptr);

    auto expect = ctx.getDevice(0)->getExecutionEnvironment()->getGmmHelper()->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER);
    auto expect2 = ctx.getDevice(0)->getExecutionEnvironment()->getGmmHelper()->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CONST);

    EXPECT_NE(NULL, surfaceState->getMemoryObjectControlState());
    EXPECT_TRUE(expect == surfaceState->getMemoryObjectControlState() || expect2 == surfaceState->getMemoryObjectControlState());

    clReleaseMemObject(image);
}

HWTEST_P(BufferL3CacheTests, givenMisalignedAndAlignedBufferWhenClEnqueueWriteBufferRectThenL3CacheIsOn) {
    if (ctx.getDevice(0)->areSharedSystemAllocationsAllowed()) {
        GTEST_SKIP();
    }
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;

    CommandQueueHw<FamilyType> cmdQ(&ctx, ctx.getDevice(0), nullptr);
    auto surfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(cmdQ.getGpgpuCommandStreamReceiver().getIndirectHeap(IndirectHeap::Type::SURFACE_STATE, 0).getSpace(0));
    auto buffer = clCreateBuffer(&ctx, CL_MEM_READ_WRITE, 36, nullptr, nullptr);

    clEnqueueWriteBufferRect(&cmdQ, buffer, false, origin, origin, region, 0, 0, 0, 0, hostPtr, 0, nullptr, nullptr);

    auto expect = ctx.getDevice(0)->getExecutionEnvironment()->getGmmHelper()->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER);
    auto expect2 = ctx.getDevice(0)->getExecutionEnvironment()->getGmmHelper()->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CONST);

    EXPECT_NE(NULL, surfaceState->getMemoryObjectControlState());
    EXPECT_TRUE(expect == surfaceState->getMemoryObjectControlState() || expect2 == surfaceState->getMemoryObjectControlState());

    clReleaseMemObject(buffer);
}

static uint64_t pointers[] = {
    0x1005,
    0x2000};

INSTANTIATE_TEST_CASE_P(
    pointers,
    BufferL3CacheTests,
    testing::ValuesIn(pointers));

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
