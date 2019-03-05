/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_stream/preemption.h"
#include "runtime/execution_environment/execution_environment.h"
#include "runtime/helpers/hw_helper.h"
#include "runtime/os_interface/os_context.h"
#include "runtime/os_interface/os_interface.h"
#include "runtime/os_interface/windows/os_context_win.h"
#include "runtime/os_interface/windows/os_interface.h"
#include "runtime/os_interface/windows/wddm/wddm_interface.h"
#include "runtime/os_interface/windows/wddm_residency_controller.h"
#include "test.h"
#include "unit_tests/mocks/mock_allocation_properties.h"
#include "unit_tests/mocks/mock_wddm.h"
#include "unit_tests/os_interface/windows/mock_gdi_interface.h"
#include "unit_tests/os_interface/windows/mock_wddm_allocation.h"
#include "unit_tests/os_interface/windows/mock_wddm_memory_manager.h"

#include "gmock/gmock.h"

#include <memory>

using namespace OCLRT;

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

    std::unique_lock<SpinLock> acquireLock() override {
        acquireLockCallCount++;
        return WddmResidencyController::acquireLock();
    }
};

struct WddmResidencyControllerTest : ::testing::Test {
    const uint32_t osContextId = 0u;

    void SetUp() {
        wddm = std::unique_ptr<WddmMock>(static_cast<WddmMock *>(Wddm::createWddm()));
        wddm->init(PreemptionHelper::getDefaultPreemptionMode(*platformDevices[0]));
        residencyController = std::make_unique<MockWddmResidencyController>(*wddm, osContextId);
        wddm->getWddmInterface()->createMonitoredFence(*residencyController);
    }

    std::unique_ptr<WddmMock> wddm;
    std::unique_ptr<MockWddmResidencyController> residencyController;
};

struct WddmResidencyControllerWithGdiTest : ::testing::Test {
    const uint32_t osContextId = 0u;

    void SetUp() {
        wddm = std::unique_ptr<WddmMock>(static_cast<WddmMock *>(Wddm::createWddm()));
        gdi = new MockGdi();
        wddm->gdi.reset(gdi);
        wddm->init(PreemptionHelper::getDefaultPreemptionMode(*platformDevices[0]));

        residencyController = std::make_unique<MockWddmResidencyController>(*wddm, osContextId);
        wddm->getWddmInterface()->createMonitoredFence(*residencyController);
        residencyController->registerCallback();
    }

    std::unique_ptr<WddmMock> wddm;
    std::unique_ptr<MockWddmResidencyController> residencyController;
    MockGdi *gdi;
};

struct WddmResidencyControllerWithMockWddmTest : public WddmResidencyControllerTest {
    void SetUp() {
        executionEnvironment = std::make_unique<ExecutionEnvironment>();
        executionEnvironment->initGmm(*platformDevices);

        wddm = new ::testing::NiceMock<GmockWddm>();
        wddm->gdi = std::make_unique<MockGdi>();
        auto preemptionMode = PreemptionHelper::getDefaultPreemptionMode(*platformDevices[0]);
        ASSERT_TRUE(wddm->init(preemptionMode));

        executionEnvironment->osInterface = std::make_unique<OSInterface>();
        executionEnvironment->osInterface->get()->setWddm(wddm);
        memoryManager = std::make_unique<MockWddmMemoryManager>(wddm, *executionEnvironment);

        osContext = memoryManager->createAndRegisterOsContext(nullptr, HwHelper::get(platformDevices[0]->pPlatform->eRenderCoreFamily).getGpgpuEngineInstances()[0], 1, preemptionMode);

        osContext->incRefInternal();
        residencyController = &static_cast<OsContextWin *>(osContext)->getResidencyController();
    }

    void TearDown() {
        osContext->decRefInternal();
    }

    std::unique_ptr<ExecutionEnvironment> executionEnvironment;
    std::unique_ptr<MockWddmMemoryManager> memoryManager;
    ::testing::NiceMock<GmockWddm> *wddm = nullptr;
    OsContext *osContext;
    WddmResidencyController *residencyController;
};

struct WddmResidencyControllerWithGdiAndMemoryManagerTest : ::testing::Test {
    const uint32_t osContextId = 0u;

    void SetUp() {
        wddm = static_cast<WddmMock *>(Wddm::createWddm());
        wddm->init(PreemptionHelper::getDefaultPreemptionMode(*platformDevices[0]));
        gdi = new MockGdi();
        wddm->gdi.reset(gdi);

        executionEnvironment = std::make_unique<ExecutionEnvironment>();
        executionEnvironment->osInterface = std::make_unique<OSInterface>();
        executionEnvironment->osInterface->get()->setWddm(wddm);

        memoryManager = std::make_unique<MockWddmMemoryManager>(wddm, *executionEnvironment);
        osContext = memoryManager->createAndRegisterOsContext(nullptr, HwHelper::get(platformDevices[0]->pPlatform->eRenderCoreFamily).getGpgpuEngineInstances()[0], 1, PreemptionHelper::getDefaultPreemptionMode(*platformDevices[0]));

        osContext->incRefInternal();

        residencyController = &static_cast<OsContextWin *>(osContext)->getResidencyController();
    }

    void TearDown() {
        osContext->decRefInternal();
    }

    std::unique_ptr<ExecutionEnvironment> executionEnvironment;
    std::unique_ptr<MockWddmMemoryManager> memoryManager;

    WddmMock *wddm = nullptr;
    OsContext *osContext = nullptr;
    MockGdi *gdi = nullptr;
    WddmResidencyController *residencyController = nullptr;
};

TEST(WddmResidencyController, givenWddmResidencyControllerWhenItIsConstructedThenDoNotRegisterTrimCallback) {
    ExecutionEnvironment executionEnvironment;
    auto gdi = new MockGdi();
    auto wddm = std::unique_ptr<WddmMock>{static_cast<WddmMock *>(Wddm::createWddm())};
    wddm->gdi.reset(gdi);
    wddm->init(PreemptionHelper::getDefaultPreemptionMode(*platformDevices[0]));

    std::memset(&gdi->getRegisterTrimNotificationArg(), 0, sizeof(D3DKMT_REGISTERTRIMNOTIFICATION));
    MockWddmResidencyController residencyController{*wddm, 0u};

    EXPECT_EQ(0u, wddm->registerTrimCallbackResult.called);
    EXPECT_EQ(nullptr, residencyController.trimCallbackHandle);

    EXPECT_EQ(nullptr, gdi->getRegisterTrimNotificationArg().Callback);
    EXPECT_EQ(nullptr, gdi->getRegisterTrimNotificationArg().Context);
    EXPECT_EQ(0u, gdi->getRegisterTrimNotificationArg().hDevice);
}

TEST(WddmResidencyController, givenWddmResidencyControllerWhenRegisterCallbackThenCallbackIsSetUpProperly) {
    ExecutionEnvironment executionEnvironment;
    auto gdi = new MockGdi();
    auto wddm = std::unique_ptr<WddmMock>{static_cast<WddmMock *>(Wddm::createWddm())};
    wddm->gdi.reset(gdi);
    wddm->init(PreemptionHelper::getDefaultPreemptionMode(*platformDevices[0]));

    std::memset(&gdi->getRegisterTrimNotificationArg(), 0, sizeof(D3DKMT_REGISTERTRIMNOTIFICATION));

    WddmResidencyController residencyController{*wddm, 0u};
    residencyController.registerCallback();

    EXPECT_EQ(1u, wddm->registerTrimCallbackResult.called);
    EXPECT_EQ(reinterpret_cast<PFND3DKMT_TRIMNOTIFICATIONCALLBACK>(WddmResidencyController::trimCallback), gdi->getRegisterTrimNotificationArg().Callback);
    EXPECT_EQ(reinterpret_cast<void *>(&residencyController), gdi->getRegisterTrimNotificationArg().Context);
    EXPECT_EQ(wddm->getDevice(), gdi->getRegisterTrimNotificationArg().hDevice);
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
    residencyController.reset();

    EXPECT_EQ(trimCallbackAddress, gdi->getUnregisterTrimNotificationArg().Callback);
    EXPECT_EQ(trimCallbackHandle, gdi->getUnregisterTrimNotificationArg().Handle);
}

TEST_F(WddmResidencyControllerTest, givenUsedAllocationWhenCallingRemoveFromTrimCandidateListIfUsedThenRemoveIt) {
    MockWddmAllocation allocation;
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
    MockWddmAllocation allocation;
    residencyController->removeFromTrimCandidateListIfUsed(&allocation, false);
    EXPECT_EQ(trimListUnusedPosition, allocation.getTrimCandidateListPosition(osContextId));
}

TEST_F(WddmResidencyControllerTest, addToTrimCandidateListPlacesAllocationInContainerAndAssignsPosition) {
    MockWddmAllocation allocation;
    residencyController->addToTrimCandidateList(&allocation);

    EXPECT_NE(0u, residencyController->trimCandidateList.size());
    EXPECT_NE(trimListUnusedPosition, allocation.getTrimCandidateListPosition(osContextId));

    size_t position = allocation.getTrimCandidateListPosition(osContextId);
    ASSERT_LT(position, residencyController->trimCandidateList.size());

    EXPECT_EQ(&allocation, residencyController->trimCandidateList[position]);
}

TEST_F(WddmResidencyControllerTest, addToTrimCandidateListDoesNotInsertAllocationAlreadyOnTheList) {
    MockWddmAllocation allocation;

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

TEST_F(WddmResidencyControllerTest, removeFromTrimCandidateListAssignsUnusedPosition) {
    MockWddmAllocation allocation;

    residencyController->addToTrimCandidateList(&allocation);
    residencyController->removeFromTrimCandidateList(&allocation, false);

    EXPECT_EQ(trimListUnusedPosition, allocation.getTrimCandidateListPosition(osContextId));
}

TEST_F(WddmResidencyControllerTest, removeFromTrimCandidateListRemovesAllocationInAssignedPosition) {
    MockWddmAllocation allocation;

    residencyController->addToTrimCandidateList(&allocation);
    size_t position = allocation.getTrimCandidateListPosition(osContextId);

    residencyController->removeFromTrimCandidateList(&allocation, false);

    if (residencyController->trimCandidateList.size() > position) {
        EXPECT_NE(&allocation, residencyController->trimCandidateList[position]);
    }
}

TEST_F(WddmResidencyControllerTest, removeFromTrimCandidateListRemovesLastAllocation) {
    MockWddmAllocation allocation;

    residencyController->trimCandidateList.resize(0);

    residencyController->addToTrimCandidateList(&allocation);

    residencyController->removeFromTrimCandidateList(&allocation, false);

    EXPECT_EQ(0u, residencyController->trimCandidateList.size());
}

TEST_F(WddmResidencyControllerTest, removeFromTrimCandidateListRemovesLastAllocationAndAllPreviousEmptyEntries) {
    MockWddmAllocation allocation1, allocation2;

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

TEST_F(WddmResidencyControllerTest, successiveAddingToTrimCandidateListAssignsNewPositions) {
    MockWddmAllocation allocation1, allocation2, allocation3;

    residencyController->addToTrimCandidateList(&allocation1);
    residencyController->addToTrimCandidateList(&allocation2);
    residencyController->addToTrimCandidateList(&allocation3);

    EXPECT_EQ(3u, residencyController->trimCandidateList.size());
    EXPECT_NE(allocation1.getTrimCandidateListPosition(osContextId), allocation2.getTrimCandidateListPosition(osContextId));
    EXPECT_NE(allocation2.getTrimCandidateListPosition(osContextId), allocation3.getTrimCandidateListPosition(osContextId));
}

TEST_F(WddmResidencyControllerTest, DISABLED_removingNotLastAllocationFromTrimCandidateListSubstituesLastPositionAllocation) {
    MockWddmAllocation allocation1, allocation2, allocation3;

    residencyController->addToTrimCandidateList(&allocation1);
    residencyController->addToTrimCandidateList(&allocation2);
    residencyController->addToTrimCandidateList(&allocation3);

    residencyController->removeFromTrimCandidateList(&allocation2, false);

    EXPECT_EQ(2u, residencyController->trimCandidateList.size());

    EXPECT_EQ(2u, allocation3.getTrimCandidateListPosition(osContextId));
    EXPECT_NE(allocation2.getTrimCandidateListPosition(osContextId), allocation3.getTrimCandidateListPosition(osContextId));
}

TEST_F(WddmResidencyControllerTest, removingNotLastAllocationFromTrimCandidateListPutsNullEntry) {
    MockWddmAllocation allocation1, allocation2, allocation3;

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

TEST_F(WddmResidencyControllerTest, compactTrimCandidateListRemovesInitialNullEntriesAndUpdatesPositions) {
    MockWddmAllocation allocation1, allocation2, allocation3, allocation4;

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

TEST_F(WddmResidencyControllerTest, compactTrimCandidateListWithNonNullEntries) {
    MockWddmAllocation allocation1, allocation2, allocation3, allocation4;

    residencyController->addToTrimCandidateList(&allocation1);
    residencyController->addToTrimCandidateList(&allocation2);
    residencyController->addToTrimCandidateList(&allocation3);
    residencyController->addToTrimCandidateList(&allocation4);

    EXPECT_EQ(4u, residencyController->trimCandidateList.size());

    residencyController->compactTrimCandidateList();

    EXPECT_EQ(4u, residencyController->trimCandidateList.size());
}

TEST_F(WddmResidencyControllerTest, checkTrimCandidateListCompaction) {
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
    MockWddmAllocation allocation1, allocation2;

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
    MockWddmAllocation allocation1, allocation2;
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
    D3DKMT_TRIMNOTIFICATION trimNotification = {0};
    trimNotification.Flags.PeriodicTrim = 1;
    trimNotification.NumBytesToTrim = 0;

    // 3-fragment Allocation
    void *ptr = reinterpret_cast<void *>(wddm->virtualAllocAddress + 0x1500);
    auto allocationTriple = static_cast<WddmAllocation *>(memoryManager->allocateGraphicsMemory(MockAllocationProperties{false, 2 * MemoryConstants::pageSize}, ptr));

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

TEST_F(WddmResidencyControllerWithGdiTest, trimToBudgetWithZeroSizeReturnsTrue) {
    bool status = residencyController->trimResidencyToBudget(0);

    EXPECT_TRUE(status);
}

TEST_F(WddmResidencyControllerWithGdiTest, trimToBudgetAllDoneAllocations) {
    gdi->setNonZeroNumBytesToTrimInEvict();

    MockWddmAllocation allocation1, allocation2, allocation3;

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

TEST_F(WddmResidencyControllerWithGdiTest, trimToBudgetReturnsFalseWhenNumBytesToTrimIsNotZero) {
    gdi->setNonZeroNumBytesToTrimInEvict();

    MockWddmAllocation allocation1;

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

TEST_F(WddmResidencyControllerWithGdiTest, trimToBudgetStopsEvictingWhenNumBytesToTrimIsZero) {
    WddmAllocation allocation1(GraphicsAllocation::AllocationType::UNDECIDED, reinterpret_cast<void *>(0x1000), 0x1000, nullptr, MemoryPool::MemoryNull, false);
    WddmAllocation allocation2(GraphicsAllocation::AllocationType::UNDECIDED, reinterpret_cast<void *>(0x1000), 0x3000, nullptr, MemoryPool::MemoryNull, false);
    WddmAllocation allocation3(GraphicsAllocation::AllocationType::UNDECIDED, reinterpret_cast<void *>(0x1000), 0x1000, nullptr, MemoryPool::MemoryNull, false);

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

TEST_F(WddmResidencyControllerWithGdiTest, trimToBudgetMarksEvictedAllocationNonResident) {
    gdi->setNonZeroNumBytesToTrimInEvict();

    MockWddmAllocation allocation1, allocation2, allocation3;

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

TEST_F(WddmResidencyControllerWithGdiTest, trimToBudgetWaitsFromCpuWhenLastFenceIsGreaterThanMonitored) {
    gdi->setNonZeroNumBytesToTrimInEvict();

    MockWddmAllocation allocation1;

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

    EXPECT_EQ(wddm->getDevice(), gdi->getWaitFromCpuArg().hDevice);
}

TEST_F(WddmResidencyControllerWithGdiAndMemoryManagerTest, trimToBudgetEvictsDoneFragmentsOnly) {
    gdi->setNonZeroNumBytesToTrimInEvict();
    void *ptr = reinterpret_cast<void *>(wddm->virtualAllocAddress + 0x1000);
    WddmAllocation allocation1(GraphicsAllocation::AllocationType::UNDECIDED, ptr, 0x1000, nullptr, MemoryPool::MemoryNull, false);
    WddmAllocation allocation2(GraphicsAllocation::AllocationType::UNDECIDED, ptr, 0x1000, nullptr, MemoryPool::MemoryNull, false);

    allocation1.getResidencyData().resident[osContextId] = true;
    allocation1.getResidencyData().updateCompletionData(0, osContextId);

    allocation2.getResidencyData().updateCompletionData(1, osContextId);
    allocation2.getResidencyData().resident[osContextId] = true;

    void *ptrTriple = reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(ptr) + 0x500);
    WddmAllocation *allocationTriple = static_cast<WddmAllocation *>(memoryManager->allocateGraphicsMemory(MockAllocationProperties{false, 2 * MemoryConstants::pageSize}, ptrTriple));

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

    WddmAllocation allocation1(GraphicsAllocation::AllocationType::UNDECIDED, ptr1, underlyingSize, nullptr, MemoryPool::MemoryNull, false);
    WddmAllocation allocation2(GraphicsAllocation::AllocationType::UNDECIDED, ptr2, underlyingSize, nullptr, MemoryPool::MemoryNull, false);
    WddmAllocation allocation3(GraphicsAllocation::AllocationType::UNDECIDED, ptr3, underlyingSize, nullptr, MemoryPool::MemoryNull, false);

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

TEST_F(WddmResidencyControllerWithGdiAndMemoryManagerTest, makeResidentResidencyAllocationsMarksAllocationsResident) {
    MockWddmAllocation allocation1, allocation2, allocation3, allocation4;
    ResidencyContainer residencyPack{&allocation1, &allocation2, &allocation3, &allocation4};

    residencyController->makeResidentResidencyAllocations(residencyPack);

    EXPECT_TRUE(allocation1.getResidencyData().resident[osContextId]);
    EXPECT_TRUE(allocation2.getResidencyData().resident[osContextId]);
    EXPECT_TRUE(allocation3.getResidencyData().resident[osContextId]);
    EXPECT_TRUE(allocation4.getResidencyData().resident[osContextId]);
}

TEST_F(WddmResidencyControllerWithGdiAndMemoryManagerTest, makeResidentResidencyAllocationsUpdatesLastFence) {
    MockWddmAllocation allocation1, allocation2, allocation3, allocation4;
    ResidencyContainer residencyPack{&allocation1, &allocation2, &allocation3, &allocation4};

    residencyController->getMonitoredFence().currentFenceValue = 20;

    residencyController->makeResidentResidencyAllocations(residencyPack);

    EXPECT_EQ(20u, allocation1.getResidencyData().getFenceValueForContextId(osContext->getContextId()));
    EXPECT_EQ(20u, allocation2.getResidencyData().getFenceValueForContextId(osContext->getContextId()));
    EXPECT_EQ(20u, allocation3.getResidencyData().getFenceValueForContextId(osContext->getContextId()));
    EXPECT_EQ(20u, allocation4.getResidencyData().getFenceValueForContextId(osContext->getContextId()));
}

TEST_F(WddmResidencyControllerWithGdiAndMemoryManagerTest, makeResidentResidencyAllocationsMarksTripleAllocationsResident) {
    MockWddmAllocation allocation1, allocation2;
    void *ptr = reinterpret_cast<void *>(wddm->virtualAllocAddress + 0x1500);

    WddmAllocation *allocationTriple = (WddmAllocation *)memoryManager->allocateGraphicsMemory(MockAllocationProperties{false, 2 * MemoryConstants::pageSize}, ptr);
    ResidencyContainer residencyPack{&allocation1, allocationTriple, &allocation2};

    residencyController->makeResidentResidencyAllocations(residencyPack);

    for (uint32_t i = 0; i < allocationTriple->fragmentsStorage.fragmentCount; i++) {
        EXPECT_TRUE(allocationTriple->fragmentsStorage.fragmentStorageData[i].residency->resident[osContextId]);
    }

    EXPECT_EQ(5u, gdi->getMakeResidentArg().NumAllocations);

    memoryManager->freeGraphicsMemory(allocationTriple);
}

TEST_F(WddmResidencyControllerWithGdiAndMemoryManagerTest, makeResidentResidencyAllocationsSetsLastFencePLusOneForTripleAllocations) {
    MockWddmAllocation allocation1, allocation2;

    WddmAllocation *allocationTriple = (WddmAllocation *)memoryManager->allocateGraphicsMemory(MockAllocationProperties{false, 2 * MemoryConstants::pageSize}, reinterpret_cast<void *>(0x1500));

    residencyController->getMonitoredFence().currentFenceValue = 20;

    ResidencyContainer residencyPack{&allocation1, allocationTriple, &allocation2};
    residencyController->makeResidentResidencyAllocations(residencyPack);

    for (uint32_t i = 0; i < allocationTriple->fragmentsStorage.fragmentCount; i++) {
        EXPECT_EQ(20u, allocationTriple->fragmentsStorage.fragmentStorageData[i].residency->getFenceValueForContextId(0));
    }

    memoryManager->freeGraphicsMemory(allocationTriple);
}

TEST_F(WddmResidencyControllerWithMockWddmTest, givenMakeResidentFailsWhenCallingMakeResidentResidencyAllocationsThenDontMarkAllocationsAsResident) {
    MockWddmAllocation allocation1, allocation2, allocation3, allocation4;

    auto makeResidentWithOutBytesToTrim = [](const D3DKMT_HANDLE *handles, uint32_t count, bool cantTrimFurther, uint64_t *numberOfBytesToTrim) -> bool { *numberOfBytesToTrim = 4 * 4096;  return false; };

    ON_CALL(*wddm, makeResident(::testing::_, ::testing::_, ::testing::_, ::testing::_)).WillByDefault(::testing::Invoke(makeResidentWithOutBytesToTrim));
    EXPECT_CALL(*wddm, makeResident(::testing::_, ::testing::_, ::testing::_, ::testing::_)).Times(2);

    ResidencyContainer residencyPack{&allocation1, &allocation2, &allocation3, &allocation4};
    bool result = residencyController->makeResidentResidencyAllocations(residencyPack);

    EXPECT_FALSE(result);

    EXPECT_FALSE(allocation1.getResidencyData().resident[osContextId]);
    EXPECT_FALSE(allocation2.getResidencyData().resident[osContextId]);
    EXPECT_FALSE(allocation3.getResidencyData().resident[osContextId]);
    EXPECT_FALSE(allocation4.getResidencyData().resident[osContextId]);
}

TEST_F(WddmResidencyControllerWithMockWddmTest, givenMakeResidentFailsWhenCallingMakeResidentResidencyAllocationsThenDontMarkTripleAllocationsAsResident) {
    MockWddmAllocation allocation1, allocation2;
    void *ptr = reinterpret_cast<void *>(wddm->getWddmMinAddress() + 0x1500);
    WddmAllocation *allocationTriple = static_cast<WddmAllocation *>(memoryManager->allocateGraphicsMemory(MockAllocationProperties{false, 2 * MemoryConstants::pageSize}, ptr));
    ASSERT_NE(nullptr, allocationTriple);

    auto makeResidentWithOutBytesToTrim = [](const D3DKMT_HANDLE *handles, uint32_t count, bool cantTrimFurther, uint64_t *numberOfBytesToTrim) -> bool { *numberOfBytesToTrim = 4 * 4096;  return false; };

    ON_CALL(*wddm, makeResident(::testing::_, ::testing::_, ::testing::_, ::testing::_)).WillByDefault(::testing::Invoke(makeResidentWithOutBytesToTrim));
    EXPECT_CALL(*wddm, makeResident(::testing::_, ::testing::_, ::testing::_, ::testing::_)).Times(2);

    ResidencyContainer residencyPack{&allocation1, allocationTriple, &allocation2};
    bool result = residencyController->makeResidentResidencyAllocations(residencyPack);

    EXPECT_FALSE(result);

    for (uint32_t i = 0; i < allocationTriple->fragmentsStorage.fragmentCount; i++) {
        EXPECT_FALSE(allocationTriple->fragmentsStorage.fragmentStorageData[i].residency->resident[osContextId]);
    }

    memoryManager->freeGraphicsMemory(allocationTriple);
}

TEST_F(WddmResidencyControllerWithMockWddmTest, givenMakeResidentFailsWhenCallingMakeResidentResidencyAllocationsThenCallItAgainWithCantTrimFurtherSetToTrue) {
    MockWddmAllocation allocation1;

    auto makeResidentWithOutBytesToTrim = [](const D3DKMT_HANDLE *handles, uint32_t count, bool cantTrimFurther, uint64_t *numberOfBytesToTrim) -> bool { *numberOfBytesToTrim = 4 * 4096;  return false; };

    ON_CALL(*wddm, makeResident(::testing::_, ::testing::_, ::testing::_, ::testing::_)).WillByDefault(::testing::Invoke(makeResidentWithOutBytesToTrim));
    EXPECT_CALL(*wddm, makeResident(::testing::_, ::testing::_, false, ::testing::_)).Times(1);
    EXPECT_CALL(*wddm, makeResident(::testing::_, ::testing::_, true, ::testing::_)).Times(1);

    ResidencyContainer residencyPack{&allocation1};
    bool result = residencyController->makeResidentResidencyAllocations(residencyPack);

    EXPECT_FALSE(result);
}

TEST_F(WddmResidencyControllerWithMockWddmTest, givenAllocationPackPassedWhenCallingMakeResidentResidencyAllocationsThenItIsUsed) {
    MockWddmAllocation allocation1;
    MockWddmAllocation allocation2;
    allocation1.handle = 1;
    allocation2.handle = 2;
    ResidencyContainer residencyPack{&allocation1, &allocation2};

    auto makeResidentWithOutBytesToTrim = [](const D3DKMT_HANDLE *handles, uint32_t count, bool cantTrimFurther, uint64_t *numberOfBytesToTrim) -> bool {
        EXPECT_EQ(1, handles[0]);
        EXPECT_EQ(2, handles[1]);
        return true;
    };
    ON_CALL(*wddm, makeResident(::testing::_, ::testing::_, ::testing::_, ::testing::_)).WillByDefault(::testing::Invoke(makeResidentWithOutBytesToTrim));
    EXPECT_CALL(*wddm, makeResident(::testing::_, 2, false, ::testing::_)).Times(1);

    bool result = residencyController->makeResidentResidencyAllocations(residencyPack);
    EXPECT_TRUE(result);
}

TEST_F(WddmResidencyControllerWithMockWddmTest, givenMakeResidentFailsAndTrimToBudgetSuceedsWhenCallingMakeResidentResidencyAllocationsThenSucceed) {
    MockWddmAllocation allocation1;
    void *cpuPtr = reinterpret_cast<void *>(wddm->getWddmMinAddress() + 0x1000);
    size_t allocationSize = 0x1000;
    WddmAllocation allocationToTrim(GraphicsAllocation::AllocationType::UNDECIDED, cpuPtr, allocationSize, nullptr, MemoryPool::MemoryNull, false);

    allocationToTrim.getResidencyData().updateCompletionData(residencyController->getMonitoredFence().lastSubmittedFence, osContext->getContextId());

    auto makeResidentWithOutBytesToTrim = [allocationSize](const D3DKMT_HANDLE *handles, uint32_t count, bool cantTrimFurther, uint64_t *numberOfBytesToTrim) -> bool { *numberOfBytesToTrim = allocationSize;  return false; };

    EXPECT_CALL(*wddm, makeResident(::testing::_, ::testing::_, ::testing::_, ::testing::_)).Times(2).WillOnce(::testing::Invoke(makeResidentWithOutBytesToTrim)).WillOnce(::testing::Return(true));

    residencyController->addToTrimCandidateList(&allocationToTrim);

    ResidencyContainer residencyPack{&allocation1};
    bool result = residencyController->makeResidentResidencyAllocations(residencyPack);

    EXPECT_TRUE(result);

    EXPECT_TRUE(allocation1.getResidencyData().resident[osContextId]);
}

TEST_F(WddmResidencyControllerWithMockWddmTest, givenMakeResidentFailsWhenCallingMakeResidentResidencyAllocationsThenMemoryBudgetExhaustedIsSetToTrue) {
    MockWddmAllocation allocation1;
    ResidencyContainer residencyPack{&allocation1};

    auto makeResidentThatFails = [](const D3DKMT_HANDLE *handles, uint32_t count, bool cantTrimFurther, uint64_t *numberOfBytesToTrim) -> bool { return false; };
    auto makeResidentThatSucceds = [](const D3DKMT_HANDLE *handles, uint32_t count, bool cantTrimFurther, uint64_t *numberOfBytesToTrim) -> bool { return true; };

    EXPECT_CALL(*wddm, makeResident(::testing::_, ::testing::_, ::testing::_, ::testing::_)).Times(2).WillOnce(::testing::Invoke(makeResidentThatFails)).WillOnce(::testing::Invoke(makeResidentThatSucceds));

    residencyController->makeResidentResidencyAllocations(residencyPack);
    EXPECT_TRUE(residencyController->isMemoryBudgetExhausted());
}
