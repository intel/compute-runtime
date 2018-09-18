/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/memory_manager/svm_memory_manager.h"
#include "runtime/event/event.h"
#include "runtime/utilities/tag_allocator.h"
#include "test.h"
#include "unit_tests/fixtures/memory_allocator_fixture.h"
#include "unit_tests/helpers/memory_management.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/utilities/containers_tests_helpers.h"
#include "gtest/gtest.h"

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

TEST_F(SVMMemoryAllocatorTest, given64kbAllowedwhenAllocatingSvmMemoryThenDontPreferRenderCompression) {
    class MyMemoryManager : public OsAgnosticMemoryManager {
      public:
        MyMemoryManager() { enable64kbpages = true; }
        GraphicsAllocation *allocateGraphicsMemory64kb(size_t size, size_t alignment, bool forcePin, bool preferRenderCompressed) override {
            preferRenderCompressedFlag = preferRenderCompressed;
            return nullptr;
        }
        bool preferRenderCompressedFlag = true;
    };

    MyMemoryManager myMemoryManager;
    myMemoryManager.allocateGraphicsMemoryForSVM(1, false);
    EXPECT_FALSE(myMemoryManager.preferRenderCompressedFlag);
}
