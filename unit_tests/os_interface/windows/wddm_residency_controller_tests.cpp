/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/execution_environment/execution_environment.h"
#include "runtime/os_interface/os_context.h"
#include "runtime/os_interface/windows/os_context_win.h"
#include "runtime/os_interface/os_interface.h"
#include "runtime/os_interface/windows/os_interface.h"
#include "runtime/os_interface/windows/wddm_residency_controller.h"
#include "unit_tests/mocks/mock_wddm.h"
#include "unit_tests/os_interface/windows/mock_gdi_interface.h"
#include "unit_tests/os_interface/windows/mock_wddm_allocation.h"
#include "unit_tests/os_interface/windows/mock_wddm_memory_manager.h"

#include "test.h"
#include <memory>

using namespace OCLRT;

class MockWddmResidencyController : public WddmResidencyController {
  public:
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
        wddm->init();
        residencyController = std::make_unique<MockWddmResidencyController>(*wddm, osContextId);
    }

    std::unique_ptr<WddmMock> wddm;
    std::unique_ptr<MockWddmResidencyController> residencyController;
};

struct WddmResidencyControllerWithGdiTest : public WddmResidencyControllerTest {
    void SetUp() {
        WddmResidencyControllerTest::SetUp();
        gdi = new MockGdi();
        wddm->gdi.reset(gdi);
        wddm->getWddmInterface()->createMonitoredFence(*residencyController);
    }

    MockGdi *gdi;
};

struct WddmResidencyControllerWithGdiAndMemoryManagerTest : ::testing::Test {
    const uint32_t osContextId = 0u;

    void SetUp() {
        wddm = static_cast<WddmMock *>(Wddm::createWddm());
        wddm->init();
        gdi = new MockGdi();
        wddm->gdi.reset(gdi);

        osInterface = std::make_unique<OSInterface>();
        osInterface->get()->setWddm(wddm);
        osContext = new OsContext(osInterface.get(), osContextId);
        osContext->incRefInternal();
        residencyController = &osContext->get()->getResidencyController();

        executionEnvironment = std::make_unique<ExecutionEnvironment>();

        memoryManager = std::make_unique<MockWddmMemoryManager>(wddm, *executionEnvironment);
        memoryManager->registerOsContext(osContext);
    }

    void TearDown() {
        osContext->decRefInternal();
    }

    std::unique_ptr<OSInterface> osInterface;
    std::unique_ptr<ExecutionEnvironment> executionEnvironment;
    std::unique_ptr<MockWddmMemoryManager> memoryManager;

    WddmMock *wddm = nullptr;
    OsContext *osContext = nullptr;
    MockGdi *gdi = nullptr;
    WddmResidencyController *residencyController = nullptr;
};

TEST(WddmResidencyController, givenWddmResidencyControllerWhenItIsConstructedThenRegisterTrimCallback) {
    ExecutionEnvironment executionEnvironment;
    auto gdi = new MockGdi();
    auto wddm = std::unique_ptr<WddmMock>{static_cast<WddmMock *>(Wddm::createWddm())};
    wddm->gdi.reset(gdi);
    wddm->init();

    std::memset(&gdi->getRegisterTrimNotificationArg(), 0, sizeof(D3DKMT_REGISTERTRIMNOTIFICATION));
    WddmResidencyController residencyController{*wddm, 0u};

    EXPECT_EQ(reinterpret_cast<PFND3DKMT_TRIMNOTIFICATIONCALLBACK>(WddmMemoryManager::trimCallback), gdi->getRegisterTrimNotificationArg().Callback);
    EXPECT_EQ(reinterpret_cast<void *>(&residencyController), gdi->getRegisterTrimNotificationArg().Context);
    EXPECT_EQ(wddm->getDevice(), gdi->getRegisterTrimNotificationArg().hDevice);
}

TEST_F(WddmResidencyControllerWithGdiTest, givenWddmResidencyControllerWhenItIsDestructedThenUnregisterTrimCallback) {
    auto trimCallbackHandle = residencyController->trimCallbackHandle;
    auto trimCallbackAddress = reinterpret_cast<PFND3DKMT_TRIMNOTIFICATIONCALLBACK>(WddmMemoryManager::trimCallback);

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

    size_t position3 = allocation3.getTrimCandidateListPosition(osContextId);
    size_t position4 = allocation4.getTrimCandidateListPosition(osContextId);

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

    allocation1.getResidencyData().resident = true;
    allocation2.getResidencyData().resident = true;

    // Set last periodic fence value
    residencyController->setLastTrimFenceValue(10);
    residencyController->setLastTrimFenceValue(10);
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
    EXPECT_FALSE(allocation1.getResidencyData().resident);
    EXPECT_FALSE(allocation2.getResidencyData().resident);
}

TEST_F(WddmResidencyControllerWithGdiTest, givenOneUsedAllocationFromPreviousPeriodicTrimWhenTrimResidencyPeriodicTrimIsCalledThenOneAllocationIsTrimmed) {
    D3DKMT_TRIMNOTIFICATION trimNotification = {0};
    trimNotification.Flags.PeriodicTrim = 1;
    trimNotification.NumBytesToTrim = 0;

    // allocations have fence value == 0 by default
    MockWddmAllocation allocation1, allocation2;
    allocation1.getResidencyData().resident = true;
    // mark allocation used from last periodic trim
    allocation1.getResidencyData().updateCompletionData(0, osContextId);
    allocation2.getResidencyData().updateCompletionData(11, osContextId);
    allocation2.getResidencyData().resident = true;

    // Set last periodic fence value
    residencyController->setLastTrimFenceValue(10);
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
    EXPECT_FALSE(allocation1.getResidencyData().resident);
    // second stays resident
    EXPECT_TRUE(allocation2.getResidencyData().resident);
}

TEST_F(WddmResidencyControllerWithGdiAndMemoryManagerTest, givenTripleAllocationWithUsedAndUnusedFragmentsSincePreviousTrimWhenTrimResidencyPeriodicTrimIsCalledThenProperFragmentsAreEvictedAndMarked) {
    D3DKMT_TRIMNOTIFICATION trimNotification = {0};
    trimNotification.Flags.PeriodicTrim = 1;
    trimNotification.NumBytesToTrim = 0;

    // 3-fragment Allocation
    void *ptr = reinterpret_cast<void *>(wddm->virtualAllocAddress + 0x1500);
    auto allocationTriple = static_cast<WddmAllocation *>(memoryManager->allocateGraphicsMemory(8196, ptr));

    // whole allocation unused since previous trim
    allocationTriple->getResidencyData().updateCompletionData(0, osContextId);

    EXPECT_EQ(3u, allocationTriple->fragmentsStorage.fragmentCount);

    allocationTriple->fragmentsStorage.fragmentStorageData[0].residency->updateCompletionData(0, osContextId);
    allocationTriple->fragmentsStorage.fragmentStorageData[0].residency->resident = true;
    // this fragment was used
    allocationTriple->fragmentsStorage.fragmentStorageData[1].residency->updateCompletionData(11, osContextId);
    allocationTriple->fragmentsStorage.fragmentStorageData[0].residency->resident = true;

    allocationTriple->fragmentsStorage.fragmentStorageData[2].residency->updateCompletionData(0, osContextId);
    allocationTriple->fragmentsStorage.fragmentStorageData[2].residency->resident = true;

    // Set last periodic fence value
    residencyController->setLastTrimFenceValue(10);
    // Set current fence value to greater value
    residencyController->getMonitoredFence().currentFenceValue = 20;

    wddm->makeNonResidentResult.called = 0;

    residencyController->addToTrimCandidateList(allocationTriple);

    residencyController->trimResidency(trimNotification.Flags, trimNotification.NumBytesToTrim);

    // 2 fragments evicted with one call
    EXPECT_EQ(1u, wddm->makeNonResidentResult.called);
    // marked nonresident
    EXPECT_FALSE(allocationTriple->fragmentsStorage.fragmentStorageData[0].residency->resident);
    EXPECT_FALSE(allocationTriple->fragmentsStorage.fragmentStorageData[2].residency->resident);

    memoryManager->freeGraphicsMemory(allocationTriple);
}

TEST_F(WddmResidencyControllerWithGdiTest, givenPeriodicTrimWhenTrimCallbackCalledThenLastPeriodicTrimFenceIsSetToCurrentFenceValue) {
    D3DKMT_TRIMNOTIFICATION trimNotification = {0};
    trimNotification.Flags.PeriodicTrim = 1;
    trimNotification.NumBytesToTrim = 0;

    // Set last periodic fence value
    residencyController->setLastTrimFenceValue(10);
    // Set current fence value to greater value
    *residencyController->getMonitoredFence().cpuAddress = 20;

    residencyController->trimResidency(trimNotification.Flags, trimNotification.NumBytesToTrim);

    EXPECT_EQ(20u, residencyController->getLastTrimFenceValue());
}

TEST_F(WddmResidencyControllerWithGdiTest, givenRestartPeriodicTrimWhenTrimCallbackCalledThenLastPeriodicTrimFenceIsSetToCurrentFenceValue) {
    D3DKMT_TRIMNOTIFICATION trimNotification = {0};
    trimNotification.Flags.RestartPeriodicTrim = 1;
    trimNotification.NumBytesToTrim = 0;

    // Set last periodic fence value
    residencyController->setLastTrimFenceValue(10);
    // Set current fence value to greater value
    *residencyController->getMonitoredFence().cpuAddress = 20;

    residencyController->trimResidency(trimNotification.Flags, trimNotification.NumBytesToTrim);

    EXPECT_EQ(20u, residencyController->getLastTrimFenceValue());
}

TEST_F(WddmResidencyControllerWithGdiTest, trimToBudgetWithZeroSizeReturnsTrue) {
    bool status = residencyController->trimResidencyToBudget(0);

    EXPECT_TRUE(status);
}

TEST_F(WddmResidencyControllerWithGdiTest, trimToBudgetAllDoneAllocations) {
    gdi->setNonZeroNumBytesToTrimInEvict();

    MockWddmAllocation allocation1, allocation2, allocation3;

    allocation1.getResidencyData().resident = true;
    allocation1.getResidencyData().updateCompletionData(0, osContextId);

    allocation2.getResidencyData().updateCompletionData(1, osContextId);
    allocation2.getResidencyData().resident = true;

    allocation3.getResidencyData().updateCompletionData(2, osContextId);
    allocation3.getResidencyData().resident = true;

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

    allocation1.getResidencyData().resident = true;
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
    WddmAllocation allocation1(reinterpret_cast<void *>(0x1000), 0x1000, nullptr, MemoryPool::MemoryNull, 1u),
        allocation2(reinterpret_cast<void *>(0x1000), 0x3000, nullptr, MemoryPool::MemoryNull, 1u),
        allocation3(reinterpret_cast<void *>(0x1000), 0x1000, nullptr, MemoryPool::MemoryNull, 1u);

    allocation1.getResidencyData().resident = true;
    allocation1.getResidencyData().updateCompletionData(0, osContextId);

    allocation2.getResidencyData().updateCompletionData(1, osContextId);
    allocation2.getResidencyData().resident = true;

    allocation3.getResidencyData().updateCompletionData(2, osContextId);
    allocation3.getResidencyData().resident = true;

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

    allocation1.getResidencyData().resident = true;
    allocation1.getResidencyData().updateCompletionData(0, osContextId);

    allocation2.getResidencyData().updateCompletionData(1, osContextId);
    allocation2.getResidencyData().resident = true;

    allocation3.getResidencyData().updateCompletionData(2, osContextId);
    allocation3.getResidencyData().resident = true;

    *residencyController->getMonitoredFence().cpuAddress = 1;
    residencyController->getMonitoredFence().lastSubmittedFence = 1;
    residencyController->getMonitoredFence().currentFenceValue = 1;

    wddm->makeNonResidentResult.called = 0;

    residencyController->addToTrimCandidateList(&allocation1);
    residencyController->addToTrimCandidateList(&allocation2);
    residencyController->addToTrimCandidateList(&allocation3);

    bool status = residencyController->trimResidencyToBudget(3 * 4096);

    EXPECT_FALSE(allocation1.getResidencyData().resident);
    EXPECT_FALSE(allocation2.getResidencyData().resident);
    EXPECT_TRUE(allocation3.getResidencyData().resident);
}

TEST_F(WddmResidencyControllerWithGdiTest, trimToBudgetWaitsFromCpuWhenLastFenceIsGreaterThanMonitored) {
    gdi->setNonZeroNumBytesToTrimInEvict();

    MockWddmAllocation allocation1;

    allocation1.getResidencyData().resident = true;
    allocation1.getResidencyData().updateCompletionData(2, osContextId);

    *residencyController->getMonitoredFence().cpuAddress = 1;
    residencyController->getMonitoredFence().lastSubmittedFence = 2;
    residencyController->getMonitoredFence().currentFenceValue = 3;

    wddm->makeNonResidentResult.called = 0;
    wddm->waitFromCpuResult.called = 0;

    residencyController->addToTrimCandidateList(&allocation1);

    gdi->getWaitFromCpuArg().hDevice = (D3DKMT_HANDLE)0;

    bool status = residencyController->trimResidencyToBudget(3 * 4096);

    EXPECT_EQ(1u, wddm->makeNonResidentResult.called);
    EXPECT_FALSE(allocation1.getResidencyData().resident);

    EXPECT_EQ(wddm->getDevice(), gdi->getWaitFromCpuArg().hDevice);
}

TEST_F(WddmResidencyControllerWithGdiAndMemoryManagerTest, trimToBudgetEvictsDoneFragmentsOnly) {
    gdi->setNonZeroNumBytesToTrimInEvict();
    void *ptr = reinterpret_cast<void *>(wddm->virtualAllocAddress + 0x1000);
    WddmAllocation allocation1(ptr, 0x1000, nullptr, MemoryPool::MemoryNull, 1u);
    WddmAllocation allocation2(ptr, 0x1000, nullptr, MemoryPool::MemoryNull, 1u);

    allocation1.getResidencyData().resident = true;
    allocation1.getResidencyData().updateCompletionData(0, osContextId);

    allocation2.getResidencyData().updateCompletionData(1, osContextId);
    allocation2.getResidencyData().resident = true;

    void *ptrTriple = reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(ptr) + 0x500);
    WddmAllocation *allocationTriple = static_cast<WddmAllocation *>(memoryManager->allocateGraphicsMemory(8196, ptrTriple));

    allocationTriple->getResidencyData().updateCompletionData(1, osContextId);
    allocationTriple->getResidencyData().resident = true;

    EXPECT_EQ(3u, allocationTriple->fragmentsStorage.fragmentCount);

    for (uint32_t i = 0; i < 3; i++) {
        allocationTriple->fragmentsStorage.fragmentStorageData[i].residency->updateCompletionData(1, osContextId);
        allocationTriple->fragmentsStorage.fragmentStorageData[i].residency->resident = true;
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

    bool status = residencyController->trimResidencyToBudget(3 * 4096);

    EXPECT_EQ(2u, wddm->makeNonResidentResult.called);

    EXPECT_FALSE(allocationTriple->fragmentsStorage.fragmentStorageData[0].residency->resident);
    EXPECT_TRUE(allocationTriple->fragmentsStorage.fragmentStorageData[1].residency->resident);
    EXPECT_FALSE(allocationTriple->fragmentsStorage.fragmentStorageData[2].residency->resident);

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

    WddmAllocation allocation1(ptr1, underlyingSize, nullptr, MemoryPool::MemoryNull, 1u);
    WddmAllocation allocation2(ptr2, underlyingSize, nullptr, MemoryPool::MemoryNull, 1u);
    WddmAllocation allocation3(ptr3, underlyingSize, nullptr, MemoryPool::MemoryNull, 1u);

    allocation1.getResidencyData().resident = true;
    allocation1.getResidencyData().updateCompletionData(0, osContextId);

    allocation2.getResidencyData().updateCompletionData(1, osContextId);
    allocation2.getResidencyData().resident = true;

    allocation3.getResidencyData().updateCompletionData(1, osContextId);
    allocation3.getResidencyData().resident = true;

    *residencyController->getMonitoredFence().cpuAddress = 1;
    residencyController->getMonitoredFence().lastSubmittedFence = 1;
    residencyController->getMonitoredFence().currentFenceValue = 1;

    wddm->makeNonResidentResult.called = 0;

    residencyController->addToTrimCandidateList(&allocation1);
    residencyController->addToTrimCandidateList(&allocation2);
    residencyController->addToTrimCandidateList(&allocation3);

    bool status = residencyController->trimResidencyToBudget(budget);
    EXPECT_TRUE(status);

    EXPECT_FALSE(allocation1.getResidencyData().resident);
    EXPECT_FALSE(allocation2.getResidencyData().resident);
    EXPECT_TRUE(allocation3.getResidencyData().resident);
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
