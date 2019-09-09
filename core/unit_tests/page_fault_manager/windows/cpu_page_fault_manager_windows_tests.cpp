/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/page_fault_manager/windows/cpu_page_fault_manager_windows.h"
#include "core/unit_tests/page_fault_manager/mock_cpu_page_fault_manager.h"

#include "gtest/gtest.h"
#include <Windows.h>

using namespace NEO;
using MockPageFaultManagerWindows = MockPageFaultManagerHandlerInvoke<PageFaultManagerWindows>;

bool raiseException(NTSTATUS type) {
    __try {
        RaiseException(type, 0, 0, NULL);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return true;
    }
    return false;
}

TEST(PageFaultManagerWindowsTest, whenPageFaultIsRaisedThenHandlerIsInvoked) {
    auto pageFaultManager = std::make_unique<MockPageFaultManagerWindows>();
    EXPECT_FALSE(pageFaultManager->handlerInvoked);
    EXPECT_FALSE(raiseException(EXCEPTION_ACCESS_VIOLATION));
    EXPECT_TRUE(pageFaultManager->handlerInvoked);
}

TEST(PageFaultManagerWindowsTest, whenExceptionAccessViolationIsRaisedButPointerIsNotKnownThenExceptionIsRethrown) {
    auto pageFaultManager = std::make_unique<MockPageFaultManagerWindows>();
    EXPECT_FALSE(pageFaultManager->handlerInvoked);
    pageFaultManager->returnStatus = false;
    EXPECT_TRUE(raiseException(EXCEPTION_ACCESS_VIOLATION));
    EXPECT_TRUE(pageFaultManager->handlerInvoked);
}

TEST(PageFaultManagerWindowsTest, whenExceptionOtherThanAccessViolationIsRaisedThenExceptionIsRethrownAndHandlerIsNotInvoked) {
    auto pageFaultManager = std::make_unique<MockPageFaultManagerWindows>();
    EXPECT_FALSE(pageFaultManager->handlerInvoked);
    EXPECT_TRUE(raiseException(EXCEPTION_DATATYPE_MISALIGNMENT));
    EXPECT_FALSE(pageFaultManager->handlerInvoked);
}

TEST(PageFaultManagerWindowsTest, givenProtectedMemoryWhenTryingToAccessThenPageFaultIsRaisedAndMemoryIsAccessibleAfterHandling) {
    auto pageFaultManager = std::make_unique<MockPageFaultManagerWindows>();
    pageFaultManager->allowCPUMemoryAccessOnPageFault = true;
    auto ptr = static_cast<int *>(VirtualAlloc(nullptr, pageFaultManager->size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));

    EXPECT_FALSE(pageFaultManager->handlerInvoked);
    ptr[0] = 10;
    EXPECT_FALSE(pageFaultManager->handlerInvoked);

    pageFaultManager->protectCPUMemoryAccess(ptr, pageFaultManager->size);

    EXPECT_FALSE(pageFaultManager->handlerInvoked);
    ptr[0] = 10;
    EXPECT_TRUE(pageFaultManager->handlerInvoked);
    EXPECT_EQ(ptr[0], 10);
}

class MockFailPageFaultManager : public PageFaultManagerWindows {
  public:
    using PageFaultManagerWindows::PageFaultManagerWindows;

    bool verifyPageFault(void *ptr) override {
        verifyCalled = true;
        return false;
    }

    static LONG NTAPI mockPageFaultHandler(struct _EXCEPTION_POINTERS *exceptionInfo) {
        mockCalled = true;
        return EXCEPTION_CONTINUE_EXECUTION;
    }

    ~MockFailPageFaultManager() override {
        mockCalled = false;
    }
    static bool mockCalled;
    bool verifyCalled = false;
};
bool MockFailPageFaultManager::mockCalled = false;

TEST(PageFaultManagerWindowsTest, givenPageFaultThatNEOShouldNotHandleThenDefaultHandlerIsCalled) {
    auto previousHandler = AddVectoredExceptionHandler(1, MockFailPageFaultManager::mockPageFaultHandler);

    MockFailPageFaultManager mockPageFaultManager;
    EXPECT_FALSE(MockFailPageFaultManager::mockCalled);

    RaiseException(EXCEPTION_ACCESS_VIOLATION, 0, 0, NULL);
    EXPECT_TRUE(mockPageFaultManager.verifyCalled);
    EXPECT_TRUE(MockFailPageFaultManager::mockCalled);

    RemoveVectoredExceptionHandler(previousHandler);
}
