/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/options.h"
#include "runtime/mem_obj/buffer.h"
#include "runtime/memory_manager/os_agnostic_memory_manager.h"
#include "test.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/fixtures/memory_management_fixture.h"
#include "unit_tests/fixtures/platform_fixture.h"
#include "unit_tests/helpers/memory_management.h"
#include "unit_tests/mocks/mock_context.h"
#include "gtest/gtest.h"

using namespace OCLRT;

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
    GraphicsAllocation *allocateGraphicsMemory64kb(AllocationData allocationData) override {
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

    size_t expectedSize = 0;
    uint32_t allocCount = 0;
    size_t HPExpectedSize = 0;
    uint32_t HPAllocCount = 0;
};

TEST(BufferTests, doPinIsSet) {
    ExecutionEnvironment executionEnvironment;
    std::unique_ptr<TestedMemoryManager> mm(new TestedMemoryManager(false, false, executionEnvironment));
    {
        MockContext context;
        auto size = MemoryConstants::pageSize * 32;
        auto retVal = CL_INVALID_OPERATION;
        mm->expectedSize = size;
        mm->HPExpectedSize = 0u;
        context.setMemoryManager(mm.get());

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
TEST(BufferTests, doPinIsSetForHostPtr) {
    ExecutionEnvironment executionEnvironment;
    std::unique_ptr<TestedMemoryManager> mm(new TestedMemoryManager(false, false, executionEnvironment));
    {
        MockContext context;
        auto retVal = CL_INVALID_OPERATION;
        auto size = MemoryConstants::pageSize * 32;
        mm->expectedSize = 0u;
        mm->HPExpectedSize = size;
        context.setMemoryManager(mm.get());

        // memory must be aligned to use zero-copy
        void *bff = alignedMalloc(size, MemoryConstants::pageSize);

        auto buffer = Buffer::create(
            &context,
            CL_MEM_USE_HOST_PTR,
            size,
            bff,
            retVal);
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_EQ(1u, mm->HPAllocCount);

        delete buffer;
        alignedFree(bff);
    }
}
