/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/windows/os_context_win.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"
#include "shared/source/os_interface/windows/windows_wrapper.h"
#include "shared/source/page_fault_manager/windows/cpu_page_fault_manager_windows.h"
#include "shared/test/common/fixtures/cpu_page_fault_manager_tests_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/mocks/mock_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_cpu_page_fault_manager.h"

#include "gtest/gtest.h"

using namespace NEO;

using PageFaultManagerWindowsTest = PageFaultManagerConfigFixture;
using MockPageFaultManagerWindows = MockPageFaultManagerHandlerInvoke<PageFaultManagerWindows>;

bool raiseException(NTSTATUS type) {
    __try {
        RaiseException(type, 0, 0, NULL);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return true;
    }
    return false;
}

TEST_F(PageFaultManagerWindowsTest, whenPageFaultIsRaisedThenHandlerIsInvoked) {
    auto pageFaultManager = std::make_unique<MockPageFaultManagerWindows>();
    EXPECT_FALSE(pageFaultManager->handlerInvoked);
    EXPECT_FALSE(raiseException(EXCEPTION_ACCESS_VIOLATION));
    EXPECT_TRUE(pageFaultManager->handlerInvoked);
}

TEST_F(PageFaultManagerWindowsTest, whenExceptionAccessViolationIsRaisedButPointerIsNotKnownThenExceptionIsRethrown) {
    auto pageFaultManager = std::make_unique<MockPageFaultManagerWindows>();
    EXPECT_FALSE(pageFaultManager->handlerInvoked);
    pageFaultManager->returnStatus = false;
    EXPECT_TRUE(raiseException(EXCEPTION_ACCESS_VIOLATION));
    EXPECT_TRUE(pageFaultManager->handlerInvoked);
}

TEST_F(PageFaultManagerWindowsTest, whenExceptionOtherThanAccessViolationIsRaisedThenExceptionIsRethrownAndHandlerIsNotInvoked) {
    auto pageFaultManager = std::make_unique<MockPageFaultManagerWindows>();
    EXPECT_FALSE(pageFaultManager->handlerInvoked);
    EXPECT_TRUE(raiseException(EXCEPTION_DATATYPE_MISALIGNMENT));
    EXPECT_FALSE(pageFaultManager->handlerInvoked);
}

TEST_F(PageFaultManagerWindowsTest, givenProtectedMemoryWhenTryingToAccessThenPageFaultIsRaisedAndMemoryIsAccessibleAfterHandling) {
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
    using PageFaultManagerWindows::checkFaultHandlerFromPageFaultManager;
    using PageFaultManagerWindows::PageFaultManagerWindows;

    bool verifyAndHandlePageFault(void *ptr, bool handlePageFault) override {
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

TEST_F(PageFaultManagerWindowsTest, givenPageFaultThatNEOShouldNotHandleThenDefaultHandlerIsCalled) {
    auto previousHandler = AddVectoredExceptionHandler(1, MockFailPageFaultManager::mockPageFaultHandler);

    MockFailPageFaultManager mockPageFaultManager;
    EXPECT_FALSE(MockFailPageFaultManager::mockCalled);

    RaiseException(EXCEPTION_ACCESS_VIOLATION, 0, 0, NULL);
    EXPECT_TRUE(mockPageFaultManager.verifyCalled);
    EXPECT_TRUE(MockFailPageFaultManager::mockCalled);

    RemoveVectoredExceptionHandler(previousHandler);
}

TEST_F(PageFaultManagerWindowsTest,
       givenDefaultSaHandlerWhenPageFaultManagerIsCreatedThenCheckFaultHandlerFromPageFaultManagerReturnsTrue) {
    auto previousHandler = AddVectoredExceptionHandler(1, MockFailPageFaultManager::mockPageFaultHandler);

    MockFailPageFaultManager mockPageFaultManager;
    EXPECT_FALSE(MockFailPageFaultManager::mockCalled);

    EXPECT_TRUE(mockPageFaultManager.checkFaultHandlerFromPageFaultManager());

    RaiseException(EXCEPTION_ACCESS_VIOLATION, 0, 0, NULL);
    EXPECT_TRUE(mockPageFaultManager.verifyCalled);
    EXPECT_TRUE(MockFailPageFaultManager::mockCalled);
    EXPECT_TRUE(mockPageFaultManager.checkFaultHandlerFromPageFaultManager());

    RemoveVectoredExceptionHandler(previousHandler);
}

TEST_F(PageFaultManagerTest,
       givenDefaultSaHandlerWhenCPUMemoryEvictionIsCalledThenAllocAddedToEvictionListOnlyOnce) {
    DebugManagerStateRestore restore;
    debugManager.flags.AllocateSharedAllocationsWithCpuAndGpuStorage.set(true);
    executionEnvironment.memoryManager.reset(memoryManager.release());
    auto rootDeviceEnvironment = executionEnvironment.rootDeviceEnvironments[0].get();
    auto wddm = std::unique_ptr<Wddm>(Wddm::createWddm(nullptr, *rootDeviceEnvironment));
    auto osContext = std::make_unique<OsContextWin>(*wddm, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    auto csr = std::make_unique<MockCommandStreamReceiver>(executionEnvironment, 0, 1);
    csr->setupContext(*osContext);
    auto unifiedMemoryManager = std::make_unique<SVMAllocsManager>(executionEnvironment.memoryManager.get());
    auto pageFaultManager = std::make_unique<MockPageFaultManagerWindows>();

    OSInterface osInterface;
    RootDeviceIndicesContainer rootDeviceIndices = {0};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{0, 0b1}};
    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::sharedUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
    auto ptr = unifiedMemoryManager->createUnifiedAllocationWithDeviceStorage(4096u, {}, unifiedMemoryProperties);
    void *cmdQ = reinterpret_cast<void *>(0xFFFF);
    pageFaultManager->insertAllocation(ptr, 10, unifiedMemoryManager.get(), cmdQ, {});

    EXPECT_EQ(0u, csr->getEvictionAllocations().size());
    pageFaultManager->allowCPUMemoryEvictionImpl(true, ptr, *csr, &osInterface);
    EXPECT_EQ(1u, csr->getEvictionAllocations().size());

    pageFaultManager->allowCPUMemoryEvictionImpl(true, ptr, *csr, &osInterface);
    EXPECT_EQ(1u, csr->getEvictionAllocations().size());

    pageFaultManager->allowCPUMemoryEvictionImpl(false, ptr, *csr, &osInterface);
    EXPECT_EQ(0u, csr->getEvictionAllocations().size());

    unifiedMemoryManager->freeSVMAlloc(ptr);
}
