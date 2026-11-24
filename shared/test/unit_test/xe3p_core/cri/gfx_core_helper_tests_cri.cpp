/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/source/release_helpers/release_helper/release_helper.h"
#include "shared/source/xe3p_core/hw_cmds_cri.h"
#include "shared/source/xe3p_core/hw_info_xe3p_core.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/gfx_core_helper_tests.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_memory_manager.h"

#include "per_product_test_definitions.h"

struct GfxCoreHelperTestsCri : public GfxCoreHelperTest {
    void setUpImpl() {
        hardwareInfo = *defaultHwInfo;
        hardwareInfoSetup[hardwareInfo.platform.eProductFamily](&hardwareInfo, true);
        DeviceFixture::setUpImpl(&hardwareInfo);
    }

    void SetUp() override {}
};

CRITEST_F(GfxCoreHelperTestsCri, givenCommandBufferAllocationTypeWhenGetAllocationDataIsCalledThenSystemMemoryIsRequested) {
    setUpImpl();
    AllocationData allocData;
    AllocationProperties properties(mockRootDeviceIndex, true, 10, AllocationType::commandBuffer, false, mockDeviceBitfield);

    MockMemoryManager mockMemoryManager{*pDevice->getExecutionEnvironment()};
    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));

    EXPECT_FALSE(allocData.flags.useSystemMemory);
}

CRITEST_F(GfxCoreHelperTestsCri, givenSingleTileCsrWhenAllocatingCsrSpecificAllocationsThenStoreThemInLocalMemory) {
    DebugManagerStateRestore restore;
    const uint32_t numDevices = 4u;
    debugManager.flags.CreateMultipleSubDevices.set(numDevices);
    debugManager.flags.EnableLocalMemory.set(true);
    debugManager.flags.EnableCommandBufferPoolAllocator.set(0);
    debugManager.flags.EnableInternalHeapPoolAllocator.set(0);
    setUpImpl();
    const uint32_t tileIndex = 2u;
    const DeviceBitfield singleTileMask{static_cast<uint32_t>(1u << tileIndex)};

    auto commandStreamReceiver = pDevice->getSubDevice(tileIndex)->getDefaultEngine().commandStreamReceiver;
    auto &heap = commandStreamReceiver->getIndirectHeap(IndirectHeap::Type::indirectObject, MemoryConstants::pageSize64k);
    auto heapAllocation = heap.getGraphicsAllocation();
    if (commandStreamReceiver->canUse4GbHeaps()) {
        EXPECT_EQ(AllocationType::internalHeap, heapAllocation->getAllocationType());
    } else {
        EXPECT_EQ(AllocationType::linearStream, heapAllocation->getAllocationType());
    }
    EXPECT_EQ(singleTileMask, heapAllocation->storageInfo.memoryBanks);

    commandStreamReceiver->ensureCommandBufferAllocation(heap, heap.getAvailableSpace() + 1, 0u);
    auto commandBufferAllocation = heap.getGraphicsAllocation();
    EXPECT_EQ(AllocationType::commandBuffer, commandBufferAllocation->getAllocationType());
    EXPECT_NE(heapAllocation, commandBufferAllocation);
    EXPECT_EQ(commandBufferAllocation->getMemoryPool(), MemoryPool::localMemory);
}

CRITEST_F(GfxCoreHelperTestsCri, WhenAskingForDcFlushThenReturnFalse) {
    setUpImpl();
    EXPECT_FALSE(MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, pDevice->getRootDeviceEnvironment()));
}

CRITEST_F(GfxCoreHelperTestsCri, givenDebugFlagDisablingContextGroupWhenQueryingEnginesThenLowPriorityAndInternalEngineIsReturned) {
    constexpr size_t numEngines = 8;

    DebugManagerStateRestore restore;
    debugManager.flags.ContextGroupSize.set(0);

    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.caps.rcsExposureDisabled = true;
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.featureTable.ftrBcsInfo = maxNBitValue(3);
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 2;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0));
    auto &gfxCoreHelper = device->getGfxCoreHelper();
    EXPECT_EQ(numEngines, device->allEngines.size());
    auto &engines = gfxCoreHelper.getGpgpuEngineInstances(device->getRootDeviceEnvironment());
    EXPECT_EQ(numEngines, engines.size());

    struct EnginePropertiesMap {
        aub_stream::EngineType engineType;
        bool isRegular;
        bool isLowPriority;
        bool isInternal;
    };

    const std::array<EnginePropertiesMap, numEngines> enginePropertiesMap = {{
        {aub_stream::ENGINE_CCS, true, false, false},
        {aub_stream::ENGINE_CCS1, true, false, false},
        {aub_stream::ENGINE_CCS, false, true, false},
        {aub_stream::ENGINE_CCS, false, false, true},
        {aub_stream::ENGINE_BCS1, true, false, false},
        {aub_stream::ENGINE_BCS1, false, false, true},
        {aub_stream::ENGINE_BCS2, true, false, false},
        {aub_stream::ENGINE_BCS2, false, false, true},
    }};

    for (size_t i = 0; i < numEngines; i++) {
        const auto &engine = engines[i];
        EXPECT_EQ(enginePropertiesMap[i].engineType, engine.first);
        EXPECT_EQ(enginePropertiesMap[i].isRegular, engine.second == EngineUsage::regular) << i;
        EXPECT_EQ(enginePropertiesMap[i].isLowPriority, engine.second == EngineUsage::lowPriority);
        EXPECT_EQ(enginePropertiesMap[i].isInternal, engine.second == EngineUsage::internal);
    }
}

CRITEST_F(GfxCoreHelperTestsCri, givenContextGroupWhenQueryingEnginesThenLowPriorityAndInternalEnginesAreReturnedAndHighPriorityEngineIsReturnedOnlyWhenDedicatedHpCopyEngineIsEnabled) {
    struct EnginePropertiesMap {
        aub_stream::EngineType engineType;
        bool isRegular;
        bool isLowPriority;
        bool isInternal;
        bool isHighPriority;
    };

    const std::vector<EnginePropertiesMap> enginesWithoutDedicatedHpCopyEngine = {
        {aub_stream::ENGINE_CCS, true, false, false, false},
        {aub_stream::ENGINE_CCS, false, true, false, false},
        {aub_stream::ENGINE_CCS, false, false, true, false},
        {aub_stream::ENGINE_BCS1, true, false, false, false},
        {aub_stream::ENGINE_BCS1, false, false, true, false},
        {aub_stream::ENGINE_BCS1, false, true, false, false},
        {aub_stream::ENGINE_BCS2, true, false, false, false},
        {aub_stream::ENGINE_BCS2, false, true, false, false},
        {aub_stream::ENGINE_BCS2, false, false, true, false},
    };

    const std::vector<EnginePropertiesMap> enginesWithDedicatedHpCopyEngine = {
        {aub_stream::ENGINE_CCS, true, false, false, false},
        {aub_stream::ENGINE_CCS, false, true, false, false},
        {aub_stream::ENGINE_CCS, false, false, true, false},
        {aub_stream::ENGINE_BCS1, true, false, false, false},
        {aub_stream::ENGINE_BCS1, false, false, true, false},
        {aub_stream::ENGINE_BCS1, false, true, false, false},
        {aub_stream::ENGINE_BCS2, false, false, false, true},
    };

    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.caps.rcsExposureDisabled = true;
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.featureTable.ftrBcsInfo = maxNBitValue(3);
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 1;

    for (auto skipHpBcsInitialization : {-1, 0}) {
        DebugManagerStateRestore restore;
        debugManager.flags.ExperimentalUseSecondaryContextForInternalEngine.set(0);
        debugManager.flags.ContextGroupSize.set(4);
        debugManager.flags.SkipHpBcsInitialization.set(skipHpBcsInitialization);

        const auto &enginePropertiesMap = (skipHpBcsInitialization == 0) ? enginesWithDedicatedHpCopyEngine : enginesWithoutDedicatedHpCopyEngine;

        auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0));
        auto &gfxCoreHelper = device->getGfxCoreHelper();
        EXPECT_EQ(enginePropertiesMap.size(), device->allEngines.size());
        auto &engines = gfxCoreHelper.getGpgpuEngineInstances(device->getRootDeviceEnvironment());
        ASSERT_EQ(enginePropertiesMap.size(), engines.size());

        for (size_t i = 0; i < enginePropertiesMap.size(); i++) {
            const auto &engine = engines[i];
            EXPECT_EQ(enginePropertiesMap[i].engineType, engine.first) << i;
            EXPECT_EQ(enginePropertiesMap[i].isRegular, engine.second == EngineUsage::regular) << i;
            EXPECT_EQ(enginePropertiesMap[i].isLowPriority, engine.second == EngineUsage::lowPriority) << i;
            EXPECT_EQ(enginePropertiesMap[i].isInternal, engine.second == EngineUsage::internal) << i;
            EXPECT_EQ(enginePropertiesMap[i].isHighPriority, engine.second == EngineUsage::highPriority) << i;
        }
    }
}

CRITEST_F(GfxCoreHelperTestsCri, whenCallingAreSecondaryContextsSupportedThenTrueIsReturnedAndContextGroupCountIs64) {
    auto executionEnvironment = std::make_unique<NEO::ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    auto hwInfo = *NEO::defaultHwInfo.get();
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(&hwInfo);

    EXPECT_TRUE(executionEnvironment->rootDeviceEnvironments[0]->gfxCoreHelper->areSecondaryContextsSupported());
    EXPECT_NE(0u, executionEnvironment->rootDeviceEnvironments[0]->gfxCoreHelper->getContextGroupContextsCount());
}

CRITEST_F(GfxCoreHelperTestsCri, givenNumGrfAndSimdSizeWhenAdjustingMaxWorkGroupSizeAndEnabled64BitAddressingThenCorrectWorkGroupSizeIsReturned) {
    setUpImpl();
    auto defaultMaxWorkGroupSize = 2048u;
    const auto &gfxCoreHelper = pDevice->getGfxCoreHelper();
    const auto &rootDeviceEnvironment = pDevice->getRootDeviceEnvironment();
    std::array<std::array<uint32_t, 3>, 15> values = {{
        {128u, 16u, 1024u}, // Grf Size, SIMT Size, Max Num of threads
        {128u, 32u, 1024u},
        {160u, 16u, 1024u},
        {160u, 32u, 1024u},
        {192u, 16u, 1024u},
        {192u, 32u, 1024u},
        {256u, 16u, 512u},
        {256u, 32u, 1024u},
        {512u, 16u, 512u},
        {512u, 32u, 1024u},
        {128u, 1u, 64u},
        {160u, 1u, 64u},
        {192u, 1u, 64u},
        {256u, 1u, 32u},
        {512u, 1u, 32u},
    }};

    for (auto &[grfSize, simtSize, expectedNumThreadsPerThreadGroup] : values) {
        EXPECT_EQ(expectedNumThreadsPerThreadGroup, gfxCoreHelper.adjustMaxWorkGroupSize(grfSize, simtSize, defaultMaxWorkGroupSize, rootDeviceEnvironment));
    }
}

CRITEST_F(GfxCoreHelperTestsCri, givenVariousGrfCountsWhenCallingCalculateAvailableThreadCountAndThreadCountAvailableIsBiggerThenCorrectValueIsReturned) {
    setUpImpl();
    const auto &gfxCoreHelper = pDevice->getGfxCoreHelper();
    const auto &rootDeviceEnvironment = pDevice->getRootDeviceEnvironment();

    constexpr auto grfTestInputs = std::to_array<std::pair<uint32_t, uint32_t>>({{32u, 8u},
                                                                                 {64u, 8u},
                                                                                 {96u, 8u},
                                                                                 {128u, 8u},
                                                                                 {160u, 8u},
                                                                                 {192u, 8u},
                                                                                 {256u, 4u},
                                                                                 {512u, 4u}});

    auto hwInfo = hardwareInfo;
    for (const auto &[grfCount, expectedThreadCountPerEu] : grfTestInputs) {
        auto expectedThreadCount = expectedThreadCountPerEu * hwInfo.gtSystemInfo.EUCount;
        // force thread count bigger than calculation
        hwInfo.gtSystemInfo.ThreadCount = expectedThreadCount * 2;
        EXPECT_EQ(expectedThreadCount, gfxCoreHelper.calculateAvailableThreadCount(hwInfo, grfCount, rootDeviceEnvironment)) << "grfCount: " << grfCount;
    }
}

CRITEST_F(GfxCoreHelperTestsCri, givenVariousGrfCountsWhenCallingCalculateAvailableThreadCountAndThreadCountAvailableIsSmallerThenCorrectValueIsReturned) {
    setUpImpl();
    const auto &gfxCoreHelper = pDevice->getGfxCoreHelper();
    const auto &rootDeviceEnvironment = pDevice->getRootDeviceEnvironment();

    constexpr auto grfTestInputs = std::to_array<std::pair<uint32_t, uint32_t>>({{32u, 8u},
                                                                                 {64u, 8u},
                                                                                 {96u, 8u},
                                                                                 {128u, 8u},
                                                                                 {160u, 8u},
                                                                                 {192u, 8u},
                                                                                 {256u, 4u},
                                                                                 {512u, 4u}});

    auto hwInfo = hardwareInfo;
    for (const auto &[grfCount, expectedThreadCountPerEu] : grfTestInputs) {
        // force thread count smaller than calculation
        hwInfo.gtSystemInfo.ThreadCount = expectedThreadCountPerEu * hwInfo.gtSystemInfo.EUCount / 2;
        EXPECT_EQ(hwInfo.gtSystemInfo.ThreadCount, gfxCoreHelper.calculateAvailableThreadCount(hwInfo, grfCount, rootDeviceEnvironment)) << "grfCount: " << grfCount;
    }
}

CRITEST_F(GfxCoreHelperTestsCri, givenModifiedGtSystemInfoWhenCallingCalculateAvailableThreadCountThenCorrectValueIsReturned) {
    setUpImpl();
    const auto &gfxCoreHelper = pDevice->getGfxCoreHelper();
    const auto &rootDeviceEnvironment = pDevice->getRootDeviceEnvironment();

    constexpr auto testInputs = std::to_array<std::pair<uint32_t, uint32_t>>({{32u, 128u},
                                                                              {48u, 192u},
                                                                              {64u, 256u}});

    auto hwInfo = hardwareInfo;
    for (const auto &[euCount, expectedThreadCount] : testInputs) {
        hwInfo.gtSystemInfo.EUCount = euCount;
        // force thread count bigger than calculation
        hwInfo.gtSystemInfo.ThreadCount = expectedThreadCount * 2;
        EXPECT_EQ(expectedThreadCount, gfxCoreHelper.calculateAvailableThreadCount(hwInfo, 256u, rootDeviceEnvironment)) << "euCount: " << euCount;
    }
}

struct GfxCoreHelperTestsCriWithEnginesCheck : public GfxCoreHelperTestWithEnginesCheck {
    void setUpImpl() {
        hardwareInfo = *defaultHwInfo;
        hardwareInfoSetup[hardwareInfo.platform.eProductFamily](&hardwareInfo, true);
        DeviceFixture::setUpImpl(&hardwareInfo);
    }

    void SetUp() override {}

    void verifyGpgpuEngines(bool dedicatedHpCopyEngineEnabled);
};

CRITEST_F(GfxCoreHelperTestsCriWithEnginesCheck, whenGetGpgpuEnginesThenReturnTwoCccsEnginesAndFourCcsEnginesAndEightLinkCopyEngines) {
    for (auto skipHpBcsInitialization : {-1, 0}) {
        DebugManagerStateRestore restore;
        debugManager.flags.ExperimentalUseSecondaryContextForInternalEngine.set(0);
        debugManager.flags.SkipHpBcsInitialization.set(skipHpBcsInitialization);
        verifyGpgpuEngines(skipHpBcsInitialization == 0);
    }
}

void GfxCoreHelperTestsCriWithEnginesCheck::verifyGpgpuEngines(bool dedicatedHpCopyEngineEnabled) {
    const size_t additionalEnginesWithoutHpCopyEngine = dedicatedHpCopyEngineEnabled ? 0u : 1u;
    const size_t numEnginesWithCccs = 24 + additionalEnginesWithoutHpCopyEngine;
    const size_t numEnginesWithoutCccs = 26 + additionalEnginesWithoutHpCopyEngine;

    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.featureTable.ftrBcsInfo = maxNBitValue(9);
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 4;

    for (auto debugFlag : {false, true}) {
        debugManager.flags.NodeOrdinal.set(debugFlag ? static_cast<int32_t>(aub_stream::ENGINE_CCCS) : -1);

        auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0));

        const auto &hwInfo = device->getHardwareInfo();
        auto &gfxCoreHelper = device->getGfxCoreHelper();

        bool cccsEnabled = !hwInfo.caps.rcsExposureDisabled || debugFlag;

        if (cccsEnabled) {
            EXPECT_EQ(numEnginesWithCccs, device->allEngines.size());
        } else {
            EXPECT_EQ(numEnginesWithoutCccs, device->allEngines.size());
        }

        auto &engines = gfxCoreHelper.getGpgpuEngineInstances(device->getRootDeviceEnvironment());
        EXPECT_EQ(cccsEnabled ? numEnginesWithCccs : numEnginesWithoutCccs, engines.size());

        for (const auto &engine : engines) {
            countEngine(engine.first, engine.second);
        }
        EXPECT_EQ(1u, getEngineCount(aub_stream::ENGINE_CCS, EngineUsage::regular));
        EXPECT_EQ(1u, getEngineCount(aub_stream::ENGINE_CCS1, EngineUsage::regular));
        EXPECT_EQ(1u, getEngineCount(aub_stream::ENGINE_CCS2, EngineUsage::regular));
        EXPECT_EQ(1u, getEngineCount(aub_stream::ENGINE_CCS3, EngineUsage::regular));

        if (cccsEnabled) {
            EXPECT_EQ(1u, getEngineCount(aub_stream::ENGINE_CCCS, EngineUsage::regular));
        }
        EXPECT_EQ(1u, getEngineCount(debugFlag ? aub_stream::ENGINE_CCCS : aub_stream::ENGINE_CCS, EngineUsage::internal));
        if (debugFlag) {
            EXPECT_EQ(1u, getEngineCount(aub_stream::ENGINE_CCCS, EngineUsage::lowPriority));
        } else {
            EXPECT_EQ(1u, getEngineCount(aub_stream::ENGINE_CCS, EngineUsage::lowPriority));
            EXPECT_EQ(1u, getEngineCount(aub_stream::ENGINE_CCS1, EngineUsage::lowPriority));
            EXPECT_EQ(1u, getEngineCount(aub_stream::ENGINE_CCS2, EngineUsage::lowPriority));
            EXPECT_EQ(1u, getEngineCount(aub_stream::ENGINE_CCS3, EngineUsage::lowPriority));
        }

        for (uint32_t idx = 1u; idx < hwInfo.featureTable.ftrBcsInfo.size(); idx++) {
            if (idx == 8 && dedicatedHpCopyEngineEnabled && gfxCoreHelper.areSecondaryContextsSupported()) {
                EXPECT_EQ(1u, getEngineCount(EngineHelpers::getBcsEngineAtIdx(idx), EngineUsage::highPriority));
            } else {
                EXPECT_EQ(1u, getEngineCount(EngineHelpers::getBcsEngineAtIdx(idx), EngineUsage::regular));
                EXPECT_EQ(1u, getEngineCount(EngineHelpers::getBcsEngineAtIdx(idx), EngineUsage::lowPriority));
            }
        }
        EXPECT_EQ(1u, getEngineCount(device->getProductHelper().getDefaultCopyEngine(), EngineUsage::internal));
        EXPECT_EQ(1u, getEngineCount(aub_stream::ENGINE_BCS3, EngineUsage::internal));
        EXPECT_TRUE(allEnginesChecked());

        resetEngineCounters();
    }
}
