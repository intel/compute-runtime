/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/mocks/linux/mock_drm_memory_manager.h"
#include "shared/test/common/mocks/linux/mock_numa_library.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/test.h"

#include "gtest/gtest.h"

using namespace NEO;

class DrmMemoryManagerNumaTest : public ::testing::Test {
  public:
    void SetUp() override {
        debugManager.flags.EnableNumaAwareHostUsmPlacement.set(1);
    }

    void createMemoryManager(std::initializer_list<int> deviceNumaNodes) {
        executionEnvironment = std::make_unique<MockExecutionEnvironment>(defaultHwInfo.get(), false, static_cast<uint32_t>(deviceNumaNodes.size()));
        uint32_t rootDeviceIndex = 0u;
        for (auto numaNode : deviceNumaNodes) {
            auto &rootDeviceEnvironment = *executionEnvironment->rootDeviceEnvironments[rootDeviceIndex++];
            auto drm = new DrmMock(rootDeviceEnvironment);
            drm->numaNode = numaNode;
            rootDeviceEnvironment.osInterface = std::make_unique<OSInterface>();
            rootDeviceEnvironment.osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
        }
        memoryManager = std::make_unique<TestedDrmMemoryManager>(false, false, false, *executionEnvironment);
    }

    DebugManagerStateRestore restorer;
    std::unique_ptr<MockExecutionEnvironment> executionEnvironment;
    std::unique_ptr<TestedDrmMemoryManager> memoryManager;
};

TEST_F(DrmMemoryManagerNumaTest, givenPolicyNodeWithDeviceWhenGettingRootDeviceIndexClosestToPreferredNumaNodeThenFirstDeviceOnThatNodeIsReturned) {
    MockNumaLibrary numaLibrary(true);
    createMemoryManager({0, 0, 1, 1});
    numaLibrary.setPolicy(MockNumaLibrary::mpolBind, {1u});

    RootDeviceIndicesContainer rootDeviceIndices = {0u, 1u, 2u, 3u};
    EXPECT_EQ(std::optional<uint32_t>(2u), memoryManager->getRootDeviceIndexClosestToPreferredNumaNode(rootDeviceIndices));

    RootDeviceIndicesContainer reversedRootDeviceIndices = {3u, 2u, 1u, 0u};
    EXPECT_EQ(std::optional<uint32_t>(3u), memoryManager->getRootDeviceIndexClosestToPreferredNumaNode(reversedRootDeviceIndices));

    numaLibrary.setPolicy(MockNumaLibrary::mpolPreferred, {0u});
    EXPECT_EQ(std::optional<uint32_t>(0u), memoryManager->getRootDeviceIndexClosestToPreferredNumaNode(rootDeviceIndices));
}

TEST_F(DrmMemoryManagerNumaTest, givenPolicyNodeWithoutDeviceWhenGettingRootDeviceIndexClosestToPreferredNumaNodeThenDeviceOnNearestNodeIsReturned) {
    MockNumaLibrary numaLibrary(true);
    createMemoryManager({0, 1});
    MockNumaLibrary::distances[2][0] = 32;
    MockNumaLibrary::distances[2][1] = 12;
    numaLibrary.setPolicy(MockNumaLibrary::mpolBind, {2u});

    RootDeviceIndicesContainer rootDeviceIndices = {0u, 1u};
    EXPECT_EQ(std::optional<uint32_t>(1u), memoryManager->getRootDeviceIndexClosestToPreferredNumaNode(rootDeviceIndices));
}

TEST_F(DrmMemoryManagerNumaTest, givenPolicyNodeWithoutDeviceAndUnknownDistanceWhenGettingRootDeviceIndexClosestToPreferredNumaNodeThenNoIndexIsReturned) {
    MockNumaLibrary numaLibrary(false);
    createMemoryManager({0, 1});
    numaLibrary.setPolicy(MockNumaLibrary::mpolBind, {2u});

    RootDeviceIndicesContainer rootDeviceIndices = {0u, 1u};
    EXPECT_EQ(std::nullopt, memoryManager->getRootDeviceIndexClosestToPreferredNumaNode(rootDeviceIndices));
}

TEST_F(DrmMemoryManagerNumaTest, givenDeviceWithUnknownNumaNodeWhenGettingRootDeviceIndexClosestToPreferredNumaNodeThenItIsSkipped) {
    MockNumaLibrary numaLibrary(true);
    createMemoryManager({-1, -1, 0, 1});
    numaLibrary.setPolicy(MockNumaLibrary::mpolBind, {0u});

    RootDeviceIndicesContainer rootDeviceIndices = {0u, 1u, 2u};
    EXPECT_EQ(std::optional<uint32_t>(2u), memoryManager->getRootDeviceIndexClosestToPreferredNumaNode(rootDeviceIndices));

    RootDeviceIndicesContainer unknownNodeRootDeviceIndices = {0u, 1u};
    EXPECT_EQ(std::nullopt, memoryManager->getRootDeviceIndexClosestToPreferredNumaNode(unknownNodeRootDeviceIndices));
}

TEST_F(DrmMemoryManagerNumaTest, givenPolicyWithoutSingleNodeWhenGettingRootDeviceIndexClosestToPreferredNumaNodeThenNoIndexIsReturned) {
    MockNumaLibrary numaLibrary(true);
    createMemoryManager({0, 1});
    RootDeviceIndicesContainer rootDeviceIndices = {0u, 1u};

    numaLibrary.setPolicy(MockNumaLibrary::mpolDefault, {});
    EXPECT_EQ(std::nullopt, memoryManager->getRootDeviceIndexClosestToPreferredNumaNode(rootDeviceIndices));

    numaLibrary.setPolicy(MockNumaLibrary::mpolInterleave, {1u});
    EXPECT_EQ(std::nullopt, memoryManager->getRootDeviceIndexClosestToPreferredNumaNode(rootDeviceIndices));

    numaLibrary.setPolicy(MockNumaLibrary::mpolBind, {0u, 1u});
    EXPECT_EQ(std::nullopt, memoryManager->getRootDeviceIndexClosestToPreferredNumaNode(rootDeviceIndices));
}

TEST_F(DrmMemoryManagerNumaTest, givenSingleRootDeviceIndexWhenGettingRootDeviceIndexClosestToPreferredNumaNodeThenNoIndexIsReturned) {
    MockNumaLibrary numaLibrary(true);
    createMemoryManager({0, 1});
    numaLibrary.setPolicy(MockNumaLibrary::mpolBind, {1u});

    RootDeviceIndicesContainer rootDeviceIndices = {1u};
    EXPECT_EQ(std::nullopt, memoryManager->getRootDeviceIndexClosestToPreferredNumaNode(rootDeviceIndices));
}

TEST_F(DrmMemoryManagerNumaTest, givenPlacementNotEnabledByDebugKeyOrMemPolicyPassedToKmdWhenGettingRootDeviceIndexClosestToPreferredNumaNodeThenNoIndexIsReturned) {
    MockNumaLibrary numaLibrary(true);
    createMemoryManager({0, 1});
    numaLibrary.setPolicy(MockNumaLibrary::mpolBind, {1u});
    RootDeviceIndicesContainer rootDeviceIndices = {0u, 1u};

    debugManager.flags.EnableNumaAwareHostUsmPlacement.set(-1);
    EXPECT_EQ(std::nullopt, memoryManager->getRootDeviceIndexClosestToPreferredNumaNode(rootDeviceIndices));

    debugManager.flags.EnableNumaAwareHostUsmPlacement.set(0);
    EXPECT_EQ(std::nullopt, memoryManager->getRootDeviceIndexClosestToPreferredNumaNode(rootDeviceIndices));

    debugManager.flags.EnableNumaAwareHostUsmPlacement.set(1);
    debugManager.flags.EnableHostAllocationMemPolicy.set(true);
    EXPECT_EQ(std::nullopt, memoryManager->getRootDeviceIndexClosestToPreferredNumaNode(rootDeviceIndices));

    debugManager.flags.EnableHostAllocationMemPolicy.set(false);
    EXPECT_EQ(std::optional<uint32_t>(1u), memoryManager->getRootDeviceIndexClosestToPreferredNumaNode(rootDeviceIndices));
}

TEST_F(DrmMemoryManagerNumaTest, givenAllDevicesOnSameNumaNodeWhenGettingRootDeviceIndexClosestToPreferredNumaNodeThenMemPolicyIsNotQueriedAndNoIndexIsReturned) {
    MockNumaLibrary numaLibrary(true);
    createMemoryManager({1, -1, 1});
    numaLibrary.setPolicy(MockNumaLibrary::mpolBind, {1u});
    MockNumaLibrary::getMemPolicyRetVal = -1;

    RootDeviceIndicesContainer rootDeviceIndices = {0u, 1u, 2u};
    EXPECT_EQ(std::nullopt, memoryManager->getRootDeviceIndexClosestToPreferredNumaNode(rootDeviceIndices));
    EXPECT_FALSE(memoryManager->rootDevicesOnMultipleNumaNodes);
}

TEST_F(DrmMemoryManagerNumaTest, givenNumaLibraryNotLoadedButAvailableWhenGettingRootDeviceIndexClosestToPreferredNumaNodeThenLibraryIsLoadedAndIndexIsReturned) {
    MockNumaLibrary numaLibrary(true);
    MockNumaLibrary::numaLoaded = false;
    createMemoryManager({0, 1});
    numaLibrary.setPolicy(MockNumaLibrary::mpolBind, {1u});

    MockOsLibrary::loadLibraryNewObject = MockNumaLibrary::createOsLibrary(true);
    VariableBackup<decltype(OsLibrary::loadFunc)> loadFuncBackup{&OsLibrary::loadFunc, MockOsLibraryCustom::load};

    RootDeviceIndicesContainer rootDeviceIndices = {0u, 1u};
    EXPECT_EQ(std::optional<uint32_t>(1u), memoryManager->getRootDeviceIndexClosestToPreferredNumaNode(rootDeviceIndices));
    EXPECT_TRUE(MockNumaLibrary::isLoaded());
    EXPECT_TRUE(memoryManager->rootDevicesOnMultipleNumaNodes);
}

TEST_F(DrmMemoryManagerNumaTest, givenNumaLibraryNotAvailableWhenGettingRootDeviceIndexClosestToPreferredNumaNodeThenNoIndexIsReturned) {
    {
        MockNumaLibrary resetNumaLibraryState(true);
    }
    ASSERT_FALSE(MockNumaLibrary::isLoaded());
    createMemoryManager({0, 1});

    MockOsLibrary::loadLibraryNewObject = nullptr;
    VariableBackup<decltype(OsLibrary::loadFunc)> loadFuncBackup{&OsLibrary::loadFunc, MockOsLibrary::load};

    RootDeviceIndicesContainer rootDeviceIndices = {0u, 1u};
    EXPECT_EQ(std::nullopt, memoryManager->getRootDeviceIndexClosestToPreferredNumaNode(rootDeviceIndices));
    EXPECT_FALSE(memoryManager->rootDevicesOnMultipleNumaNodes);
}
