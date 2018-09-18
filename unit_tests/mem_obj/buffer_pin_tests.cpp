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
    GraphicsAllocation *allocateGraphicsMemory(size_t size, size_t alignment, bool forcePin, bool uncacheable) override {
        EXPECT_NE(0u, expectedSize);
        if (expectedSize == size) {
            EXPECT_TRUE(forcePin);
            allocCount++;
        }
        return OsAgnosticMemoryManager::allocateGraphicsMemory(size, alignment, forcePin, uncacheable);
    };
    GraphicsAllocation *allocateGraphicsMemory64kb(size_t size, size_t alignment, bool forcePin, bool preferRenderCompressed) override {
        return nullptr;
    };
    GraphicsAllocation *allocateGraphicsMemory(size_t size, const void *ptr, bool forcePin) override {
        EXPECT_NE(0u, HPExpectedSize);
        if (HPExpectedSize == size) {
            EXPECT_TRUE(forcePin);
            HPAllocCount++;
        }
        return OsAgnosticMemoryManager::allocateGraphicsMemory(size, ptr, forcePin);
    }

    size_t expectedSize = 0;
    uint32_t allocCount = 0;
    size_t HPExpectedSize = 0;
    uint32_t HPAllocCount = 0;
};

TEST(BufferTests, doPinIsSet) {
    std::unique_ptr<TestedMemoryManager> mm(new TestedMemoryManager());
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
    std::unique_ptr<TestedMemoryManager> mm(new TestedMemoryManager());
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
