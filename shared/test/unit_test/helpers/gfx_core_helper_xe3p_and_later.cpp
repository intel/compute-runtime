/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/helpers/engine_node_helper.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/gfx_core_helper_tests.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/hw_test.h"

using namespace NEO;

namespace ContextGroup {
extern uint32_t maxContextCount;
}

using GfxCoreHelperXe3pAndLaterTests = GfxCoreHelperTest;
using GfxCoreHelperXe3pAndLaterTestsWithEnginesCheck = GfxCoreHelperTestWithEnginesCheck;

HWTEST2_F(GfxCoreHelperXe3pAndLaterTests, givenHwQueuesSupportFlagWhenInitializeFromProductHelperThenSecondaryContextsFollowIt, IsAtLeastXe3pCore) {
    DebugManagerStateRestore restore;
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    const auto &productHelper = getHelper<ProductHelper>();

    gfxCoreHelper.initializeFromProductHelper(productHelper, false);
    EXPECT_FALSE(gfxCoreHelper.areSecondaryContextsSupported());
    EXPECT_EQ(0u, gfxCoreHelper.getContextGroupContextsCount());

    gfxCoreHelper.initializeFromProductHelper(productHelper, true);
    EXPECT_TRUE(gfxCoreHelper.areSecondaryContextsSupported());
    EXPECT_EQ(ContextGroup::maxContextCount, gfxCoreHelper.getContextGroupContextsCount());
}

HWTEST2_F(GfxCoreHelperXe3pAndLaterTests, givenMaxContextCountAboveDefaultWhenGettingContextGroupContextsCountThenValueIsNotClamped, IsAtLeastXe3pCore) {
    DebugManagerStateRestore restore;
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    const auto &productHelper = getHelper<ProductHelper>();
    gfxCoreHelper.initializeFromProductHelper(productHelper, true);

    constexpr uint32_t defaultContextGroupCount = 64u;
    auto maxContextCountToRestore = ContextGroup::maxContextCount;
    ContextGroup::maxContextCount = defaultContextGroupCount * 2;
    const auto contextGroupCount = gfxCoreHelper.getContextGroupContextsCount();
    ContextGroup::maxContextCount = maxContextCountToRestore;

    EXPECT_EQ(defaultContextGroupCount, contextGroupCount);
}

HWTEST2_F(GfxCoreHelperXe3pAndLaterTests, givenContextGroupSizeDebugFlagSetWhenGettingContextGroupContextsCountThenDebugFlagValueIsReturned, IsAtLeastXe3pCore) {
    DebugManagerStateRestore restore;
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    const auto &productHelper = getHelper<ProductHelper>();
    gfxCoreHelper.initializeFromProductHelper(productHelper, false);

    debugManager.flags.ContextGroupSize.set(5);
    EXPECT_EQ(5u, gfxCoreHelper.getContextGroupContextsCount());
}

HWTEST2_F(GfxCoreHelperXe3pAndLaterTests, givenAllocDataWhenSetExtraAllocationDataThenSetLocalMemForProperTypes, IsAtLeastXe3pCore) {
    DebugManagerStateRestore restorer;
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();

    for (auto duplicatedCounterStorage : {0, 1}) {
        debugManager.flags.InOrderDuplicatedCounterStorageEnabled.set(duplicatedCounterStorage);

        for (int type = 0; type < static_cast<int>(AllocationType::count); type++) {
            AllocationProperties allocProperties(0, 1, static_cast<AllocationType>(type), {});
            AllocationData allocData{};
            allocData.flags.useSystemMemory = true;
            allocData.flags.requiresCpuAccess = false;

            gfxCoreHelper.setExtraAllocationData(allocData, allocProperties, pDevice->getRootDeviceEnvironment());

            if (defaultHwInfo->featureTable.flags.ftrLocalMemory) {
                if (allocProperties.allocationType == AllocationType::commandBuffer ||
                    allocProperties.allocationType == AllocationType::ringBuffer) {
                    EXPECT_FALSE(allocData.flags.useSystemMemory);
                    EXPECT_TRUE(allocData.flags.requiresCpuAccess);
                } else if (allocProperties.allocationType == AllocationType::semaphoreBuffer) {
                    if (getHelper<ProductHelper>().isAcquireGlobalFenceInDirectSubmissionRequired(pDevice->getHardwareInfo())) {
                        EXPECT_FALSE(allocData.flags.useSystemMemory);
                    } else {
                        EXPECT_TRUE(allocData.flags.useSystemMemory);
                    }
                    EXPECT_TRUE(allocData.flags.requiresCpuAccess);
                } else if (allocProperties.allocationType == AllocationType::timestampPacketTagBuffer) {
                    EXPECT_EQ(duplicatedCounterStorage == 1, !!allocData.flags.useSystemMemory);
                    EXPECT_FALSE(allocData.flags.requiresCpuAccess);
                } else {
                    EXPECT_FALSE(allocData.flags.useSystemMemory);
                    EXPECT_FALSE(allocData.flags.requiresCpuAccess);
                }
            }
        }
    }
}

HWTEST2_F(GfxCoreHelperXe3pAndLaterTests, givenAtLeastXe3pWhenEncodeAdditionalTimestampOffsetsThenOffsetsEncoded, IsAtLeastXe3pCore) {
    using MI_STORE_REGISTER_MEM = typename FamilyType::MI_STORE_REGISTER_MEM;
    constexpr static auto bufferSize = sizeof(MI_STORE_REGISTER_MEM) * 2;

    char streamBuffer[bufferSize];
    LinearStream stream(streamBuffer, bufferSize);
    uint64_t fstAddress = 12;
    uint64_t sndAddress = 100;
    MemorySynchronizationCommands<FamilyType>::encodeAdditionalTimestampOffsets(stream, fstAddress, sndAddress, false);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(stream, 0);
    GenCmdList storeRegMemList = hwParser.getCommandsList<MI_STORE_REGISTER_MEM>();
    EXPECT_EQ(2u, storeRegMemList.size());
    auto storeRegMemIt = find<MI_STORE_REGISTER_MEM *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    EXPECT_NE(storeRegMemIt, hwParser.cmdList.end());

    auto storeRegMem = genCmdCast<MI_STORE_REGISTER_MEM *>(*storeRegMemIt);
    EXPECT_EQ(storeRegMem->getRegisterAddress(), ContextTimestampRegister<FamilyType>::getRegisterOffsetHigh());
    EXPECT_EQ(storeRegMem->getMemoryAddress(), fstAddress + sizeof(uint32_t));

    storeRegMem = genCmdCast<MI_STORE_REGISTER_MEM *>(*(++storeRegMemIt));
    EXPECT_EQ(storeRegMem->getRegisterAddress(), RegisterOffsets::globalTimestampUn);
    EXPECT_EQ(storeRegMem->getMemoryAddress(), sndAddress + sizeof(uint32_t));
}

HWTEST2_F(GfxCoreHelperXe3pAndLaterTestsWithEnginesCheck, givenGroupContextWhenCreatingDeviceThenCreateBcsLpContexts, IsAtLeastXe3pCore) {
    DebugManagerStateRestore restore;
    debugManager.flags.ContextGroupSize.set(2);

    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.featureTable.ftrBcsInfo = maxNBitValue(9);
    hwInfo.featureTable.ftrBcsInfo.set(0, false);
    hwInfo.featureTable.ftrBcsInfo.set(2, false);
    hwInfo.featureTable.ftrBcsInfo.set(7, false);
    hwInfo.featureTable.ftrBcsInfo.set(8, false);
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 1;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0));
    auto &gfxCoreHelper = device->getGfxCoreHelper();
    auto &engines = gfxCoreHelper.getGpgpuEngineInstances(device->getRootDeviceEnvironment());

    uint32_t bcsLpContextsCount = 0;
    for (auto &engine : device->getAllEngines()) {
        if (EngineHelpers::isBcs(engine.getEngineType()) && engine.getEngineUsage() == EngineUsage::lowPriority) {
            EXPECT_EQ(1u, EngineHelpers::getBcsIndex(engine.getEngineType()));
            bcsLpContextsCount++;
        }
    }

    EXPECT_EQ(1u, bcsLpContextsCount);

    bcsLpContextsCount = 0;
    for (auto &engine : engines) {
        if (EngineHelpers::isBcs(engine.first) && engine.second == EngineUsage::lowPriority) {
            EXPECT_EQ(1u, EngineHelpers::getBcsIndex(engine.first));
            bcsLpContextsCount++;
        }
    }

    EXPECT_EQ(1u, bcsLpContextsCount);
}

HWTEST2_F(GfxCoreHelperXe3pAndLaterTests, givenBcsDisabledWhenGetEnginesCalledThenDontCreateAnyBcs, IsAtLeastXe3pCore) {
    const size_t numEngines = 6;

    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.featureTable.ftrBcsInfo = 0;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 4;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0));
    auto &gfxCoreHelper = device->getGfxCoreHelper();
    EXPECT_EQ(numEngines, device->allEngines.size());
    auto &engines = gfxCoreHelper.getGpgpuEngineInstances(device->getRootDeviceEnvironment());
    EXPECT_EQ(numEngines, engines.size());

    struct EnginePropertiesMap {
        aub_stream::EngineType engineType;
        bool isCcs;
        bool isBcs;
    };

    const std::array<EnginePropertiesMap, numEngines> enginePropertiesMap = {{
        {aub_stream::ENGINE_CCS, true, false},
        {aub_stream::ENGINE_CCS1, true, false},
        {aub_stream::ENGINE_CCS2, true, false},
        {aub_stream::ENGINE_CCS3, true, false},
        {aub_stream::ENGINE_CCS, true, false},
        {aub_stream::ENGINE_CCS, true, false},
    }};

    for (size_t i = 0; i < numEngines; i++) {
        EXPECT_EQ(enginePropertiesMap[i].engineType, engines[i].first);
        EXPECT_EQ(enginePropertiesMap[i].isCcs, EngineHelpers::isCcs(enginePropertiesMap[i].engineType));
        EXPECT_EQ(enginePropertiesMap[i].isBcs, EngineHelpers::isBcs(enginePropertiesMap[i].engineType));
    }
}

HWTEST2_F(GfxCoreHelperXe3pAndLaterTests, givenCcsDisabledAndNumberOfCcsEnabledWhenGetGpgpuEnginesThenReturnCcsAndCccsEngines, IsAtLeastXe3pCore) {
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrCCSNode = false;
    hwInfo.featureTable.ftrBcsInfo = 0;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 4;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0));
    auto &gfxCoreHelper = device->getGfxCoreHelper();
    EXPECT_EQ(6u, device->allEngines.size());
    auto &engines = gfxCoreHelper.getGpgpuEngineInstances(device->getRootDeviceEnvironment());
    EXPECT_EQ(6u, engines.size());

    EXPECT_EQ(aub_stream::ENGINE_CCS, engines[0].first);
    EXPECT_EQ(aub_stream::ENGINE_CCS1, engines[1].first);
    EXPECT_EQ(aub_stream::ENGINE_CCS2, engines[2].first);
    EXPECT_EQ(aub_stream::ENGINE_CCS3, engines[3].first);
    EXPECT_EQ(aub_stream::ENGINE_CCCS, engines[4].first);
    EXPECT_EQ(aub_stream::ENGINE_CCCS, engines[5].first);
}

HWTEST2_F(GfxCoreHelperXe3pAndLaterTests, givenCcsDisabledWhenGetGpgpuEnginesThenReturnCccsEngines, IsAtLeastXe3pCore) {
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrCCSNode = false;
    hwInfo.featureTable.ftrBcsInfo = 0;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 0;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0));
    auto &gfxCoreHelper = device->getGfxCoreHelper();
    EXPECT_EQ(2u, device->allEngines.size());
    auto &engines = gfxCoreHelper.getGpgpuEngineInstances(device->getRootDeviceEnvironment());
    EXPECT_EQ(2u, engines.size());

    EXPECT_EQ(aub_stream::ENGINE_CCCS, engines[0].first);
    EXPECT_EQ(aub_stream::ENGINE_CCCS, engines[1].first);
}
