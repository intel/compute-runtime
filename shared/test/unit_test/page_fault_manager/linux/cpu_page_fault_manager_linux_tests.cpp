/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/page_fault_manager/linux/cpu_page_fault_manager_linux.h"
#include "shared/test/common/fixtures/cpu_page_fault_manager_tests_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_cpu_page_fault_manager.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_memory_operations_handler.h"

#include "gtest/gtest.h"

#include <csignal>
#include <sys/mman.h>

using namespace NEO;

using PageFaultManagerLinuxTest = PageFaultManagerConfigFixture;
using MockPageFaultManagerLinux = MockPageFaultManagerHandlerInvoke<PageFaultManagerLinux>;

TEST_F(PageFaultManagerLinuxTest, whenPageFaultIsRaisedWithFaultHandlerRegisteredThenHandlerIsInvoked) {
    auto pageFaultManager = std::make_unique<MockPageFaultManagerLinux>();
    EXPECT_FALSE(pageFaultManager->handlerInvoked);
    std::raise(SIGSEGV);
    EXPECT_TRUE(pageFaultManager->handlerInvoked);
}

TEST_F(PageFaultManagerLinuxTest, givenProtectedMemoryWithFaultHandlerRegisteredWhenTryingToAccessThenPageFaultIsRaisedAndMemoryIsAccessibleAfterHandling) {
    auto pageFaultManager = std::make_unique<MockPageFaultManagerLinux>();
    pageFaultManager->allowCPUMemoryAccessOnPageFault = true;
    auto ptr = static_cast<int *>(mmap(nullptr, pageFaultManager->size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0));

    EXPECT_FALSE(pageFaultManager->handlerInvoked);
    ptr[0] = 10;
    EXPECT_FALSE(pageFaultManager->handlerInvoked);

    pageFaultManager->protectCPUMemoryAccess(ptr, pageFaultManager->size);

    EXPECT_FALSE(pageFaultManager->handlerInvoked);
    ptr[0] = 10;
    std::atomic_thread_fence(std::memory_order_seq_cst);
    EXPECT_TRUE(pageFaultManager->handlerInvoked);
    EXPECT_EQ(ptr[0], 10);
}

class MockFailPageFaultManager : public PageFaultManagerLinux {
  public:
    using PageFaultManagerLinux::callPreviousHandler;
    using PageFaultManagerLinux::checkFaultHandlerFromPageFaultManager;
    using PageFaultManagerLinux::PageFaultManagerLinux;
    using PageFaultManagerLinux::previousHandlerRestored;
    using PageFaultManagerLinux::previousPageFaultHandlers;
    using PageFaultManagerLinux::registerFaultHandler;

    bool verifyAndHandlePageFault(void *ptr, bool handlePageFault) override {
        verifyCalled = true;
        if (handlePageFault) {
            numPageFaultHandled++;
        }
        return false;
    }

    static void mockPageFaultHandler(int signal, siginfo_t *info, void *context) {
        mockCalled = true;
    }

    static void mockPageFaultHandler2(int signal, siginfo_t *info, void *context) {
        mockCalled2 = true;
        pageFaultHandlerWrapper(signal, info, context);
    }

    static void mockPageFaultSimpleHandler(int signal) {
        simpleMockCalled = true;
    }

    static void mockPageFaultSimpleHandler2(int signal) {
        simpleMockCalled2 = true;
    }

    ~MockFailPageFaultManager() override {
        mockCalled = false;
        mockCalled2 = false;
        simpleMockCalled = false;
        simpleMockCalled2 = false;
    }

    static bool mockCalled;
    static bool mockCalled2;
    static bool simpleMockCalled;
    static bool simpleMockCalled2;
    bool verifyCalled = false;
    int numPageFaultHandled = 0;
};

bool MockFailPageFaultManager::mockCalled = false;
bool MockFailPageFaultManager::mockCalled2 = false;
bool MockFailPageFaultManager::simpleMockCalled = false;
bool MockFailPageFaultManager::simpleMockCalled2 = false;

TEST_F(PageFaultManagerLinuxTest, givenPageFaultThatNEOShouldNotHandleAndSigInfoFlagSetThenSaSigactionIsCalled) {
    struct sigaction previousHandler = {};
    struct sigaction mockHandler = {};
    mockHandler.sa_flags = SA_SIGINFO;
    mockHandler.sa_sigaction = MockFailPageFaultManager::mockPageFaultHandler;
    auto retVal = sigaction(SIGSEGV, &mockHandler, &previousHandler);
    EXPECT_EQ(retVal, 0);

    auto mockPageFaultManager = std::make_unique<MockFailPageFaultManager>();
    EXPECT_FALSE(MockFailPageFaultManager::mockCalled);
    EXPECT_FALSE(MockFailPageFaultManager::simpleMockCalled);

    std::raise(SIGSEGV);
    EXPECT_TRUE(mockPageFaultManager->verifyCalled);
    EXPECT_TRUE(MockFailPageFaultManager::mockCalled);
    EXPECT_FALSE(MockFailPageFaultManager::simpleMockCalled);

    mockPageFaultManager.reset();
    sigaction(SIGSEGV, &previousHandler, nullptr);
}

TEST_F(PageFaultManagerLinuxTest, givenPageFaultThatNEOShouldNotHandleThenSaHandlerIsCalled) {
    struct sigaction previousHandler = {};
    struct sigaction mockHandler = {};
    mockHandler.sa_handler = MockFailPageFaultManager::mockPageFaultSimpleHandler;
    auto retVal = sigaction(SIGSEGV, &mockHandler, &previousHandler);
    EXPECT_EQ(retVal, 0);

    auto mockPageFaultManager = std::make_unique<MockFailPageFaultManager>();
    EXPECT_FALSE(MockFailPageFaultManager::mockCalled);
    EXPECT_FALSE(MockFailPageFaultManager::simpleMockCalled);
    EXPECT_EQ(1ul, mockPageFaultManager->previousPageFaultHandlers.size());

    std::raise(SIGSEGV);
    EXPECT_TRUE(mockPageFaultManager->verifyCalled);
    EXPECT_FALSE(MockFailPageFaultManager::mockCalled);
    EXPECT_TRUE(MockFailPageFaultManager::simpleMockCalled);

    mockPageFaultManager.reset();
    sigaction(SIGSEGV, &previousHandler, nullptr);
}

TEST_F(PageFaultManagerLinuxTest, givenDefaultSaHandlerWhenInvokeCallPreviousSaHandlerThenPreviousHandlerIsRestored) {
    struct sigaction originalHandler = {};
    struct sigaction mockDefaultHandler = {};
    mockDefaultHandler.sa_handler = SIG_DFL;
    auto retVal = sigaction(SIGSEGV, &mockDefaultHandler, &originalHandler);
    EXPECT_EQ(retVal, 0);

    auto mockPageFaultManager = std::make_unique<MockFailPageFaultManager>();
    mockPageFaultManager->callPreviousHandler(0, nullptr, nullptr);

    EXPECT_TRUE(mockPageFaultManager->previousHandlerRestored);

    mockPageFaultManager.reset();
    sigaction(SIGSEGV, &originalHandler, nullptr);
}

TEST_F(PageFaultManagerLinuxTest, givenIgnoringSaHandlerWithFaultHanderRegisteredWhenInvokeCallPreviousSaHandlerThenNothingHappend) {
    struct sigaction originalHandler = {};
    struct sigaction mockDefaultHandler = {};
    mockDefaultHandler.sa_handler = SIG_IGN;
    auto retVal = sigaction(SIGSEGV, &mockDefaultHandler, &originalHandler);
    EXPECT_EQ(retVal, 0);

    auto mockPageFaultManager = std::make_unique<MockFailPageFaultManager>();
    mockPageFaultManager->callPreviousHandler(0, nullptr, nullptr);

    EXPECT_FALSE(mockPageFaultManager->previousHandlerRestored);

    mockPageFaultManager.reset();
    sigaction(SIGSEGV, &originalHandler, nullptr);
}

TEST_F(PageFaultManagerLinuxTest, givenDefaultSaHandlerWhenOverwritingNewHandlersThenCheckFaultHandlerFromPageFaultManagerReturnsFalse) {
    struct sigaction originalHandler = {};
    struct sigaction mockDefaultHandler = {};
    mockDefaultHandler.sa_handler = SIG_DFL;
    auto retVal = sigaction(SIGSEGV, &mockDefaultHandler, &originalHandler);
    EXPECT_EQ(retVal, 0);

    auto mockPageFaultManager = std::make_unique<MockFailPageFaultManager>();
    EXPECT_TRUE(mockPageFaultManager->checkFaultHandlerFromPageFaultManager());

    retVal = sigaction(SIGSEGV, &mockDefaultHandler, nullptr);
    EXPECT_EQ(retVal, 0);
    EXPECT_FALSE(mockPageFaultManager->checkFaultHandlerFromPageFaultManager());

    mockPageFaultManager.reset();
    sigaction(SIGSEGV, &originalHandler, nullptr);
}

TEST_F(PageFaultManagerLinuxTest, givenPageFaultThatNEOShouldNotHandleWhenRegisterSimpleHandlerTwiceThenSimpleHandlerIsRegisteredOnce) {
    struct sigaction previousHandler = {};
    struct sigaction previousHandler2 = {};
    struct sigaction mockHandler = {};
    struct sigaction mockHandler2 = {};
    mockHandler.sa_handler = MockFailPageFaultManager::mockPageFaultSimpleHandler;
    auto retVal = sigaction(SIGSEGV, &mockHandler, &previousHandler);
    EXPECT_EQ(retVal, 0);

    auto mockPageFaultManager = std::make_unique<MockFailPageFaultManager>();
    EXPECT_FALSE(MockFailPageFaultManager::mockCalled);
    EXPECT_FALSE(MockFailPageFaultManager::simpleMockCalled);
    EXPECT_EQ(1ul, mockPageFaultManager->previousPageFaultHandlers.size());

    std::raise(SIGSEGV);
    EXPECT_TRUE(mockPageFaultManager->verifyCalled);
    EXPECT_EQ(1, mockPageFaultManager->numPageFaultHandled);
    EXPECT_FALSE(MockFailPageFaultManager::mockCalled);
    EXPECT_TRUE(MockFailPageFaultManager::simpleMockCalled);

    MockFailPageFaultManager::simpleMockCalled = false;

    mockHandler2.sa_handler = MockFailPageFaultManager::mockPageFaultSimpleHandler;
    retVal = sigaction(SIGSEGV, &mockHandler2, &previousHandler2);
    EXPECT_EQ(retVal, 0);
    mockPageFaultManager->registerFaultHandler();

    std::raise(SIGSEGV);
    EXPECT_TRUE(mockPageFaultManager->verifyCalled);
    EXPECT_EQ(2, mockPageFaultManager->numPageFaultHandled);
    EXPECT_FALSE(MockFailPageFaultManager::mockCalled);
    EXPECT_TRUE(MockFailPageFaultManager::simpleMockCalled);
    EXPECT_EQ(1ul, mockPageFaultManager->previousPageFaultHandlers.size());

    mockPageFaultManager.reset();
    sigaction(SIGSEGV, &previousHandler, nullptr);
}

TEST_F(PageFaultManagerLinuxTest, givenPageFaultThatNEOShouldNotHandleWhenRegisterTwoSimpleHandlersThenBothHandlersAreRegistered) {
    struct sigaction previousHandler = {};
    struct sigaction previousHandler2 = {};
    struct sigaction mockHandler = {};
    struct sigaction mockHandler2 = {};
    mockHandler.sa_handler = MockFailPageFaultManager::mockPageFaultSimpleHandler;
    auto retVal = sigaction(SIGSEGV, &mockHandler, &previousHandler);
    EXPECT_EQ(retVal, 0);

    auto mockPageFaultManager = std::make_unique<MockFailPageFaultManager>();
    EXPECT_FALSE(MockFailPageFaultManager::mockCalled);
    EXPECT_FALSE(MockFailPageFaultManager::mockCalled2);
    EXPECT_FALSE(MockFailPageFaultManager::simpleMockCalled);
    EXPECT_FALSE(MockFailPageFaultManager::simpleMockCalled2);
    EXPECT_EQ(1ul, mockPageFaultManager->previousPageFaultHandlers.size());

    std::raise(SIGSEGV);
    EXPECT_TRUE(mockPageFaultManager->verifyCalled);
    EXPECT_FALSE(MockFailPageFaultManager::mockCalled);
    EXPECT_FALSE(MockFailPageFaultManager::mockCalled2);
    EXPECT_TRUE(MockFailPageFaultManager::simpleMockCalled);
    EXPECT_FALSE(MockFailPageFaultManager::simpleMockCalled2);

    MockFailPageFaultManager::simpleMockCalled = false;

    mockHandler2.sa_handler = MockFailPageFaultManager::mockPageFaultSimpleHandler2;
    retVal = sigaction(SIGSEGV, &mockHandler2, &previousHandler2);
    EXPECT_EQ(retVal, 0);
    mockPageFaultManager->registerFaultHandler();

    std::raise(SIGSEGV);
    EXPECT_TRUE(mockPageFaultManager->verifyCalled);
    EXPECT_FALSE(MockFailPageFaultManager::mockCalled);
    EXPECT_FALSE(MockFailPageFaultManager::mockCalled2);
    EXPECT_FALSE(MockFailPageFaultManager::simpleMockCalled);
    EXPECT_TRUE(MockFailPageFaultManager::simpleMockCalled2);
    EXPECT_EQ(2ul, mockPageFaultManager->previousPageFaultHandlers.size());

    mockPageFaultManager.reset();
    sigaction(SIGSEGV, &previousHandler, nullptr);
}

TEST_F(PageFaultManagerLinuxTest, givenPageFaultThatNEOShouldNotHandleWhenRegisterTwoHandlersThenBothHandlersAreRegistered) {
    struct sigaction previousHandler = {};
    struct sigaction previousHandler2 = {};
    struct sigaction mockHandler = {};
    struct sigaction mockHandler2 = {};
    mockHandler.sa_flags = SA_SIGINFO;
    mockHandler.sa_sigaction = MockFailPageFaultManager::mockPageFaultHandler;
    auto retVal = sigaction(SIGSEGV, &mockHandler, &previousHandler);
    EXPECT_EQ(retVal, 0);

    auto mockPageFaultManager = std::make_unique<MockFailPageFaultManager>();
    EXPECT_FALSE(MockFailPageFaultManager::mockCalled);
    EXPECT_FALSE(MockFailPageFaultManager::mockCalled2);
    EXPECT_FALSE(MockFailPageFaultManager::simpleMockCalled);
    EXPECT_FALSE(MockFailPageFaultManager::simpleMockCalled2);
    EXPECT_EQ(1ul, mockPageFaultManager->previousPageFaultHandlers.size());

    std::raise(SIGSEGV);
    EXPECT_TRUE(mockPageFaultManager->verifyCalled);
    EXPECT_TRUE(MockFailPageFaultManager::mockCalled);
    EXPECT_FALSE(MockFailPageFaultManager::mockCalled2);
    EXPECT_FALSE(MockFailPageFaultManager::simpleMockCalled);
    EXPECT_FALSE(MockFailPageFaultManager::simpleMockCalled2);

    MockFailPageFaultManager::mockCalled = false;

    mockHandler2.sa_flags = SA_SIGINFO;
    mockHandler2.sa_sigaction = MockFailPageFaultManager::mockPageFaultHandler2;
    retVal = sigaction(SIGSEGV, &mockHandler2, &previousHandler2);
    EXPECT_EQ(retVal, 0);
    mockPageFaultManager->registerFaultHandler();

    std::raise(SIGSEGV);
    EXPECT_TRUE(mockPageFaultManager->verifyCalled);
    EXPECT_TRUE(MockFailPageFaultManager::mockCalled);
    EXPECT_TRUE(MockFailPageFaultManager::mockCalled2);
    EXPECT_FALSE(MockFailPageFaultManager::simpleMockCalled);
    EXPECT_FALSE(MockFailPageFaultManager::simpleMockCalled2);
    EXPECT_EQ(2ul, mockPageFaultManager->previousPageFaultHandlers.size());

    mockPageFaultManager.reset();
    sigaction(SIGSEGV, &previousHandler, nullptr);
}

TEST_F(PageFaultManagerLinuxTest, givenPageFaultThatNEOShouldNotHandleWhenRegisterSimpleAndRegularHandlersThenBothHandlersAreRegistered) {
    struct sigaction previousHandler = {};
    struct sigaction previousHandler2 = {};
    struct sigaction mockHandler = {};
    struct sigaction mockHandler2 = {};
    mockHandler.sa_handler = MockFailPageFaultManager::mockPageFaultSimpleHandler;
    auto retVal = sigaction(SIGSEGV, &mockHandler, &previousHandler);
    EXPECT_EQ(retVal, 0);

    auto mockPageFaultManager = std::make_unique<MockFailPageFaultManager>();
    EXPECT_FALSE(MockFailPageFaultManager::mockCalled);
    EXPECT_FALSE(MockFailPageFaultManager::mockCalled2);
    EXPECT_FALSE(MockFailPageFaultManager::simpleMockCalled);
    EXPECT_FALSE(MockFailPageFaultManager::simpleMockCalled2);
    EXPECT_EQ(1ul, mockPageFaultManager->previousPageFaultHandlers.size());

    std::raise(SIGSEGV);
    EXPECT_TRUE(mockPageFaultManager->verifyCalled);
    EXPECT_FALSE(MockFailPageFaultManager::mockCalled);
    EXPECT_FALSE(MockFailPageFaultManager::mockCalled2);
    EXPECT_TRUE(MockFailPageFaultManager::simpleMockCalled);
    EXPECT_FALSE(MockFailPageFaultManager::simpleMockCalled2);

    MockFailPageFaultManager::simpleMockCalled = false;

    mockHandler2.sa_flags = SA_SIGINFO;
    mockHandler2.sa_sigaction = MockFailPageFaultManager::mockPageFaultHandler2;
    retVal = sigaction(SIGSEGV, &mockHandler2, &previousHandler2);
    EXPECT_EQ(retVal, 0);
    mockPageFaultManager->registerFaultHandler();

    std::raise(SIGSEGV);
    EXPECT_TRUE(mockPageFaultManager->verifyCalled);
    EXPECT_FALSE(MockFailPageFaultManager::mockCalled);
    EXPECT_TRUE(MockFailPageFaultManager::mockCalled2);
    EXPECT_TRUE(MockFailPageFaultManager::simpleMockCalled);
    EXPECT_FALSE(MockFailPageFaultManager::simpleMockCalled2);
    EXPECT_EQ(2ul, mockPageFaultManager->previousPageFaultHandlers.size());

    mockPageFaultManager.reset();
    sigaction(SIGSEGV, &previousHandler, nullptr);
}
