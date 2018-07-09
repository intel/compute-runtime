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

#include "gtest/gtest.h"
#include "test.h"
#include "runtime/event/event.h"
#include "runtime/memory_manager/svm_memory_manager.h"
#include "runtime/utilities/tag_allocator.h"
#include "unit_tests/helpers/memory_management.h"
#include "unit_tests/utilities/containers_tests_helpers.h"
#include "unit_tests/fixtures/memory_allocator_fixture.h"
#include "unit_tests/mocks/mock_context.h"

#include <future>

using namespace OCLRT;

typedef Test<MemoryAllocatorFixture> SVMMemoryAllocatorTest;

TEST_F(SVMMemoryAllocatorTest, allocateSystem) {
    auto ptr = memoryManager->allocateSystemMemory(sizeof(char), 0);
    EXPECT_NE(nullptr, ptr);
    memoryManager->freeSystemMemory(ptr);
}

TEST_F(SVMMemoryAllocatorTest, SVMAllocCreateNullFreeNull) {
    OsAgnosticMemoryManager umm;
    {
        SVMAllocsManager svmM(&umm);
        char *Ptr1 = (char *)svmM.createSVMAlloc(0);
        EXPECT_EQ(Ptr1, nullptr);
        svmM.freeSVMAlloc(nullptr);
    }
}

TEST_F(SVMMemoryAllocatorTest, SVMAllocCreateFree) {
    OsAgnosticMemoryManager umm;
    {
        SVMAllocsManager svmM(&umm);
        char *Ptr1 = (char *)svmM.createSVMAlloc(4096);
        EXPECT_NE(Ptr1, nullptr);

        svmM.freeSVMAlloc(Ptr1);
        GraphicsAllocation *GA2 = svmM.getSVMAlloc(Ptr1);
        EXPECT_EQ(GA2, nullptr);
    }
}

TEST_F(SVMMemoryAllocatorTest, SVMAllocGetNoSVMAdrees) {
    char array[100];
    OsAgnosticMemoryManager umm;
    {
        SVMAllocsManager svmM(&umm);

        char *Ptr1 = (char *)100;
        GraphicsAllocation *GA1 = svmM.getSVMAlloc(Ptr1);
        EXPECT_EQ(GA1, nullptr);

        GraphicsAllocation *GA2 = svmM.getSVMAlloc(array);
        EXPECT_EQ(GA2, nullptr);
    }
}

TEST_F(SVMMemoryAllocatorTest, SVMAllocGetBeforeAndInside) {
    OsAgnosticMemoryManager umm;
    {
        SVMAllocsManager svmM(&umm);
        char *Ptr1 = (char *)svmM.createSVMAlloc(4096);
        EXPECT_NE(Ptr1, nullptr);

        char *Ptr2 = Ptr1 - 4;
        GraphicsAllocation *GA2 = svmM.getSVMAlloc(Ptr2);
        EXPECT_EQ(GA2, nullptr);

        char *Ptr3 = Ptr1 + 4;
        GraphicsAllocation *GA3 = svmM.getSVMAlloc(Ptr3);
        EXPECT_NE(GA3, nullptr);
        EXPECT_EQ(GA3->getUnderlyingBuffer(), Ptr1);

        svmM.freeSVMAlloc(Ptr1);
    }
}

TEST_F(SVMMemoryAllocatorTest, SVMAllocgetAfterSVM) {
    OsAgnosticMemoryManager umm;
    {
        SVMAllocsManager svmM(&umm);
        char *Ptr1 = (char *)svmM.createSVMAlloc(4096);
        EXPECT_NE(Ptr1, nullptr);

        char *Ptr2 = Ptr1 + 4096 + 100;
        GraphicsAllocation *GA2 = svmM.getSVMAlloc(Ptr2);
        EXPECT_EQ(GA2, nullptr);

        svmM.freeSVMAlloc(Ptr1);
    }
}

TEST_F(SVMMemoryAllocatorTest, WhenCouldNotAllocateInMemoryManagerThenReturnsNullAndDoesNotChangeAllocsMap) {
    struct MockMemManager : public OsAgnosticMemoryManager {
        using OsAgnosticMemoryManager::allocateGraphicsMemory;

      public:
        GraphicsAllocation *allocateGraphicsMemory(size_t size, size_t alignment, bool forcePin, bool uncacheable) override {
            return nullptr;
        }
    };

    struct MockSVMAllocsManager : SVMAllocsManager {

        MockSVMAllocsManager(MemoryManager *m)
            : SVMAllocsManager(m) {
        }

        MapBasedAllocationTracker &GetSVMAllocs() {
            return SVMAllocs;
        }
    };

    MockMemManager mm;
    {
        MockSVMAllocsManager svmM{&mm};
        void *svmPtr = svmM.createSVMAlloc(512);
        EXPECT_EQ(nullptr, svmPtr);

        EXPECT_EQ(0U, svmM.GetSVMAllocs().getNumAllocs());
    }
}
