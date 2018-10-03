/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/os_interface/windows/wddm_residency_controller.h"
#include "unit_tests/os_interface/windows/mock_wddm_allocation.h"

#include "test.h"
#include <memory>

using namespace OCLRT;

class MockWddmResidencyController : public WddmResidencyController {
  public:
    using WddmResidencyController::trimCandidateList;
    using WddmResidencyController::trimCandidatesCount;
};

struct WddmResidencyControllerTest : ::testing::Test {
    void SetUp() {
        residencyController = std::make_unique<MockWddmResidencyController>();
    }

    std::unique_ptr<MockWddmResidencyController> residencyController;
};

TEST_F(WddmResidencyControllerTest, addToTrimCandidateListPlacesAllocationInContainerAndAssignsPosition) {
    MockWddmAllocation allocation;
    residencyController->addToTrimCandidateList(&allocation);

    EXPECT_NE(0u, residencyController->trimCandidateList.size());
    EXPECT_NE(trimListUnusedPosition, allocation.getTrimCandidateListPosition());

    size_t position = allocation.getTrimCandidateListPosition();
    ASSERT_LT(position, residencyController->trimCandidateList.size());

    EXPECT_EQ(&allocation, residencyController->trimCandidateList[position]);
}

TEST_F(WddmResidencyControllerTest, addToTrimCandidateListDoesNotInsertAllocationAlreadyOnTheList) {
    MockWddmAllocation allocation;

    residencyController->trimCandidateList.resize(0);

    residencyController->addToTrimCandidateList(&allocation);

    EXPECT_NE(trimListUnusedPosition, allocation.getTrimCandidateListPosition());

    size_t position = allocation.getTrimCandidateListPosition();
    ASSERT_LT(position, residencyController->trimCandidateList.size());

    EXPECT_EQ(&allocation, residencyController->trimCandidateList[position]);

    size_t previousSize = residencyController->trimCandidateList.size();
    residencyController->addToTrimCandidateList(&allocation);

    EXPECT_EQ(previousSize, residencyController->trimCandidateList.size());
    EXPECT_EQ(position, allocation.getTrimCandidateListPosition());
}

TEST_F(WddmResidencyControllerTest, removeFromTrimCandidateListAssignsUnusedPosition) {
    MockWddmAllocation allocation;

    residencyController->addToTrimCandidateList(&allocation);
    residencyController->removeFromTrimCandidateList(&allocation, false);

    EXPECT_EQ(trimListUnusedPosition, allocation.getTrimCandidateListPosition());
}

TEST_F(WddmResidencyControllerTest, removeFromTrimCandidateListRemovesAllocationInAssignedPosition) {
    MockWddmAllocation allocation;

    residencyController->addToTrimCandidateList(&allocation);
    size_t position = allocation.getTrimCandidateListPosition();

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
    EXPECT_NE(allocation1.getTrimCandidateListPosition(), allocation2.getTrimCandidateListPosition());
    EXPECT_NE(allocation2.getTrimCandidateListPosition(), allocation3.getTrimCandidateListPosition());
}

TEST_F(WddmResidencyControllerTest, DISABLED_removingNotLastAllocationFromTrimCandidateListSubstituesLastPositionAllocation) {
    MockWddmAllocation allocation1, allocation2, allocation3;

    residencyController->addToTrimCandidateList(&allocation1);
    residencyController->addToTrimCandidateList(&allocation2);
    residencyController->addToTrimCandidateList(&allocation3);

    residencyController->removeFromTrimCandidateList(&allocation2, false);

    EXPECT_EQ(2u, residencyController->trimCandidateList.size());

    EXPECT_EQ(2u, allocation3.getTrimCandidateListPosition());
    EXPECT_NE(allocation2.getTrimCandidateListPosition(), allocation3.getTrimCandidateListPosition());
}

TEST_F(WddmResidencyControllerTest, removingNotLastAllocationFromTrimCandidateListPutsNullEntry) {
    MockWddmAllocation allocation1, allocation2, allocation3;

    residencyController->addToTrimCandidateList(&allocation1);
    residencyController->addToTrimCandidateList(&allocation2);
    residencyController->addToTrimCandidateList(&allocation3);
    size_t position2 = allocation2.getTrimCandidateListPosition();
    size_t position3 = allocation3.getTrimCandidateListPosition();

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

    size_t position3 = allocation3.getTrimCandidateListPosition();
    size_t position4 = allocation4.getTrimCandidateListPosition();

    residencyController->removeFromTrimCandidateList(&allocation2, false);
    residencyController->removeFromTrimCandidateList(&allocation1, false);

    EXPECT_EQ(4u, residencyController->trimCandidateList.size());

    residencyController->compactTrimCandidateList();

    EXPECT_EQ(2u, residencyController->trimCandidateList.size());

    EXPECT_EQ(residencyController->trimCandidateList[0], &allocation3);
    EXPECT_EQ(0u, allocation3.getTrimCandidateListPosition());

    EXPECT_EQ(residencyController->trimCandidateList[1], &allocation4);
    EXPECT_EQ(1u, allocation4.getTrimCandidateListPosition());
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
