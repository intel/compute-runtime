/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/libult/linux/drm_mock_prelim_context.h"
#include "shared/test/common/libult/linux/drm_query_mock.h"
#include "shared/test/common/mocks/linux/mock_drm_allocation.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/test.h"

#include "gtest/gtest.h"

TEST(DrmQueryTopologyTest, givenDrmWhenQueryTopologyCalledThenPassNoFlags) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmQueryMock drm{*executionEnvironment->rootDeviceEnvironments[0]};

    Drm::QueryTopologyData topologyData = {};

    EXPECT_TRUE(drm.queryTopology(*drm.context.hwInfo, topologyData));

    constexpr uint32_t expectedFlag = 0;
    EXPECT_EQ(expectedFlag, drm.storedQueryItem.flags);
}

struct QueryTopologyTests : ::testing::Test {
    class MyDrmQueryMock : public DrmQueryMock {
      public:
        using DrmQueryMock::DrmQueryMock;

        bool handleQueryItem(void *arg) override {
            const auto queryItem = reinterpret_cast<QueryItem *>(arg);
            if (queryItem->queryId != DrmPrelimHelper::getQueryComputeSlicesIoctl()) {
                return DrmQueryMock::handleQueryItem(queryItem);
            }

            queryComputeSlicesCallCount++;

            if (failOnQuery) {
                return false;
            }

            auto realEuCount = rootDeviceEnvironment.getHardwareInfo()->gtSystemInfo.EUCount;
            auto dataSize = static_cast<size_t>(std::ceil(realEuCount / 8.0));

            if (queryItem->length == 0) {
                queryItem->length = static_cast<int32_t>(sizeof(QueryTopologyInfo) + dataSize);
            } else {
                auto topologyArg = reinterpret_cast<QueryTopologyInfo *>(queryItem->dataPtr);

                uint16_t finalSVal = queryComputeSlicesSCount;
                uint16_t finalSSVal = queryComputeSlicesSSCount;
                uint16_t finalEUVal = queryComputeSlicesEuCount;

                if (useSmallerValuesOnSecondCall && queryComputeSlicesCallCount == 2) {
                    finalSVal /= 2;
                    finalSSVal /= 2;
                    finalEUVal /= 2;
                }

                topologyArg->maxSlices = finalSVal;
                topologyArg->maxSubslices = (finalSSVal / finalSVal);
                topologyArg->maxEusPerSubslice = (finalEUVal / finalSSVal);

                memset(topologyArg->data, 0xFF, dataSize);
            }

            return true;
        }

        uint32_t queryComputeSlicesCallCount = 0;

        uint16_t queryComputeSlicesSCount = 0;
        uint16_t queryComputeSlicesSSCount = 0;
        uint16_t queryComputeSlicesEuCount = 0;

        bool useSmallerValuesOnSecondCall = false;
        bool failOnQuery = false;
    };

    void SetUp() override {
        executionEnvironment = std::make_unique<MockExecutionEnvironment>();
        rootDeviceEnvironment = executionEnvironment->rootDeviceEnvironments[0].get();
    }

    void createDrm(uint32_t tileCount) {
        GT_MULTI_TILE_ARCH_INFO &multiTileArch = rootDeviceEnvironment->getMutableHardwareInfo()->gtSystemInfo.MultiTileArchInfo;
        multiTileArch.IsValid = (tileCount > 0);
        multiTileArch.TileCount = tileCount;
        multiTileArch.TileMask = static_cast<uint8_t>(maxNBitValue(tileCount));

        drmMock = std::make_unique<MyDrmQueryMock>(*rootDeviceEnvironment, rootDeviceEnvironment->getHardwareInfo());

        drmMock->storedSVal = 8;
        drmMock->storedSSVal = 32;
        drmMock->storedEUVal = 512;

        drmMock->queryComputeSlicesSCount = 4;
        drmMock->queryComputeSlicesSSCount = 16;
        drmMock->queryComputeSlicesEuCount = 256;

        EXPECT_TRUE(drmMock->queryMemoryInfo());
        EXPECT_TRUE(drmMock->queryEngineInfo());
    }

    DebugManagerStateRestore stateRestorer;
    std::unique_ptr<ExecutionEnvironment> executionEnvironment;
    std::unique_ptr<MyDrmQueryMock> drmMock;
    RootDeviceEnvironment *rootDeviceEnvironment;
    int receivedSliceCount = 0;
    int receivedSubSliceCount = 0;
    int receivedEuCount = 0;
};

TEST_F(QueryTopologyTests, givenZeroTilesWhenQueryingThenFallbackToQueryTopology) {
    createDrm(0);

    Drm::QueryTopologyData topologyData = {};

    drmMock->queryTopology(*rootDeviceEnvironment->getHardwareInfo(), topologyData);

    EXPECT_EQ(0u, drmMock->queryComputeSlicesCallCount);

    EXPECT_EQ(drmMock->storedSVal, topologyData.sliceCount);
    EXPECT_EQ(drmMock->storedSSVal, topologyData.subSliceCount);
    EXPECT_EQ(drmMock->storedEUVal, topologyData.euCount);

    EXPECT_EQ(drmMock->storedSVal, topologyData.maxSliceCount);
    EXPECT_EQ(drmMock->storedSSVal / drmMock->storedSVal, topologyData.maxSubSliceCount);
    EXPECT_EQ(drmMock->storedEUVal / drmMock->storedSSVal, topologyData.maxEuCount);
}

TEST_F(QueryTopologyTests, givenNonZeroTilesWhenDebugFlagDisabledThenFallbackToQueryTopology) {
    DebugManager.flags.UseNewQueryTopoIoctl.set(false);
    createDrm(2);

    Drm::QueryTopologyData topologyData = {};

    drmMock->queryTopology(*rootDeviceEnvironment->getHardwareInfo(), topologyData);

    EXPECT_EQ(0u, drmMock->queryComputeSlicesCallCount);

    EXPECT_EQ(drmMock->storedSVal, topologyData.sliceCount);
    EXPECT_EQ(drmMock->storedSSVal, topologyData.subSliceCount);
    EXPECT_EQ(drmMock->storedEUVal, topologyData.euCount);

    EXPECT_EQ(drmMock->storedSVal, topologyData.maxSliceCount);
    EXPECT_EQ(drmMock->storedSSVal / drmMock->storedSVal, topologyData.maxSubSliceCount);
    EXPECT_EQ(drmMock->storedEUVal / drmMock->storedSSVal, topologyData.maxEuCount);
}

TEST_F(QueryTopologyTests, givenNonZeroTilesWhenQueryingThenUseOnlyNewIoctl) {
    createDrm(2);

    Drm::QueryTopologyData topologyData = {};

    drmMock->queryTopology(*rootDeviceEnvironment->getHardwareInfo(), topologyData);

    EXPECT_EQ(4u, drmMock->queryComputeSlicesCallCount); // 2x length + 2x query

    EXPECT_EQ(drmMock->queryComputeSlicesSCount, topologyData.sliceCount);
    EXPECT_EQ(drmMock->queryComputeSlicesSSCount, topologyData.subSliceCount);
    EXPECT_EQ(drmMock->queryComputeSlicesEuCount, topologyData.euCount);

    EXPECT_EQ(drmMock->queryComputeSlicesSCount, topologyData.maxSliceCount);
    EXPECT_EQ(drmMock->queryComputeSlicesSSCount / drmMock->queryComputeSlicesSCount, topologyData.maxSubSliceCount);
    EXPECT_EQ(drmMock->queryComputeSlicesEuCount / drmMock->queryComputeSlicesSSCount, topologyData.maxEuCount);
}

TEST_F(QueryTopologyTests, givenNonZeroTilesWithoutEngineInfoThenFallback) {
    createDrm(2);

    drmMock->engineInfo.reset();

    Drm::QueryTopologyData topologyData = {};
    drmMock->queryTopology(*rootDeviceEnvironment->getHardwareInfo(), topologyData);

    EXPECT_EQ(0u, drmMock->queryComputeSlicesCallCount);

    EXPECT_EQ(drmMock->storedSVal, topologyData.sliceCount);
    EXPECT_EQ(drmMock->storedSSVal, topologyData.subSliceCount);
    EXPECT_EQ(drmMock->storedEUVal, topologyData.euCount);

    EXPECT_EQ(drmMock->storedSVal, topologyData.maxSliceCount);
    EXPECT_EQ(drmMock->storedSSVal / drmMock->storedSVal, topologyData.maxSubSliceCount);
    EXPECT_EQ(drmMock->storedEUVal / drmMock->storedSSVal, topologyData.maxEuCount);
}

TEST_F(QueryTopologyTests, givenNonZeroTilesWhenFailOnNewQueryThenFallback) {
    createDrm(2);

    drmMock->queryComputeSlicesEuCount = 0;

    Drm::QueryTopologyData topologyData = {};
    drmMock->queryTopology(*rootDeviceEnvironment->getHardwareInfo(), topologyData);

    EXPECT_EQ(2u, drmMock->queryComputeSlicesCallCount);

    EXPECT_EQ(drmMock->storedSVal, topologyData.sliceCount);
    EXPECT_EQ(drmMock->storedSSVal, topologyData.subSliceCount);
    EXPECT_EQ(drmMock->storedEUVal, topologyData.euCount);

    EXPECT_EQ(drmMock->storedSVal, topologyData.maxSliceCount);
    EXPECT_EQ(drmMock->storedSSVal / drmMock->storedSVal, topologyData.maxSubSliceCount);
    EXPECT_EQ(drmMock->storedEUVal / drmMock->storedSSVal, topologyData.maxEuCount);
}

TEST_F(QueryTopologyTests, givenNonZeroTilesWhenIncorrectValuesQueriedThenFallback) {
    createDrm(2);

    drmMock->failOnQuery = true;

    Drm::QueryTopologyData topologyData = {};
    drmMock->queryTopology(*rootDeviceEnvironment->getHardwareInfo(), topologyData);

    EXPECT_EQ(1u, drmMock->queryComputeSlicesCallCount);

    EXPECT_EQ(drmMock->storedSVal, topologyData.sliceCount);
    EXPECT_EQ(drmMock->storedSSVal, topologyData.subSliceCount);
    EXPECT_EQ(drmMock->storedEUVal, topologyData.euCount);

    EXPECT_EQ(drmMock->storedSVal, topologyData.maxSliceCount);
    EXPECT_EQ(drmMock->storedSSVal / drmMock->storedSVal, topologyData.maxSubSliceCount);
    EXPECT_EQ(drmMock->storedEUVal / drmMock->storedSSVal, topologyData.maxEuCount);
}

TEST_F(QueryTopologyTests, givenAsymetricTilesWhenQueryingThenPickSmallerValue) {
    createDrm(2);

    drmMock->useSmallerValuesOnSecondCall = true;

    Drm::QueryTopologyData topologyData = {};
    drmMock->queryTopology(*rootDeviceEnvironment->getHardwareInfo(), topologyData);

    EXPECT_EQ(drmMock->queryComputeSlicesSCount / 2, topologyData.sliceCount);
    EXPECT_EQ(drmMock->queryComputeSlicesSSCount / 2, topologyData.subSliceCount);
    EXPECT_EQ(drmMock->queryComputeSlicesEuCount / 2, topologyData.euCount);

    EXPECT_EQ(drmMock->queryComputeSlicesSCount, topologyData.maxSliceCount);
    EXPECT_EQ(drmMock->queryComputeSlicesSSCount / drmMock->queryComputeSlicesSCount, topologyData.maxSubSliceCount);
    EXPECT_EQ(drmMock->queryComputeSlicesEuCount / drmMock->queryComputeSlicesSSCount, topologyData.maxEuCount);
}

TEST_F(QueryTopologyTests, givenAsymetricTilesWhenGettingSliceMappingsThenCorrectMappingsReturnedForBothDeviceIndexes) {
    createDrm(2);

    drmMock->useSmallerValuesOnSecondCall = true;

    Drm::QueryTopologyData topologyData = {};
    drmMock->queryTopology(*rootDeviceEnvironment->getHardwareInfo(), topologyData);

    auto device0SliceMapping = drmMock->getSliceMappings(0);
    auto device1SliceMapping = drmMock->getSliceMappings(1);

    ASSERT_EQ(static_cast<size_t>(drmMock->queryComputeSlicesSCount / 2), device0SliceMapping.size());
    for (int i = 0; i < drmMock->queryComputeSlicesSCount / 2; i++) {
        EXPECT_EQ(i, device0SliceMapping[i]);
    }

    ASSERT_EQ(static_cast<size_t>(drmMock->queryComputeSlicesSCount), device1SliceMapping.size());
    for (int i = 0; i < drmMock->queryComputeSlicesSCount; i++) {
        EXPECT_EQ(i, device1SliceMapping[i]);
    }
}

TEST_F(QueryTopologyTests, givenNonZeroTilesAndFallbackPathWhenGettingSliceMappingsThenMappingStoredForIndexZeroOnly) {
    DebugManager.flags.UseNewQueryTopoIoctl.set(false);
    createDrm(2);

    Drm::QueryTopologyData topologyData = {};

    drmMock->queryTopology(*rootDeviceEnvironment->getHardwareInfo(), topologyData);

    EXPECT_EQ(0u, drmMock->queryComputeSlicesCallCount);

    EXPECT_EQ(drmMock->storedSVal, topologyData.sliceCount);
    EXPECT_EQ(drmMock->storedSSVal, topologyData.subSliceCount);
    EXPECT_EQ(drmMock->storedEUVal, topologyData.euCount);

    auto device0SliceMapping = drmMock->getSliceMappings(0);
    auto device1SliceMapping = drmMock->getSliceMappings(1);

    ASSERT_EQ(static_cast<size_t>(drmMock->storedSVal), device0SliceMapping.size());
    for (int i = 0; i < drmMock->storedSVal; i++) {
        EXPECT_EQ(i, device0SliceMapping[i]);
    }

    ASSERT_EQ(0u, device1SliceMapping.size());
}

TEST_F(QueryTopologyTests, givenDrmWhenGettingTopologyMapThenCorrectMapIsReturned) {
    createDrm(2);
    Drm::QueryTopologyData topologyData = {};
    drmMock->queryTopology(*rootDeviceEnvironment->getHardwareInfo(), topologyData);

    auto topologyMap = drmMock->getTopologyMap();

    if (executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo()->gtSystemInfo.MultiTileArchInfo.TileCount > 0) {
        EXPECT_EQ(executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo()->gtSystemInfo.MultiTileArchInfo.TileCount, topologyMap.size());
    } else {
        EXPECT_EQ(1u, topologyMap.size());
    }

    for (uint32_t i = 0; i < topologyMap.size(); i++) {
        EXPECT_EQ(drmMock->queryComputeSlicesSCount, topologyMap.at(i).sliceIndices.size());
    }
}

TEST(DrmQueryTest, WhenCallingQueryPageFaultSupportThenReturnFalseByDefault) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmQueryMock drm{*executionEnvironment->rootDeviceEnvironments[0]};

    drm.queryPageFaultSupport();

    EXPECT_FALSE(drm.hasPageFaultSupport());
}

TEST(DrmQueryTest, givenPageFaultSupportEnabledWhenCallingQueryPageFaultSupportThenReturnCorrectValue) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmQueryMock drm{*executionEnvironment->rootDeviceEnvironments[0]};

    for (bool hasPageFaultSupport : {false, true}) {
        drm.context.hasPageFaultQueryValue = hasPageFaultSupport;
        drm.queryPageFaultSupport();

        EXPECT_EQ(hasPageFaultSupport, drm.hasPageFaultSupport());
    }
}

TEST(DrmQueryTest, WhenQueryPageFaultSupportFailsThenReturnFalse) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmQueryMock drm{*executionEnvironment->rootDeviceEnvironments[0]};

    drm.context.hasPageFaultQueryReturn = -1;
    drm.queryPageFaultSupport();

    EXPECT_FALSE(drm.hasPageFaultSupport());
}

TEST(DrmQueryTest, givenUseKmdMigrationWhenShouldAllocationFaultIsCalledOnFaultableHardwareThenReturnCorrectValue) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.UseKmdMigration.set(true);

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmQueryMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    drm.pageFaultSupported = true;

    AllocationType allocationTypesThatShouldFault[] = {
        AllocationType::UNIFIED_SHARED_MEMORY};

    for (auto allocationType : allocationTypesThatShouldFault) {
        MockDrmAllocation allocation(allocationType, MemoryPool::MemoryNull);
        EXPECT_TRUE(allocation.shouldAllocationPageFault(&drm));
    }

    MockDrmAllocation allocation(AllocationType::BUFFER, MemoryPool::MemoryNull);
    EXPECT_FALSE(allocation.shouldAllocationPageFault(&drm));
}

TEST(DrmQueryTest, givenRecoverablePageFaultsEnabledWhenCallingHasPageFaultSupportThenReturnCorrectValue) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmQueryMock drm{*executionEnvironment->rootDeviceEnvironments[0]};

    for (bool hasPageFaultSupport : {false, true}) {
        drm.pageFaultSupported = hasPageFaultSupport;

        EXPECT_EQ(hasPageFaultSupport, drm.hasPageFaultSupport());
    }
}

TEST(DrmQueryTest, givenDrmAllocationWhenShouldAllocationFaultIsCalledOnNonFaultableHardwareThenReturnFalse) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmQueryMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    drm.pageFaultSupported = false;

    MockDrmAllocation allocation(AllocationType::BUFFER, MemoryPool::MemoryNull);
    EXPECT_FALSE(allocation.shouldAllocationPageFault(&drm));
}

TEST(DrmQueryTest, givenEnableImplicitMigrationOnFaultableHardwareWhenShouldAllocationFaultIsCalledThenReturnTrue) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableImplicitMigrationOnFaultableHardware.set(true);

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmQueryMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    drm.pageFaultSupported = true;

    MockDrmAllocation allocation(AllocationType::BUFFER, MemoryPool::MemoryNull);
    EXPECT_TRUE(allocation.shouldAllocationPageFault(&drm));
}
