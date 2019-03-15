/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/properties_helper.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/mocks/mock_buffer.h"
#include "unit_tests/mocks/mock_memory_manager.h"

#include "gtest/gtest.h"

using namespace OCLRT;

TEST(TransferPropertiesTest, givenTransferPropertiesCreatedWhenDefaultDebugSettingThenLockPtrIsNotSet) {
    MockBuffer buffer;

    size_t offset = 0;
    size_t size = 4096u;
    TransferProperties transferProperties(&buffer, CL_COMMAND_MAP_BUFFER, 0, false, &offset, &size, nullptr);
    EXPECT_EQ(nullptr, transferProperties.lockedPtr);
}

TEST(TransferPropertiesTest, givenAllocationInNonSystemPoolWhenTransferPropertiesAreCreatedForMapBufferThenLockPtrIsSet) {
    ExecutionEnvironment executionEnvironment;
    MemoryManagerCreate<OsAgnosticMemoryManager> memoryManager(false, true, executionEnvironment);

    MockContext ctx;
    ctx.setMemoryManager(&memoryManager);
    cl_int retVal;
    std::unique_ptr<Buffer> buffer(Buffer::create(&ctx, 0, 1, nullptr, retVal));
    static_cast<MemoryAllocation *>(buffer->getGraphicsAllocation())->overrideMemoryPool(MemoryPool::SystemCpuInaccessible);

    size_t offset = 0;
    size_t size = 4096u;

    TransferProperties transferProperties(buffer.get(), CL_COMMAND_MAP_BUFFER, 0, false, &offset, &size, nullptr);
    EXPECT_NE(nullptr, transferProperties.lockedPtr);
}

TEST(TransferPropertiesTest, givenAllocationInSystemPoolWhenTransferPropertiesAreCreatedForMapBufferThenLockPtrIsNotSet) {
    ExecutionEnvironment executionEnvironment;
    MemoryManagerCreate<OsAgnosticMemoryManager> memoryManager(false, true, executionEnvironment);

    MockContext ctx;
    ctx.setMemoryManager(&memoryManager);
    cl_int retVal;
    std::unique_ptr<Buffer> buffer(Buffer::create(&ctx, 0, 1, nullptr, retVal));
    static_cast<MemoryAllocation *>(buffer->getGraphicsAllocation())->overrideMemoryPool(MemoryPool::System4KBPages);

    size_t offset = 0;
    size_t size = 4096u;

    TransferProperties transferProperties(buffer.get(), CL_COMMAND_MAP_BUFFER, 0, false, &offset, &size, nullptr);
    EXPECT_EQ(nullptr, transferProperties.lockedPtr);
}

TEST(TransferPropertiesTest, givenTransferPropertiesCreatedWhenMemoryManagerInMemObjectIsNotSetThenLockPtrIsNotSet) {
    MockBuffer buffer;

    size_t offset = 0;
    size_t size = 4096u;
    TransferProperties transferProperties(&buffer, CL_COMMAND_MAP_BUFFER, 0, false, &offset, &size, nullptr);
    EXPECT_EQ(nullptr, transferProperties.lockedPtr);
}

TEST(TransferPropertiesTest, givenTransferPropertiesWhenLockedPtrIsSetThenItIsReturnedForReadWrite) {
    ExecutionEnvironment executionEnvironment;
    MemoryManagerCreate<OsAgnosticMemoryManager> memoryManager(false, true, executionEnvironment);

    MockContext ctx;
    ctx.setMemoryManager(&memoryManager);
    cl_int retVal;
    std::unique_ptr<Buffer> buffer(Buffer::create(&ctx, 0, 1, nullptr, retVal));
    static_cast<MemoryAllocation *>(buffer->getGraphicsAllocation())->overrideMemoryPool(MemoryPool::SystemCpuInaccessible);

    size_t offset = 0;
    size_t size = 4096u;

    TransferProperties transferProperties(buffer.get(), CL_COMMAND_MAP_BUFFER, 0, false, &offset, &size, nullptr);
    ASSERT_NE(nullptr, transferProperties.lockedPtr);
    EXPECT_EQ(transferProperties.lockedPtr, transferProperties.getCpuPtrForReadWrite());
}

TEST(TransferPropertiesTest, givenTransferPropertiesWhenLockedPtrIsNotSetThenItIsNotReturnedForReadWrite) {
    MockBuffer buffer;

    size_t offset = 0;
    size_t size = 4096u;
    TransferProperties transferProperties(&buffer, CL_COMMAND_MAP_BUFFER, 0, false, &offset, &size, nullptr);
    ASSERT_EQ(nullptr, transferProperties.lockedPtr);
    EXPECT_NE(transferProperties.lockedPtr, transferProperties.getCpuPtrForReadWrite());
}

TEST(TransferPropertiesTest, givenTransferPropertiesWhenLockedPtrIsSetThenLockedPtrWithMemObjOffsetIsReturnedForReadWrite) {
    MockBuffer buffer;

    void *lockedPtr = reinterpret_cast<void *>(0x1000);
    auto memObjOffset = MemoryConstants::cacheLineSize;
    buffer.offset = memObjOffset;

    size_t offset = 0;
    size_t size = 4096u;
    TransferProperties transferProperties(&buffer, CL_COMMAND_MAP_BUFFER, 0, false, &offset, &size, nullptr);
    transferProperties.lockedPtr = lockedPtr;
    auto expectedPtr = ptrOffset(lockedPtr, memObjOffset);
    EXPECT_EQ(expectedPtr, transferProperties.getCpuPtrForReadWrite());
}
