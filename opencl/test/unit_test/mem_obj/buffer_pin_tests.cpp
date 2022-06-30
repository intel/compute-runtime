/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/os_agnostic_memory_manager.h"
#include "shared/test/common/fixtures/memory_management_fixture.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/source/mem_obj/buffer.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/fixtures/platform_fixture.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

#include "gtest/gtest.h"

using namespace NEO;

class TestedMemoryManager : public OsAgnosticMemoryManager {
  public:
    using OsAgnosticMemoryManager::OsAgnosticMemoryManager;
    GraphicsAllocation *allocateGraphicsMemoryWithAlignment(const AllocationData &allocationData) override {
        EXPECT_NE(0u, expectedSize);
        if (expectedSize == allocationData.size) {
            EXPECT_TRUE(allocationData.flags.forcePin);
            allocCount++;
        }
        return OsAgnosticMemoryManager::allocateGraphicsMemoryWithAlignment(allocationData);
    };
    GraphicsAllocation *allocateGraphicsMemory64kb(const AllocationData &allocationData) override {
        return nullptr;
    };
    GraphicsAllocation *allocateGraphicsMemoryWithHostPtr(const AllocationData &properties) override {
        EXPECT_NE(0u, HPExpectedSize);
        if (HPExpectedSize == properties.size) {
            EXPECT_TRUE(properties.flags.forcePin);
            HPAllocCount++;
        }
        return OsAgnosticMemoryManager::allocateGraphicsMemoryWithHostPtr(properties);
    }
    GraphicsAllocation *allocateGraphicsMemoryForNonSvmHostPtr(const AllocationData &properties) override {
        EXPECT_NE(0u, HPExpectedSize);
        if (HPExpectedSize == properties.size) {
            EXPECT_TRUE(properties.flags.forcePin);
            HPAllocCount++;
        }
        return OsAgnosticMemoryManager::allocateGraphicsMemoryForNonSvmHostPtr(properties);
    }

    size_t expectedSize = 0;
    uint32_t allocCount = 0;
    size_t HPExpectedSize = 0;
    uint32_t HPAllocCount = 0;
};

TEST(BufferTests, WhenBufferIsCreatedThenPinIsSet) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    std::unique_ptr<TestedMemoryManager> mm(new MemoryManagerCreate<TestedMemoryManager>(false, false, executionEnvironment));
    if (mm->isLimitedGPU(0)) {
        GTEST_SKIP();
    }
    {
        MockContext context;
        auto size = MemoryConstants::pageSize * 32;
        auto retVal = CL_INVALID_OPERATION;
        mm->expectedSize = size;
        mm->HPExpectedSize = 0u;
        context.memoryManager = mm.get();

        auto buffer = Buffer::create(
            &context,
            0,
            size,
            nullptr,
            retVal);
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_EQ(1u, mm->allocCount);

        delete buffer;
    }
}
TEST(BufferTests, GivenHostPtrWhenBufferIsCreatedThenPinIsSet) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    std::unique_ptr<TestedMemoryManager> mm(new TestedMemoryManager(executionEnvironment));
    if (mm->isLimitedGPU(0)) {
        GTEST_SKIP();
    }

    {
        MockContext context;
        auto retVal = CL_INVALID_OPERATION;
        auto size = MemoryConstants::pageSize * 32;
        mm->expectedSize = 0u;
        mm->HPExpectedSize = size;
        context.memoryManager = mm.get();

        // memory must be aligned to use zero-copy
        void *bff = alignedMalloc(size, MemoryConstants::pageSize);

        auto buffer = Buffer::create(
            &context,
            CL_MEM_USE_HOST_PTR | CL_MEM_FORCE_HOST_MEMORY_INTEL,
            size,
            bff,
            retVal);
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_EQ(1u, mm->HPAllocCount);

        delete buffer;
        alignedFree(bff);
    }
}
