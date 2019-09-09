/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/page_fault_manager/linux/cpu_page_fault_manager_linux.h"
#include "core/unit_tests/page_fault_manager/mock_cpu_page_fault_manager.h"

#include "gtest/gtest.h"

#include <csignal>
#include <sys/mman.h>

using namespace NEO;
using MockPageFaultManagerLinux = MockPageFaultManagerHandlerInvoke<PageFaultManagerLinux>;

TEST(PageFaultManagerLinuxTest, whenPageFaultIsRaisedThenHandlerIsInvoked) {
    auto pageFaultManager = std::make_unique<MockPageFaultManagerLinux>();
    EXPECT_FALSE(pageFaultManager->handlerInvoked);
    std::raise(SIGSEGV);
    EXPECT_TRUE(pageFaultManager->handlerInvoked);
}

TEST(PageFaultManagerLinuxTest, givenProtectedMemoryWhenTryingToAccessThenPageFaultIsRaisedAndMemoryIsAccessibleAfterHandling) {
    auto pageFaultManager = std::make_unique<MockPageFaultManagerLinux>();
    pageFaultManager->allowCPUMemoryAccessOnPageFault = true;
    auto ptr = static_cast<int *>(mmap(nullptr, pageFaultManager->size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0));

    EXPECT_FALSE(pageFaultManager->handlerInvoked);
    ptr[0] = 10;
    EXPECT_FALSE(pageFaultManager->handlerInvoked);

    pageFaultManager->protectCPUMemoryAccess(ptr, pageFaultManager->size);

    EXPECT_FALSE(pageFaultManager->handlerInvoked);
    ptr[0] = 10;
    EXPECT_TRUE(pageFaultManager->handlerInvoked);
    EXPECT_EQ(ptr[0], 10);
}

class MockFailPageFaultManager : public PageFaultManagerLinux {
  public:
    using PageFaultManagerLinux::PageFaultManagerLinux;

    bool verifyPageFault(void *ptr) override {
        verifyCalled = true;
        return false;
    }

    static void mockPageFaultHandler(int signal, siginfo_t *info, void *context) {
        mockCalled = true;
    }

    ~MockFailPageFaultManager() override {
        mockCalled = false;
    }
    static bool mockCalled;
    bool verifyCalled = false;
};
bool MockFailPageFaultManager::mockCalled = false;

TEST(PageFaultManagerLinuxTest, givenPageFaultThatNEOShouldNotHandleThenDefaultHandlerIsCalled) {
    struct sigaction previousHandler = {};
    struct sigaction mockHandler = {};
    mockHandler.sa_flags = SA_SIGINFO;
    mockHandler.sa_sigaction = MockFailPageFaultManager::mockPageFaultHandler;
    auto retVal = sigaction(SIGSEGV, &mockHandler, &previousHandler);
    EXPECT_EQ(retVal, 0);

    MockFailPageFaultManager mockPageFaultManager;
    EXPECT_FALSE(MockFailPageFaultManager::mockCalled);

    std::raise(SIGSEGV);
    EXPECT_TRUE(mockPageFaultManager.verifyCalled);
    EXPECT_TRUE(MockFailPageFaultManager::mockCalled);

    sigaction(SIGSEGV, &previousHandler, nullptr);
}
