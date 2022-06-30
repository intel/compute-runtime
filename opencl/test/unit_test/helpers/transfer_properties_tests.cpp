/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_memory_manager.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/helpers/properties_helper.h"
#include "opencl/test/unit_test/mocks/mock_buffer.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(TransferPropertiesTest, givenTransferPropertiesCreatedWhenDefaultDebugSettingThenLockPtrIsNotSet) {
    MockBuffer buffer;
    const uint32_t rootDeviceIndex = buffer.mockGfxAllocation.getRootDeviceIndex();

    size_t offset = 0;
    size_t size = 4096u;
    TransferProperties transferProperties(&buffer, CL_COMMAND_MAP_BUFFER, 0, false, &offset, &size, nullptr, true, rootDeviceIndex);
    EXPECT_EQ(nullptr, transferProperties.lockedPtr);
}

TEST(TransferPropertiesTest, givenAllocationInNonSystemPoolWhenTransferPropertiesAreCreatedForMapBufferAndCpuTransferIsRequestedThenLockPtrIsSet) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MemoryManagerCreate<OsAgnosticMemoryManager> memoryManager(false, true, executionEnvironment);

    MockContext context;
    context.memoryManager = &memoryManager;
    cl_int retVal;
    std::unique_ptr<Buffer> buffer(Buffer::create(&context, 0, 1, nullptr, retVal));
    static_cast<MemoryAllocation *>(buffer->getGraphicsAllocation(context.getDevice(0)->getRootDeviceIndex()))->overrideMemoryPool(MemoryPool::SystemCpuInaccessible);

    size_t offset = 0;
    size_t size = 4096u;

    TransferProperties transferProperties(buffer.get(), CL_COMMAND_MAP_BUFFER, 0, false, &offset, &size, nullptr, true, context.getDevice(0)->getRootDeviceIndex());
    EXPECT_NE(nullptr, transferProperties.lockedPtr);
}
TEST(TransferPropertiesTest, givenAllocationInNonSystemPoolWhenTransferPropertiesAreCreatedForMapBufferAndCpuTransferIsNotRequestedThenLockPtrIsNotSet) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MemoryManagerCreate<OsAgnosticMemoryManager> memoryManager(false, true, executionEnvironment);

    MockContext context;
    context.memoryManager = &memoryManager;
    cl_int retVal;
    std::unique_ptr<Buffer> buffer(Buffer::create(&context, 0, 1, nullptr, retVal));
    static_cast<MemoryAllocation *>(buffer->getGraphicsAllocation(context.getDevice(0)->getRootDeviceIndex()))->overrideMemoryPool(MemoryPool::SystemCpuInaccessible);

    size_t offset = 0;
    size_t size = 4096u;

    TransferProperties transferProperties(buffer.get(), CL_COMMAND_MAP_BUFFER, 0, false, &offset, &size, nullptr, false, context.getDevice(0)->getRootDeviceIndex());
    EXPECT_EQ(nullptr, transferProperties.lockedPtr);
}

TEST(TransferPropertiesTest, givenAllocationInSystemPoolWhenTransferPropertiesAreCreatedForMapBufferThenLockPtrIsNotSet) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MemoryManagerCreate<OsAgnosticMemoryManager> memoryManager(false, true, executionEnvironment);

    MockContext context;
    context.memoryManager = &memoryManager;
    cl_int retVal;
    std::unique_ptr<Buffer> buffer(Buffer::create(&context, 0, 1, nullptr, retVal));
    static_cast<MemoryAllocation *>(buffer->getGraphicsAllocation(context.getDevice(0)->getRootDeviceIndex()))->overrideMemoryPool(MemoryPool::System4KBPages);

    size_t offset = 0;
    size_t size = 4096u;

    TransferProperties transferProperties(buffer.get(), CL_COMMAND_MAP_BUFFER, 0, false, &offset, &size, nullptr, true, context.getDevice(0)->getRootDeviceIndex());
    EXPECT_EQ(nullptr, transferProperties.lockedPtr);
}

TEST(TransferPropertiesTest, givenTransferPropertiesWhenLockedPtrIsSetThenItIsReturnedForReadWrite) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MemoryManagerCreate<OsAgnosticMemoryManager> memoryManager(false, true, executionEnvironment);

    MockContext context;
    context.memoryManager = &memoryManager;
    cl_int retVal;
    std::unique_ptr<Buffer> buffer(Buffer::create(&context, 0, 1, nullptr, retVal));
    static_cast<MemoryAllocation *>(buffer->getGraphicsAllocation(context.getDevice(0)->getRootDeviceIndex()))->overrideMemoryPool(MemoryPool::SystemCpuInaccessible);

    size_t offset = 0;
    size_t size = 4096u;

    TransferProperties transferProperties(buffer.get(), CL_COMMAND_MAP_BUFFER, 0, false, &offset, &size, nullptr, true, context.getDevice(0)->getRootDeviceIndex());
    ASSERT_NE(nullptr, transferProperties.lockedPtr);
    EXPECT_EQ(transferProperties.lockedPtr, transferProperties.getCpuPtrForReadWrite());
}

TEST(TransferPropertiesTest, givenTransferPropertiesWhenLockedPtrIsNotSetThenItIsNotReturnedForReadWrite) {
    MockBuffer buffer;
    const uint32_t rootDeviceIndex = buffer.mockGfxAllocation.getRootDeviceIndex();

    size_t offset = 0;
    size_t size = 4096u;
    TransferProperties transferProperties(&buffer, CL_COMMAND_MAP_BUFFER, 0, false, &offset, &size, nullptr, true, rootDeviceIndex);
    ASSERT_EQ(nullptr, transferProperties.lockedPtr);
    EXPECT_NE(transferProperties.lockedPtr, transferProperties.getCpuPtrForReadWrite());
}

TEST(TransferPropertiesTest, givenTransferPropertiesWhenLockedPtrIsSetThenLockedPtrWithMemObjOffsetIsReturnedForReadWrite) {
    MockBuffer buffer;
    const uint32_t rootDeviceIndex = buffer.mockGfxAllocation.getRootDeviceIndex();

    void *lockedPtr = reinterpret_cast<void *>(0x1000);
    auto memObjOffset = MemoryConstants::cacheLineSize;
    buffer.offset = memObjOffset;

    size_t offset = 0;
    size_t size = 4096u;
    TransferProperties transferProperties(&buffer, CL_COMMAND_MAP_BUFFER, 0, false, &offset, &size, nullptr, true, rootDeviceIndex);
    transferProperties.lockedPtr = lockedPtr;
    auto expectedPtr = ptrOffset(lockedPtr, memObjOffset);
    EXPECT_EQ(expectedPtr, transferProperties.getCpuPtrForReadWrite());
}
