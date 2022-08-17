/*
 * Copyright (C) 2019-2022 Intel Corporation
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

TEST_F(PageFaultManagerLinuxTest, whenPageFaultIsRaisedThenHandlerIsInvoked) {
    auto pageFaultManager = std::make_unique<MockPageFaultManagerLinux>();
    EXPECT_FALSE(pageFaultManager->handlerInvoked);
    std::raise(SIGSEGV);
    EXPECT_TRUE(pageFaultManager->handlerInvoked);
}

struct MockOperationsInterface : public MockMemoryOperationsHandler {
    bool evictCalled = false;
    MemoryOperationsStatus evict(Device *device, GraphicsAllocation &gfxAllocation) override {
        this->evictCalled = true;
        return MemoryOperationsStatus::UNSUPPORTED;
    }
};

TEST_F(PageFaultManagerLinuxTest, givenDirectSubmissionAndUSMEvictWaEnabledWhenEvitMemoryAfterCopyThenMemoryOperationsHandlerEvictMethodIsCalled) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableDirectSubmission.set(true);
    DebugManager.flags.USMEvictAfterMigration.set(true);

    auto pageFaultManager = std::make_unique<MockPageFaultManagerLinux>();
    std::unique_ptr<Device> device(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    device->getExecutionEnvironment()->rootDeviceEnvironments[0u]->memoryOperationsInterface = std::make_unique<MockOperationsInterface>();
    auto operationInterface = static_cast<MockOperationsInterface *>(device->getExecutionEnvironment()->rootDeviceEnvironments[0u]->memoryOperationsInterface.get());
    MockGraphicsAllocation allocation;

    EXPECT_FALSE(operationInterface->evictCalled);
    pageFaultManager->evictMemoryAfterImplCopy(&allocation, device.get());
    EXPECT_TRUE(operationInterface->evictCalled);
}

TEST_F(PageFaultManagerLinuxTest, givenDirectSubmissionEnabledAndUSMEvictWaDisabledWhenEvitMemoryAfterCopyThenMemoryOperationsHandlerEvictMethodIsNotCalled) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableDirectSubmission.set(true);
    DebugManager.flags.USMEvictAfterMigration.set(false);

    auto pageFaultManager = std::make_unique<MockPageFaultManagerLinux>();
    std::unique_ptr<Device> device(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    device->getExecutionEnvironment()->rootDeviceEnvironments[0u]->memoryOperationsInterface = std::make_unique<MockOperationsInterface>();
    auto operationInterface = static_cast<MockOperationsInterface *>(device->getExecutionEnvironment()->rootDeviceEnvironments[0u]->memoryOperationsInterface.get());
    MockGraphicsAllocation allocation;

    EXPECT_FALSE(operationInterface->evictCalled);
    pageFaultManager->evictMemoryAfterImplCopy(&allocation, device.get());
    EXPECT_FALSE(operationInterface->evictCalled);
}

TEST_F(PageFaultManagerLinuxTest, givenDirectSubmissionAndUSMEvictWaDisabledWhenEvitMemoryAfterCopyThenMemoryOperationsHandlerEvictMethodIsNotCalled) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableDirectSubmission.set(false);
    DebugManager.flags.USMEvictAfterMigration.set(false);

    auto pageFaultManager = std::make_unique<MockPageFaultManagerLinux>();
    std::unique_ptr<Device> device(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    device->getExecutionEnvironment()->rootDeviceEnvironments[0u]->memoryOperationsInterface = std::make_unique<MockOperationsInterface>();
    auto operationInterface = static_cast<MockOperationsInterface *>(device->getExecutionEnvironment()->rootDeviceEnvironments[0u]->memoryOperationsInterface.get());
    MockGraphicsAllocation allocation;

    EXPECT_FALSE(operationInterface->evictCalled);
    pageFaultManager->evictMemoryAfterImplCopy(&allocation, device.get());
    EXPECT_FALSE(operationInterface->evictCalled);
}

TEST_F(PageFaultManagerLinuxTest, givenDirectSubmissionDisabledAndUSMEvictWaEnabledWhenEvitMemoryAfterCopyThenMemoryOperationsHandlerEvictMethodIsNotCalled) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableDirectSubmission.set(false);
    DebugManager.flags.USMEvictAfterMigration.set(true);

    auto pageFaultManager = std::make_unique<MockPageFaultManagerLinux>();
    std::unique_ptr<Device> device(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    device->getExecutionEnvironment()->rootDeviceEnvironments[0u]->memoryOperationsInterface = std::make_unique<MockOperationsInterface>();
    auto operationInterface = static_cast<MockOperationsInterface *>(device->getExecutionEnvironment()->rootDeviceEnvironments[0u]->memoryOperationsInterface.get());
    MockGraphicsAllocation allocation;

    EXPECT_FALSE(operationInterface->evictCalled);
    pageFaultManager->evictMemoryAfterImplCopy(&allocation, device.get());
    EXPECT_FALSE(operationInterface->evictCalled);
}

TEST_F(PageFaultManagerLinuxTest, givenProtectedMemoryWhenTryingToAccessThenPageFaultIsRaisedAndMemoryIsAccessibleAfterHandling) {
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
    using PageFaultManagerLinux::callPreviousHandler;
    using PageFaultManagerLinux::PageFaultManagerLinux;
    using PageFaultManagerLinux::previousHandlerRestored;

    bool verifyPageFault(void *ptr) override {
        verifyCalled = true;
        return false;
    }

    static void mockPageFaultHandler(int signal, siginfo_t *info, void *context) {
        mockCalled = true;
    }

    static void mockPageFaultSimpleHandler(int signal) {
        simpleMockCalled = true;
    }

    ~MockFailPageFaultManager() override {
        mockCalled = false;
        simpleMockCalled = false;
    }

    static bool mockCalled;
    static bool simpleMockCalled;
    bool verifyCalled = false;
};

bool MockFailPageFaultManager::mockCalled = false;
bool MockFailPageFaultManager::simpleMockCalled = false;

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

TEST_F(PageFaultManagerLinuxTest, givenIgnoringSaHandlerWhenInvokeCallPreviousSaHandlerThenNothingHappend) {
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
