/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/command_stream/preemption.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/memory_manager/memory_operations_handler.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/windows/os_context_win.h"
#include "shared/source/os_interface/windows/wddm/wddm_interface.h"
#include "shared/source/os_interface/windows/wddm_memory_operations_handler.h"
#include "shared/source/os_interface/windows/wddm_residency_controller.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/libult/create_command_stream.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_wddm.h"
#include "shared/test/common/os_interface/windows/mock_wddm_memory_manager.h"
#include "shared/test/unit_test/os_interface/windows/mock_gdi_interface.h"

#include "opencl/source/platform/platform.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"
#include "opencl/test/unit_test/os_interface/windows/mock_wddm_allocation.h"
#include "test.h"

#include "gmock/gmock.h"

#include <memory>

using namespace NEO;

class MockWddmResidencyController : public WddmResidencyController {
  public:
    using WddmResidencyController::lastTrimFenceValue;
    using WddmResidencyController::trimCallbackHandle;
    using WddmResidencyController::trimCandidateList;
    using WddmResidencyController::trimCandidatesCount;
    using WddmResidencyController::trimResidency;
    using WddmResidencyController::trimResidencyToBudget;
    using WddmResidencyController::WddmResidencyController;

    uint32_t acquireLockCallCount = 0u;
    bool forceTrimCandidateListCompaction = false;

    std::unique_lock<SpinLock> acquireLock() override {
        acquireLockCallCount++;
        return WddmResidencyController::acquireLock();
    }

    bool checkTrimCandidateListCompaction() override {
        return forceTrimCandidateListCompaction || WddmResidencyController::checkTrimCandidateListCompaction();
    }
};

class MockOsContextWin : public OsContextWin {
  public:
    MockOsContextWin(Wddm &wddm, uint32_t contextId, const EngineDescriptor &engineDescriptor)
        : OsContextWin(wddm, contextId, engineDescriptor),
          mockResidencyController(wddm, contextId) {}

    WddmResidencyController &getResidencyController() override { return mockResidencyController; };

    MockWddmResidencyController mockResidencyController;
};

struct WddmResidencyControllerTest : ::testing::Test {
    const uint32_t osContextId = 0u;

    void SetUp() {
        executionEnvironment = std::make_unique<MockExecutionEnvironment>();
        rootDeviceEnvironment = std::make_unique<RootDeviceEnvironment>(*executionEnvironment);
        wddm = static_cast<WddmMock *>(Wddm::createWddm(nullptr, *rootDeviceEnvironment));
        wddm->init();
        mockOsContextWin = std::make_unique<MockOsContextWin>(*wddm, osContextId, EngineDescriptorHelper::getDefaultDescriptor());
        wddm->getWddmInterface()->createMonitoredFence(*mockOsContextWin);
        residencyController = &mockOsContextWin->mockResidencyController;
    }

    std::unique_ptr<MockExecutionEnvironment> executionEnvironment;
    std::unique_ptr<RootDeviceEnvironment> rootDeviceEnvironment;
    WddmMock *wddm = nullptr;
    std::unique_ptr<MockOsContextWin> mockOsContextWin;
    MockWddmResidencyController *residencyController = nullptr;
};

struct WddmResidencyControllerWithGdiTest : ::testing::Test {
    const uint32_t osContextId = 0u;

    void SetUp() {
        executionEnvironment = std::make_unique<MockExecutionEnvironment>();
        rootDeviceEnvironment = std::make_unique<RootDeviceEnvironment>(*executionEnvironment);
        wddm = static_cast<WddmMock *>(Wddm::createWddm(nullptr, *rootDeviceEnvironment));
        gdi = new MockGdi();
        wddm->resetGdi(gdi);
        wddm->init();

        mockOsContextWin = std::make_unique<MockOsContextWin>(*wddm, osContextId, EngineDescriptorHelper::getDefaultDescriptor());
        wddm->getWddmInterface()->createMonitoredFence(*mockOsContextWin);
        residencyController = &mockOsContextWin->mockResidencyController;
        residencyController->registerCallback();
    }

    std::unique_ptr<MockExecutionEnvironment> executionEnvironment;
    std::unique_ptr<RootDeviceEnvironment> rootDeviceEnvironment;
    WddmMock *wddm = nullptr;
    std::unique_ptr<MockOsContextWin> mockOsContextWin;
    MockWddmResidencyController *residencyController = nullptr;
    MockGdi *gdi = nullptr;
};

struct WddmResidencyControllerWithMockWddmTest : public WddmResidencyControllerTest {
    void SetUp() {
        executionEnvironment = platform()->peekExecutionEnvironment();

        wddm = new ::testing::NiceMock<GmockWddm>(*executionEnvironment->rootDeviceEnvironments[0].get());
        wddm->resetGdi(new MockGdi());
        auto preemptionMode = PreemptionHelper::getDefaultPreemptionMode(*defaultHwInfo);
        wddm->init();

        executionEnvironment->rootDeviceEnvironments[0]->memoryOperationsInterface = std::make_unique<WddmMemoryOperationsHandler>(wddm);
        executionEnvironment->initializeMemoryManager();

        memoryManager = std::make_unique<MockWddmMemoryManager>(*executionEnvironment);

        csr.reset(createCommandStream(*executionEnvironment, 0u, 1));
        auto hwInfo = executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo();
        osContext = memoryManager->createAndRegisterOsContext(csr.get(), EngineDescriptorHelper::getDefaultDescriptor(HwHelper::get(hwInfo->platform.eRenderCoreFamily).getGpgpuEngineInstances(*hwInfo)[0],
                                                                                                                      preemptionMode));
        osContext->ensureContextInitialized();

        osContext->incRefInternal();
        residencyController = &static_cast<OsContextWin *>(osContext)->getResidencyController();
        gmmClientContext = executionEnvironment->rootDeviceEnvironments[0]->getGmmClientContext();
    }

    void TearDown() {
        osContext->decRefInternal();
    }

    ExecutionEnvironment *executionEnvironment;
    std::unique_ptr<MockWddmMemoryManager> memoryManager;
    std::unique_ptr<CommandStreamReceiver> csr;
    ::testing::NiceMock<GmockWddm> *wddm = nullptr;
    OsContext *osContext;
    WddmResidencyController *residencyController;
    GmmClientContext *gmmClientContext = nullptr;
};

struct WddmResidencyControllerWithGdiAndMemoryManagerTest : ::testing::Test {
    const uint32_t osContextId = 0u;

    void SetUp() {
        executionEnvironment = platform()->peekExecutionEnvironment();
        wddm = static_cast<WddmMock *>(Wddm::createWddm(nullptr, *executionEnvironment->rootDeviceEnvironments[0].get()));
        wddm->init();
        gdi = new MockGdi();
        wddm->resetGdi(gdi);

        executionEnvironment->rootDeviceEnvironments[0]->memoryOperationsInterface = std::make_unique<WddmMemoryOperationsHandler>(wddm);
        executionEnvironment->initializeMemoryManager();

        memoryManager = std::make_unique<MockWddmMemoryManager>(*executionEnvironment);
        csr.reset(createCommandStream(*executionEnvironment, 0u, 1));
        auto hwInfo = executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo();
        osContext = memoryManager->createAndRegisterOsContext(csr.get(), EngineDescriptorHelper::getDefaultDescriptor(HwHelper::get(hwInfo->platform.eRenderCoreFamily).getGpgpuEngineInstances(*hwInfo)[0],
                                                                                                                      PreemptionHelper::getDefaultPreemptionMode(*defaultHwInfo)));
        osContext->ensureContextInitialized();

        osContext->incRefInternal();

        residencyController = &static_cast<OsContextWin *>(osContext)->getResidencyController();
        gmmClientContext = executionEnvironment->rootDeviceEnvironments[0]->getGmmClientContext();
    }

    void TearDown() {
        osContext->decRefInternal();
    }

    ExecutionEnvironment *executionEnvironment;
    std::unique_ptr<MockWddmMemoryManager> memoryManager;
    std::unique_ptr<CommandStreamReceiver> csr;

    WddmMock *wddm = nullptr;
    OsContext *osContext = nullptr;
    MockGdi *gdi = nullptr;
    WddmResidencyController *residencyController = nullptr;
    GmmClientContext *gmmClientContext = nullptr;
};

TEST(WddmResidencyController, givenWddmResidencyControllerWhenItIsConstructedThenDoNotRegisterTrimCallback) {
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.prepareRootDeviceEnvironments(1);
    auto gdi = new MockGdi();
    auto wddm = static_cast<WddmMock *>(Wddm::createWddm(nullptr, *executionEnvironment.rootDeviceEnvironments[0].get()));
    wddm->resetGdi(gdi);
    wddm->init();

    std::memset(&gdi->getRegisterTrimNotificationArg(), 0, sizeof(D3DKMT_REGISTERTRIMNOTIFICATION));
    MockWddmResidencyController residencyController{*wddm, 0u};

    EXPECT_EQ(0u, wddm->registerTrimCallbackResult.called);
    EXPECT_EQ(nullptr, residencyController.trimCallbackHandle);

    EXPECT_EQ(nullptr, gdi->getRegisterTrimNotificationArg().Callback);
    EXPECT_EQ(nullptr, gdi->getRegisterTrimNotificationArg().Context);
    EXPECT_EQ(0u, gdi->getRegisterTrimNotificationArg().hDevice);
}

TEST(WddmResidencyController, givenWddmResidencyControllerWhenRegisterCallbackThenCallbackIsSetUpProperly) {
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.prepareRootDeviceEnvironments(1);
    auto gdi = new MockGdi();
    auto wddm = static_cast<WddmMock *>(Wddm::createWddm(nullptr, *executionEnvironment.rootDeviceEnvironments[0].get()));
    wddm->resetGdi(gdi);
    wddm->init();

    std::memset(&gdi->getRegisterTrimNotificationArg(), 0, sizeof(D3DKMT_REGISTERTRIMNOTIFICATION));

    WddmResidencyController residencyController{*wddm, 0u};
    residencyController.registerCallback();

    EXPECT_EQ(1u, wddm->registerTrimCallbackResult.called);
    EXPECT_EQ(reinterpret_cast<PFND3DKMT_TRIMNOTIFICATIONCALLBACK>(WddmResidencyController::trimCallback), gdi->getRegisterTrimNotificationArg().Callback);
    EXPECT_EQ(reinterpret_cast<void *>(&residencyController), gdi->getRegisterTrimNotificationArg().Context);
    EXPECT_EQ(wddm->getDeviceHandle(), gdi->getRegisterTrimNotificationArg().hDevice);
}

TEST_F(WddmResidencyControllerTest, givenWddmResidencyControllerWhenCallingWasAllocationUsedSinceLastTrimThenReturnCorrectValues) {
    residencyController->lastTrimFenceValue = 100;
    EXPECT_FALSE(residencyController->wasAllocationUsedSinceLastTrim(99));
    EXPECT_FALSE(residencyController->wasAllocationUsedSinceLastTrim(99));
    EXPECT_TRUE(residencyController->wasAllocationUsedSinceLastTrim(101));
}

TEST_F(WddmResidencyControllerTest, givenWddmResidencyControllerThenUpdateLastTrimFenceValueUsesMonitoredFence) {
    *residencyController->getMonitoredFence().cpuAddress = 1234;
    residencyController->updateLastTrimFenceValue();
    EXPECT_EQ(1234, residencyController->lastTrimFenceValue);

    *residencyController->getMonitoredFence().cpuAddress = 12345;
    residencyController->updateLastTrimFenceValue();
    EXPECT_EQ(12345, residencyController->lastTrimFenceValue);
}

TEST_F(WddmResidencyControllerWithGdiTest, givenWddmResidencyControllerWhenItIsDestructedThenUnregisterTrimCallback) {
    auto trimCallbackHandle = residencyController->trimCallbackHandle;
    auto trimCallbackAddress = reinterpret_cast<PFND3DKMT_TRIMNOTIFICATIONCALLBACK>(WddmResidencyController::trimCallback);

    std::memset(&gdi->getUnregisterTrimNotificationArg(), 0, sizeof(D3DKMT_UNREGISTERTRIMNOTIFICATION));
    mockOsContextWin.reset();

    EXPECT_EQ(trimCallbackAddress, gdi->getUnregisterTrimNotificationArg().Callback);
    EXPECT_EQ(trimCallbackHandle, gdi->getUnregisterTrimNotificationArg().Handle);
}

TEST_F(WddmResidencyControllerWithGdiTest, givenWddmResidencyControllerWhenItIsDestructedDuringProcessShutdownThenDontUnregisterTrimCallback) {
    wddm->shutdownStatus = true;

    std::memset(&gdi->getUnregisterTrimNotificationArg(), 0, sizeof(D3DKMT_UNREGISTERTRIMNOTIFICATION));
    mockOsContextWin.reset();

    EXPECT_EQ(nullptr, gdi->getUnregisterTrimNotificationArg().Callback);
    EXPECT_EQ(nullptr, gdi->getUnregisterTrimNotificationArg().Handle);
}

TEST_F(WddmResidencyControllerTest, givenUsedAllocationWhenCallingRemoveFromTrimCandidateListIfUsedThenRemoveIt) {
    MockWddmAllocation allocation(rootDeviceEnvironment->getGmmClientContext());
    residencyController->addToTrimCandidateList(&allocation);
    residencyController->removeFromTrimCandidateListIfUsed(&allocation, false);
    EXPECT_EQ(trimListUnusedPosition, allocation.getTrimCandidateListPosition(osContextId));
}

TEST_F(WddmResidencyControllerTest, givenWddmResidencyControllerWhenIsMemoryExhaustedIsCalledThenReturnCorrectResult) {
    EXPECT_FALSE(residencyController->isMemoryBudgetExhausted());
    residencyController->setMemoryBudgetExhausted();
    EXPECT_TRUE(residencyController->isMemoryBudgetExhausted());
}

TEST_F(WddmResidencyControllerTest, givenUnusedAllocationWhenCallingRemoveFromTrimCandidateListIfUsedThenIgnore) {
    MockWddmAllocation allocation(rootDeviceEnvironment->getGmmClientContext());
    residencyController->removeFromTrimCandidateListIfUsed(&allocation, false);
    EXPECT_EQ(trimListUnusedPosition, allocation.getTrimCandidateListPosition(osContextId));
}

TEST_F(WddmResidencyControllerTest, WhenAddingToTrimCandidateListThenAllocationIsPlacedInContainerAndAssignedPosition) {
    MockWddmAllocation allocation(rootDeviceEnvironment->getGmmClientContext());
    residencyController->addToTrimCandidateList(&allocation);

    EXPECT_NE(0u, residencyController->trimCandidateList.size());
    EXPECT_NE(trimListUnusedPosition, allocation.getTrimCandidateListPosition(osContextId));

    size_t position = allocation.getTrimCandidateListPosition(osContextId);
    ASSERT_LT(position, residencyController->trimCandidateList.size());

    EXPECT_EQ(&allocation, residencyController->trimCandidateList[position]);
}

TEST_F(WddmResidencyControllerTest, WhenAddingToTrimCandidateListThenDoNotInsertAllocationAlreadyOnTheList) {
    MockWddmAllocation allocation(rootDeviceEnvironment->getGmmClientContext());

    residencyController->trimCandidateList.resize(0);

    residencyController->addToTrimCandidateList(&allocation);

    EXPECT_NE(trimListUnusedPosition, allocation.getTrimCandidateListPosition(osContextId));

    size_t position = allocation.getTrimCandidateListPosition(osContextId);
    ASSERT_LT(position, residencyController->trimCandidateList.size());

    EXPECT_EQ(&allocation, residencyController->trimCandidateList[position]);

    size_t previousSize = residencyController->trimCandidateList.size();
    residencyController->addToTrimCandidateList(&allocation);

    EXPECT_EQ(previousSize, residencyController->trimCandidateList.size());
    EXPECT_EQ(position, allocation.getTrimCandidateListPosition(osContextId));
}

TEST_F(WddmResidencyControllerTest, WhenRemovingFromTrimCandidateListThenUnusedPositionIsAssigned) {
    MockWddmAllocation allocation(rootDeviceEnvironment->getGmmClientContext());

    residencyController->addToTrimCandidateList(&allocation);
    residencyController->removeFromTrimCandidateList(&allocation, false);

    EXPECT_EQ(trimListUnusedPosition, allocation.getTrimCandidateListPosition(osContextId));
}

TEST_F(WddmResidencyControllerTest, WhenRemovingFromTrimCandidateListThenAllocationInAssignedPositionIsRemoved) {
    MockWddmAllocation allocation(rootDeviceEnvironment->getGmmClientContext());

    residencyController->addToTrimCandidateList(&allocation);
    size_t position = allocation.getTrimCandidateListPosition(osContextId);

    residencyController->removeFromTrimCandidateList(&allocation, false);

    if (residencyController->trimCandidateList.size() > position) {
        EXPECT_NE(&allocation, residencyController->trimCandidateList[position]);
    }
}

TEST_F(WddmResidencyControllerTest, GivenOneAllocationWhenRemovingFromTrimCandidateListThenTrimCandidateListIsEmpty) {
    MockWddmAllocation allocation(rootDeviceEnvironment->getGmmClientContext());

    residencyController->trimCandidateList.resize(0);

    residencyController->addToTrimCandidateList(&allocation);

    residencyController->removeFromTrimCandidateList(&allocation, false);

    EXPECT_EQ(0u, residencyController->trimCandidateList.size());
}

TEST_F(WddmResidencyControllerTest, WhenRemovingFromTrimCandidateListThenLastAllocationAndAllPreviousEmptyEntriesAreRemoved) {
    MockWddmAllocation allocation1(rootDeviceEnvironment->getGmmClientContext());
    MockWddmAllocation allocation2(rootDeviceEnvironment->getGmmClientContext());

    residencyController->trimCandidateList.resize(0);

    residencyController->addToTrimCandidateList(&allocation1);

    residencyController->trimCandidateList.push_back(nullptr);
    residencyController->trimCandidateList.push_back(nullptr);
    residencyController->trimCandidateList.push_back(nullptr);

    residencyController->addToTrimCandidateList(&allocation2);

    EXPECT_EQ(5u, residencyController->trimCandidateList.size());

    residencyController->removeFromTrimCandidateList(&allocation2, false);

    EXPECT_EQ(1u, residencyController->trimCandidateList.size());
}

TEST_F(WddmResidencyControllerTest, WhenAddingToTrimCandidateListThenSuccessivePositionIsAssigned) {
    MockWddmAllocation allocation1(rootDeviceEnvironment->getGmmClientContext());
    MockWddmAllocation allocation2(rootDeviceEnvironment->getGmmClientContext());
    MockWddmAllocation allocation3(rootDeviceEnvironment->getGmmClientContext());

    residencyController->addToTrimCandidateList(&allocation1);
    residencyController->addToTrimCandidateList(&allocation2);
    residencyController->addToTrimCandidateList(&allocation3);

    EXPECT_EQ(3u, residencyController->trimCandidateList.size());
    EXPECT_NE(allocation1.getTrimCandidateListPosition(osContextId), allocation2.getTrimCandidateListPosition(osContextId));
    EXPECT_NE(allocation2.getTrimCandidateListPosition(osContextId), allocation3.getTrimCandidateListPosition(osContextId));
}

TEST_F(WddmResidencyControllerTest, GivenAllocationThatIsNotLastWhenRemovingFromTrimCandidateListAndCompactingThenRemoveEntry) {
    MockWddmAllocation allocation1(rootDeviceEnvironment->getGmmClientContext());
    MockWddmAllocation allocation2(rootDeviceEnvironment->getGmmClientContext());
    MockWddmAllocation allocation3(rootDeviceEnvironment->getGmmClientContext());

    residencyController->addToTrimCandidateList(&allocation1);
    residencyController->addToTrimCandidateList(&allocation2);
    residencyController->addToTrimCandidateList(&allocation3);

    residencyController->forceTrimCandidateListCompaction = true;
    residencyController->removeFromTrimCandidateList(&allocation2, true);

    EXPECT_EQ(2u, residencyController->trimCandidateList.size());

    EXPECT_EQ(1u, allocation3.getTrimCandidateListPosition(osContextId));
    EXPECT_EQ(trimListUnusedPosition, allocation2.getTrimCandidateListPosition(osContextId));
}

TEST_F(WddmResidencyControllerTest, GivenAllocationThatIsNotLastWhenRemovingFromTrimCandidateListThenReplaceWithNullEntry) {
    MockWddmAllocation allocation1(rootDeviceEnvironment->getGmmClientContext());
    MockWddmAllocation allocation2(rootDeviceEnvironment->getGmmClientContext());
    MockWddmAllocation allocation3(rootDeviceEnvironment->getGmmClientContext());

    residencyController->addToTrimCandidateList(&allocation1);
    residencyController->addToTrimCandidateList(&allocation2);
    residencyController->addToTrimCandidateList(&allocation3);
    size_t position2 = allocation2.getTrimCandidateListPosition(osContextId);
    size_t position3 = allocation3.getTrimCandidateListPosition(osContextId);

    residencyController->removeFromTrimCandidateList(&allocation2, false);

    EXPECT_EQ(3u, residencyController->trimCandidateList.size());
    EXPECT_EQ(2u, position3);
    EXPECT_EQ(nullptr, residencyController->trimCandidateList[position2]);
}

TEST_F(WddmResidencyControllerTest, WhenCompactingTrimCandidateListThenInitialNullEntriesAreRemovedAndPositionsAreUpdated) {
    MockWddmAllocation allocation1(rootDeviceEnvironment->getGmmClientContext());
    MockWddmAllocation allocation2(rootDeviceEnvironment->getGmmClientContext());
    MockWddmAllocation allocation3(rootDeviceEnvironment->getGmmClientContext());
    MockWddmAllocation allocation4(rootDeviceEnvironment->getGmmClientContext());

    residencyController->addToTrimCandidateList(&allocation1);
    residencyController->addToTrimCandidateList(&allocation2);
    residencyController->addToTrimCandidateList(&allocation3);
    residencyController->addToTrimCandidateList(&allocation4);

    allocation3.getTrimCandidateListPosition(osContextId);
    allocation4.getTrimCandidateListPosition(osContextId);

    residencyController->removeFromTrimCandidateList(&allocation2, false);
    residencyController->removeFromTrimCandidateList(&allocation1, false);

    EXPECT_EQ(4u, residencyController->trimCandidateList.size());

    residencyController->compactTrimCandidateList();

    EXPECT_EQ(2u, residencyController->trimCandidateList.size());

    EXPECT_EQ(residencyController->trimCandidateList[0], &allocation3);
    EXPECT_EQ(0u, allocation3.getTrimCandidateListPosition(osContextId));

    EXPECT_EQ(residencyController->trimCandidateList[1], &allocation4);
    EXPECT_EQ(1u, allocation4.getTrimCandidateListPosition(osContextId));
}

TEST_F(WddmResidencyControllerTest, WhenCompactingTrimCandidateListThenNonNullEntriesAreNotRemoved) {
    MockWddmAllocation allocation1(rootDeviceEnvironment->getGmmClientContext());
    MockWddmAllocation allocation2(rootDeviceEnvironment->getGmmClientContext());
    MockWddmAllocation allocation3(rootDeviceEnvironment->getGmmClientContext());
    MockWddmAllocation allocation4(rootDeviceEnvironment->getGmmClientContext());

    residencyController->addToTrimCandidateList(&allocation1);
    residencyController->addToTrimCandidateList(&allocation2);
    residencyController->addToTrimCandidateList(&allocation3);
    residencyController->addToTrimCandidateList(&allocation4);

    EXPECT_EQ(4u, residencyController->trimCandidateList.size());

    residencyController->compactTrimCandidateList();

    EXPECT_EQ(4u, residencyController->trimCandidateList.size());
}

TEST_F(WddmResidencyControllerTest, GivenListSizeLessThenDoubleCandidateCountWhenCheckingTrimCandidateListCompactionThenCompactionIsRequired) {
    bool comapactionRequired;

    residencyController->trimCandidatesCount = 10;
    residencyController->trimCandidateList.resize(20);
    comapactionRequired = residencyController->checkTrimCandidateListCompaction();
    EXPECT_TRUE(comapactionRequired);

    residencyController->trimCandidatesCount = 5;
    residencyController->trimCandidateList.resize(20);
    comapactionRequired = residencyController->checkTrimCandidateListCompaction();
    EXPECT_TRUE(comapactionRequired);

    residencyController->trimCandidatesCount = 18;
    residencyController->trimCandidateList.resize(20);
    comapactionRequired = residencyController->checkTrimCandidateListCompaction();
    EXPECT_FALSE(comapactionRequired);
}

TEST_F(WddmResidencyControllerWithGdiTest, givenNotUsedAllocationsFromPreviousPeriodicTrimWhenTrimResidencyPeriodicTrimIsCalledThenAllocationsAreEvictedMarkedAndRemovedFromTrimCandidateList) {
    D3DKMT_TRIMNOTIFICATION trimNotification = {0};
    trimNotification.Flags.PeriodicTrim = 1;
    trimNotification.NumBytesToTrim = 0;

    // allocations have fence value == 0 by default
    MockWddmAllocation allocation1(rootDeviceEnvironment->getGmmClientContext());
    MockWddmAllocation allocation2(rootDeviceEnvironment->getGmmClientContext());

    allocation1.getResidencyData().updateCompletionData(0, osContextId);
    allocation2.getResidencyData().updateCompletionData(0, osContextId);

    allocation1.getResidencyData().resident[osContextId] = true;
    allocation2.getResidencyData().resident[osContextId] = true;

    // Set last periodic fence value
    residencyController->lastTrimFenceValue = 10;
    // Set current fence value to greater value
    residencyController->getMonitoredFence().currentFenceValue = 20;

    wddm->makeNonResidentResult.called = 0;

    residencyController->addToTrimCandidateList(&allocation1);
    residencyController->addToTrimCandidateList(&allocation2);

    residencyController->trimResidency(trimNotification.Flags, trimNotification.NumBytesToTrim);

    // 2 allocations evicted
    EXPECT_EQ(2u, wddm->makeNonResidentResult.called);
    // removed from trim candidate list
    EXPECT_EQ(0u, residencyController->peekTrimCandidateList().size());
    // marked nonresident
    EXPECT_FALSE(allocation1.getResidencyData().resident[osContextId]);
    EXPECT_FALSE(allocation2.getResidencyData().resident[osContextId]);
}

TEST_F(WddmResidencyControllerWithGdiTest, givenOneUsedAllocationFromPreviousPeriodicTrimWhenTrimResidencyPeriodicTrimIsCalledThenOneAllocationIsTrimmed) {
    D3DKMT_TRIMNOTIFICATION trimNotification = {0};
    trimNotification.Flags.PeriodicTrim = 1;
    trimNotification.NumBytesToTrim = 0;

    // allocations have fence value == 0 by default
    MockWddmAllocation allocation1(rootDeviceEnvironment->getGmmClientContext());
    MockWddmAllocation allocation2(rootDeviceEnvironment->getGmmClientContext());
    allocation1.getResidencyData().resident[osContextId] = true;
    // mark allocation used from last periodic trim
    allocation1.getResidencyData().updateCompletionData(0, osContextId);
    allocation2.getResidencyData().updateCompletionData(11, osContextId);
    allocation2.getResidencyData().resident[osContextId] = true;

    // Set last periodic fence value
    residencyController->lastTrimFenceValue = 10;
    // Set current fence value to greater value
    residencyController->getMonitoredFence().currentFenceValue = 20;

    wddm->makeNonResidentResult.called = 0;

    residencyController->addToTrimCandidateList(&allocation1);
    residencyController->addToTrimCandidateList(&allocation2);

    residencyController->trimResidency(trimNotification.Flags, trimNotification.NumBytesToTrim);

    // 1 allocation evicted
    EXPECT_EQ(1u, wddm->makeNonResidentResult.called);
    // removed from trim candidate list
    EXPECT_EQ(trimListUnusedPosition, allocation1.getTrimCandidateListPosition(osContextId));

    //marked nonresident
    EXPECT_FALSE(allocation1.getResidencyData().resident[osContextId]);
    // second stays resident
    EXPECT_TRUE(allocation2.getResidencyData().resident[osContextId]);
}

TEST_F(WddmResidencyControllerWithGdiAndMemoryManagerTest, givenTripleAllocationWithUsedAndUnusedFragmentsSincePreviousTrimWhenTrimResidencyPeriodicTrimIsCalledThenProperFragmentsAreEvictedAndMarked) {
    if (memoryManager->isLimitedGPU(0)) {
        GTEST_SKIP();
    }
    D3DKMT_TRIMNOTIFICATION trimNotification = {0};
    trimNotification.Flags.PeriodicTrim = 1;
    trimNotification.NumBytesToTrim = 0;

    // 3-fragment Allocation
    void *ptr = reinterpret_cast<void *>(wddm->virtualAllocAddress + 0x1500);
    auto allocationTriple = static_cast<WddmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), false, 2 * MemoryConstants::pageSize}, ptr));

    // whole allocation unused since previous trim
    allocationTriple->getResidencyData().updateCompletionData(0, osContextId);

    EXPECT_EQ(3u, allocationTriple->fragmentsStorage.fragmentCount);

    allocationTriple->fragmentsStorage.fragmentStorageData[0].residency->updateCompletionData(0, osContextId);
    allocationTriple->fragmentsStorage.fragmentStorageData[0].residency->resident[osContextId] = true;
    // this fragment was used
    allocationTriple->fragmentsStorage.fragmentStorageData[1].residency->updateCompletionData(11, osContextId);
    allocationTriple->fragmentsStorage.fragmentStorageData[0].residency->resident[osContextId] = true;

    allocationTriple->fragmentsStorage.fragmentStorageData[2].residency->updateCompletionData(0, osContextId);
    allocationTriple->fragmentsStorage.fragmentStorageData[2].residency->resident[osContextId] = true;

    // Set last periodic fence value
    *residencyController->getMonitoredFence().cpuAddress = 10;
    residencyController->updateLastTrimFenceValue();
    // Set current fence value to greater value
    residencyController->getMonitoredFence().currentFenceValue = 20;

    wddm->makeNonResidentResult.called = 0;

    residencyController->addToTrimCandidateList(allocationTriple);

    residencyController->trimResidency(trimNotification.Flags, trimNotification.NumBytesToTrim);

    // 2 fragments evicted with one call
    EXPECT_EQ(1u, wddm->makeNonResidentResult.called);
    // marked nonresident
    EXPECT_FALSE(allocationTriple->fragmentsStorage.fragmentStorageData[0].residency->resident[osContextId]);
    EXPECT_FALSE(allocationTriple->fragmentsStorage.fragmentStorageData[2].residency->resident[osContextId]);

    memoryManager->freeGraphicsMemory(allocationTriple);
}

TEST_F(WddmResidencyControllerWithGdiTest, givenPeriodicTrimWhenTrimCallbackCalledThenLastPeriodicTrimFenceIsSetToCurrentFenceValue) {
    D3DKMT_TRIMNOTIFICATION trimNotification = {0};
    trimNotification.Flags.PeriodicTrim = 1;
    trimNotification.NumBytesToTrim = 0;

    // Set last periodic fence value
    residencyController->lastTrimFenceValue = 10;
    // Set current fence value to greater value
    *residencyController->getMonitoredFence().cpuAddress = 20;

    residencyController->trimResidency(trimNotification.Flags, trimNotification.NumBytesToTrim);

    EXPECT_EQ(20u, residencyController->lastTrimFenceValue);
}

TEST_F(WddmResidencyControllerWithGdiTest, givenRestartPeriodicTrimWhenTrimCallbackCalledThenLastPeriodicTrimFenceIsSetToCurrentFenceValue) {
    D3DKMT_TRIMNOTIFICATION trimNotification = {0};
    trimNotification.Flags.RestartPeriodicTrim = 1;
    trimNotification.NumBytesToTrim = 0;

    // Set last periodic fence value
    residencyController->lastTrimFenceValue = 10;
    // Set current fence value to greater value
    *residencyController->getMonitoredFence().cpuAddress = 20;

    residencyController->trimResidency(trimNotification.Flags, trimNotification.NumBytesToTrim);

    EXPECT_EQ(20u, residencyController->lastTrimFenceValue);
}

TEST_F(WddmResidencyControllerWithGdiTest, GivenZeroWhenTrimmingToBudgetThenTrueIsReturned) {
    bool status = residencyController->trimResidencyToBudget(0);

    EXPECT_TRUE(status);
}

TEST_F(WddmResidencyControllerWithGdiTest, WhenTrimmingToBudgetThenAllDoneAllocationsAreTrimmed) {
    gdi->setNonZeroNumBytesToTrimInEvict();

    MockWddmAllocation allocation1(rootDeviceEnvironment->getGmmClientContext());
    MockWddmAllocation allocation2(rootDeviceEnvironment->getGmmClientContext());
    MockWddmAllocation allocation3(rootDeviceEnvironment->getGmmClientContext());

    allocation1.getResidencyData().resident[osContextId] = true;
    allocation1.getResidencyData().updateCompletionData(0, osContextId);

    allocation2.getResidencyData().updateCompletionData(1, osContextId);
    allocation2.getResidencyData().resident[osContextId] = true;

    allocation3.getResidencyData().updateCompletionData(2, osContextId);
    allocation3.getResidencyData().resident[osContextId] = true;

    *residencyController->getMonitoredFence().cpuAddress = 1;
    residencyController->getMonitoredFence().lastSubmittedFence = 1;
    residencyController->getMonitoredFence().currentFenceValue = 1;

    wddm->makeNonResidentResult.called = 0;

    residencyController->addToTrimCandidateList(&allocation1);
    residencyController->addToTrimCandidateList(&allocation2);
    residencyController->addToTrimCandidateList(&allocation3);

    residencyController->trimResidencyToBudget(3 * 4096);

    EXPECT_EQ(2u, wddm->makeNonResidentResult.called);

    EXPECT_EQ(1u, residencyController->peekTrimCandidatesCount());
    residencyController->compactTrimCandidateList();
    EXPECT_EQ(1u, residencyController->peekTrimCandidateList().size());

    EXPECT_EQ(trimListUnusedPosition, allocation1.getTrimCandidateListPosition(osContextId));
    EXPECT_EQ(trimListUnusedPosition, allocation2.getTrimCandidateListPosition(osContextId));
    EXPECT_NE(trimListUnusedPosition, allocation3.getTrimCandidateListPosition(osContextId));
}

TEST_F(WddmResidencyControllerWithGdiTest, GivenNumBytesToTrimIsNotZeroWhenTrimmingToBudgetThenFalseIsReturned) {
    gdi->setNonZeroNumBytesToTrimInEvict();

    MockWddmAllocation allocation1(rootDeviceEnvironment->getGmmClientContext());

    allocation1.getResidencyData().resident[osContextId] = true;
    allocation1.getResidencyData().updateCompletionData(0, osContextId);

    *residencyController->getMonitoredFence().cpuAddress = 1;
    residencyController->getMonitoredFence().lastSubmittedFence = 1;

    wddm->makeNonResidentResult.called = 0;

    residencyController->addToTrimCandidateList(&allocation1);

    bool status = residencyController->trimResidencyToBudget(3 * 4096);

    EXPECT_EQ(1u, wddm->makeNonResidentResult.called);
    EXPECT_EQ(0u, residencyController->peekTrimCandidateList().size());

    EXPECT_FALSE(status);
}

TEST_F(WddmResidencyControllerWithGdiTest, GivenNumBytesToTrimIsZeroWhenTrimmingToBudgetThenEvictingStops) {
    WddmAllocation allocation1(0, GraphicsAllocation::AllocationType::UNKNOWN, reinterpret_cast<void *>(0x1000), 0x1000, nullptr, MemoryPool::MemoryNull, 0u, 1u);
    WddmAllocation allocation2(0, GraphicsAllocation::AllocationType::UNKNOWN, reinterpret_cast<void *>(0x1000), 0x3000, nullptr, MemoryPool::MemoryNull, 0u, 1u);
    WddmAllocation allocation3(0, GraphicsAllocation::AllocationType::UNKNOWN, reinterpret_cast<void *>(0x1000), 0x1000, nullptr, MemoryPool::MemoryNull, 0u, 1u);

    allocation1.getResidencyData().resident[osContextId] = true;
    allocation1.getResidencyData().updateCompletionData(0, osContextId);

    allocation2.getResidencyData().updateCompletionData(1, osContextId);
    allocation2.getResidencyData().resident[osContextId] = true;

    allocation3.getResidencyData().updateCompletionData(2, osContextId);
    allocation3.getResidencyData().resident[osContextId] = true;

    *residencyController->getMonitoredFence().cpuAddress = 1;
    residencyController->getMonitoredFence().lastSubmittedFence = 1;
    residencyController->getMonitoredFence().currentFenceValue = 1;

    wddm->makeNonResidentResult.called = 0;

    residencyController->addToTrimCandidateList(&allocation1);
    residencyController->addToTrimCandidateList(&allocation2);
    residencyController->addToTrimCandidateList(&allocation3);

    bool status = residencyController->trimResidencyToBudget(3 * 4096);

    EXPECT_TRUE(status);
    EXPECT_EQ(2u, wddm->makeNonResidentResult.called);
    EXPECT_EQ(1u, residencyController->peekTrimCandidateList().size());

    EXPECT_EQ(trimListUnusedPosition, allocation1.getTrimCandidateListPosition(osContextId));
    EXPECT_EQ(trimListUnusedPosition, allocation2.getTrimCandidateListPosition(osContextId));
    EXPECT_NE(trimListUnusedPosition, allocation3.getTrimCandidateListPosition(osContextId));
}

TEST_F(WddmResidencyControllerWithGdiTest, WhenTrimmingToBudgetThenEvictedAllocationIsMarkedNonResident) {
    gdi->setNonZeroNumBytesToTrimInEvict();

    MockWddmAllocation allocation1(rootDeviceEnvironment->getGmmClientContext());
    MockWddmAllocation allocation2(rootDeviceEnvironment->getGmmClientContext());
    MockWddmAllocation allocation3(rootDeviceEnvironment->getGmmClientContext());

    allocation1.getResidencyData().resident[osContextId] = true;
    allocation1.getResidencyData().updateCompletionData(0, osContextId);

    allocation2.getResidencyData().updateCompletionData(1, osContextId);
    allocation2.getResidencyData().resident[osContextId] = true;

    allocation3.getResidencyData().updateCompletionData(2, osContextId);
    allocation3.getResidencyData().resident[osContextId] = true;

    *residencyController->getMonitoredFence().cpuAddress = 1;
    residencyController->getMonitoredFence().lastSubmittedFence = 1;
    residencyController->getMonitoredFence().currentFenceValue = 1;

    wddm->makeNonResidentResult.called = 0;

    residencyController->addToTrimCandidateList(&allocation1);
    residencyController->addToTrimCandidateList(&allocation2);
    residencyController->addToTrimCandidateList(&allocation3);

    residencyController->trimResidencyToBudget(3 * 4096);

    EXPECT_FALSE(allocation1.getResidencyData().resident[osContextId]);
    EXPECT_FALSE(allocation2.getResidencyData().resident[osContextId]);
    EXPECT_TRUE(allocation3.getResidencyData().resident[osContextId]);
}

TEST_F(WddmResidencyControllerWithGdiTest, GivenLastFenceIsGreaterThanMonitoredWhenTrimmingToBudgetThenWaitForCpu) {
    gdi->setNonZeroNumBytesToTrimInEvict();

    MockWddmAllocation allocation1(rootDeviceEnvironment->getGmmClientContext());

    allocation1.getResidencyData().resident[osContextId] = true;
    allocation1.getResidencyData().updateCompletionData(2, osContextId);

    *residencyController->getMonitoredFence().cpuAddress = 1;
    residencyController->getMonitoredFence().lastSubmittedFence = 2;
    residencyController->getMonitoredFence().currentFenceValue = 3;

    wddm->makeNonResidentResult.called = 0;
    wddm->waitFromCpuResult.called = 0;

    residencyController->addToTrimCandidateList(&allocation1);

    gdi->getWaitFromCpuArg().hDevice = 0;

    residencyController->trimResidencyToBudget(3 * 4096);

    EXPECT_EQ(1u, wddm->makeNonResidentResult.called);
    EXPECT_FALSE(allocation1.getResidencyData().resident[osContextId]);

    EXPECT_EQ(wddm->getDeviceHandle(), gdi->getWaitFromCpuArg().hDevice);
}

TEST_F(WddmResidencyControllerWithGdiAndMemoryManagerTest, WhenTrimmingToBudgetThenOnlyDoneFragmentsAreEvicted) {
    if (memoryManager->isLimitedGPU(0)) {
        GTEST_SKIP();
    }
    gdi->setNonZeroNumBytesToTrimInEvict();
    void *ptr = reinterpret_cast<void *>(wddm->virtualAllocAddress + 0x1000);
    WddmAllocation allocation1(0, GraphicsAllocation::AllocationType::UNKNOWN, ptr, 0x1000, nullptr, MemoryPool::MemoryNull, 0u, 1u);
    WddmAllocation allocation2(0, GraphicsAllocation::AllocationType::UNKNOWN, ptr, 0x1000, nullptr, MemoryPool::MemoryNull, 0u, 1u);

    allocation1.getResidencyData().resident[osContextId] = true;
    allocation1.getResidencyData().updateCompletionData(0, osContextId);

    allocation2.getResidencyData().updateCompletionData(1, osContextId);
    allocation2.getResidencyData().resident[osContextId] = true;

    void *ptrTriple = reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(ptr) + 0x500);
    WddmAllocation *allocationTriple = static_cast<WddmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), false, 2 * MemoryConstants::pageSize}, ptrTriple));

    allocationTriple->getResidencyData().updateCompletionData(1, osContextId);
    allocationTriple->getResidencyData().resident[osContextId] = true;

    EXPECT_EQ(3u, allocationTriple->fragmentsStorage.fragmentCount);

    for (uint32_t i = 0; i < 3; i++) {
        allocationTriple->fragmentsStorage.fragmentStorageData[i].residency->updateCompletionData(1, osContextId);
        allocationTriple->fragmentsStorage.fragmentStorageData[i].residency->resident[osContextId] = true;
    }

    // This should not be evicted
    allocationTriple->fragmentsStorage.fragmentStorageData[1].residency->updateCompletionData(2, osContextId);

    residencyController->addToTrimCandidateList(&allocation1);
    residencyController->addToTrimCandidateList(allocationTriple);
    residencyController->addToTrimCandidateList(&allocation2);

    *residencyController->getMonitoredFence().cpuAddress = 1;
    residencyController->getMonitoredFence().lastSubmittedFence = 1;
    residencyController->getMonitoredFence().currentFenceValue = 2;

    wddm->makeNonResidentResult.called = 0;

    residencyController->trimResidencyToBudget(3 * 4096);

    EXPECT_EQ(2u, wddm->makeNonResidentResult.called);

    EXPECT_FALSE(allocationTriple->fragmentsStorage.fragmentStorageData[0].residency->resident[osContextId]);
    EXPECT_TRUE(allocationTriple->fragmentsStorage.fragmentStorageData[1].residency->resident[osContextId]);
    EXPECT_FALSE(allocationTriple->fragmentsStorage.fragmentStorageData[2].residency->resident[osContextId]);

    memoryManager->freeGraphicsMemory(allocationTriple);
}

TEST_F(WddmResidencyControllerWithGdiTest, givenThreeAllocationsAlignedSizeBiggerThanAllocSizeWhenBudgetEqualTwoAlignedAllocationThenEvictOnlyTwo) {
    gdi->setNonZeroNumBytesToTrimInEvict();
    size_t underlyingSize = 0xF00;
    size_t alignedSize = 0x1000;
    size_t budget = 2 * alignedSize;

    //trim budget should consider aligned size, not underlying, so if function considers underlying, it should evict three, not two
    EXPECT_GT((3 * underlyingSize), budget);
    EXPECT_LT((2 * underlyingSize), budget);
    void *ptr1 = reinterpret_cast<void *>(wddm->virtualAllocAddress + 0x1000);
    void *ptr2 = reinterpret_cast<void *>(wddm->virtualAllocAddress + 0x3000);
    void *ptr3 = reinterpret_cast<void *>(wddm->virtualAllocAddress + 0x5000);

    WddmAllocation allocation1(0, GraphicsAllocation::AllocationType::UNKNOWN, ptr1, underlyingSize, nullptr, MemoryPool::MemoryNull, 0u, 1u);
    WddmAllocation allocation2(0, GraphicsAllocation::AllocationType::UNKNOWN, ptr2, underlyingSize, nullptr, MemoryPool::MemoryNull, 0u, 1u);
    WddmAllocation allocation3(0, GraphicsAllocation::AllocationType::UNKNOWN, ptr3, underlyingSize, nullptr, MemoryPool::MemoryNull, 0u, 1u);

    allocation1.getResidencyData().resident[osContextId] = true;
    allocation1.getResidencyData().updateCompletionData(0, osContextId);

    allocation2.getResidencyData().updateCompletionData(1, osContextId);
    allocation2.getResidencyData().resident[osContextId] = true;

    allocation3.getResidencyData().updateCompletionData(1, osContextId);
    allocation3.getResidencyData().resident[osContextId] = true;

    *residencyController->getMonitoredFence().cpuAddress = 1;
    residencyController->getMonitoredFence().lastSubmittedFence = 1;
    residencyController->getMonitoredFence().currentFenceValue = 1;

    wddm->makeNonResidentResult.called = 0;

    residencyController->addToTrimCandidateList(&allocation1);
    residencyController->addToTrimCandidateList(&allocation2);
    residencyController->addToTrimCandidateList(&allocation3);

    bool status = residencyController->trimResidencyToBudget(budget);
    EXPECT_TRUE(status);

    EXPECT_FALSE(allocation1.getResidencyData().resident[osContextId]);
    EXPECT_FALSE(allocation2.getResidencyData().resident[osContextId]);
    EXPECT_TRUE(allocation3.getResidencyData().resident[osContextId]);
}

using WddmResidencyControllerLockTest = WddmResidencyControllerWithGdiTest;

TEST_F(WddmResidencyControllerLockTest, givenPeriodicTrimWhenTrimmingResidencyThenLockOnce) {
    D3DKMT_TRIMNOTIFICATION trimNotification = {0};
    trimNotification.Flags.PeriodicTrim = 1;
    trimNotification.NumBytesToTrim = 0;

    residencyController->trimResidency(trimNotification.Flags, trimNotification.NumBytesToTrim);
    EXPECT_EQ(1, residencyController->acquireLockCallCount);
}

TEST_F(WddmResidencyControllerLockTest, givenTrimToBudgetWhenTrimmingResidencyThenLockOnce) {
    D3DKMT_TRIMNOTIFICATION trimNotification = {0};
    trimNotification.Flags.TrimToBudget = 1;
    trimNotification.NumBytesToTrim = 0;

    residencyController->trimResidency(trimNotification.Flags, trimNotification.NumBytesToTrim);
    EXPECT_EQ(1, residencyController->acquireLockCallCount);
}

TEST_F(WddmResidencyControllerLockTest, givenPeriodicTrimAndTrimToBudgetWhenTrimmingResidencyThenLockTwice) {
    D3DKMT_TRIMNOTIFICATION trimNotification = {0};
    trimNotification.Flags.PeriodicTrim = 1;
    trimNotification.Flags.TrimToBudget = 1;
    trimNotification.NumBytesToTrim = 0;

    residencyController->trimResidency(trimNotification.Flags, trimNotification.NumBytesToTrim);
    EXPECT_EQ(2, residencyController->acquireLockCallCount);
}

TEST_F(WddmResidencyControllerWithGdiAndMemoryManagerTest, WhenMakingResidentResidencyAllocationsThenAllAllocationsAreMarked) {
    MockWddmAllocation allocation1(gmmClientContext);
    MockWddmAllocation allocation2(gmmClientContext);
    MockWddmAllocation allocation3(gmmClientContext);
    MockWddmAllocation allocation4(gmmClientContext);
    ResidencyContainer residencyPack{&allocation1, &allocation2, &allocation3, &allocation4};

    residencyController->makeResidentResidencyAllocations(residencyPack);

    EXPECT_TRUE(allocation1.getResidencyData().resident[osContextId]);
    EXPECT_TRUE(allocation2.getResidencyData().resident[osContextId]);
    EXPECT_TRUE(allocation3.getResidencyData().resident[osContextId]);
    EXPECT_TRUE(allocation4.getResidencyData().resident[osContextId]);
}

TEST_F(WddmResidencyControllerWithGdiAndMemoryManagerTest, WhenMakingResidentResidencyAllocationsThenLastFenceIsUpdated) {
    MockWddmAllocation allocation1(gmmClientContext);
    MockWddmAllocation allocation2(gmmClientContext);
    MockWddmAllocation allocation3(gmmClientContext);
    MockWddmAllocation allocation4(gmmClientContext);
    ResidencyContainer residencyPack{&allocation1, &allocation2, &allocation3, &allocation4};

    residencyController->getMonitoredFence().currentFenceValue = 20;

    residencyController->makeResidentResidencyAllocations(residencyPack);

    EXPECT_EQ(20u, allocation1.getResidencyData().getFenceValueForContextId(osContext->getContextId()));
    EXPECT_EQ(20u, allocation2.getResidencyData().getFenceValueForContextId(osContext->getContextId()));
    EXPECT_EQ(20u, allocation3.getResidencyData().getFenceValueForContextId(osContext->getContextId()));
    EXPECT_EQ(20u, allocation4.getResidencyData().getFenceValueForContextId(osContext->getContextId()));
}

TEST_F(WddmResidencyControllerWithGdiAndMemoryManagerTest, GivenTripleAllocationsWhenMakingResidentResidencyAllocationsThenAllAllocationsAreMarkedResident) {
    if (executionEnvironment->memoryManager.get()->isLimitedGPU(0)) {
        GTEST_SKIP();
    }
    MockWddmAllocation allocation1(gmmClientContext);
    MockWddmAllocation allocation2(gmmClientContext);
    void *ptr = reinterpret_cast<void *>(wddm->virtualAllocAddress + 0x1500);

    WddmAllocation *allocationTriple = (WddmAllocation *)memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), false, 2 * MemoryConstants::pageSize}, ptr);
    ResidencyContainer residencyPack{&allocation1, allocationTriple, &allocation2};

    residencyController->makeResidentResidencyAllocations(residencyPack);

    for (uint32_t i = 0; i < allocationTriple->fragmentsStorage.fragmentCount; i++) {
        EXPECT_TRUE(allocationTriple->fragmentsStorage.fragmentStorageData[i].residency->resident[osContextId]);
    }

    EXPECT_EQ(EngineLimits::maxHandleCount + 3 + EngineLimits::maxHandleCount, gdi->getMakeResidentArg().NumAllocations);

    memoryManager->freeGraphicsMemory(allocationTriple);
}

TEST_F(WddmResidencyControllerWithGdiAndMemoryManagerTest, GivenTripleAllocationsWhenMakingResidentResidencyAllocationsThenLastFencePlusOneIsSet) {
    MockWddmAllocation allocation1(gmmClientContext);
    MockWddmAllocation allocation2(gmmClientContext);

    WddmAllocation *allocationTriple = static_cast<WddmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), false, 2 * MemoryConstants::pageSize}, reinterpret_cast<void *>(0x1500)));

    residencyController->getMonitoredFence().currentFenceValue = 20;

    ResidencyContainer residencyPack{&allocation1, allocationTriple, &allocation2};
    residencyController->makeResidentResidencyAllocations(residencyPack);

    for (uint32_t i = 0; i < allocationTriple->fragmentsStorage.fragmentCount; i++) {
        EXPECT_EQ(20u, allocationTriple->fragmentsStorage.fragmentStorageData[i].residency->getFenceValueForContextId(0));
    }

    memoryManager->freeGraphicsMemory(allocationTriple);
}

TEST_F(WddmResidencyControllerWithMockWddmTest, givenMakeResidentFailsWhenCallingMakeResidentResidencyAllocationsThenDontMarkAllocationsAsResident) {
    MockWddmAllocation allocation1(gmmClientContext);
    MockWddmAllocation allocation2(gmmClientContext);
    MockWddmAllocation allocation3(gmmClientContext);
    MockWddmAllocation allocation4(gmmClientContext);

    auto makeResidentWithOutBytesToTrim = [](const D3DKMT_HANDLE *handles, uint32_t count, bool cantTrimFurther, uint64_t *numberOfBytesToTrim, size_t size) -> bool { *numberOfBytesToTrim = 4 * 4096;  return false; };

    ON_CALL(*wddm, makeResident(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_)).WillByDefault(::testing::Invoke(makeResidentWithOutBytesToTrim));
    EXPECT_CALL(*wddm, makeResident(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_)).Times(2);

    ResidencyContainer residencyPack{&allocation1, &allocation2, &allocation3, &allocation4};
    bool result = residencyController->makeResidentResidencyAllocations(residencyPack);

    EXPECT_FALSE(result);

    EXPECT_FALSE(allocation1.getResidencyData().resident[osContextId]);
    EXPECT_FALSE(allocation2.getResidencyData().resident[osContextId]);
    EXPECT_FALSE(allocation3.getResidencyData().resident[osContextId]);
    EXPECT_FALSE(allocation4.getResidencyData().resident[osContextId]);
}

TEST_F(WddmResidencyControllerWithMockWddmTest, givenMakeResidentFailsWhenCallingMakeResidentResidencyAllocationsThenDontMarkTripleAllocationsAsResident) {
    MockWddmAllocation allocation1(gmmClientContext);
    MockWddmAllocation allocation2(gmmClientContext);
    void *ptr = reinterpret_cast<void *>(wddm->getWddmMinAddress() + 0x1500);
    WddmAllocation *allocationTriple = static_cast<WddmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), false, 2 * MemoryConstants::pageSize}, ptr));
    ASSERT_NE(nullptr, allocationTriple);

    auto makeResidentWithOutBytesToTrim = [](const D3DKMT_HANDLE *handles, uint32_t count, bool cantTrimFurther, uint64_t *numberOfBytesToTrim, size_t size) -> bool { *numberOfBytesToTrim = 4 * 4096;  return false; };

    ON_CALL(*wddm, makeResident(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_)).WillByDefault(::testing::Invoke(makeResidentWithOutBytesToTrim));
    EXPECT_CALL(*wddm, makeResident(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_)).Times(2);

    ResidencyContainer residencyPack{&allocation1, allocationTriple, &allocation2};
    bool result = residencyController->makeResidentResidencyAllocations(residencyPack);

    EXPECT_FALSE(result);

    for (uint32_t i = 0; i < allocationTriple->fragmentsStorage.fragmentCount; i++) {
        EXPECT_FALSE(allocationTriple->fragmentsStorage.fragmentStorageData[i].residency->resident[osContextId]);
    }

    memoryManager->freeGraphicsMemory(allocationTriple);
}

TEST_F(WddmResidencyControllerWithMockWddmTest, givenMakeResidentFailsWhenCallingMakeResidentResidencyAllocationsThenCallItAgainWithCantTrimFurtherSetToTrue) {
    MockWddmAllocation allocation1(gmmClientContext);

    auto makeResidentWithOutBytesToTrim = [](const D3DKMT_HANDLE *handles, uint32_t count, bool cantTrimFurther, uint64_t *numberOfBytesToTrim, size_t size) -> bool { *numberOfBytesToTrim = 4 * 4096;  return false; };

    ON_CALL(*wddm, makeResident(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_)).WillByDefault(::testing::Invoke(makeResidentWithOutBytesToTrim));
    EXPECT_CALL(*wddm, makeResident(::testing::_, ::testing::_, false, ::testing::_, ::testing::_)).Times(1);
    EXPECT_CALL(*wddm, makeResident(::testing::_, ::testing::_, true, ::testing::_, ::testing::_)).Times(1);

    ResidencyContainer residencyPack{&allocation1};
    bool result = residencyController->makeResidentResidencyAllocations(residencyPack);

    EXPECT_FALSE(result);
}

TEST_F(WddmResidencyControllerWithMockWddmTest, givenAllocationPackPassedWhenCallingMakeResidentResidencyAllocationsThenItIsUsed) {
    MockWddmAllocation allocation1(gmmClientContext);
    MockWddmAllocation allocation2(gmmClientContext);
    allocation1.handle = 1;
    allocation2.handle = 2;
    ResidencyContainer residencyPack{&allocation1, &allocation2};

    auto makeResidentWithOutBytesToTrim = [](const D3DKMT_HANDLE *handles, uint32_t count, bool cantTrimFurther, uint64_t *numberOfBytesToTrim, size_t size) -> bool {
        EXPECT_EQ(1, handles[0 * EngineLimits::maxHandleCount]);
        EXPECT_EQ(2, handles[1 * EngineLimits::maxHandleCount]);
        return true;
    };
    ON_CALL(*wddm, makeResident(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_)).WillByDefault(::testing::Invoke(makeResidentWithOutBytesToTrim));
    EXPECT_CALL(*wddm, makeResident(::testing::_, 2 * EngineLimits::maxHandleCount, false, ::testing::_, ::testing::_)).Times(1);

    bool result = residencyController->makeResidentResidencyAllocations(residencyPack);
    EXPECT_TRUE(result);
}

TEST_F(WddmResidencyControllerWithMockWddmTest, givenMakeResidentFailsAndTrimToBudgetSuceedsWhenCallingMakeResidentResidencyAllocationsThenSucceed) {
    MockWddmAllocation allocation1(gmmClientContext);
    void *cpuPtr = reinterpret_cast<void *>(wddm->getWddmMinAddress() + 0x1000);
    size_t allocationSize = 0x1000;
    WddmAllocation allocationToTrim(0, GraphicsAllocation::AllocationType::UNKNOWN, cpuPtr, allocationSize, nullptr, MemoryPool::MemoryNull, 0u, 1u);

    allocationToTrim.getResidencyData().updateCompletionData(residencyController->getMonitoredFence().lastSubmittedFence, osContext->getContextId());

    auto makeResidentWithOutBytesToTrim = [allocationSize](const D3DKMT_HANDLE *handles, uint32_t count, bool cantTrimFurther, uint64_t *numberOfBytesToTrim, size_t size) -> bool { *numberOfBytesToTrim = allocationSize;  return false; };

    EXPECT_CALL(*wddm, makeResident(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_)).Times(2).WillOnce(::testing::Invoke(makeResidentWithOutBytesToTrim)).WillOnce(::testing::Return(true));

    residencyController->addToTrimCandidateList(&allocationToTrim);

    ResidencyContainer residencyPack{&allocation1};
    bool result = residencyController->makeResidentResidencyAllocations(residencyPack);

    EXPECT_TRUE(result);

    EXPECT_TRUE(allocation1.getResidencyData().resident[osContextId]);
}

TEST_F(WddmResidencyControllerWithMockWddmTest, givenMakeResidentFailsWhenCallingMakeResidentResidencyAllocationsThenMemoryBudgetExhaustedIsSetToTrue) {
    MockWddmAllocation allocation1(gmmClientContext);
    ResidencyContainer residencyPack{&allocation1};

    auto makeResidentThatFails = [](const D3DKMT_HANDLE *handles, uint32_t count, bool cantTrimFurther, uint64_t *numberOfBytesToTrim, size_t size) -> bool { return false; };
    auto makeResidentThatSucceds = [](const D3DKMT_HANDLE *handles, uint32_t count, bool cantTrimFurther, uint64_t *numberOfBytesToTrim, size_t size) -> bool { return true; };

    EXPECT_CALL(*wddm, makeResident(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_)).Times(2).WillOnce(::testing::Invoke(makeResidentThatFails)).WillOnce(::testing::Invoke(makeResidentThatSucceds));

    residencyController->makeResidentResidencyAllocations(residencyPack);
    EXPECT_TRUE(residencyController->isMemoryBudgetExhausted());
}
