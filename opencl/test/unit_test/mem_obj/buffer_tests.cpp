/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/array_count.h"
#include "shared/source/helpers/compiler_hw_info_config.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/memory_manager/memory_operations_handler.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/test/common/fixtures/memory_management_fixture.h"
#include "shared/test/common/helpers/ult_hw_config.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_gmm.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/extensions/public/cl_ext_private.h"
#include "opencl/source/command_queue/command_queue_hw.h"
#include "opencl/source/mem_obj/mem_obj_helper.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/fixtures/multi_root_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_buffer.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"

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

TEST(Buffer, whenBufferAllocatedInLocalMemoryThenCpuCopyIsDisallowed) {
    MockGraphicsAllocation allocation{};
    MockBuffer buffer(allocation);
    UltDeviceFactory factory{1, 0};
    auto &device = *factory.rootDevices[0];

    allocation.memoryPool = MemoryPool::LocalMemory;
    EXPECT_FALSE(buffer.isReadWriteOnCpuAllowed(device));

    allocation.memoryPool = MemoryPool::System4KBPages;
    EXPECT_TRUE(buffer.isReadWriteOnCpuAllowed(device));
}

TEST(Buffer, givenReadOnlySetOfInputFlagsWhenPassedToisReadOnlyMemoryPermittedByFlagsThenTrueIsReturned) {
    class MockBuffer : public Buffer {
      public:
        using Buffer::isReadOnlyMemoryPermittedByFlags;
    };
    UltDeviceFactory deviceFactory{1, 0};
    auto pDevice = deviceFactory.rootDevices[0];
    cl_mem_flags flags = CL_MEM_HOST_NO_ACCESS | CL_MEM_READ_ONLY;
    MemoryProperties memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, pDevice);
    EXPECT_TRUE(MockBuffer::isReadOnlyMemoryPermittedByFlags(memoryProperties));

    flags = CL_MEM_HOST_READ_ONLY | CL_MEM_READ_ONLY;
    memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, pDevice);
    EXPECT_TRUE(MockBuffer::isReadOnlyMemoryPermittedByFlags(memoryProperties));
}

TEST(TestBufferRectCheck, givenSmallerDstBufferWhenCallBufferRectPitchSetThenCorrectValidationIsDone) {
    auto srcBuffer = std::make_unique<MockBuffer>();
    ASSERT_NE(nullptr, srcBuffer);
    srcBuffer->size = 500;

    size_t originBuffer[] = {0, 0, 0};
    size_t region[] = {10, 20, 1};
    size_t srcRowPitch = 20u;
    size_t srcSlicePitch = 0u;
    size_t dstRowPitch = 10u;
    size_t dstSlicePitch = 0u;

    auto retVal = srcBuffer->bufferRectPitchSet(originBuffer, region, srcRowPitch, srcSlicePitch, dstRowPitch, dstSlicePitch, true);
    EXPECT_TRUE(retVal);

    auto dstBuffer = std::make_unique<MockBuffer>();
    ASSERT_NE(nullptr, dstBuffer);
    dstBuffer->size = 200;

    EXPECT_GT(srcBuffer->size, dstBuffer->size);

    retVal = dstBuffer->bufferRectPitchSet(originBuffer, region, srcRowPitch, srcSlicePitch, dstRowPitch, dstSlicePitch, false);
    EXPECT_TRUE(retVal);
    retVal = dstBuffer->bufferRectPitchSet(originBuffer, region, srcRowPitch, srcSlicePitch, dstRowPitch, dstSlicePitch, true);
    EXPECT_FALSE(retVal);
}

TEST(TestBufferRectCheck, givenInvalidSrcPitchWhenCallBufferRectPitchSetThenReturnFalse) {
    auto buffer = std::make_unique<MockBuffer>();
    ASSERT_NE(nullptr, buffer);
    buffer->size = 200;

    size_t originBuffer[] = {0, 0, 0};
    size_t region[] = {3, 1, 1};
    size_t srcRowPitch = 10u;
    size_t srcSlicePitch = 10u;
    size_t dstRowPitch = 3u;
    size_t dstSlicePitch = 10u;

    auto retVal = buffer->bufferRectPitchSet(originBuffer, region, srcRowPitch, srcSlicePitch, dstRowPitch, dstSlicePitch, true);
    EXPECT_FALSE(retVal);
}

TEST(TestBufferRectCheck, givenInvalidDstPitchWhenCallBufferRectPitchSetThenReturnFalse) {
    auto buffer = std::make_unique<MockBuffer>();
    ASSERT_NE(nullptr, buffer);
    buffer->size = 200;

    size_t originBuffer[] = {0, 0, 0};
    size_t region[] = {3, 1, 1};
    size_t srcRowPitch = 3u;
    size_t srcSlicePitch = 10u;
    size_t dstRowPitch = 10u;
    size_t dstSlicePitch = 10u;

    auto retVal = buffer->bufferRectPitchSet(originBuffer, region, srcRowPitch, srcSlicePitch, dstRowPitch, dstSlicePitch, true);
    EXPECT_FALSE(retVal);
}

TEST(TestBufferRectCheck, givenInvalidDstAndSrcPitchWhenCallBufferRectPitchSetThenReturnFalse) {
    auto buffer = std::make_unique<MockBuffer>();
    ASSERT_NE(nullptr, buffer);
    buffer->size = 200;

    size_t originBuffer[] = {0, 0, 0};
    size_t region[] = {3, 2, 1};
    size_t srcRowPitch = 10u;
    size_t srcSlicePitch = 10u;
    size_t dstRowPitch = 10u;
    size_t dstSlicePitch = 10u;

    auto retVal = buffer->bufferRectPitchSet(originBuffer, region, srcRowPitch, srcSlicePitch, dstRowPitch, dstSlicePitch, true);
    EXPECT_FALSE(retVal);
}

TEST(TestBufferRectCheck, givenCorrectDstAndSrcPitchWhenCallBufferRectPitchSetThenReturnTrue) {
    auto buffer = std::make_unique<MockBuffer>();
    ASSERT_NE(nullptr, buffer);
    buffer->size = 200;

    size_t originBuffer[] = {0, 0, 0};
    size_t region[] = {3, 1, 1};
    size_t srcRowPitch = 10u;
    size_t srcSlicePitch = 10u;
    size_t dstRowPitch = 10u;
    size_t dstSlicePitch = 10u;

    auto retVal = buffer->bufferRectPitchSet(originBuffer, region, srcRowPitch, srcSlicePitch, dstRowPitch, dstSlicePitch, true);
    EXPECT_TRUE(retVal);
}

class BufferReadOnlyTest : public testing::TestWithParam<uint64_t> {
};

TEST_P(BufferReadOnlyTest, givenNonReadOnlySetOfInputFlagsWhenPassedToisReadOnlyMemoryPermittedByFlagsThenFalseIsReturned) {
    class MockBuffer : public Buffer {
      public:
        using Buffer::isReadOnlyMemoryPermittedByFlags;
    };

    UltDeviceFactory deviceFactory{1, 0};
    auto pDevice = deviceFactory.rootDevices[0];
    cl_mem_flags flags = GetParam() | CL_MEM_USE_HOST_PTR;
    MemoryProperties memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, pDevice);
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

    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MockContext ctx(device.get());

    // First fail simulates error for read only memory allocation
    auto memoryManager = std::make_unique<MockMemoryManagerFailFirstAllocation>(*device->getExecutionEnvironment());
    memoryManager->returnNullptr = true;
    memoryManager->returnBaseAllocateGraphicsMemoryInDevicePool = true;
    cl_int retVal;
    cl_mem_flags flags = CL_MEM_HOST_READ_ONLY | CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR;

    ctx.memoryManager = memoryManager.get();
    std::unique_ptr<Buffer> buffer(Buffer::create(&ctx, flags, MemoryConstants::pageSize, (void *)memory, retVal));
    ctx.memoryManager = device->getMemoryManager();

    EXPECT_FALSE(buffer->isMemObjZeroCopy());
    void *memoryStorage = buffer->getCpuAddressForMemoryTransfer();
    EXPECT_NE((void *)memory, memoryStorage);
    EXPECT_EQ(0, memcmp(buffer->getCpuAddressForMemoryTransfer(), memory, MemoryConstants::pageSize));

    alignedFree(memory);
}

TEST(Buffer, givenReadOnlyHostPtrMemoryWhenBufferIsCreatedWithReadOnlyFlagsAndSecondAllocationFailsThenNullptrIsReturned) {
    void *memory = alignedMalloc(MemoryConstants::pageSize, MemoryConstants::pageSize);
    ASSERT_NE(nullptr, memory);

    memset(memory, 0xAA, MemoryConstants::pageSize);

    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MockContext ctx(device.get());

    // First fail simulates error for read only memory allocation
    // Second fail returns nullptr
    auto memoryManager = std::make_unique<MockMemoryManagerFailFirstAllocation>(*device->getExecutionEnvironment());

    cl_int retVal;
    cl_mem_flags flags = CL_MEM_HOST_READ_ONLY | CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR;

    ctx.memoryManager = memoryManager.get();
    std::unique_ptr<Buffer> buffer(Buffer::create(&ctx, flags, MemoryConstants::pageSize, (void *)memory, retVal));
    ctx.memoryManager = device->getMemoryManager();

    EXPECT_EQ(nullptr, buffer.get());
    alignedFree(memory);
}

TEST(Buffer, givenReadOnlyHostPtrMemoryWhenBufferIsCreatedWithKernelWriteFlagThenBufferAllocationFailsAndReturnsNullptr) {
    void *memory = alignedMalloc(MemoryConstants::pageSize, MemoryConstants::pageSize);
    ASSERT_NE(nullptr, memory);

    memset(memory, 0xAA, MemoryConstants::pageSize);

    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MockContext ctx(device.get());

    // First fail simulates error for read only memory allocation
    auto memoryManager = std::make_unique<MockMemoryManagerFailFirstAllocation>(*device->getExecutionEnvironment());

    cl_int retVal;
    cl_mem_flags flags = CL_MEM_HOST_READ_ONLY | CL_MEM_WRITE_ONLY | CL_MEM_USE_HOST_PTR;

    ctx.memoryManager = memoryManager.get();
    std::unique_ptr<Buffer> buffer(Buffer::create(&ctx, flags, MemoryConstants::pageSize, (void *)memory, retVal));
    ctx.memoryManager = device->getMemoryManager();

    EXPECT_EQ(nullptr, buffer.get());
    alignedFree(memory);
}

TEST(Buffer, givenNullPtrWhenBufferIsCreatedWithKernelReadOnlyFlagsThenBufferAllocationFailsAndReturnsNullptr) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MockContext ctx(device.get());

    // First fail simulates error for read only memory allocation
    auto memoryManager = std::make_unique<MockMemoryManagerFailFirstAllocation>(*device->getExecutionEnvironment());

    cl_int retVal;
    cl_mem_flags flags = CL_MEM_HOST_READ_ONLY | CL_MEM_WRITE_ONLY;

    ctx.memoryManager = memoryManager.get();
    std::unique_ptr<Buffer> buffer(Buffer::create(&ctx, flags, MemoryConstants::pageSize, nullptr, retVal));
    ctx.memoryManager = device->getMemoryManager();

    EXPECT_EQ(nullptr, buffer.get());
}

TEST(Buffer, givenNullptrPassedToBufferCreateWhenAllocationIsNotSystemMemoryPoolThenBufferIsNotZeroCopy) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MockContext ctx(device.get());

    auto memoryManager = std::make_unique<MockMemoryManagerFailFirstAllocation>(*device->getExecutionEnvironment());
    memoryManager->returnAllocateNonSystemGraphicsMemoryInDevicePool = true;
    cl_int retVal = 0;
    cl_mem_flags flags = CL_MEM_READ_WRITE;

    ctx.memoryManager = memoryManager.get();
    std::unique_ptr<Buffer> buffer(Buffer::create(&ctx, flags, MemoryConstants::pageSize, nullptr, retVal));
    ctx.memoryManager = device->getMemoryManager();

    ASSERT_NE(nullptr, buffer.get());
    EXPECT_FALSE(buffer->isMemObjZeroCopy());
}

TEST(Buffer, givenNullptrPassedToBufferCreateWhenAllocationIsNotSystemMemoryPoolThenAllocationIsNotAddedToHostPtrManager) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MockContext ctx(device.get());

    auto memoryManager = std::make_unique<MockMemoryManagerFailFirstAllocation>(*device->getExecutionEnvironment());
    memoryManager->returnAllocateNonSystemGraphicsMemoryInDevicePool = true;
    cl_int retVal = 0;
    cl_mem_flags flags = CL_MEM_READ_WRITE;

    auto hostPtrManager = static_cast<MockHostPtrManager *>(memoryManager->getHostPtrManager());
    auto hostPtrAllocationCountBefore = hostPtrManager->getFragmentCount();
    ctx.memoryManager = memoryManager.get();
    std::unique_ptr<Buffer> buffer(Buffer::create(&ctx, flags, MemoryConstants::pageSize, nullptr, retVal));
    ctx.memoryManager = device->getMemoryManager();

    ASSERT_NE(nullptr, buffer.get());
    auto hostPtrAllocationCountAfter = hostPtrManager->getFragmentCount();

    EXPECT_EQ(hostPtrAllocationCountBefore, hostPtrAllocationCountAfter);
}

TEST(Buffer, givenNullptrPassedToBufferCreateWhenNoSharedContextOrCompressedBuffersThenBuffersAllocationTypeIsBufferOrBufferHostMemory) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MockContext ctx(device.get());

    cl_int retVal = 0;
    cl_mem_flags flags = CL_MEM_READ_WRITE;

    std::unique_ptr<Buffer> buffer(Buffer::create(&ctx, flags, MemoryConstants::pageSize, nullptr, retVal));

    ASSERT_NE(nullptr, buffer.get());
    if (MemoryPool::isSystemMemoryPool(buffer->getGraphicsAllocation(device->getRootDeviceIndex())->getMemoryPool())) {
        EXPECT_EQ(AllocationType::BUFFER_HOST_MEMORY, buffer->getGraphicsAllocation(device->getRootDeviceIndex())->getAllocationType());
    } else {
        EXPECT_EQ(AllocationType::BUFFER, buffer->getGraphicsAllocation(device->getRootDeviceIndex())->getAllocationType());
    }
}

TEST(Buffer, givenHostPtrPassedToBufferCreateWhenMemUseHostPtrFlagisSetAndBufferIsNotZeroCopyThenCreateMapAllocationWithHostPtr) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MockContext ctx(device.get());

    cl_int retVal = 0;
    cl_mem_flags flags = CL_MEM_USE_HOST_PTR;
    auto size = MemoryConstants::pageSize;
    void *ptr = (void *)alignedMalloc(size * 2, MemoryConstants::pageSize);
    auto ptrOffset = 1;
    void *offsetedPtr = (void *)((uintptr_t)ptr + ptrOffset);

    std::unique_ptr<Buffer> buffer(Buffer::create(&ctx, flags, MemoryConstants::pageSize, offsetedPtr, retVal));
    ASSERT_NE(nullptr, buffer.get());

    auto mapAllocation = buffer->getMapAllocation(device.get()->getRootDeviceIndex());
    EXPECT_NE(nullptr, mapAllocation);
    EXPECT_EQ(offsetedPtr, mapAllocation->getUnderlyingBuffer());
    EXPECT_EQ(AllocationType::MAP_ALLOCATION, mapAllocation->getAllocationType());

    alignedFree(ptr);
}

TEST(Buffer, givenAlignedHostPtrPassedToBufferCreateWhenNoSharedContextOrCompressedBuffersThenBuffersAllocationTypeIsBufferHostMemory) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MockContext ctx(device.get());

    cl_int retVal = 0;
    cl_mem_flags flags = CL_MEM_USE_HOST_PTR;
    void *hostPtr = reinterpret_cast<void *>(0x3000);

    std::unique_ptr<Buffer> buffer(Buffer::create(&ctx, flags, MemoryConstants::pageSize, hostPtr, retVal));

    ASSERT_NE(nullptr, buffer.get());
    EXPECT_EQ(AllocationType::BUFFER_HOST_MEMORY, buffer->getMultiGraphicsAllocation().getAllocationType());
}

TEST(Buffer, givenAllocHostPtrFlagPassedToBufferCreateWhenNoSharedContextOrCompressedBuffersThenBuffersAllocationTypeIsBufferHostMemory) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MockContext ctx(device.get());

    cl_int retVal = 0;
    cl_mem_flags flags = CL_MEM_ALLOC_HOST_PTR;

    std::unique_ptr<Buffer> buffer(Buffer::create(&ctx, flags, MemoryConstants::pageSize, nullptr, retVal));

    ASSERT_NE(nullptr, buffer.get());
    EXPECT_EQ(AllocationType::BUFFER_HOST_MEMORY, buffer->getMultiGraphicsAllocation().getAllocationType());
}

TEST(Buffer, givenCompressedBuffersEnabledWhenAllocationTypeIsQueriedThenBufferCompressedTypeIsReturnedIn64Bit) {
    MockContext context;
    MemoryProperties memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(0, 0, 0, &context.getDevice(0)->getDevice());
    context.contextType = ContextType::CONTEXT_TYPE_UNRESTRICTIVE;
    context.isSharedContext = false;

    bool compressionEnabled = MemObjHelper::isSuitableForCompression(true, memoryProperties, context, true);
    EXPECT_TRUE(compressionEnabled);

    auto type = MockPublicAccessBuffer::getGraphicsAllocationTypeAndCompressionPreference(memoryProperties, context, compressionEnabled, false);
    EXPECT_TRUE(compressionEnabled);
    EXPECT_EQ(AllocationType::BUFFER, type);
}

TEST(Buffer, givenCompressedBuffersDisabledLocalMemoryEnabledWhenAllocationTypeIsQueriedThenBufferTypeIsReturnedIn64Bit) {
    MockContext context;
    MemoryProperties memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(0, 0, 0, &context.getDevice(0)->getDevice());
    context.contextType = ContextType::CONTEXT_TYPE_UNRESTRICTIVE;
    context.isSharedContext = false;

    bool compressionEnabled = MemObjHelper::isSuitableForCompression(false, memoryProperties, context, true);
    EXPECT_FALSE(compressionEnabled);

    auto type = MockPublicAccessBuffer::getGraphicsAllocationTypeAndCompressionPreference(memoryProperties, context, compressionEnabled, true);
    EXPECT_FALSE(compressionEnabled);
    EXPECT_EQ(AllocationType::BUFFER, type);
}

TEST(Buffer, givenSharedContextWhenAllocationTypeIsQueriedThenBufferHostMemoryTypeIsReturned) {
    MockContext context;
    MemoryProperties memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(0, 0, 0, &context.getDevice(0)->getDevice());
    context.contextType = ContextType::CONTEXT_TYPE_UNRESTRICTIVE;
    context.isSharedContext = true;

    bool compressionEnabled = MemObjHelper::isSuitableForCompression(false, memoryProperties, context, true);
    EXPECT_FALSE(compressionEnabled);

    auto type = MockPublicAccessBuffer::getGraphicsAllocationTypeAndCompressionPreference(memoryProperties, context, compressionEnabled, false);
    EXPECT_FALSE(compressionEnabled);
    EXPECT_EQ(AllocationType::BUFFER_HOST_MEMORY, type);
}

TEST(Buffer, givenSharedContextAndCompressedBuffersEnabledWhenAllocationTypeIsQueriedThenBufferHostMemoryTypeIsReturned) {
    MockContext context;
    MemoryProperties memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(0, 0, 0, &context.getDevice(0)->getDevice());
    context.contextType = ContextType::CONTEXT_TYPE_UNRESTRICTIVE;
    context.isSharedContext = true;

    bool compressionEnabled = MemObjHelper::isSuitableForCompression(true, memoryProperties, context, true);
    EXPECT_TRUE(compressionEnabled);

    auto type = MockPublicAccessBuffer::getGraphicsAllocationTypeAndCompressionPreference(memoryProperties, context, compressionEnabled, false);
    EXPECT_FALSE(compressionEnabled);
    EXPECT_EQ(AllocationType::BUFFER_HOST_MEMORY, type);
}

TEST(Buffer, givenUseHostPtrFlagAndLocalMemoryDisabledWhenAllocationTypeIsQueriedThenBufferHostMemoryTypeIsReturned) {
    cl_mem_flags flags = CL_MEM_USE_HOST_PTR;
    MockContext context;
    MemoryProperties memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context.getDevice(0)->getDevice());
    context.contextType = ContextType::CONTEXT_TYPE_UNRESTRICTIVE;
    context.isSharedContext = false;

    bool compressionEnabled = MemObjHelper::isSuitableForCompression(false, memoryProperties, context, true);
    EXPECT_FALSE(compressionEnabled);

    auto type = MockPublicAccessBuffer::getGraphicsAllocationTypeAndCompressionPreference(memoryProperties, context, compressionEnabled, false);
    EXPECT_FALSE(compressionEnabled);
    EXPECT_EQ(AllocationType::BUFFER_HOST_MEMORY, type);
}

TEST(Buffer, givenUseHostPtrFlagAndLocalMemoryEnabledWhenAllocationTypeIsQueriedThenBufferTypeIsReturned) {
    cl_mem_flags flags = CL_MEM_USE_HOST_PTR;
    MockContext context;
    MemoryProperties memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context.getDevice(0)->getDevice());
    context.contextType = ContextType::CONTEXT_TYPE_UNRESTRICTIVE;
    context.isSharedContext = false;

    bool compressionEnabled = MemObjHelper::isSuitableForCompression(false, memoryProperties, context, true);
    EXPECT_FALSE(compressionEnabled);

    auto type = MockPublicAccessBuffer::getGraphicsAllocationTypeAndCompressionPreference(memoryProperties, context, compressionEnabled, true);
    EXPECT_FALSE(compressionEnabled);
    EXPECT_EQ(AllocationType::BUFFER, type);
}

TEST(Buffer, givenAllocHostPtrFlagWhenAllocationTypeIsQueriedThenBufferTypeIsReturned) {
    cl_mem_flags flags = CL_MEM_ALLOC_HOST_PTR;
    MockContext context;
    MemoryProperties memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context.getDevice(0)->getDevice());
    context.contextType = ContextType::CONTEXT_TYPE_UNRESTRICTIVE;
    context.isSharedContext = false;

    bool compressionEnabled = MemObjHelper::isSuitableForCompression(false, memoryProperties, context, true);
    EXPECT_FALSE(compressionEnabled);

    auto type = MockPublicAccessBuffer::getGraphicsAllocationTypeAndCompressionPreference(memoryProperties, context, compressionEnabled, false);
    EXPECT_FALSE(compressionEnabled);
    EXPECT_EQ(AllocationType::BUFFER, type);
}

TEST(Buffer, givenUseHostPtrFlagAndLocalMemoryDisabledAndCompressedBuffersEnabledWhenAllocationTypeIsQueriedThenBufferMemoryTypeIsReturned) {
    cl_mem_flags flags = CL_MEM_USE_HOST_PTR;
    MockContext context;
    MemoryProperties memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context.getDevice(0)->getDevice());
    context.contextType = ContextType::CONTEXT_TYPE_UNRESTRICTIVE;
    context.isSharedContext = false;

    bool compressionEnabled = MemObjHelper::isSuitableForCompression(true, memoryProperties, context, true);
    EXPECT_TRUE(compressionEnabled);

    auto type = MockPublicAccessBuffer::getGraphicsAllocationTypeAndCompressionPreference(memoryProperties, context, compressionEnabled, false);
    EXPECT_FALSE(compressionEnabled);
    EXPECT_EQ(AllocationType::BUFFER_HOST_MEMORY, type);
}

TEST(Buffer, givenUseHostPtrFlagAndLocalMemoryEnabledAndCompressedBuffersEnabledWhenAllocationTypeIsQueriedThenBufferMemoryTypeIsReturned) {
    cl_mem_flags flags = CL_MEM_USE_HOST_PTR;
    MockContext context;
    MemoryProperties memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context.getDevice(0)->getDevice());
    context.contextType = ContextType::CONTEXT_TYPE_UNRESTRICTIVE;
    context.isSharedContext = false;

    bool compressionEnabled = MemObjHelper::isSuitableForCompression(true, memoryProperties, context, true);
    EXPECT_TRUE(compressionEnabled);

    auto type = MockPublicAccessBuffer::getGraphicsAllocationTypeAndCompressionPreference(memoryProperties, context, compressionEnabled, true);
    EXPECT_TRUE(compressionEnabled);
    EXPECT_EQ(AllocationType::BUFFER, type);
}

TEST(Buffer, givenUseHostPointerFlagAndForceSharedPhysicalStorageWhenLocalMemoryIsEnabledThenBufferHostMemoryTypeIsReturned) {
    cl_mem_flags flags = CL_MEM_USE_HOST_PTR | CL_MEM_FORCE_HOST_MEMORY_INTEL;
    MockContext context;
    MemoryProperties memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context.getDevice(0)->getDevice());
    context.contextType = ContextType::CONTEXT_TYPE_UNRESTRICTIVE;
    context.isSharedContext = false;

    bool compressionEnabled = MemObjHelper::isSuitableForCompression(true, memoryProperties, context, true);
    EXPECT_TRUE(compressionEnabled);

    auto type = MockPublicAccessBuffer::getGraphicsAllocationTypeAndCompressionPreference(memoryProperties, context, compressionEnabled, true);
    EXPECT_FALSE(compressionEnabled);
    EXPECT_EQ(AllocationType::BUFFER_HOST_MEMORY, type);
}

TEST(Buffer, givenAllocHostPtrFlagAndCompressedBuffersEnabledWhenAllocationTypeIsQueriedThenBufferCompressedTypeIsReturned) {
    cl_mem_flags flags = CL_MEM_ALLOC_HOST_PTR;
    MockContext context;
    MemoryProperties memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context.getDevice(0)->getDevice());
    context.contextType = ContextType::CONTEXT_TYPE_UNRESTRICTIVE;
    context.isSharedContext = false;

    bool compressionEnabled = MemObjHelper::isSuitableForCompression(true, memoryProperties, context, true);
    EXPECT_TRUE(compressionEnabled);

    auto type = MockPublicAccessBuffer::getGraphicsAllocationTypeAndCompressionPreference(memoryProperties, context, compressionEnabled, false);
    EXPECT_TRUE(compressionEnabled);
    EXPECT_EQ(AllocationType::BUFFER, type);
}

TEST(Buffer, givenZeroFlagsNoSharedContextAndCompressedBuffersDisabledWhenAllocationTypeIsQueriedThenBufferTypeIsReturned) {
    MockContext context;
    MemoryProperties memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(0, 0, 0, &context.getDevice(0)->getDevice());
    context.contextType = ContextType::CONTEXT_TYPE_UNRESTRICTIVE;
    context.isSharedContext = false;

    bool compressionEnabled = MemObjHelper::isSuitableForCompression(false, memoryProperties, context, true);
    EXPECT_FALSE(compressionEnabled);

    auto type = MockPublicAccessBuffer::getGraphicsAllocationTypeAndCompressionPreference(memoryProperties, context, compressionEnabled, false);
    EXPECT_FALSE(compressionEnabled);
    EXPECT_EQ(AllocationType::BUFFER, type);
}

TEST(Buffer, givenClMemCopyHostPointerPassedToBufferCreateWhenAllocationIsNotInSystemMemoryPoolThenAllocationIsWrittenByEnqueueWriteBuffer) {
    ExecutionEnvironment *executionEnvironment = MockClDevice::prepareExecutionEnvironment(defaultHwInfo.get(), 0u);

    auto *memoryManager = new MockMemoryManagerFailFirstAllocation(*executionEnvironment);
    executionEnvironment->memoryManager.reset(memoryManager);
    memoryManager->returnBaseAllocateGraphicsMemoryInDevicePool = true;
    auto device = std::make_unique<MockClDevice>(MockDevice::create<MockDevice>(executionEnvironment, 0));

    MockContext ctx(device.get());

    cl_int retVal = 0;
    cl_mem_flags flags = CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR;
    char memory[] = {1, 2, 3, 4, 5, 6, 7, 8};
    auto taskCount = device->getGpgpuCommandStreamReceiver().peekLatestFlushedTaskCount();

    memoryManager->returnAllocateNonSystemGraphicsMemoryInDevicePool = true;
    std::unique_ptr<Buffer> buffer(Buffer::create(&ctx, flags, sizeof(memory), memory, retVal));
    ASSERT_NE(nullptr, buffer.get());
    auto taskCountSent = device->getGpgpuCommandStreamReceiver().peekLatestFlushedTaskCount();
    if constexpr (is64bit) {
        EXPECT_LT(taskCount, taskCountSent);
    }
}
struct CompressedBuffersTests : public ::testing::Test {
    void SetUp() override {
        ExecutionEnvironment *executionEnvironment = MockDevice::prepareExecutionEnvironment(defaultHwInfo.get(), 0u);
        for (auto &rootDeviceEnvironment : executionEnvironment->rootDeviceEnvironments) {
            rootDeviceEnvironment->initGmm();
        }
        SetUp(executionEnvironment);
    }
    void SetUp(ExecutionEnvironment *executionEnvironment) {
        hwInfo = executionEnvironment->rootDeviceEnvironments[0]->getMutableHardwareInfo();
        device = std::make_unique<MockClDevice>(MockDevice::create<MockDevice>(executionEnvironment, 0u));
        context = std::make_unique<MockContext>(device.get(), true);
        context->contextType = ContextType::CONTEXT_TYPE_UNRESTRICTIVE;
    }

    cl_int retVal = CL_SUCCESS;
    HardwareInfo *hwInfo = nullptr;
    std::unique_ptr<MockClDevice> device;
    std::unique_ptr<MockContext> context;
    std::unique_ptr<Buffer> buffer;

    uint32_t hostPtr[2048];
    size_t bufferSize = sizeof(hostPtr);
};

TEST_F(CompressedBuffersTests, givenBufferCompressedAllocationAndZeroCopyHostPtrWhenCheckingMemoryPropertiesThenUseHostPtrAndDontAllocateStorage) {
    hwInfo->capabilityTable.ftrRenderCompressedBuffers = false;

    void *cacheAlignedHostPtr = alignedMalloc(MemoryConstants::cacheLineSize, MemoryConstants::cacheLineSize);

    buffer.reset(Buffer::create(context.get(), CL_MEM_FORCE_HOST_MEMORY_INTEL | CL_MEM_USE_HOST_PTR, MemoryConstants::cacheLineSize, cacheAlignedHostPtr, retVal));
    auto allocation = buffer->getGraphicsAllocation(device->getRootDeviceIndex());
    EXPECT_EQ(cacheAlignedHostPtr, allocation->getUnderlyingBuffer());
    EXPECT_TRUE(buffer->isMemObjZeroCopy());
    EXPECT_EQ(allocation->getAllocationType(), AllocationType::BUFFER_HOST_MEMORY);

    uint32_t pattern[2] = {0, 0};
    pattern[0] = 0xdeadbeef;
    pattern[1] = 0xdeadbeef;

    static_assert(sizeof(pattern) <= MemoryConstants::cacheLineSize, "Incorrect pattern size");

    uint32_t *dest = reinterpret_cast<uint32_t *>(cacheAlignedHostPtr);

    for (size_t i = 0; i < arrayCount(pattern); i++) {
        dest[i] = pattern[i];
    }

    hwInfo->capabilityTable.ftrRenderCompressedBuffers = true;
    buffer.reset(Buffer::create(context.get(), CL_MEM_FORCE_HOST_MEMORY_INTEL | CL_MEM_USE_HOST_PTR, MemoryConstants::cacheLineSize, cacheAlignedHostPtr, retVal));
    allocation = buffer->getGraphicsAllocation(device->getRootDeviceIndex());
    EXPECT_EQ(cacheAlignedHostPtr, allocation->getUnderlyingBuffer());
    EXPECT_TRUE(buffer->isMemObjZeroCopy());
    EXPECT_EQ(allocation->getAllocationType(), AllocationType::BUFFER_HOST_MEMORY);

    EXPECT_EQ(0, memcmp(allocation->getUnderlyingBuffer(), &pattern[0], sizeof(pattern)));

    alignedFree(cacheAlignedHostPtr);
}

TEST_F(CompressedBuffersTests, givenAllocationCreatedWithForceSharedPhysicalMemoryWhenItIsCreatedThenItIsZeroCopy) {
    buffer.reset(Buffer::create(context.get(), CL_MEM_FORCE_HOST_MEMORY_INTEL, 1u, nullptr, retVal));
    EXPECT_EQ(buffer->getGraphicsAllocation(device->getRootDeviceIndex())->getAllocationType(), AllocationType::BUFFER_HOST_MEMORY);
    EXPECT_TRUE(buffer->isMemObjZeroCopy());
    EXPECT_EQ(1u, buffer->getSize());
}

TEST_F(CompressedBuffersTests, givenCompressedBuffersAndAllocationCreatedWithForceSharedPhysicalMemoryWhenItIsCreatedThenItIsZeroCopy) {
    hwInfo->capabilityTable.ftrRenderCompressedBuffers = true;
    buffer.reset(Buffer::create(context.get(), CL_MEM_FORCE_HOST_MEMORY_INTEL, 1u, nullptr, retVal));
    EXPECT_EQ(buffer->getGraphicsAllocation(device->getRootDeviceIndex())->getAllocationType(), AllocationType::BUFFER_HOST_MEMORY);
    EXPECT_TRUE(buffer->isMemObjZeroCopy());
    EXPECT_EQ(1u, buffer->getSize());
}

TEST_F(CompressedBuffersTests, givenBufferNotCompressedAllocationAndNoHostPtrWhenCheckingMemoryPropertiesThenForceDisableZeroCopy) {
    hwInfo->capabilityTable.ftrRenderCompressedBuffers = false;

    buffer.reset(Buffer::create(context.get(), 0, bufferSize, nullptr, retVal));
    auto allocation = buffer->getGraphicsAllocation(device->getRootDeviceIndex());
    EXPECT_TRUE(buffer->isMemObjZeroCopy());
    if (MemoryPool::isSystemMemoryPool(allocation->getMemoryPool())) {
        EXPECT_EQ(allocation->getAllocationType(), AllocationType::BUFFER_HOST_MEMORY);
    } else {
        EXPECT_EQ(allocation->getAllocationType(), AllocationType::BUFFER);
    }

    auto memoryManager = static_cast<MockMemoryManager *>(device->getExecutionEnvironment()->memoryManager.get());

    hwInfo->capabilityTable.ftrRenderCompressedBuffers = true;
    buffer.reset(Buffer::create(context.get(), 0, bufferSize, nullptr, retVal));
    allocation = buffer->getGraphicsAllocation(device->getRootDeviceIndex());
    if (HwHelper::get(context->getDevice(0)->getHardwareInfo().platform.eRenderCoreFamily).isBufferSizeSuitableForCompression(bufferSize, *hwInfo)) {
        EXPECT_FALSE(buffer->isMemObjZeroCopy());
        EXPECT_EQ(allocation->getAllocationType(), AllocationType::BUFFER);
        EXPECT_EQ(!memoryManager->allocate32BitGraphicsMemoryImplCalled, allocation->isCompressionEnabled());
    } else {
        EXPECT_TRUE(buffer->isMemObjZeroCopy());
        EXPECT_EQ(allocation->getAllocationType(), AllocationType::BUFFER_HOST_MEMORY);
    }
}

TEST_F(CompressedBuffersTests, givenBufferCompressedAllocationWhenSharedContextIsUsedThenForceDisableCompression) {
    hwInfo->capabilityTable.ftrRenderCompressedBuffers = true;
    context->isSharedContext = false;

    auto memoryManager = static_cast<MockMemoryManager *>(device->getExecutionEnvironment()->memoryManager.get());

    buffer.reset(Buffer::create(context.get(), CL_MEM_READ_WRITE, bufferSize, nullptr, retVal));
    auto graphicsAllocation = buffer->getGraphicsAllocation(context->getDevice(0)->getRootDeviceIndex());
    if (HwHelper::get(context->getDevice(0)->getHardwareInfo().platform.eRenderCoreFamily).isBufferSizeSuitableForCompression(bufferSize, *hwInfo)) {
        EXPECT_EQ(graphicsAllocation->getAllocationType(), AllocationType::BUFFER);
        EXPECT_EQ(!memoryManager->allocate32BitGraphicsMemoryImplCalled, graphicsAllocation->isCompressionEnabled());
    } else {
        EXPECT_EQ(graphicsAllocation->getAllocationType(), AllocationType::BUFFER_HOST_MEMORY);
    }
    context->isSharedContext = true;
    buffer.reset(Buffer::create(context.get(), CL_MEM_USE_HOST_PTR, bufferSize, hostPtr, retVal));
    graphicsAllocation = buffer->getGraphicsAllocation(context->getDevice(0)->getRootDeviceIndex());
    EXPECT_EQ(graphicsAllocation->getAllocationType(), AllocationType::BUFFER_HOST_MEMORY);
}

TEST_F(CompressedBuffersTests, givenDebugVariableSetWhenHwFlagIsNotSetThenSelectOptionFromDebugFlag) {
    DebugManagerStateRestore restore;

    auto memoryManager = static_cast<MockMemoryManager *>(device->getExecutionEnvironment()->memoryManager.get());

    hwInfo->capabilityTable.ftrRenderCompressedBuffers = false;

    DebugManager.flags.RenderCompressedBuffersEnabled.set(1);
    buffer.reset(Buffer::create(context.get(), 0, bufferSize, nullptr, retVal));
    auto graphicsAllocation = buffer->getGraphicsAllocation(context->getDevice(0)->getRootDeviceIndex());
    if (HwHelper::get(context->getDevice(0)->getHardwareInfo().platform.eRenderCoreFamily).isBufferSizeSuitableForCompression(bufferSize, *hwInfo)) {
        EXPECT_EQ(graphicsAllocation->getAllocationType(), AllocationType::BUFFER);
        EXPECT_EQ(!memoryManager->allocate32BitGraphicsMemoryImplCalled, graphicsAllocation->isCompressionEnabled());
    } else {
        EXPECT_EQ(graphicsAllocation->getAllocationType(), AllocationType::BUFFER_HOST_MEMORY);
    }

    DebugManager.flags.RenderCompressedBuffersEnabled.set(0);
    buffer.reset(Buffer::create(context.get(), 0, bufferSize, nullptr, retVal));
    graphicsAllocation = buffer->getGraphicsAllocation(context->getDevice(0)->getRootDeviceIndex());
    EXPECT_FALSE(graphicsAllocation->isCompressionEnabled());
}

struct CompressedBuffersSvmTests : public CompressedBuffersTests {
    void SetUp() override {
        ExecutionEnvironment *executionEnvironment = MockDevice::prepareExecutionEnvironment(defaultHwInfo.get(), 0u);
        for (auto &rootDeviceEnvironment : executionEnvironment->rootDeviceEnvironments) {
            rootDeviceEnvironment->initGmm();
        }
        executionEnvironment->prepareRootDeviceEnvironments(1u);
        hwInfo = executionEnvironment->rootDeviceEnvironments[0]->getMutableHardwareInfo();
        hwInfo->capabilityTable.gpuAddressSpace = MemoryConstants::max48BitAddress;
        CompressedBuffersTests::SetUp(executionEnvironment);
    }
};

TEST_F(CompressedBuffersSvmTests, givenSvmAllocationWhenCreatingBufferThenForceDisableCompression) {
    hwInfo->capabilityTable.ftrRenderCompressedBuffers = true;

    auto svmPtr = context->getSVMAllocsManager()->createSVMAlloc(sizeof(uint32_t), {}, context->getRootDeviceIndices(), context->getDeviceBitfields());
    auto expectedAllocationType = context->getSVMAllocsManager()->getSVMAlloc(svmPtr)->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex())->getAllocationType();
    buffer.reset(Buffer::create(context.get(), CL_MEM_USE_HOST_PTR, sizeof(uint32_t), svmPtr, retVal));
    EXPECT_EQ(expectedAllocationType, buffer->getGraphicsAllocation(device->getRootDeviceIndex())->getAllocationType());

    buffer.reset(nullptr);
    context->getSVMAllocsManager()->freeSVMAlloc(svmPtr);
}

struct CompressedBuffersCopyHostMemoryTests : public CompressedBuffersTests {
    void SetUp() override {
        CompressedBuffersTests::SetUp();
        device->injectMemoryManager(new MockMemoryManager(true, false, *device->getExecutionEnvironment()));
        context->memoryManager = device->getMemoryManager();
        mockCmdQ = new MockCommandQueue();
        context->setSpecialQueue(mockCmdQ, device->getRootDeviceIndex());
    }

    MockCommandQueue *mockCmdQ = nullptr;
};

TEST_F(CompressedBuffersCopyHostMemoryTests, givenCompressedBufferWhenCopyFromHostPtrIsRequiredThenCallWriteBuffer) {
    if (is32bit) {
        return;
    }
    hwInfo->capabilityTable.ftrRenderCompressedBuffers = true;

    buffer.reset(Buffer::create(context.get(), CL_MEM_COPY_HOST_PTR, bufferSize, hostPtr, retVal));
    auto graphicsAllocation = buffer->getGraphicsAllocation(context->getDevice(0)->getRootDeviceIndex());
    if (HwHelper::get(context->getDevice(0)->getHardwareInfo().platform.eRenderCoreFamily).isBufferSizeSuitableForCompression(bufferSize, *hwInfo)) {
        EXPECT_TRUE(graphicsAllocation->isCompressionEnabled());
        EXPECT_EQ(1u, mockCmdQ->writeBufferCounter);
        EXPECT_TRUE(mockCmdQ->writeBufferBlocking);
        EXPECT_EQ(0u, mockCmdQ->writeBufferOffset);
        EXPECT_EQ(bufferSize, mockCmdQ->writeBufferSize);
        EXPECT_EQ(hostPtr, mockCmdQ->writeBufferPtr);
    } else {
        EXPECT_EQ(graphicsAllocation->getAllocationType(), AllocationType::BUFFER_HOST_MEMORY);
        EXPECT_EQ(0u, mockCmdQ->writeBufferCounter);
        EXPECT_FALSE(mockCmdQ->writeBufferBlocking);
        EXPECT_EQ(0u, mockCmdQ->writeBufferOffset);
        EXPECT_EQ(0u, mockCmdQ->writeBufferSize);
        EXPECT_EQ(nullptr, mockCmdQ->writeBufferPtr);
    }
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(CompressedBuffersCopyHostMemoryTests, givenBufferCreateWhenMemoryTransferWithEnqueueWriteBufferThenMapAllocationIsReused) {
    cl_mem_flags flags = CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR;
    auto &capabilityTable = device->getRootDeviceEnvironment().getMutableHardwareInfo()->capabilityTable;
    capabilityTable.blitterOperationsSupported = false;
    static_cast<MockMemoryManager *>(context->memoryManager)->forceCompressed = true;
    std::unique_ptr<Buffer> buffer(Buffer::create(context.get(), flags, bufferSize, hostPtr, retVal));
    EXPECT_NE(nullptr, mockCmdQ->writeMapAllocation);
    EXPECT_EQ(buffer->getMapAllocation(device.get()->getRootDeviceIndex()), mockCmdQ->writeMapAllocation);
}

TEST_F(CompressedBuffersCopyHostMemoryTests, givenNonCompressedBufferWhenCopyFromHostPtrIsRequiredThenDontCallWriteBuffer) {
    hwInfo->capabilityTable.ftrRenderCompressedBuffers = false;

    buffer.reset(Buffer::create(context.get(), CL_MEM_COPY_HOST_PTR, sizeof(uint32_t), &hostPtr, retVal));
    EXPECT_FALSE(buffer->getGraphicsAllocation(0)->isCompressionEnabled());

    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(0u, mockCmdQ->writeBufferCounter);
}

TEST_F(CompressedBuffersCopyHostMemoryTests, givenCompressedBufferWhenWriteBufferFailsThenReturnErrorCode) {
    if (is32bit || !HwHelper::get(context->getDevice(0)->getHardwareInfo().platform.eRenderCoreFamily).isBufferSizeSuitableForCompression(bufferSize, *hwInfo)) {
        return;
    }
    hwInfo->capabilityTable.ftrRenderCompressedBuffers = true;
    mockCmdQ->writeBufferRetValue = CL_INVALID_VALUE;

    buffer.reset(Buffer::create(context.get(), CL_MEM_COPY_HOST_PTR, bufferSize, hostPtr, retVal));
    EXPECT_EQ(CL_OUT_OF_RESOURCES, retVal);
    EXPECT_EQ(nullptr, buffer.get());
}

class BufferTest : public ClDeviceFixture,
                   public testing::TestWithParam<uint64_t /*cl_mem_flags*/> {
  public:
    BufferTest() {
    }

  protected:
    void SetUp() override {
        flags = GetParam();
        ClDeviceFixture::SetUp();
        context.reset(new MockContext(pClDevice));
    }

    void TearDown() override {
        context.reset();
        ClDeviceFixture::TearDown();
    }

    cl_int retVal = CL_SUCCESS;
    std::unique_ptr<MockContext> context;
    MemoryManager *contextMemoryManager;
    cl_mem_flags flags = 0;
    unsigned char pHostPtr[g_scTestBufferSizeInBytes];
};

typedef BufferTest NoHostPtr;

TEST_P(NoHostPtr, GivenValidFlagsWhenCreatingBufferThenBufferIsCreated) {
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
    BufferFactoryFuncs BufferFuncsBackup[IGFX_MAX_CORE];

    for (uint32_t i = 0; i < IGFX_MAX_CORE; i++) {
        BufferFuncsBackup[i] = bufferFactory[i];
        bufferFactory[i].createBufferFunction =
            [](Context *,
               MemoryProperties,
               cl_mem_flags,
               cl_mem_flags_intel,
               size_t,
               void *,
               void *,
               MultiGraphicsAllocation,
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

TEST_P(NoHostPtr, GivenNoHostPtrWhenCreatingBufferWithMemUseHostPtrThenInvalidHostPtrErrorIsReturned) {
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

TEST_P(NoHostPtr, GivenNoHostPtrWhenCreatingBufferWithMemCopyHostPtrThenInvalidHostPtrErrorIsReturned) {
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

TEST_P(NoHostPtr, WhenGettingAllocationTypeThenCorrectBufferTypeIsReturned) {
    auto buffer = Buffer::create(
        context.get(),
        flags,
        g_scTestBufferSizeInBytes,
        nullptr,
        retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, buffer);

    auto allocation = buffer->getGraphicsAllocation(pClDevice->getRootDeviceIndex());
    if (MemoryPool::isSystemMemoryPool(allocation->getMemoryPool())) {
        EXPECT_EQ(allocation->getAllocationType(), AllocationType::BUFFER_HOST_MEMORY);
    } else {
        EXPECT_EQ(allocation->getAllocationType(), AllocationType::BUFFER);
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

TEST_P(ValidHostPtr, WhenBufferIsCreatedThenItIsNotResident) {
    buffer = createBuffer();
    ASSERT_NE(nullptr, buffer);

    EXPECT_FALSE(buffer->getGraphicsAllocation(pDevice->getRootDeviceIndex())->isResident(pDevice->getDefaultEngine().osContext->getContextId()));
}

TEST_P(ValidHostPtr, WhenBufferIsCreatedThenAddressMatechesOnlyForHostPtr) {
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

TEST_P(ValidHostPtr, WhenGettingBufferSizeThenSizeIsCorrect) {
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

TEST_P(ValidHostPtr, GivenFailedAllocationWhenCreatingBufferThenBufferIsNotCreated) {
    InjectedFunction method = [this](size_t failureIndex) {
        delete buffer;
        buffer = nullptr;

        // System under test
        buffer = createBuffer();

        if (MemoryManagement::nonfailingAllocation == failureIndex) {
            EXPECT_EQ(CL_SUCCESS, retVal);
            EXPECT_NE(nullptr, buffer);
        } else {
            EXPECT_EQ(nullptr, buffer);
        };
    };
    injectFailures(method);
}

TEST_P(ValidHostPtr, GivenSvmHostPtrWhenCreatingBufferThenBufferIsCreatedCorrectly) {
    const ClDeviceInfo &devInfo = pClDevice->getDeviceInfo();
    if (devInfo.svmCapabilities != 0) {
        auto ptr = context->getSVMAllocsManager()->createSVMAlloc(64, {}, context->getRootDeviceIndices(), context->getDeviceBitfields());

        auto bufferSvm = Buffer::create(context.get(), CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR, 64, ptr, retVal);
        EXPECT_NE(nullptr, bufferSvm);
        EXPECT_TRUE(bufferSvm->isMemObjWithHostPtrSVM());
        auto svmData = context->getSVMAllocsManager()->getSVMAlloc(ptr);
        ASSERT_NE(nullptr, svmData);
        EXPECT_EQ(svmData->gpuAllocations.getGraphicsAllocation(pDevice->getRootDeviceIndex()), bufferSvm->getGraphicsAllocation(pDevice->getRootDeviceIndex()));
        EXPECT_EQ(CL_SUCCESS, retVal);

        delete bufferSvm;
        context->getSVMAllocsManager()->freeSVMAlloc(ptr);
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

TEST_P(BufferCalculateHostPtrSize, WhenCalculatingHostPtrSizeThenItIsCorrect) {
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
    auto graphicsAlloaction = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{context.getDevice(0)->getRootDeviceIndex(), MemoryConstants::pageSize});
    auto buffer = Buffer::createSharedBuffer(&context, CL_MEM_READ_ONLY, handler, GraphicsAllocationHelper::toMultiGraphicsAllocation(graphicsAlloaction));
    ASSERT_NE(nullptr, buffer);
    EXPECT_EQ(handler, buffer->peekSharingHandler());
    buffer->release();
}

class BufferTests : public ::testing::Test {
  protected:
    void SetUp() override {
        device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    }
    void TearDown() override {
    }
    std::unique_ptr<MockDevice> device;
};

typedef BufferTests BufferSetSurfaceTests;

HWCMDTEST_F(IGFX_GEN8_CORE, BufferSetSurfaceTests, givenBufferSetSurfaceThatMemoryPtrAndSizeIsAlignedToCachelineThenL3CacheShouldBeOn) {

    auto size = MemoryConstants::pageSize;
    auto ptr = (void *)alignedMalloc(size * 2, MemoryConstants::pageSize);

    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    RENDER_SURFACE_STATE surfaceState = {};

    Buffer::setSurfaceState(device.get(), &surfaceState, false, false, size, ptr, 0, nullptr, 0, 0, false, false);

    auto mocs = surfaceState.getMemoryObjectControlState();
    auto gmmHelper = device->getGmmHelper();
    EXPECT_EQ(gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER), mocs);

    alignedFree(ptr);
}

HWTEST_F(BufferSetSurfaceTests, givenDebugVariableToDisableCachingForStatefulBufferThenL3CacheShouldBeOff) {
    DebugManagerStateRestore restore;
    DebugManager.flags.DisableCachingForStatefulBufferAccess.set(true);

    auto size = MemoryConstants::pageSize;
    auto ptr = (void *)alignedMalloc(size * 2, MemoryConstants::pageSize);

    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    RENDER_SURFACE_STATE surfaceState = {};

    Buffer::setSurfaceState(device.get(), &surfaceState, false, false, size, ptr, 0, nullptr, 0, 0, false, false);

    auto mocs = surfaceState.getMemoryObjectControlState();
    auto gmmHelper = device->getGmmHelper();
    EXPECT_EQ(gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED), mocs);

    alignedFree(ptr);
    DebugManager.flags.DisableCachingForStatefulBufferAccess.set(false);
}

HWTEST_F(BufferSetSurfaceTests, givenBufferSetSurfaceThatMemoryPtrIsUnalignedToCachelineThenL3CacheShouldBeOff) {

    auto size = MemoryConstants::pageSize;
    auto ptr = alignedMalloc(size * 2, MemoryConstants::pageSize);
    auto ptrOffset = 1;
    auto offsetedPtr = (void *)((uintptr_t)ptr + ptrOffset);

    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    RENDER_SURFACE_STATE surfaceState = {};

    Buffer::setSurfaceState(device.get(), &surfaceState, false, false, size, offsetedPtr, 0, nullptr, 0, 0, false, false);

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

    Buffer::setSurfaceState(device.get(), &surfaceState, false, false, offsetedSize, ptr, 0, nullptr, 0, 0, false, false);

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

    Buffer::setSurfaceState(device.get(), &surfaceState, false, false, offsetedSize, ptr, 0, nullptr, CL_MEM_READ_ONLY, 0, false, false);

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

    Buffer::setSurfaceState(device.get(), &surfaceState, false, false, offsetedSize, ptr, 0, nullptr, 0, 0, false, false);

    auto width = surfaceState.getWidth();
    EXPECT_EQ(alignUp(width, 4), width);

    alignedFree(ptr);
}

HWTEST_F(BufferSetSurfaceTests, givenBufferSetSurfaceWhenOffsetIsSpecifiedForSvmAllocationThenSetSurfaceAddressWithOffsetedPointer) {

    auto size = 2 * MemoryConstants::pageSize;
    auto ptr = alignedMalloc(size, MemoryConstants::pageSize);
    auto offset = 4;
    MockGraphicsAllocation svmAlloc(ptr, size);

    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    RENDER_SURFACE_STATE surfaceState = {};

    Buffer::setSurfaceState(device.get(), &surfaceState, false, false, size, ptr, offset, &svmAlloc, 0, 0, false, false);

    auto baseAddress = surfaceState.getSurfaceBaseAddress();
    EXPECT_EQ(svmAlloc.getGpuAddress() + offset, baseAddress);

    alignedFree(ptr);
}

HWTEST_F(BufferSetSurfaceTests, givenBufferSetSurfaceThatMemoryPtrIsNotNullThenBufferSurfaceShouldBeUsed) {

    auto size = MemoryConstants::pageSize;
    auto ptr = alignedMalloc(size * 2, MemoryConstants::pageSize);

    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    RENDER_SURFACE_STATE surfaceState = {};

    Buffer::setSurfaceState(device.get(), &surfaceState, false, false, size, ptr, 0, nullptr, 0, 0, false, false);

    auto surfType = surfaceState.getSurfaceType();
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_BUFFER, surfType);

    alignedFree(ptr);
}

HWTEST_F(BufferSetSurfaceTests, givenBufferSetSurfaceThatMemoryPtrIsNullThenNullSurfaceShouldBeUsed) {

    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    RENDER_SURFACE_STATE surfaceState = {};

    Buffer::setSurfaceState(device.get(), &surfaceState, false, false, 0, nullptr, 0, nullptr, 0, 0, false, false);

    auto surfType = surfaceState.getSurfaceType();
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_NULL, surfType);
}

HWTEST_F(BufferSetSurfaceTests, givenBufferSetSurfaceThatAddressIsForcedTo32bitWhenSetArgStatefulIsCalledThenSurfaceBaseAddressIsPopulatedWithGpuAddress) {
    DebugManagerStateRestore dbgRestorer;
    {
        DebugManager.flags.Force32bitAddressing.set(true);
        MockContext context;
        auto rootDeviceIndex = context.getDevice(0)->getRootDeviceIndex();
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

        EXPECT_TRUE(is64bit ? buffer->getGraphicsAllocation(rootDeviceIndex)->is32BitAllocation() : true);

        using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
        RENDER_SURFACE_STATE surfaceState = {};

        buffer->setArgStateful(&surfaceState, false, false, false, false, context.getDevice(0)->getDevice(), false, false);

        auto surfBaseAddress = surfaceState.getSurfaceBaseAddress();
        auto bufferAddress = buffer->getGraphicsAllocation(rootDeviceIndex)->getGpuAddress();
        EXPECT_EQ(bufferAddress, surfBaseAddress);

        delete buffer;
        alignedFree(ptr);
        DebugManager.flags.Force32bitAddressing.set(false);
    }
}

HWTEST_F(BufferSetSurfaceTests, givenBufferWithOffsetWhenSetArgStatefulIsCalledThenSurfaceBaseAddressIsProperlyOffseted) {
    MockContext context;
    auto rootDeviceIndex = context.getDevice(0)->getRootDeviceIndex();
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

    subBuffer->setArgStateful(&surfaceState, false, false, false, false, context.getDevice(0)->getDevice(), false, false);

    auto surfBaseAddress = surfaceState.getSurfaceBaseAddress();
    auto bufferAddress = buffer->getGraphicsAllocation(rootDeviceIndex)->getGpuAddress();
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

    buffer->setArgStateful(&surfaceState, false, true, true, false, context.getDevice(0)->getDevice(), false, false);

    auto mocs = surfaceState.getMemoryObjectControlState();
    auto gmmHelper = device->getGmmHelper();
    EXPECT_EQ(gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED), mocs);
    EXPECT_EQ(128u, surfaceState.getWidth());
    EXPECT_EQ(4u, surfaceState.getHeight());
}

HWTEST_F(BufferSetSurfaceTests, givenBufferThatIsMisalignedButIsAReadOnlyArgumentWhenSurfaceStateIsSetThenL3IsOn) {
    MockContext context;
    auto rootDeviceIndex = context.getDevice(0)->getRootDeviceIndex();
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

    buffer->getGraphicsAllocation(rootDeviceIndex)->setSize(127);

    buffer->setArgStateful(&surfaceState, false, false, false, true, context.getDevice(0)->getDevice(), false, false);

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
    buffer->setArgStateful(&surfaceState, false, false, false, false, context.getDevice(0)->getDevice(), false, false);

    const auto expectedMocs = device->getGmmHelper()->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER);
    const auto actualMocs = surfaceState.getMemoryObjectControlState();
    EXPECT_EQ(expectedMocs, actualMocs);

    alignedFree(ptr);
}

HWCMDTEST_F(IGFX_GEN8_CORE, BufferSetSurfaceTests, givenAlignedCacheableNonReadOnlyBufferThenChooseOclBufferPolicy) {
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
    buffer->setArgStateful(&surfaceState, false, false, false, false, context.getDevice(0)->getDevice(), false, false);

    const auto expectedMocs = device->getGmmHelper()->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER);
    const auto actualMocs = surfaceState.getMemoryObjectControlState();
    EXPECT_EQ(expectedMocs, actualMocs);

    alignedFree(ptr);
}

HWTEST_F(BufferSetSurfaceTests, givenCompressedGmmResourceWhenSurfaceStateIsProgrammedThenSetAuxParams) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    using AUXILIARY_SURFACE_MODE = typename RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE;

    RENDER_SURFACE_STATE surfaceState = {};
    MockContext context;
    auto rootDeviceIndex = context.getDevice(0)->getRootDeviceIndex();
    auto retVal = CL_SUCCESS;

    std::unique_ptr<Buffer> buffer(Buffer::create(&context, CL_MEM_READ_WRITE, 1, nullptr, retVal));
    auto graphicsAllocation = buffer->getGraphicsAllocation(rootDeviceIndex);
    auto gmm = new Gmm(context.getDevice(0)->getGmmClientContext(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, {}, true);
    graphicsAllocation->setDefaultGmm(gmm);
    gmm->isCompressionEnabled = true;

    buffer->setArgStateful(&surfaceState, false, false, false, false, context.getDevice(0)->getDevice(), false, false);

    EXPECT_EQ(0u, surfaceState.getAuxiliarySurfaceBaseAddress());
    EXPECT_TRUE(EncodeSurfaceState<FamilyType>::isAuxModeEnabled(&surfaceState, gmm));
    EXPECT_TRUE(RENDER_SURFACE_STATE::COHERENCY_TYPE_GPU_COHERENT == surfaceState.getCoherencyType());
}

HWTEST_F(BufferSetSurfaceTests, givenNonCompressedGmmResourceWhenSurfaceStateIsProgrammedThenDontSetAuxParams) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    using AUXILIARY_SURFACE_MODE = typename RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE;

    RENDER_SURFACE_STATE surfaceState = {};
    MockContext context;
    auto rootDeviceIndex = context.getDevice(0)->getRootDeviceIndex();
    auto retVal = CL_SUCCESS;

    std::unique_ptr<Buffer> buffer(Buffer::create(&context, CL_MEM_READ_WRITE, 1, nullptr, retVal));
    auto gmm = new Gmm(context.getDevice(0)->getGmmClientContext(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, {}, true);
    buffer->getGraphicsAllocation(rootDeviceIndex)->setDefaultGmm(gmm);
    gmm->isCompressionEnabled = false;

    buffer->setArgStateful(&surfaceState, false, false, false, false, context.getDevice(0)->getDevice(), false, false);

    EXPECT_EQ(0u, surfaceState.getAuxiliarySurfaceBaseAddress());
    EXPECT_TRUE(AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_NONE == surfaceState.getAuxiliarySurfaceMode());
    EXPECT_TRUE(UnitTestHelper<FamilyType>::getCoherencyTypeSupported(RENDER_SURFACE_STATE::COHERENCY_TYPE_IA_COHERENT) == surfaceState.getCoherencyType());
}

HWTEST_F(BufferSetSurfaceTests, givenMisalignedPointerWhenSurfaceStateIsProgrammedThenBaseAddressAndLengthAreAlignedToDword) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    using AUXILIARY_SURFACE_MODE = typename RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE;

    RENDER_SURFACE_STATE surfaceState = {};
    MockContext context;
    uintptr_t ptr = 0xfffff000;
    void *svmPtr = reinterpret_cast<void *>(ptr);

    Buffer::setSurfaceState(device.get(), &surfaceState, false, false, 5, svmPtr, 0, nullptr, 0, 0, false, false);

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

    Buffer::setSurfaceState(device.get(), &surfaceState, false, false, 5, svmPtr, 0, nullptr, 0, 0, false, false);

    EXPECT_EQ(0u, surfaceState.getMemoryObjectControlState());
}

using BufferHwFromDeviceTests = BufferTests;

HWTEST_F(BufferHwFromDeviceTests, givenMultiGraphicsAllocationWhenCreateBufferHwFromDeviceThenMultiGraphicsAllocationInBufferIsProperlySet) {
    auto size = 2 * MemoryConstants::pageSize;
    auto ptr = alignedMalloc(size, MemoryConstants::pageSize);
    MockGraphicsAllocation svmAlloc(ptr, size);

    auto multiGraphicsAllocation = MultiGraphicsAllocation(device.get()->getRootDeviceIndex());
    multiGraphicsAllocation.addAllocation(&svmAlloc);
    auto buffer = std::unique_ptr<Buffer>(Buffer::createBufferHwFromDevice(device.get(), 0, 0, size, ptr, ptr, multiGraphicsAllocation, 0, true, false, false));

    EXPECT_EQ(device.get()->getRootDeviceIndex(), 0u);
    EXPECT_EQ(buffer->getMultiGraphicsAllocation().getGraphicsAllocations().size(), multiGraphicsAllocation.getGraphicsAllocations().size());
    EXPECT_EQ(buffer->getMultiGraphicsAllocation().getGraphicsAllocation(device.get()->getRootDeviceIndex()), multiGraphicsAllocation.getGraphicsAllocation(device.get()->getRootDeviceIndex()));

    alignedFree(ptr);
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
    const auto &compilerHwInfoConfig = *CompilerHwInfoConfig::get(defaultHwInfo->platform.eProductFamily);
    if (compilerHwInfoConfig.isForceToStatelessRequired() || !ctx.getDevice(0)->getHardwareInfo().capabilityTable.supportsImages) {
        GTEST_SKIP();
    }
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;

    CommandQueueHw<FamilyType> cmdQ(&ctx, ctx.getDevice(0), nullptr, false);
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

    auto expect = ctx.getDevice(0)->getGmmHelper()->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER);
    auto expect2 = ctx.getDevice(0)->getGmmHelper()->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CONST);

    EXPECT_NE(NULL, surfaceState->getMemoryObjectControlState());
    EXPECT_TRUE(expect == surfaceState->getMemoryObjectControlState() || expect2 == surfaceState->getMemoryObjectControlState());

    clReleaseMemObject(image);
}

HWTEST_P(BufferL3CacheTests, givenMisalignedAndAlignedBufferWhenClEnqueueWriteBufferRectThenL3CacheIsOn) {
    const auto &compilerHwInfoConfig = *CompilerHwInfoConfig::get(defaultHwInfo->platform.eProductFamily);
    if (compilerHwInfoConfig.isForceToStatelessRequired()) {
        GTEST_SKIP();
    }
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;

    CommandQueueHw<FamilyType> cmdQ(&ctx, ctx.getDevice(0), nullptr, false);
    auto surfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(cmdQ.getGpgpuCommandStreamReceiver().getIndirectHeap(IndirectHeap::Type::SURFACE_STATE, 0).getSpace(0));
    auto buffer = clCreateBuffer(&ctx, CL_MEM_READ_WRITE, 36, nullptr, nullptr);

    clEnqueueWriteBufferRect(&cmdQ, buffer, false, origin, origin, region, 0, 0, 0, 0, hostPtr, 0, nullptr, nullptr);

    auto expect = ctx.getDevice(0)->getGmmHelper()->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER);
    auto expect2 = ctx.getDevice(0)->getGmmHelper()->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CONST);

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

struct BufferUnmapTest : public ClDeviceFixture, public ::testing::Test {
    void SetUp() override {
        ClDeviceFixture::SetUp();
    }
    void TearDown() override {
        ClDeviceFixture::TearDown();
    }
};

HWTEST_F(BufferUnmapTest, givenBufferWithSharingHandlerWhenUnmappingThenUseNonBlockingEnqueueWriteBuffer) {
    MockContext context(pClDevice);
    MockCommandQueueHw<FamilyType> cmdQ(&context, pClDevice, nullptr);

    auto retVal = CL_SUCCESS;
    std::unique_ptr<Buffer> buffer(Buffer::create(&context, CL_MEM_ALLOC_HOST_PTR, 123, nullptr, retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);

    buffer->setSharingHandler(new SharingHandler());
    EXPECT_NE(nullptr, buffer->peekSharingHandler());

    auto gfxAllocation = buffer->getGraphicsAllocation(pDevice->getRootDeviceIndex());
    for (auto handleId = 0u; handleId < gfxAllocation->getNumGmms(); handleId++) {
        gfxAllocation->setGmm(new MockGmm(pDevice->getGmmClientContext()), handleId);
    }

    auto mappedPtr = clEnqueueMapBuffer(&cmdQ, buffer.get(), CL_TRUE, CL_MAP_WRITE, 0, 1, 0, nullptr, nullptr, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(0u, cmdQ.EnqueueWriteBufferCounter);
    retVal = clEnqueueUnmapMemObject(&cmdQ, buffer.get(), mappedPtr, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(1u, cmdQ.EnqueueWriteBufferCounter);
    EXPECT_FALSE(cmdQ.blockingWriteBuffer);
}

HWTEST_F(BufferUnmapTest, givenBufferWithoutSharingHandlerWhenUnmappingThenDontUseEnqueueWriteBuffer) {
    MockContext context(pClDevice);
    MockCommandQueueHw<FamilyType> cmdQ(&context, pClDevice, nullptr);

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
    MockContext context(pClDevice);
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
    MockContext context(pClDevice);
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

using MultiRootDeviceBufferTest = MultiRootDeviceFixture;

TEST_F(MultiRootDeviceBufferTest, WhenCleanAllGraphicsAllocationsCalledThenGraphicsAllocationsAreProperlyRemovedAccordingToIsParentObjectFlag) {
    AllocationInfoType allocationInfo;
    allocationInfo.resize(3u);

    allocationInfo[1u] = {};
    allocationInfo[1u].memory = mockMemoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{1u, MemoryConstants::pageSize});

    bool isParentObject = true;
    Buffer::cleanAllGraphicsAllocations(*context, *context->getMemoryManager(), allocationInfo, isParentObject);
    EXPECT_EQ(mockMemoryManager->freeGraphicsMemoryCalled, 0u);

    isParentObject = false;
    Buffer::cleanAllGraphicsAllocations(*context, *context->getMemoryManager(), allocationInfo, isParentObject);
    EXPECT_EQ(mockMemoryManager->freeGraphicsMemoryCalled, 1u);
}

TEST_F(MultiRootDeviceBufferTest, WhenBufferIsCreatedThenBufferGraphicsAllocationHasCorrectRootDeviceIndex) {
    cl_int retVal = 0;
    cl_mem_flags flags = CL_MEM_READ_WRITE;

    std::unique_ptr<Buffer> buffer(Buffer::create(context.get(), flags, MemoryConstants::pageSize, nullptr, retVal));

    auto graphicsAllocation = buffer->getGraphicsAllocation(expectedRootDeviceIndex);
    ASSERT_NE(nullptr, graphicsAllocation);
    EXPECT_EQ(expectedRootDeviceIndex, graphicsAllocation->getRootDeviceIndex());
}

TEST_F(MultiRootDeviceBufferTest, WhenBufferIsCreatedThenBufferMultiGraphicsAllocationIsCreatedInSystemMemoryPool) {
    cl_int retVal = 0;

    std::unique_ptr<Buffer> buffer1(Buffer::create(context.get(), 0, MemoryConstants::pageSize, nullptr, retVal));

    EXPECT_TRUE(MemoryPool::isSystemMemoryPool(buffer1->getMultiGraphicsAllocation().getGraphicsAllocation(1u)->getMemoryPool()));
    EXPECT_TRUE(MemoryPool::isSystemMemoryPool(buffer1->getMultiGraphicsAllocation().getGraphicsAllocation(2u)->getMemoryPool()));
}

TEST(MultiRootDeviceBufferTest2, WhenBufferIsCreatedThenSecondAndSubsequentAllocationsAreCreatedFromExisitingStorage) {
    cl_int retVal = 0;
    MockDefaultContext context;
    auto memoryManager = static_cast<MockMemoryManager *>(context.getMemoryManager());
    memoryManager->createGraphicsAllocationFromExistingStorageCalled = 0u;
    memoryManager->allocationsFromExistingStorage.clear();
    std::unique_ptr<Buffer> buffer(Buffer::create(&context, 0, MemoryConstants::pageSize, nullptr, retVal));

    EXPECT_EQ(3u, context.getRootDeviceIndices().size());
    EXPECT_NE(nullptr, buffer->getMultiGraphicsAllocation().getGraphicsAllocation(0u));
    EXPECT_NE(nullptr, buffer->getMultiGraphicsAllocation().getGraphicsAllocation(1u));
    EXPECT_NE(nullptr, buffer->getMultiGraphicsAllocation().getGraphicsAllocation(2u));

    EXPECT_EQ(2u, memoryManager->createGraphicsAllocationFromExistingStorageCalled);
    EXPECT_EQ(memoryManager->allocationsFromExistingStorage[0], buffer->getMultiGraphicsAllocation().getGraphicsAllocation(1u));
    EXPECT_EQ(memoryManager->allocationsFromExistingStorage[1], buffer->getMultiGraphicsAllocation().getGraphicsAllocation(2u));
}

TEST_F(MultiRootDeviceBufferTest, givenBufferWhenGetSurfaceSizeCalledWithoutAlignSizeForAuxTranslationThenCorrectValueReturned) {
    cl_int retVal = 0;
    cl_mem_flags flags = CL_MEM_READ_WRITE;
    uint32_t size = 0x131;
    std::unique_ptr<Buffer> buffer(Buffer::create(context.get(), flags, size, nullptr, retVal));

    auto surfaceSize = buffer->getSurfaceSize(false, expectedRootDeviceIndex);
    EXPECT_EQ(surfaceSize, alignUp(size, 4));
}

TEST_F(MultiRootDeviceBufferTest, givenBufferWhenGetSurfaceSizeCalledWithAlignSizeForAuxTranslationThenCorrectValueReturned) {
    cl_int retVal = 0;
    cl_mem_flags flags = CL_MEM_READ_WRITE;
    uint32_t size = 0x131;
    std::unique_ptr<Buffer> buffer(Buffer::create(context.get(), flags, size, nullptr, retVal));

    auto surfaceSize = buffer->getSurfaceSize(true, expectedRootDeviceIndex);
    EXPECT_EQ(surfaceSize, alignUp(size, 512));
}

TEST_F(MultiRootDeviceBufferTest, givenNullptrGraphicsAllocationForRootDeviceIndexWhenGettingBufferAddressThenHostPtrReturned) {
    cl_int retVal = 0;
    cl_mem_flags flags = CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR;

    char *hostPtr[MemoryConstants::pageSize]{};
    std::unique_ptr<Buffer> buffer(Buffer::create(context.get(), flags, MemoryConstants::pageSize, hostPtr, retVal));

    auto address = buffer->getBufferAddress(expectedRootDeviceIndex);
    auto graphicsAllocation = buffer->getGraphicsAllocation(expectedRootDeviceIndex);
    EXPECT_EQ(graphicsAllocation->getGpuAddress(), address);

    address = buffer->getBufferAddress(0);
    EXPECT_EQ(reinterpret_cast<uint64_t>(buffer->getHostPtr()), address);
}
