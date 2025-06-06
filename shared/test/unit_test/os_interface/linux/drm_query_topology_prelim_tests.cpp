/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/linux/engine_info.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/stream_capture.h"
#include "shared/test/common/libult/linux/drm_mock_prelim_context.h"
#include "shared/test/common/libult/linux/drm_query_mock.h"
#include "shared/test/common/mocks/linux/mock_drm_allocation.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/test.h"

#include "gtest/gtest.h"

TEST(DrmQueryTopologyTest, givenDrmWhenQueryTopologyCalledThenPassNoFlags) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmQueryMock drm{*executionEnvironment->rootDeviceEnvironments[0]};

    DrmQueryTopologyData topologyData = {};
    drm.engineInfoQueried = true;
    drm.systemInfoQueried = true;
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

            // Values cannot exceed what this GT allows
            queryComputeSlicesSCount = std::min(queryComputeSlicesSCount, static_cast<uint16_t>(rootDeviceEnvironment.getHardwareInfo()->gtSystemInfo.SliceCount));
            queryComputeSlicesSSCount = std::min(queryComputeSlicesSSCount, static_cast<uint16_t>(rootDeviceEnvironment.getHardwareInfo()->gtSystemInfo.SubSliceCount));
            queryComputeSlicesEuCount = std::min(queryComputeSlicesEuCount, static_cast<uint16_t>(rootDeviceEnvironment.getHardwareInfo()->gtSystemInfo.EUCount));

            UNRECOVERABLE_IF((queryComputeSlicesSCount != 0) && (queryComputeSlicesSSCount % queryComputeSlicesSCount != 0));
            UNRECOVERABLE_IF((queryComputeSlicesSSCount != 0) && (queryComputeSlicesEuCount % queryComputeSlicesSSCount != 0));

            uint16_t subslicesPerSlice = queryComputeSlicesSCount ? (queryComputeSlicesSSCount / queryComputeSlicesSCount) : 0u;
            uint16_t eusPerSubslice = queryComputeSlicesSSCount ? (queryComputeSlicesEuCount / queryComputeSlicesSSCount) : 0u;
            uint16_t subsliceOffset = static_cast<uint16_t>(Math::divideAndRoundUp(queryComputeSlicesSCount, 8u));
            uint16_t subsliceStride = static_cast<uint16_t>(Math::divideAndRoundUp(subslicesPerSlice, 8u));
            uint16_t euOffset = subsliceOffset + queryComputeSlicesSCount * subsliceStride;
            uint16_t euStride = static_cast<uint16_t>(Math::divideAndRoundUp(eusPerSubslice, 8u));

            const uint16_t dataSize = euOffset + queryComputeSlicesSSCount * euStride;

            if (queryItem->length == 0) {
                queryItem->length = static_cast<int32_t>(sizeof(QueryTopologyInfo) + dataSize);
            } else {
                auto topologyArg = reinterpret_cast<QueryTopologyInfo *>(queryItem->dataPtr);

                uint16_t finalSVal = queryComputeSlicesSCount;
                uint16_t finalSSVal = queryComputeSlicesSSCount;
                uint16_t finalEUVal = queryComputeSlicesEuCount;

                if (useSmallerValuesOnSecondCall && queryComputeSlicesCallCount == 2) {
                    if ((finalSSVal % 2 == 0) && (finalEUVal % 2 == 0)) {
                        finalSSVal /= 2;
                        finalEUVal /= 2;
                    } else if ((finalSSVal % 3 == 0) && (finalEUVal % 3 == 0)) {
                        finalSSVal /= 3;
                        finalEUVal /= 3;
                    } else {
                        finalSVal = 1;
                        finalSSVal = subslicesPerSlice;
                        finalEUVal = subslicesPerSlice * eusPerSubslice;
                    }

                    subslicesPerSlice = finalSVal ? (finalSSVal / finalSVal) : 0u;
                    eusPerSubslice = finalSSVal ? (finalEUVal / finalSSVal) : 0u;
                    subsliceOffset = static_cast<uint16_t>(Math::divideAndRoundUp(finalSVal, 8u));
                    subsliceStride = static_cast<uint16_t>(Math::divideAndRoundUp(subslicesPerSlice, 8u));
                    euOffset = subsliceOffset + finalSVal * subsliceStride;
                    euStride = static_cast<uint16_t>(Math::divideAndRoundUp(eusPerSubslice, 8u));
                }

                topologyArg->maxSlices = finalSVal;
                topologyArg->maxSubslices = subslicesPerSlice;
                topologyArg->maxEusPerSubslice = eusPerSubslice;
                topologyArg->subsliceOffset = subsliceOffset;
                topologyArg->subsliceStride = subsliceStride;
                topologyArg->euOffset = euOffset;
                topologyArg->euStride = euStride;

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

        drmMock = std::make_unique<MyDrmQueryMock>(*rootDeviceEnvironment);

        drmMock->storedSVal = rootDeviceEnvironment->getHardwareInfo()->gtSystemInfo.SliceCount;
        drmMock->storedSSVal = rootDeviceEnvironment->getHardwareInfo()->gtSystemInfo.SubSliceCount;
        drmMock->storedEUVal = rootDeviceEnvironment->getHardwareInfo()->gtSystemInfo.EUCount;

        drmMock->queryComputeSlicesSCount = rootDeviceEnvironment->getHardwareInfo()->gtSystemInfo.SliceCount;
        drmMock->queryComputeSlicesSSCount = rootDeviceEnvironment->getHardwareInfo()->gtSystemInfo.SubSliceCount;
        drmMock->queryComputeSlicesEuCount = rootDeviceEnvironment->getHardwareInfo()->gtSystemInfo.EUCount;

        drmMock->memoryInfoQueried = false;
        EXPECT_TRUE(drmMock->queryMemoryInfo());
        EXPECT_TRUE(drmMock->queryEngineInfo());
        drmMock->systemInfoQueried = true;
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

    DrmQueryTopologyData topologyData = {};

    drmMock->queryTopology(*rootDeviceEnvironment->getHardwareInfo(), topologyData);

    EXPECT_EQ(0u, drmMock->queryComputeSlicesCallCount);

    EXPECT_EQ(drmMock->storedSVal, topologyData.sliceCount);
    EXPECT_EQ(drmMock->storedSSVal, topologyData.subSliceCount);
    EXPECT_EQ(drmMock->storedEUVal, topologyData.euCount);

    EXPECT_EQ(drmMock->storedSVal, topologyData.maxSlices);
    EXPECT_EQ(drmMock->storedSSVal / drmMock->storedSVal, topologyData.maxSubSlicesPerSlice);
    EXPECT_EQ(drmMock->storedEUVal / drmMock->storedSSVal, topologyData.maxEusPerSubSlice);
}

TEST_F(QueryTopologyTests, givenNonZeroTilesWhenDebugFlagDisabledThenFallbackToQueryTopology) {
    debugManager.flags.UseNewQueryTopoIoctl.set(false);
    createDrm(2);

    DrmQueryTopologyData topologyData = {};

    drmMock->queryTopology(*rootDeviceEnvironment->getHardwareInfo(), topologyData);

    EXPECT_EQ(0u, drmMock->queryComputeSlicesCallCount);

    EXPECT_EQ(drmMock->storedSVal, topologyData.sliceCount);
    EXPECT_EQ(drmMock->storedSSVal, topologyData.subSliceCount);
    EXPECT_EQ(drmMock->storedEUVal, topologyData.euCount);

    EXPECT_EQ(drmMock->storedSVal, topologyData.maxSlices);
    EXPECT_EQ(drmMock->storedSSVal / drmMock->storedSVal, topologyData.maxSubSlicesPerSlice);
    EXPECT_EQ(drmMock->storedEUVal / drmMock->storedSSVal, topologyData.maxEusPerSubSlice);
}

TEST_F(QueryTopologyTests, givenNonZeroTilesWhenQueryingThenUseOnlyNewIoctl) {
    createDrm(2);

    DrmQueryTopologyData topologyData = {};

    drmMock->queryTopology(*rootDeviceEnvironment->getHardwareInfo(), topologyData);

    EXPECT_EQ(4u, drmMock->queryComputeSlicesCallCount); // 2x length + 2x query

    EXPECT_EQ(drmMock->queryComputeSlicesSCount, topologyData.sliceCount);
    EXPECT_EQ(drmMock->queryComputeSlicesSSCount, topologyData.subSliceCount);
    EXPECT_EQ(drmMock->queryComputeSlicesEuCount, topologyData.euCount);

    EXPECT_EQ(drmMock->queryComputeSlicesSCount, topologyData.maxSlices);
    EXPECT_EQ(drmMock->queryComputeSlicesSSCount / drmMock->queryComputeSlicesSCount, topologyData.maxSubSlicesPerSlice);
    EXPECT_EQ(drmMock->queryComputeSlicesEuCount / drmMock->queryComputeSlicesSSCount, topologyData.maxEusPerSubSlice);
}

TEST_F(QueryTopologyTests, givenNonZeroTilesWithoutEngineInfoThenFallback) {
    createDrm(2);

    drmMock->engineInfo.reset();

    DrmQueryTopologyData topologyData = {};
    drmMock->queryTopology(*rootDeviceEnvironment->getHardwareInfo(), topologyData);

    EXPECT_EQ(0u, drmMock->queryComputeSlicesCallCount);

    EXPECT_EQ(drmMock->storedSVal, topologyData.sliceCount);
    EXPECT_EQ(drmMock->storedSSVal, topologyData.subSliceCount);
    EXPECT_EQ(drmMock->storedEUVal, topologyData.euCount);

    EXPECT_EQ(drmMock->storedSVal, topologyData.maxSlices);
    EXPECT_EQ(drmMock->storedSSVal / drmMock->storedSVal, topologyData.maxSubSlicesPerSlice);
    EXPECT_EQ(drmMock->storedEUVal / drmMock->storedSSVal, topologyData.maxEusPerSubSlice);
}

TEST_F(QueryTopologyTests, givenNonZeroTilesWhenFailOnNewQueryThenFallback) {
    createDrm(2);

    drmMock->queryComputeSlicesEuCount = 0;

    DrmQueryTopologyData topologyData = {};
    drmMock->queryTopology(*rootDeviceEnvironment->getHardwareInfo(), topologyData);

    EXPECT_EQ(2u, drmMock->queryComputeSlicesCallCount);

    EXPECT_EQ(drmMock->storedSVal, topologyData.sliceCount);
    EXPECT_EQ(drmMock->storedSSVal, topologyData.subSliceCount);
    EXPECT_EQ(drmMock->storedEUVal, topologyData.euCount);

    EXPECT_EQ(drmMock->storedSVal, topologyData.maxSlices);
    EXPECT_EQ(drmMock->storedSSVal / drmMock->storedSVal, topologyData.maxSubSlicesPerSlice);
    EXPECT_EQ(drmMock->storedEUVal / drmMock->storedSSVal, topologyData.maxEusPerSubSlice);
}

TEST_F(QueryTopologyTests, givenNonZeroTilesWhenIncorrectValuesQueriedThenFallback) {
    createDrm(2);

    drmMock->failOnQuery = true;

    DrmQueryTopologyData topologyData = {};
    drmMock->queryTopology(*rootDeviceEnvironment->getHardwareInfo(), topologyData);

    EXPECT_EQ(1u, drmMock->queryComputeSlicesCallCount);

    EXPECT_EQ(drmMock->storedSVal, topologyData.sliceCount);
    EXPECT_EQ(drmMock->storedSSVal, topologyData.subSliceCount);
    EXPECT_EQ(drmMock->storedEUVal, topologyData.euCount);

    EXPECT_EQ(drmMock->storedSVal, topologyData.maxSlices);
    EXPECT_EQ(drmMock->storedSSVal / drmMock->storedSVal, topologyData.maxSubSlicesPerSlice);
    EXPECT_EQ(drmMock->storedEUVal / drmMock->storedSSVal, topologyData.maxEusPerSubSlice);
}

TEST_F(QueryTopologyTests, givenAsymetricTilesWhenQueryingThenPickSmallerValue) {
    createDrm(2);

    drmMock->useSmallerValuesOnSecondCall = true;

    DrmQueryTopologyData topologyData = {};
    drmMock->queryTopology(*rootDeviceEnvironment->getHardwareInfo(), topologyData);

    EXPECT_NE(0, drmMock->queryComputeSlicesSCount);
    EXPECT_NE(0, topologyData.sliceCount);
    EXPECT_LE(topologyData.sliceCount, drmMock->queryComputeSlicesSCount);
    if ((drmMock->queryComputeSlicesSSCount % 2 == 0) && (drmMock->queryComputeSlicesEuCount % 2 == 0)) {
        EXPECT_EQ(drmMock->queryComputeSlicesSSCount / 2, topologyData.subSliceCount);
        EXPECT_EQ(drmMock->queryComputeSlicesEuCount / 2, topologyData.euCount);
    } else if ((drmMock->queryComputeSlicesSSCount % 3 == 0) && (drmMock->queryComputeSlicesEuCount % 3 == 0)) {
        EXPECT_EQ(drmMock->queryComputeSlicesSSCount / 3, topologyData.subSliceCount);
        EXPECT_EQ(drmMock->queryComputeSlicesEuCount / 3, topologyData.euCount);
    } else {
        EXPECT_EQ(drmMock->queryComputeSlicesSSCount / drmMock->queryComputeSlicesSCount, topologyData.subSliceCount);
        EXPECT_EQ(drmMock->queryComputeSlicesEuCount / drmMock->queryComputeSlicesSSCount * topologyData.subSliceCount, topologyData.euCount);
    }

    EXPECT_EQ(drmMock->queryComputeSlicesSCount, topologyData.maxSlices);
    EXPECT_EQ(drmMock->queryComputeSlicesSSCount / drmMock->queryComputeSlicesSCount, topologyData.maxSubSlicesPerSlice);
    EXPECT_EQ(drmMock->queryComputeSlicesEuCount / drmMock->queryComputeSlicesSSCount, topologyData.maxEusPerSubSlice);
}

TEST_F(QueryTopologyTests, givenAsymetricTilesWhenGettingSliceMappingsThenCorrectMappingsReturnedForBothDeviceIndexes) {
    createDrm(2);

    drmMock->useSmallerValuesOnSecondCall = true;

    DrmQueryTopologyData topologyData = {};
    drmMock->queryTopology(*rootDeviceEnvironment->getHardwareInfo(), topologyData);

    auto device0SliceMapping = drmMock->getSliceMappings(0);
    auto device1SliceMapping = drmMock->getSliceMappings(1);

    ASSERT_GE(static_cast<size_t>(drmMock->queryComputeSlicesSCount), device0SliceMapping.size());
    for (int i = 0; i < static_cast<int>(device0SliceMapping.size()); i++) {
        EXPECT_EQ(i, device0SliceMapping[i]);
    }

    ASSERT_EQ(static_cast<size_t>(drmMock->queryComputeSlicesSCount), device1SliceMapping.size());
    for (int i = 0; i < drmMock->queryComputeSlicesSCount; i++) {
        EXPECT_EQ(i, device1SliceMapping[i]);
    }
}

TEST_F(QueryTopologyTests, givenNonZeroTilesAndFallbackPathWhenGettingSliceMappingsThenMappingStoredForIndexZeroOnly) {
    debugManager.flags.UseNewQueryTopoIoctl.set(false);
    createDrm(2);

    DrmQueryTopologyData topologyData = {};

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
    DrmQueryTopologyData topologyData = {};
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
    const auto &productHelper = executionEnvironment->rootDeviceEnvironments[0]->getHelper<ProductHelper>();

    for (bool hasPageFaultSupport : {false, true}) {
        drm.context.hasPageFaultQueryValue = hasPageFaultSupport;
        drm.queryPageFaultSupport();

        if (productHelper.isPageFaultSupported()) {
            EXPECT_EQ(hasPageFaultSupport, drm.hasPageFaultSupport());
        } else {
            EXPECT_FALSE(drm.hasPageFaultSupport());
        }
    }
}

TEST(DrmQueryTest, givenPrintIoctlDebugFlagSetWhenCallingQueryPageFaultSupportThenCaptureExpectedOutput) {
    DebugManagerStateRestore restore;
    debugManager.flags.PrintIoctlEntries.set(true);

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmQueryMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    const auto &productHelper = executionEnvironment->rootDeviceEnvironments[0]->getHelper<ProductHelper>();

    bool hasPageFaultSupport = true;
    drm.context.hasPageFaultQueryValue = hasPageFaultSupport;

    StreamCapture capture;
    capture.captureStdout(); // start capturing
    drm.queryPageFaultSupport();
    debugManager.flags.PrintIoctlEntries.set(false);
    std::string outputString = capture.getCapturedStdout(); // stop capturing

    if (productHelper.isPageFaultSupported()) {
        std::string expectedString = "DRM_IOCTL_I915_GETPARAM: param: PRELIM_I915_PARAM_HAS_PAGE_FAULT, output value: 1, retCode: 0\n";
        EXPECT_NE(std::string::npos, outputString.find(expectedString));
    } else {
        EXPECT_TRUE(outputString.empty());
    }
}

TEST(DrmQueryTest, givenPrintIoctlDebugFlagNotSetWhenIsPageFaultSupportedCalledThenNoCapturedOutput) {
    DebugManagerStateRestore restore;
    debugManager.flags.PrintIoctlEntries.set(false);

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmQueryMock drm{*executionEnvironment->rootDeviceEnvironments[0]};

    bool hasPageFaultSupport = true;
    drm.context.hasPageFaultQueryValue = hasPageFaultSupport;

    StreamCapture capture;
    capture.captureStdout(); // start capturing
    drm.queryPageFaultSupport();
    debugManager.flags.PrintIoctlEntries.set(false);
    std::string outputString = capture.getCapturedStdout(); // stop capturing

    EXPECT_TRUE(outputString.empty());
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
    debugManager.flags.UseKmdMigration.set(true);

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmQueryMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    drm.pageFaultSupported = true;

    AllocationType allocationTypesThatShouldFault[] = {
        AllocationType::unifiedSharedMemory};

    for (auto allocationType : allocationTypesThatShouldFault) {
        MockDrmAllocation allocation(0u, allocationType, MemoryPool::memoryNull);
        EXPECT_TRUE(allocation.shouldAllocationPageFault(&drm));
    }

    MockDrmAllocation allocation(0u, AllocationType::buffer, MemoryPool::memoryNull);
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

    MockDrmAllocation allocation(0u, AllocationType::buffer, MemoryPool::memoryNull);
    EXPECT_FALSE(allocation.shouldAllocationPageFault(&drm));
}

TEST(DrmQueryTest, givenEnableImplicitMigrationOnFaultableHardwareWhenShouldAllocationFaultIsCalledThenReturnTrue) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableImplicitMigrationOnFaultableHardware.set(true);

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmQueryMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    drm.pageFaultSupported = true;

    MockDrmAllocation allocation(0u, AllocationType::buffer, MemoryPool::memoryNull);
    EXPECT_TRUE(allocation.shouldAllocationPageFault(&drm));
}

TEST(DrmQueryTest, givenUseKmdMigrationSetWhenCallingHasKmdMigrationSupportThenReturnCorrectValue) {
    DebugManagerStateRestore restorer;

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->initializeMemoryManager();

    DrmQueryMock drm(*executionEnvironment->rootDeviceEnvironments[0]);
    drm.pageFaultSupported = true;

    for (auto useKmdMigration : {-1, 0, 1}) {
        debugManager.flags.UseKmdMigration.set(useKmdMigration);
        if (useKmdMigration == -1) {
            auto &productHelper = drm.getRootDeviceEnvironment().getHelper<ProductHelper>();
            EXPECT_EQ(productHelper.isKmdMigrationSupported(), drm.hasKmdMigrationSupport());
        } else {
            EXPECT_EQ(useKmdMigration, drm.hasKmdMigrationSupport());
        }
    }
}

TEST(DrmQueryTest, givenKmdMigrationSupportedWhenShouldAllocationPageFaultIsCalledOnUnifiedSharedMemoryThenReturnTrue) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->initializeMemoryManager();

    DrmQueryMock drm(*executionEnvironment->rootDeviceEnvironments[0]);
    drm.pageFaultSupported = true;

    MockBufferObject bo(0u, &drm, 3, 0, 0, 1);
    MockDrmAllocation allocation(0u, AllocationType::unifiedSharedMemory, MemoryPool::localMemory);
    allocation.bufferObjects[0] = &bo;

    EXPECT_EQ(drm.hasKmdMigrationSupport(), allocation.shouldAllocationPageFault(&drm));
}
