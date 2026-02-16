/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/source/release_helper/release_helper.h"
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
        auto releaseHelper = ReleaseHelper::create(hardwareInfo.ipVersion);
        hardwareInfoSetup[hardwareInfo.platform.eProductFamily](&hardwareInfo, true, 0, releaseHelper.get());
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
    hwInfo.featureTable.flags.ftrRcsNode = false;
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

CRITEST_F(GfxCoreHelperTestsCri, givenContextGroupWhenQueryingEnginesThenLowPriorityHighPriorityAndInternalEngineIsReturned) {
    constexpr size_t numEngines = 7;

    DebugManagerStateRestore restore;
    debugManager.flags.ContextGroupSize.set(4);

    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrRcsNode = false;
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.featureTable.ftrBcsInfo = maxNBitValue(3);
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 1;

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
        bool isHighPriority;
    };

    const std::array<EnginePropertiesMap, numEngines> enginePropertiesMap = {{
        {aub_stream::ENGINE_CCS, true, false, false, false},
        {aub_stream::ENGINE_CCS, false, true, false, false},
        {aub_stream::ENGINE_CCS, false, false, true, false},
        {aub_stream::ENGINE_BCS1, true, false, false, false},
        {aub_stream::ENGINE_BCS1, false, false, true, false},
        {aub_stream::ENGINE_BCS1, false, true, false, false},
        {aub_stream::ENGINE_BCS2, false, false, false, true},
    }};

    for (size_t i = 0; i < numEngines; i++) {
        const auto &engine = engines[i];
        EXPECT_EQ(enginePropertiesMap[i].engineType, engine.first);
        EXPECT_EQ(enginePropertiesMap[i].isRegular, engine.second == EngineUsage::regular);
        EXPECT_EQ(enginePropertiesMap[i].isLowPriority, engine.second == EngineUsage::lowPriority);
        EXPECT_EQ(enginePropertiesMap[i].isInternal, engine.second == EngineUsage::internal);
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
    DebugManagerStateRestore restore;
    debugManager.flags.Enable64BitAddressing.set(1);

    setUpImpl();
    auto defaultMaxWorkGroupSize = 2048u;
    const auto &gfxCoreHelper = pDevice->getGfxCoreHelper();
    const auto &rootDeviceEnvironment = pDevice->getRootDeviceEnvironment();
    std::array<std::array<uint32_t, 3>, 15> values = {{
        {128u, 16u, 1024u}, // Grf Size, SIMT Size, Max Num of threads
        {128u, 32u, 1024u},
        {160u, 16u, 768u},
        {160u, 32u, 1024u},
        {192u, 16u, 640u},
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

struct GfxCoreHelperTestsCriWithEnginesCheck : public GfxCoreHelperTestWithEnginesCheck {
    void setUpImpl() {
        hardwareInfo = *defaultHwInfo;
        auto releaseHelper = ReleaseHelper::create(hardwareInfo.ipVersion);
        hardwareInfoSetup[hardwareInfo.platform.eProductFamily](&hardwareInfo, true, 0, releaseHelper.get());
        DeviceFixture::setUpImpl(&hardwareInfo);
    }

    void SetUp() override {}
};

CRITEST_F(GfxCoreHelperTestsCriWithEnginesCheck, whenGetGpgpuEnginesThenReturnTwoCccsEnginesAndFourCcsEnginesAndEightLinkCopyEngines) {
    DebugManagerStateRestore restore;

    const size_t numEnginesWithCccs = 18;
    const size_t numEnginesWithoutCccs = 17;

    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.featureTable.ftrBcsInfo = maxNBitValue(9);
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 4;

    for (auto debugFlag : {false, true}) {
        debugManager.flags.NodeOrdinal.set(debugFlag ? static_cast<int32_t>(aub_stream::ENGINE_CCCS) : -1);

        auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0));

        auto releaseHelper = device->getReleaseHelper();
        auto &gfxCoreHelper = device->getGfxCoreHelper();

        bool cccsEnabled = !releaseHelper->isRcsExposureDisabled() || debugFlag;

        if (cccsEnabled) {
            EXPECT_EQ(numEnginesWithCccs, device->allEngines.size());
        } else {
            EXPECT_EQ(numEnginesWithoutCccs, device->allEngines.size());
        }

        device->getRootDeviceEnvironmentRef().setRcsExposure();
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
        EXPECT_EQ(1u, getEngineCount(debugFlag ? aub_stream::ENGINE_CCCS : aub_stream::ENGINE_CCS, EngineUsage::lowPriority));

        for (uint32_t idx = 1u; idx < hwInfo.featureTable.ftrBcsInfo.size(); idx++) {
            if (idx == 8 && gfxCoreHelper.areSecondaryContextsSupported()) {
                EXPECT_EQ(1u, getEngineCount(EngineHelpers::getBcsEngineAtIdx(idx), EngineUsage::highPriority));
            } else {
                EXPECT_EQ(1u, getEngineCount(EngineHelpers::getBcsEngineAtIdx(idx), EngineUsage::regular));
            }
        }
        EXPECT_EQ(1u, getEngineCount(device->getProductHelper().getDefaultCopyEngine(), EngineUsage::internal));
        if (gfxCoreHelper.areSecondaryContextsSupported()) {
            EXPECT_EQ(1u, getEngineCount(device->getProductHelper().getDefaultCopyEngine(), EngineUsage::lowPriority));
        }
        EXPECT_EQ(1u, getEngineCount(aub_stream::ENGINE_BCS3, EngineUsage::internal));
        EXPECT_TRUE(allEnginesChecked());

        resetEngineCounters();
    }
}
