/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/engine_node_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/memory_manager/memory_banks.h"
#include "shared/source/os_interface/linux/engine_info.h"
#include "shared/source/os_interface/linux/i915.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/libult/linux/drm_mock_helper.h"
#include "shared/test/common/libult/linux/drm_query_mock.h"
#include "shared/test/common/test_macros/hw_test.h"

using namespace NEO;

using DrmTestXeHPAndLater = ::testing::Test;
using DrmTestXeHPCAndLater = ::testing::Test;

TEST(DrmTest, givenEngineQuerySupportedWhenQueryingEngineInfoThenEngineInfoIsInitialized) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(NEO::defaultHwInfo.get());
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();

    auto drm = std::make_unique<DrmQueryMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    auto hwInfo = drm->rootDeviceEnvironment.getHardwareInfo();
    ASSERT_NE(nullptr, drm);
    drm->queryEngineInfo();
    auto haveLocalMemory = hwInfo->gtSystemInfo.MultiTileArchInfo.IsValid;
    EXPECT_EQ(haveLocalMemory ? 3u : 2u, drm->ioctlCallsCount);
    ASSERT_NE(nullptr, drm->engineInfo);

    auto renderEngine = EngineHelpers::remapEngineTypeToHwSpecific(aub_stream::EngineType::ENGINE_RCS, *hwInfo);

    auto engineInfo = drm->engineInfo.get();
    auto pEngine = engineInfo->getEngineInstance(0, renderEngine);
    ASSERT_NE(nullptr, pEngine);
    EXPECT_EQ(drm_i915_gem_engine_class::I915_ENGINE_CLASS_RENDER, pEngine->engineClass);
    EXPECT_EQ(0, DrmMockHelper::getIdFromEngineOrMemoryInstance(pEngine->engineInstance));
    EXPECT_EQ(0u, engineInfo->getEngineTileIndex(*pEngine));

    auto numberOfCCS = hwInfo->gtSystemInfo.CCSInfo.NumberOfCCSEnabled;
    EXPECT_EQ(numberOfCCS > 0u, hwInfo->featureTable.flags.ftrCCSNode);
    for (auto i = 0u; i < numberOfCCS; i++) {
        auto engineType = static_cast<aub_stream::EngineType>(aub_stream::ENGINE_CCS + i);
        pEngine = engineInfo->getEngineInstance(0, engineType);
        ASSERT_NE(nullptr, pEngine);
        EXPECT_EQ(DrmPrelimHelper::getComputeEngineClass(), pEngine->engineClass);
        EXPECT_EQ(i, DrmMockHelper::getIdFromEngineOrMemoryInstance(pEngine->engineInstance));
    }

    pEngine = engineInfo->getEngineInstance(10, renderEngine);
    EXPECT_EQ(nullptr, pEngine);
    pEngine = engineInfo->getEngineInstance(42, aub_stream::ENGINE_CCS);
    EXPECT_EQ(nullptr, pEngine);
    pEngine = engineInfo->getEngineInstance(0, aub_stream::ENGINE_VECS);
    EXPECT_EQ(nullptr, pEngine);
    EngineClassInstance engineTest = {};
    engineTest.engineClass = DrmPrelimHelper::getComputeEngineClass();
    engineTest.engineInstance = 20;
    EXPECT_EQ(0u, engineInfo->getEngineTileIndex(engineTest));
}

TEST(DrmTest, givenEngineQueryNotSupportedWhenQueryingEngineInfoThenFailGracefully) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(NEO::defaultHwInfo.get());
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();

    auto drm = std::make_unique<DrmQueryMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    ASSERT_NE(nullptr, drm);
    drm->i915QuerySuccessCount = 0;
    drm->queryEngineInfo();
    EXPECT_EQ(1u, drm->ioctlCallsCount);
    EXPECT_EQ(nullptr, drm->engineInfo);
}

TEST(DrmTest, givenMemRegionQueryNotSupportedWhenQueryingMemRegionInfoThenFailGracefully) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(NEO::defaultHwInfo.get());
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();

    auto drm = std::make_unique<DrmQueryMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    ASSERT_NE(nullptr, drm);
    drm->i915QuerySuccessCount = 1;
    drm->queryEngineInfo();
    EXPECT_EQ(2u, drm->ioctlCallsCount);
    EXPECT_EQ(nullptr, drm->engineInfo);
}

TEST(DrmTest, givenDistanceQueryNotSupportedWhenQueryingDistanceInfoThenFailGracefully) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(NEO::defaultHwInfo.get());
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();

    auto drm = std::make_unique<DrmQueryMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    ASSERT_NE(nullptr, drm);
    drm->i915QuerySuccessCount = 2;
    drm->queryEngineInfo();
    auto hwInfo = drm->rootDeviceEnvironment.getHardwareInfo();
    auto haveLocalMemory = hwInfo->gtSystemInfo.MultiTileArchInfo.IsValid;
    if (haveLocalMemory) {
        EXPECT_EQ(3u, drm->ioctlCallsCount);
        EXPECT_EQ(nullptr, drm->engineInfo);
    } else {
        EXPECT_EQ(2u, drm->ioctlCallsCount);
        EXPECT_NE(nullptr, drm->engineInfo);
    }
}

TEST(DrmTest, givenDistanceQueryFailsWhenQueryingDistanceInfoThenFailGracefully) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(NEO::defaultHwInfo.get());
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();

    auto drm = std::make_unique<DrmQueryMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    ASSERT_NE(nullptr, drm);
    drm->context.failDistanceInfoQuery = true;
    drm->queryEngineInfo();
    auto hwInfo = drm->rootDeviceEnvironment.getHardwareInfo();
    auto haveLocalMemory = hwInfo->gtSystemInfo.MultiTileArchInfo.IsValid;

    if (haveLocalMemory) {
        EXPECT_EQ(3u, drm->ioctlCallsCount);

        for (uint32_t i = 0; i < hwInfo->gtSystemInfo.MultiTileArchInfo.TileCount; i++) {
            EXPECT_NO_THROW(drm->memoryInfo->getMemoryRegionClassAndInstance((1 << i), *drm->context.hwInfo));
        }
    } else {
        EXPECT_EQ(2u, drm->ioctlCallsCount);
    }
}

static void givenEngineTypeWhenBindingDrmContextThenContextParamEngineIsSet(std::unique_ptr<DrmQueryMock> &drm, aub_stream::EngineType engineType, uint16_t engineClass) {
    drm->queryEngineInfo();
    auto hwInfo = drm->rootDeviceEnvironment.getHardwareInfo();
    auto haveLocalMemory = hwInfo->gtSystemInfo.MultiTileArchInfo.IsValid;
    EXPECT_EQ(haveLocalMemory ? 3u : 2u, drm->ioctlCallsCount);
    ASSERT_NE(nullptr, drm->engineInfo);

    auto drmContextId = 42u;
    auto engineFlag = drm->bindDrmContext(drmContextId, 0u, engineType, false);

    I915_DEFINE_CONTEXT_PARAM_ENGINES(enginesStruct, 1){};
    EXPECT_EQ(static_cast<unsigned int>(I915_EXEC_DEFAULT), engineFlag);
    EXPECT_EQ(haveLocalMemory ? 4u : 3u, drm->ioctlCallsCount);
    EXPECT_EQ(1u, drm->receivedContextParamRequestCount);
    EXPECT_EQ(drmContextId, drm->receivedContextParamRequest.contextId);
    EXPECT_EQ(static_cast<uint64_t>(I915_CONTEXT_PARAM_ENGINES), drm->receivedContextParamRequest.param);
    EXPECT_EQ(ptrDiff(enginesStruct.engines + 1u, &enginesStruct), drm->receivedContextParamRequest.size);
    auto extensions = drm->receivedContextParamEngines.extensions;
    EXPECT_EQ(0ull, extensions);
    EXPECT_EQ(engineClass, drm->receivedContextParamEngines.engines[0].engineClass);
    EXPECT_EQ(0u, DrmMockHelper::getIdFromEngineOrMemoryInstance(drm->receivedContextParamEngines.engines[0].engineInstance));
}

TEST(DrmTest, givenRcsEngineWhenBindingDrmContextThenContextParamEngineIsSet) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(NEO::defaultHwInfo.get());
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();

    auto drm = std::make_unique<DrmQueryMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    auto hwInfo = drm->rootDeviceEnvironment.getHardwareInfo();

    auto renderEngine = EngineHelpers::remapEngineTypeToHwSpecific(aub_stream::EngineType::ENGINE_RCS, *hwInfo);

    givenEngineTypeWhenBindingDrmContextThenContextParamEngineIsSet(drm, renderEngine, drm_i915_gem_engine_class::I915_ENGINE_CLASS_RENDER);
}

static void givenBcsEngineTypeWhenBindingDrmContextThenContextParamEngineIsSet(std::unique_ptr<DrmQueryMock> &drm, aub_stream::EngineType engineType, size_t numBcsSiblings, unsigned int engineIndex, uint32_t tileId) {
    auto drmContextId = 42u;
    drm->receivedContextParamRequestCount = 0u;
    auto engineFlag = drm->bindDrmContext(drmContextId, tileId, engineType, false);
    EXPECT_EQ(static_cast<unsigned int>(I915_EXEC_DEFAULT), engineFlag);
    EXPECT_EQ(1u, drm->receivedContextParamRequestCount);
    EXPECT_EQ(drmContextId, drm->receivedContextParamRequest.contextId);
    EXPECT_EQ(static_cast<uint64_t>(I915_CONTEXT_PARAM_ENGINES), drm->receivedContextParamRequest.param);
    auto extensions = drm->receivedContextParamEngines.extensions;

    if (EngineHelpers::isBcsVirtualEngineEnabled(engineType)) {
        EXPECT_NE(0ull, extensions);
        EXPECT_EQ(ptrDiff(drm->receivedContextParamEngines.engines + 1 + numBcsSiblings, &drm->receivedContextParamEngines), drm->receivedContextParamRequest.size);
        EXPECT_EQ(static_cast<__u16>(drm_i915_gem_engine_class::I915_ENGINE_CLASS_INVALID), drm->receivedContextParamEngines.engines[0].engineClass);
        EXPECT_EQ(static_cast<__u16>(I915_ENGINE_CLASS_INVALID_NONE), drm->receivedContextParamEngines.engines[0].engineInstance);
        EXPECT_EQ(static_cast<__u32>(I915_CONTEXT_ENGINES_EXT_LOAD_BALANCE), drm->receivedContextEnginesLoadBalance.base.name);
        EXPECT_EQ(numBcsSiblings, drm->receivedContextEnginesLoadBalance.numSiblings);
        for (auto balancedEngine = 0u; balancedEngine < numBcsSiblings; balancedEngine++) {
            EXPECT_EQ(drm_i915_gem_engine_class::I915_ENGINE_CLASS_COPY, drm->receivedContextEnginesLoadBalance.engines[balancedEngine].engineClass);
            auto engineInstance = engineIndex ? balancedEngine + 1 : balancedEngine;
            EXPECT_EQ(engineInstance, DrmMockHelper::getIdFromEngineOrMemoryInstance(drm->receivedContextEnginesLoadBalance.engines[balancedEngine].engineInstance));
            EXPECT_EQ(drm_i915_gem_engine_class::I915_ENGINE_CLASS_COPY, drm->receivedContextParamEngines.engines[1 + balancedEngine].engineClass);
            EXPECT_EQ(engineInstance, DrmMockHelper::getIdFromEngineOrMemoryInstance(drm->receivedContextParamEngines.engines[1 + balancedEngine].engineInstance));
            auto engineTile = DrmMockHelper::getTileFromEngineOrMemoryInstance(drm->receivedContextEnginesLoadBalance.engines[balancedEngine].engineInstance);
            EXPECT_EQ(engineTile, tileId);
        }
    } else {
        EXPECT_EQ(0ull, extensions);
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE, DrmTestXeHPAndLater, givenBcsEngineWhenBindingDrmContextThenContextParamEngineIsSet) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(NEO::defaultHwInfo.get());
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();

    auto drm = std::make_unique<DrmQueryMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    givenEngineTypeWhenBindingDrmContextThenContextParamEngineIsSet(drm, aub_stream::ENGINE_BCS, drm_i915_gem_engine_class::I915_ENGINE_CLASS_COPY);
    auto hwInfo = drm->rootDeviceEnvironment.getHardwareInfo();
    EXPECT_EQ(1u, hwInfo->featureTable.ftrBcsInfo.to_ulong());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, DrmTestXeHPAndLater, givenLinkBcsEngineWhenBindingSingleTileDrmContextThenContextParamEngineIsSet) {
    HardwareInfo localHwInfo = *defaultHwInfo;
    localHwInfo.gtSystemInfo.MultiTileArchInfo.IsValid = false;
    localHwInfo.gtSystemInfo.MultiTileArchInfo.TileCount = 0;
    localHwInfo.gtSystemInfo.MultiTileArchInfo.TileMask = 0;

    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    auto drm = std::make_unique<DrmQueryMock>(*executionEnvironment->rootDeviceEnvironments[0], &localHwInfo);
    drm->supportedCopyEnginesMask = maxNBitValue(9);

    drm->queryEngineInfo();
    EXPECT_NE(nullptr, drm->engineInfo);
    auto hwInfo = drm->rootDeviceEnvironment.getHardwareInfo();
    EXPECT_EQ(drm->supportedCopyEnginesMask.count(), hwInfo->featureTable.ftrBcsInfo.count());

    for (auto engineIndex = 0u; engineIndex < drm->supportedCopyEnginesMask.size(); engineIndex++) {
        auto engineType = aub_stream::ENGINE_BCS;
        auto numBcsSiblings = drm->supportedCopyEnginesMask.count();
        if (engineIndex > 0) {
            engineType = static_cast<aub_stream::EngineType>(aub_stream::ENGINE_BCS1 + engineIndex - 1);
            numBcsSiblings -= 1;
        }
        givenBcsEngineTypeWhenBindingDrmContextThenContextParamEngineIsSet(drm, engineType, numBcsSiblings, engineIndex, 0u);
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE, DrmTestXeHPAndLater, givenLinkBcsEngineWithoutMainCopyEngineWhenBindingSingleTileDrmContextThenContextParamEngineIsSet) {
    DebugManagerStateRestore restore;
    DebugManager.flags.UseDrmVirtualEnginesForBcs.set(1);
    HardwareInfo localHwInfo = *defaultHwInfo;
    localHwInfo.gtSystemInfo.MultiTileArchInfo.IsValid = false;
    localHwInfo.gtSystemInfo.MultiTileArchInfo.TileCount = 0;
    localHwInfo.gtSystemInfo.MultiTileArchInfo.TileMask = 0;

    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    auto drm = std::make_unique<DrmQueryMock>(*executionEnvironment->rootDeviceEnvironments[0], &localHwInfo);
    drm->supportedCopyEnginesMask = maxNBitValue(9);
    drm->supportedCopyEnginesMask.set(0, false);

    drm->queryEngineInfo();
    EXPECT_NE(nullptr, drm->engineInfo);
    auto hwInfo = drm->rootDeviceEnvironment.getHardwareInfo();
    EXPECT_EQ(drm->supportedCopyEnginesMask.count(), hwInfo->featureTable.ftrBcsInfo.count());

    for (auto engineIndex = 0u; engineIndex < drm->supportedCopyEnginesMask.size(); engineIndex++) {
        drm->receivedContextParamRequestCount = 0u;
        auto drmContextId = 42u;
        auto engineType = aub_stream::ENGINE_BCS;
        auto numBcsSiblings = drm->supportedCopyEnginesMask.count();
        if (engineIndex > 0) {
            engineType = static_cast<aub_stream::EngineType>(aub_stream::ENGINE_BCS1 + engineIndex - 1);
            numBcsSiblings -= 1;
        }
        auto engineFlag = drm->bindDrmContext(drmContextId, 0u, engineType, false);

        if (engineIndex == 0) {
            EXPECT_EQ(static_cast<unsigned int>(I915_EXEC_BLT), engineFlag);
            EXPECT_EQ(0u, drm->receivedContextParamRequestCount);
        } else {
            EXPECT_EQ(static_cast<unsigned int>(I915_EXEC_DEFAULT), engineFlag);
            EXPECT_EQ(1u, drm->receivedContextParamRequestCount);
            EXPECT_EQ(drmContextId, drm->receivedContextParamRequest.contextId);
            EXPECT_EQ(static_cast<uint64_t>(I915_CONTEXT_PARAM_ENGINES), drm->receivedContextParamRequest.param);
            auto extensions = drm->receivedContextParamEngines.extensions;
            EXPECT_NE(0ull, extensions);
            EXPECT_EQ(ptrDiff(drm->receivedContextParamEngines.engines + 1 + numBcsSiblings, &drm->receivedContextParamEngines), drm->receivedContextParamRequest.size);
            EXPECT_EQ(static_cast<__u16>(drm_i915_gem_engine_class::I915_ENGINE_CLASS_INVALID), drm->receivedContextParamEngines.engines[0].engineClass);
            EXPECT_EQ(static_cast<__u16>(I915_ENGINE_CLASS_INVALID_NONE), drm->receivedContextParamEngines.engines[0].engineInstance);
            EXPECT_EQ(static_cast<__u32>(I915_CONTEXT_ENGINES_EXT_LOAD_BALANCE), drm->receivedContextEnginesLoadBalance.base.name);
            EXPECT_EQ(numBcsSiblings, drm->receivedContextEnginesLoadBalance.numSiblings);
        }
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE, DrmTestXeHPAndLater, giveNotAllLinkBcsEnginesWhenBindingSingleTileDrmContextThenContextParamEngineIsSet) {
    DebugManagerStateRestore restore;
    DebugManager.flags.UseDrmVirtualEnginesForBcs.set(1);
    HardwareInfo localHwInfo = *defaultHwInfo;
    localHwInfo.gtSystemInfo.MultiTileArchInfo.IsValid = false;
    localHwInfo.gtSystemInfo.MultiTileArchInfo.TileCount = 0;
    localHwInfo.gtSystemInfo.MultiTileArchInfo.TileMask = 0;

    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    auto drm = std::make_unique<DrmQueryMock>(*executionEnvironment->rootDeviceEnvironments[0], &localHwInfo);
    drm->supportedCopyEnginesMask = maxNBitValue(9);
    drm->supportedCopyEnginesMask.set(2, false);
    drm->supportedCopyEnginesMask.set(7, false);
    drm->supportedCopyEnginesMask.set(8, false);

    drm->queryEngineInfo();
    EXPECT_NE(nullptr, drm->engineInfo);
    auto hwInfo = drm->rootDeviceEnvironment.getHardwareInfo();
    EXPECT_EQ(drm->supportedCopyEnginesMask.count(), hwInfo->featureTable.ftrBcsInfo.count());

    for (auto engineIndex = 0u; engineIndex < drm->supportedCopyEnginesMask.size(); engineIndex++) {
        auto numBcsSiblings = drm->supportedCopyEnginesMask.count();
        auto engineType = aub_stream::ENGINE_BCS;
        if (engineIndex > 0) {
            engineType = static_cast<aub_stream::EngineType>(aub_stream::ENGINE_BCS1 + engineIndex - 1);
            numBcsSiblings -= 1;
        }

        auto drmContextId = 42u;
        drm->receivedContextParamRequestCount = 0u;
        auto engineFlag = drm->bindDrmContext(drmContextId, 0u, engineType, false);
        if (drm->supportedCopyEnginesMask.test(engineIndex)) {
            EXPECT_EQ(static_cast<unsigned int>(I915_EXEC_DEFAULT), engineFlag);
            EXPECT_EQ(1u, drm->receivedContextParamRequestCount);
            EXPECT_EQ(drmContextId, drm->receivedContextParamRequest.contextId);
            EXPECT_EQ(static_cast<uint64_t>(I915_CONTEXT_PARAM_ENGINES), drm->receivedContextParamRequest.param);
            auto extensions = drm->receivedContextParamEngines.extensions;
            EXPECT_NE(0ull, extensions);
            EXPECT_EQ(ptrDiff(drm->receivedContextParamEngines.engines + 1 + numBcsSiblings, &drm->receivedContextParamEngines), drm->receivedContextParamRequest.size);
            EXPECT_EQ(static_cast<__u16>(drm_i915_gem_engine_class::I915_ENGINE_CLASS_INVALID), drm->receivedContextParamEngines.engines[0].engineClass);
            EXPECT_EQ(static_cast<__u16>(I915_ENGINE_CLASS_INVALID_NONE), drm->receivedContextParamEngines.engines[0].engineInstance);
            EXPECT_EQ(static_cast<__u32>(I915_CONTEXT_ENGINES_EXT_LOAD_BALANCE), drm->receivedContextEnginesLoadBalance.base.name);
            EXPECT_EQ(numBcsSiblings, drm->receivedContextEnginesLoadBalance.numSiblings);
        } else {
            EXPECT_EQ(static_cast<unsigned int>(I915_EXEC_BLT), engineFlag);
            EXPECT_EQ(0u, drm->receivedContextParamRequestCount);
        }
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE, DrmTestXeHPAndLater, givenLinkBcsEngineWhenBindingMultitileDrmContextThenContextParamEngineIsSet) {
    auto localHwInfo = *defaultHwInfo;
    localHwInfo.gtSystemInfo.MultiTileArchInfo.IsValid = true;
    localHwInfo.gtSystemInfo.MultiTileArchInfo.TileCount = 4;
    localHwInfo.gtSystemInfo.MultiTileArchInfo.TileMask = 0b1111;

    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    auto drm = std::make_unique<DrmQueryMock>(*executionEnvironment->rootDeviceEnvironments[0], &localHwInfo);
    drm->supportedCopyEnginesMask = maxNBitValue(9);

    drm->queryEngineInfo();
    EXPECT_NE(nullptr, drm->engineInfo);
    auto hwInfo = drm->rootDeviceEnvironment.getHardwareInfo();
    EXPECT_EQ(drm->supportedCopyEnginesMask.count(), hwInfo->featureTable.ftrBcsInfo.count());

    for (auto tileId = 0; tileId < hwInfo->gtSystemInfo.MultiTileArchInfo.TileCount; tileId++) {
        for (auto engineIndex = 0u; engineIndex < drm->supportedCopyEnginesMask.count(); engineIndex++) {
            auto numBcsSiblings = drm->supportedCopyEnginesMask.count();
            auto engineType = aub_stream::ENGINE_BCS;
            if (engineIndex > 0) {
                engineType = static_cast<aub_stream::EngineType>(aub_stream::ENGINE_BCS1 + engineIndex - 1);
                numBcsSiblings -= 1;
            }

            givenBcsEngineTypeWhenBindingDrmContextThenContextParamEngineIsSet(drm, engineType, numBcsSiblings, engineIndex, static_cast<uint32_t>(tileId));
        }
    }
}

HWTEST2_F(DrmTestXeHPCAndLater, givenBcsVirtualEnginesEnabledWhenCreatingContextThenEnableLoadBalancing, IsAtLeastXeHpcCore) {
    DebugManagerStateRestore restore;
    DebugManager.flags.UseDrmVirtualEnginesForBcs.set(1);
    HardwareInfo localHwInfo = *defaultHwInfo;
    localHwInfo.gtSystemInfo.MultiTileArchInfo.IsValid = false;
    localHwInfo.gtSystemInfo.MultiTileArchInfo.TileCount = 0;
    localHwInfo.gtSystemInfo.MultiTileArchInfo.TileMask = 0;
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    auto drm = std::make_unique<DrmQueryMock>(*executionEnvironment->rootDeviceEnvironments[0], &localHwInfo);
    drm->supportedCopyEnginesMask = maxNBitValue(9);
    drm->queryEngineInfo();
    EXPECT_NE(nullptr, drm->engineInfo);
    auto hwInfo = drm->rootDeviceEnvironment.getHardwareInfo();
    EXPECT_EQ(drm->supportedCopyEnginesMask.count(), hwInfo->featureTable.ftrBcsInfo.count());
    auto numBcs = drm->supportedCopyEnginesMask.count();
    auto engineBase = static_cast<aub_stream::EngineType>(aub_stream::ENGINE_BCS);
    auto numBcsSiblings = numBcs;
    for (auto engineIndex = 0u; engineIndex < numBcs; engineIndex++) {
        if (engineIndex > 0u) {
            engineBase = static_cast<aub_stream::EngineType>(aub_stream::ENGINE_BCS1 - 1);
            numBcsSiblings = numBcs - 1;
        }
        auto engineType = static_cast<aub_stream::EngineType>(engineBase + engineIndex);
        givenBcsEngineTypeWhenBindingDrmContextThenContextParamEngineIsSet(drm, engineType, numBcsSiblings, engineIndex, 0u);
    }
}

HWTEST2_F(DrmTestXeHPCAndLater, givenBcsVirtualEnginesEnabledWhenCreatingContextThenEnableLoadBalancingLimitedToMaxCount, IsAtLeastXeHpcCore) {
    DebugManagerStateRestore restore;
    DebugManager.flags.UseDrmVirtualEnginesForBcs.set(1);
    DebugManager.flags.LimitEngineCountForVirtualBcs.set(3);
    HardwareInfo localHwInfo = *defaultHwInfo;
    localHwInfo.gtSystemInfo.MultiTileArchInfo.IsValid = false;
    localHwInfo.gtSystemInfo.MultiTileArchInfo.TileCount = 0;
    localHwInfo.gtSystemInfo.MultiTileArchInfo.TileMask = 0;
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    auto drm = std::make_unique<DrmQueryMock>(*executionEnvironment->rootDeviceEnvironments[0], &localHwInfo);
    drm->supportedCopyEnginesMask = maxNBitValue(9);
    drm->queryEngineInfo();
    EXPECT_NE(nullptr, drm->engineInfo);
    auto hwInfo = drm->rootDeviceEnvironment.getHardwareInfo();
    EXPECT_EQ(drm->supportedCopyEnginesMask.count(), hwInfo->featureTable.ftrBcsInfo.count());
    auto numBcs = 3u;
    auto engineBase = static_cast<aub_stream::EngineType>(aub_stream::ENGINE_BCS);
    auto numBcsSiblings = numBcs;
    for (auto engineIndex = 0u; engineIndex < numBcs; engineIndex++) {
        if (engineIndex > 0u) {
            engineBase = static_cast<aub_stream::EngineType>(aub_stream::ENGINE_BCS1 - 1);
            numBcsSiblings = numBcs - 1;
        }
        auto engineType = static_cast<aub_stream::EngineType>(engineBase + engineIndex);
        givenBcsEngineTypeWhenBindingDrmContextThenContextParamEngineIsSet(drm, engineType, numBcsSiblings, engineIndex, 0u);
    }
}

HWTEST2_F(DrmTestXeHPCAndLater, givenBcsVirtualEnginesDisabledWhenCreatingContextThenDisableLoadBalancing, IsAtLeastXeHpcCore) {
    DebugManagerStateRestore restore;
    DebugManager.flags.UseDrmVirtualEnginesForBcs.set(0);

    HardwareInfo localHwInfo = *defaultHwInfo;
    localHwInfo.gtSystemInfo.MultiTileArchInfo.IsValid = false;
    localHwInfo.gtSystemInfo.MultiTileArchInfo.TileCount = 0;
    localHwInfo.gtSystemInfo.MultiTileArchInfo.TileMask = 0;
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    auto drm = std::make_unique<DrmQueryMock>(*executionEnvironment->rootDeviceEnvironments[0], &localHwInfo);
    ASSERT_NE(nullptr, drm);
    drm->supportedCopyEnginesMask = maxNBitValue(9);
    drm->queryEngineInfo();
    EXPECT_NE(nullptr, drm->engineInfo);
    auto hwInfo = drm->rootDeviceEnvironment.getHardwareInfo();
    EXPECT_EQ(drm->supportedCopyEnginesMask.count(), hwInfo->featureTable.ftrBcsInfo.count());

    auto numBcs = drm->supportedCopyEnginesMask.count();
    auto engineBase = static_cast<aub_stream::EngineType>(aub_stream::ENGINE_BCS);
    for (auto engineIndex = 0u; engineIndex < numBcs; engineIndex++) {
        if (engineIndex > 0u) {
            engineBase = static_cast<aub_stream::EngineType>(aub_stream::ENGINE_BCS1 - 1);
        }
        drm->receivedContextParamRequestCount = 0u;
        auto drmContextId = 42u;
        auto engineType = static_cast<aub_stream::EngineType>(engineBase + engineIndex);
        auto engineFlag = drm->bindDrmContext(drmContextId, 0u, engineType, false);
        EXPECT_EQ(static_cast<unsigned int>(I915_EXEC_DEFAULT), engineFlag);
        EXPECT_EQ(1u, drm->receivedContextParamRequestCount);
        EXPECT_EQ(drmContextId, drm->receivedContextParamRequest.contextId);
        EXPECT_EQ(static_cast<uint64_t>(I915_CONTEXT_PARAM_ENGINES), drm->receivedContextParamRequest.param);
        EXPECT_EQ(ptrDiff(drm->receivedContextParamEngines.engines + 1, &drm->receivedContextParamEngines), drm->receivedContextParamRequest.size);
        auto extensions = drm->receivedContextParamEngines.extensions;
        EXPECT_EQ(0ull, extensions);
        EXPECT_EQ(static_cast<__u16>(drm_i915_gem_engine_class::I915_ENGINE_CLASS_COPY), drm->receivedContextParamEngines.engines[0].engineClass);
        EXPECT_EQ(static_cast<__u16>(engineIndex), DrmMockHelper::getIdFromEngineOrMemoryInstance(drm->receivedContextParamEngines.engines[0].engineInstance));
    }
}

TEST(DrmTest, givenOneCcsEngineWhenBindingDrmContextThenContextParamEngineIsSet) {
    auto &ccsInfo = defaultHwInfo->gtSystemInfo.CCSInfo;
    if (ccsInfo.IsValid && ccsInfo.NumberOfCCSEnabled == 1u) {
        auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
        executionEnvironment->prepareRootDeviceEnvironments(1);
        executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(NEO::defaultHwInfo.get());
        executionEnvironment->rootDeviceEnvironments[0]->initGmm();

        auto drm = std::make_unique<DrmQueryMock>(*executionEnvironment->rootDeviceEnvironments[0]);
        givenEngineTypeWhenBindingDrmContextThenContextParamEngineIsSet(drm, aub_stream::ENGINE_CCS, DrmPrelimHelper::getComputeEngineClass());
    }
}

TEST(DrmTest, givenVirtualEnginesEnabledWhenCreatingContextThenEnableLoadBalancing) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(NEO::defaultHwInfo.get());
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();

    auto drm = std::make_unique<DrmQueryMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    auto &ccsInfo = drm->rootDeviceEnvironment.getMutableHardwareInfo()->gtSystemInfo.CCSInfo;
    ccsInfo.IsValid = true;
    ccsInfo.NumberOfCCSEnabled = 4;
    auto numberOfCCS = ccsInfo.NumberOfCCSEnabled;

    ASSERT_NE(nullptr, drm);
    drm->queryEngineInfo();
    EXPECT_NE(nullptr, drm->engineInfo);

    for (auto engineIndex = 0u; engineIndex < numberOfCCS; engineIndex++) {
        drm->receivedContextParamRequestCount = 0u;
        auto drmContextId = 42u;
        auto engineType = static_cast<aub_stream::EngineType>(aub_stream::ENGINE_CCS + engineIndex);
        auto engineFlag = drm->bindDrmContext(drmContextId, 0u, engineType, false);
        EXPECT_EQ(static_cast<unsigned int>(I915_EXEC_DEFAULT), engineFlag);
        EXPECT_EQ(1u, drm->receivedContextParamRequestCount);
        EXPECT_EQ(drmContextId, drm->receivedContextParamRequest.contextId);
        EXPECT_EQ(static_cast<uint64_t>(I915_CONTEXT_PARAM_ENGINES), drm->receivedContextParamRequest.param);
        EXPECT_EQ(ptrDiff(drm->receivedContextParamEngines.engines + 1 + numberOfCCS, &drm->receivedContextParamEngines), drm->receivedContextParamRequest.size);
        auto extensions = drm->receivedContextParamEngines.extensions;
        EXPECT_NE(0ull, extensions);
        EXPECT_EQ(static_cast<__u32>(I915_CONTEXT_ENGINES_EXT_LOAD_BALANCE), drm->receivedContextEnginesLoadBalance.base.name);
        EXPECT_EQ(numberOfCCS, drm->receivedContextEnginesLoadBalance.numSiblings);
        EXPECT_EQ(static_cast<__u16>(drm_i915_gem_engine_class::I915_ENGINE_CLASS_INVALID), drm->receivedContextParamEngines.engines[0].engineClass);
        EXPECT_EQ(static_cast<__u16>(I915_ENGINE_CLASS_INVALID_NONE), drm->receivedContextParamEngines.engines[0].engineInstance);
        for (auto balancedEngine = 0u; balancedEngine < numberOfCCS; balancedEngine++) {
            EXPECT_EQ(DrmPrelimHelper::getComputeEngineClass(), drm->receivedContextEnginesLoadBalance.engines[balancedEngine].engineClass);
            EXPECT_EQ(balancedEngine, DrmMockHelper::getIdFromEngineOrMemoryInstance(drm->receivedContextEnginesLoadBalance.engines[balancedEngine].engineInstance));
            EXPECT_EQ(DrmPrelimHelper::getComputeEngineClass(), drm->receivedContextParamEngines.engines[1 + balancedEngine].engineClass);
            EXPECT_EQ(balancedEngine, DrmMockHelper::getIdFromEngineOrMemoryInstance(drm->receivedContextParamEngines.engines[1 + balancedEngine].engineInstance));
        }
    }
}

TEST(DrmTest, givenVirtualEnginesEnabledWhenCreatingContextThenEnableLoadBalancingOnlyBelowDebugVar) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.LimitEngineCountForVirtualCcs.set(2);

    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(NEO::defaultHwInfo.get());
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();

    auto drm = std::make_unique<DrmQueryMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    auto &ccsInfo = drm->rootDeviceEnvironment.getMutableHardwareInfo()->gtSystemInfo.CCSInfo;
    ccsInfo.IsValid = true;
    ccsInfo.NumberOfCCSEnabled = 4;
    auto numberOfCCS = 2u;

    ASSERT_NE(nullptr, drm);
    drm->queryEngineInfo();
    EXPECT_NE(nullptr, drm->engineInfo);

    for (auto engineIndex = 0u; engineIndex < numberOfCCS; engineIndex++) {
        drm->receivedContextParamRequestCount = 0u;
        auto drmContextId = 42u;
        auto engineType = static_cast<aub_stream::EngineType>(aub_stream::ENGINE_CCS + engineIndex);
        auto engineFlag = drm->bindDrmContext(drmContextId, 0u, engineType, false);
        EXPECT_EQ(static_cast<unsigned int>(I915_EXEC_DEFAULT), engineFlag);
        EXPECT_EQ(1u, drm->receivedContextParamRequestCount);
        EXPECT_EQ(drmContextId, drm->receivedContextParamRequest.contextId);
        EXPECT_EQ(static_cast<uint64_t>(I915_CONTEXT_PARAM_ENGINES), drm->receivedContextParamRequest.param);
        EXPECT_EQ(ptrDiff(drm->receivedContextParamEngines.engines + 1 + numberOfCCS, &drm->receivedContextParamEngines), drm->receivedContextParamRequest.size);
        auto extensions = drm->receivedContextParamEngines.extensions;
        EXPECT_NE(0ull, extensions);
        EXPECT_EQ(static_cast<__u32>(I915_CONTEXT_ENGINES_EXT_LOAD_BALANCE), drm->receivedContextEnginesLoadBalance.base.name);
        EXPECT_EQ(numberOfCCS, drm->receivedContextEnginesLoadBalance.numSiblings);
        EXPECT_EQ(static_cast<__u16>(drm_i915_gem_engine_class::I915_ENGINE_CLASS_INVALID), drm->receivedContextParamEngines.engines[0].engineClass);
        EXPECT_EQ(static_cast<__u16>(I915_ENGINE_CLASS_INVALID_NONE), drm->receivedContextParamEngines.engines[0].engineInstance);
        for (auto balancedEngine = 0u; balancedEngine < numberOfCCS; balancedEngine++) {
            EXPECT_EQ(DrmPrelimHelper::getComputeEngineClass(), drm->receivedContextEnginesLoadBalance.engines[balancedEngine].engineClass);
            EXPECT_EQ(balancedEngine, DrmMockHelper::getIdFromEngineOrMemoryInstance(drm->receivedContextEnginesLoadBalance.engines[balancedEngine].engineInstance));
            EXPECT_EQ(DrmPrelimHelper::getComputeEngineClass(), drm->receivedContextParamEngines.engines[1 + balancedEngine].engineClass);
            EXPECT_EQ(balancedEngine, DrmMockHelper::getIdFromEngineOrMemoryInstance(drm->receivedContextParamEngines.engines[1 + balancedEngine].engineInstance));
        }
    }
}

TEST(DrmTest, givenDisabledCcsSupportWhenQueryingThenResetHwInfoParams) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(NEO::defaultHwInfo.get());
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();

    auto drm = std::make_unique<DrmQueryMock>(*executionEnvironment->rootDeviceEnvironments[0]);

    drm->context.disableCcsSupport = true;

    auto hwInfo = drm->rootDeviceEnvironment.getMutableHardwareInfo();
    hwInfo->gtSystemInfo.CCSInfo.IsValid = true;
    hwInfo->gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 4;
    hwInfo->gtSystemInfo.CCSInfo.Instances.CCSEnableMask = 0b1111;
    hwInfo->capabilityTable.defaultEngineType = aub_stream::EngineType::ENGINE_CCS;

    drm->queryEngineInfo();
    EXPECT_NE(nullptr, drm->engineInfo);

    EXPECT_FALSE(hwInfo->gtSystemInfo.CCSInfo.IsValid);
    EXPECT_EQ(0u, hwInfo->gtSystemInfo.CCSInfo.NumberOfCCSEnabled);
    EXPECT_EQ(0u, hwInfo->gtSystemInfo.CCSInfo.Instances.CCSEnableMask);
    EXPECT_EQ(EngineHelpers::remapEngineTypeToHwSpecific(aub_stream::EngineType::ENGINE_RCS, *hwInfo),
              hwInfo->capabilityTable.defaultEngineType);
}

TEST(DrmTest, givenVirtualEnginesDisabledWhenCreatingContextThenDontEnableLoadBalancing) {
    DebugManagerStateRestore restore;
    DebugManager.flags.UseDrmVirtualEnginesForCcs.set(0);

    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(NEO::defaultHwInfo.get());
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();

    auto drm = std::make_unique<DrmQueryMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    auto &ccsInfo = drm->rootDeviceEnvironment.getMutableHardwareInfo()->gtSystemInfo.CCSInfo;
    ccsInfo.IsValid = true;
    ccsInfo.NumberOfCCSEnabled = 4;
    auto numberOfCCS = ccsInfo.NumberOfCCSEnabled;

    ASSERT_NE(nullptr, drm);
    drm->queryEngineInfo();
    EXPECT_NE(nullptr, drm->engineInfo);

    for (auto engineIndex = 0u; engineIndex < numberOfCCS; engineIndex++) {
        drm->receivedContextParamRequestCount = 0u;
        auto drmContextId = 42u;
        auto engineType = static_cast<aub_stream::EngineType>(aub_stream::ENGINE_CCS + engineIndex);
        auto engineFlag = drm->bindDrmContext(drmContextId, 0u, engineType, false);
        EXPECT_EQ(static_cast<unsigned int>(I915_EXEC_DEFAULT), engineFlag);
        EXPECT_EQ(1u, drm->receivedContextParamRequestCount);
        EXPECT_EQ(drmContextId, drm->receivedContextParamRequest.contextId);
        EXPECT_EQ(static_cast<uint64_t>(I915_CONTEXT_PARAM_ENGINES), drm->receivedContextParamRequest.param);
        EXPECT_EQ(ptrDiff(drm->receivedContextParamEngines.engines + 1, &drm->receivedContextParamEngines), drm->receivedContextParamRequest.size);
        auto extensions = drm->receivedContextParamEngines.extensions;
        EXPECT_EQ(0ull, extensions);
        EXPECT_EQ(static_cast<__u16>(DrmPrelimHelper::getComputeEngineClass()), drm->receivedContextParamEngines.engines[0].engineClass);
        EXPECT_EQ(static_cast<__u16>(engineIndex), DrmMockHelper::getIdFromEngineOrMemoryInstance(drm->receivedContextParamEngines.engines[0].engineInstance));
    }
}

TEST(DrmTest, givenEngineInstancedDeviceWhenCreatingContextThenDontUseVirtualEngines) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(NEO::defaultHwInfo.get());
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();

    auto drm = std::make_unique<DrmQueryMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    auto &ccsInfo = drm->rootDeviceEnvironment.getMutableHardwareInfo()->gtSystemInfo.CCSInfo;
    ccsInfo.IsValid = true;
    ccsInfo.NumberOfCCSEnabled = 4;
    auto numberOfCCS = ccsInfo.NumberOfCCSEnabled;

    ASSERT_NE(nullptr, drm);
    drm->queryEngineInfo();
    EXPECT_NE(nullptr, drm->engineInfo);

    for (auto engineIndex = 0u; engineIndex < numberOfCCS; engineIndex++) {
        drm->receivedContextParamRequestCount = 0u;
        auto drmContextId = 42u;
        auto engineType = static_cast<aub_stream::EngineType>(aub_stream::ENGINE_CCS + engineIndex);
        auto engineFlag = drm->bindDrmContext(drmContextId, 0u, engineType, true);
        EXPECT_EQ(static_cast<unsigned int>(I915_EXEC_DEFAULT), engineFlag);
        EXPECT_EQ(1u, drm->receivedContextParamRequestCount);
        EXPECT_EQ(drmContextId, drm->receivedContextParamRequest.contextId);
        EXPECT_EQ(static_cast<uint64_t>(I915_CONTEXT_PARAM_ENGINES), drm->receivedContextParamRequest.param);
        EXPECT_EQ(ptrDiff(drm->receivedContextParamEngines.engines + 1, &drm->receivedContextParamEngines), drm->receivedContextParamRequest.size);
        auto extensions = drm->receivedContextParamEngines.extensions;
        EXPECT_EQ(0ull, extensions);
        EXPECT_EQ(static_cast<__u16>(DrmPrelimHelper::getComputeEngineClass()), drm->receivedContextParamEngines.engines[0].engineClass);
        EXPECT_EQ(static_cast<__u16>(engineIndex), DrmMockHelper::getIdFromEngineOrMemoryInstance(drm->receivedContextParamEngines.engines[0].engineInstance));
    }
}

TEST(DrmTest, givenVirtualEnginesEnabledAndNotEnoughCcsEnginesWhenCreatingContextThenDontEnableLoadBalancing) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(NEO::defaultHwInfo.get());
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();

    auto drm = std::make_unique<DrmQueryMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    auto &ccsInfo = drm->rootDeviceEnvironment.getMutableHardwareInfo()->gtSystemInfo.CCSInfo;
    ccsInfo.IsValid = true;
    ccsInfo.NumberOfCCSEnabled = 1;

    drm->queryEngineInfo();
    EXPECT_NE(nullptr, drm->engineInfo);

    drm->receivedContextParamRequestCount = 0u;
    auto drmContextId = 42u;
    auto engineFlag = drm->bindDrmContext(drmContextId, 0u, aub_stream::ENGINE_CCS, false);

    EXPECT_EQ(static_cast<unsigned int>(I915_EXEC_DEFAULT), engineFlag);
    EXPECT_EQ(1u, drm->receivedContextParamRequestCount);
    EXPECT_EQ(drmContextId, drm->receivedContextParamRequest.contextId);
    EXPECT_EQ(static_cast<uint64_t>(I915_CONTEXT_PARAM_ENGINES), drm->receivedContextParamRequest.param);
    EXPECT_EQ(ptrDiff(drm->receivedContextParamEngines.engines + 1, &drm->receivedContextParamEngines), drm->receivedContextParamRequest.size);
    auto extensions = drm->receivedContextParamEngines.extensions;
    EXPECT_EQ(0ull, extensions);

    EXPECT_EQ(DrmPrelimHelper::getComputeEngineClass(), drm->receivedContextParamEngines.engines[0].engineClass);
    EXPECT_EQ(0, DrmMockHelper::getIdFromEngineOrMemoryInstance(drm->receivedContextParamEngines.engines[0].engineInstance));
}

TEST(DrmTest, givenVirtualEnginesEnabledAndNonCcsEnginesWhenCreatingContextThenDontEnableLoadBalancing) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(NEO::defaultHwInfo.get());
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();

    auto drm = std::make_unique<DrmQueryMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    drm->rootDeviceEnvironment.getMutableHardwareInfo()->featureTable.ftrBcsInfo = 1;

    drm->queryEngineInfo();
    EXPECT_NE(nullptr, drm->engineInfo);

    drm->receivedContextParamRequestCount = 0u;
    auto drmContextId = 42u;
    auto engineFlag = drm->bindDrmContext(drmContextId, 0u, aub_stream::ENGINE_BCS, false);

    EXPECT_EQ(static_cast<unsigned int>(I915_EXEC_DEFAULT), engineFlag);
    EXPECT_EQ(1u, drm->receivedContextParamRequestCount);
    EXPECT_EQ(drmContextId, drm->receivedContextParamRequest.contextId);
    EXPECT_EQ(static_cast<uint64_t>(I915_CONTEXT_PARAM_ENGINES), drm->receivedContextParamRequest.param);
    EXPECT_EQ(ptrDiff(drm->receivedContextParamEngines.engines + 1, &drm->receivedContextParamEngines), drm->receivedContextParamRequest.size);
    auto extensions = drm->receivedContextParamEngines.extensions;
    EXPECT_EQ(0ull, extensions);

    EXPECT_EQ(drm_i915_gem_engine_class::I915_ENGINE_CLASS_COPY, drm->receivedContextParamEngines.engines[0].engineClass);
}

TEST(DrmTest, givenInvalidTileWhenBindingDrmContextThenErrorIsReturned) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(NEO::defaultHwInfo.get());
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();

    auto drm = std::make_unique<DrmQueryMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    ASSERT_NE(nullptr, drm);

    drm->queryEngineInfo();
    auto hwInfo = drm->rootDeviceEnvironment.getHardwareInfo();
    auto haveLocalMemory = hwInfo->gtSystemInfo.MultiTileArchInfo.IsValid;
    EXPECT_EQ(haveLocalMemory ? 3u : 2u, drm->ioctlCallsCount);
    ASSERT_NE(nullptr, drm->engineInfo);

    auto engineFlag = drm->bindDrmContext(42u, 20u, aub_stream::ENGINE_CCS, false);
    EXPECT_EQ(static_cast<unsigned int>(I915_EXEC_DEFAULT), engineFlag);
    EXPECT_EQ(haveLocalMemory ? 3u : 2u, drm->ioctlCallsCount);
    EXPECT_EQ(0u, drm->receivedContextParamRequestCount);
}

TEST(DrmTest, givenInvalidEngineTypeWhenBindingDrmContextThenExceptionIsThrown) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(NEO::defaultHwInfo.get());
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();

    auto drm = std::make_unique<DrmQueryMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    ASSERT_NE(nullptr, drm);

    drm->queryEngineInfo();
    auto hwInfo = drm->rootDeviceEnvironment.getHardwareInfo();
    auto haveLocalMemory = hwInfo->gtSystemInfo.MultiTileArchInfo.IsValid;
    EXPECT_EQ(haveLocalMemory ? 3u : 2u, drm->ioctlCallsCount);
    ASSERT_NE(nullptr, drm->engineInfo);

    EXPECT_THROW(drm->bindDrmContext(42u, 0u, aub_stream::ENGINE_VCS, false), std::exception);
    EXPECT_EQ(haveLocalMemory ? 3u : 2u, drm->ioctlCallsCount);
    EXPECT_EQ(0u, drm->receivedContextParamRequestCount);
}

TEST(DrmTest, givenSetParamEnginesFailsWhenBindingDrmContextThenCallUnrecoverable) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(NEO::defaultHwInfo.get());
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();

    auto drm = std::make_unique<DrmQueryMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    ASSERT_NE(nullptr, drm);

    drm->queryEngineInfo();
    auto hwInfo = drm->rootDeviceEnvironment.getHardwareInfo();
    auto haveLocalMemory = hwInfo->gtSystemInfo.MultiTileArchInfo.IsValid;
    EXPECT_EQ(haveLocalMemory ? 3u : 2u, drm->ioctlCallsCount);
    ASSERT_NE(nullptr, drm->engineInfo);

    auto renderEngine = EngineHelpers::remapEngineTypeToHwSpecific(aub_stream::EngineType::ENGINE_RCS, *hwInfo);

    drm->storedRetValForSetParamEngines = -1;
    auto drmContextId = 42u;
    EXPECT_ANY_THROW(drm->bindDrmContext(drmContextId, 0u, renderEngine, false));
}

TEST(DrmTest, whenQueryingEngineInfoThenMultiTileArchInfoIsUnchanged) {
    auto originalMultiTileArchInfo = defaultHwInfo->gtSystemInfo.MultiTileArchInfo;
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(NEO::defaultHwInfo.get());
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();

    auto drm = std::make_unique<DrmQueryMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    ASSERT_NE(nullptr, drm);

    drm->queryEngineInfo();
    EXPECT_NE(nullptr, drm->engineInfo);

    auto hwInfo = drm->rootDeviceEnvironment.getHardwareInfo();
    EXPECT_EQ(originalMultiTileArchInfo.IsValid, hwInfo->gtSystemInfo.MultiTileArchInfo.IsValid);
    auto tileCount = hwInfo->gtSystemInfo.MultiTileArchInfo.TileCount;
    EXPECT_EQ(originalMultiTileArchInfo.TileCount, tileCount);
    auto tileMask = hwInfo->gtSystemInfo.MultiTileArchInfo.TileMask;
    EXPECT_EQ(originalMultiTileArchInfo.TileMask, tileMask);
}

TEST(DrmTest, givenNewMemoryInfoQuerySupportedWhenQueryingEngineInfoThenEngineInfoIsInitialized) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(NEO::defaultHwInfo.get());
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();

    auto drm = std::make_unique<DrmQueryMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    ASSERT_NE(nullptr, drm);

    drm->queryEngineInfo();
    EXPECT_NE(nullptr, drm->engineInfo);
}

struct DistanceQueryDrmTests : ::testing::Test {
    struct MyDrm : DrmQueryMock {
        using DrmQueryMock::DrmQueryMock;

        bool supportDistanceInfoQuery = true;
        bool handleQueryItem(void *arg) override {
            auto *queryItem = reinterpret_cast<QueryItem *>(arg);
            if (queryItem->queryId == DrmPrelimHelper::getDistanceInfoQueryId() && !supportDistanceInfoQuery) {
                queryItem->length = -EINVAL;
                return true; // successful query with incorrect length
            }
            return DrmQueryMock::handleQueryItem(queryItem);
        }
    };

    void SetUp() override {
        executionEnvironment = std::make_unique<ExecutionEnvironment>();
        executionEnvironment->prepareRootDeviceEnvironments(1);

        rootDeviceEnvironment = executionEnvironment->rootDeviceEnvironments[0].get();
        rootDeviceEnvironment->setHwInfo(NEO::defaultHwInfo.get());
    }

    std::unique_ptr<MyDrm> createDrm(bool supportDistanceInfoQuery) {
        auto drm = std::make_unique<MyDrm>(*rootDeviceEnvironment, rootDeviceEnvironment->getHardwareInfo());
        drm->supportDistanceInfoQuery = supportDistanceInfoQuery;
        return drm;
    }

    void setTileCount(size_t tileCount) {
        GT_MULTI_TILE_ARCH_INFO &multiTileArch = rootDeviceEnvironment->getMutableHardwareInfo()->gtSystemInfo.MultiTileArchInfo;
        multiTileArch.IsValid = (tileCount > 0);
        multiTileArch.TileCount = tileCount;
        multiTileArch.TileMask = static_cast<uint8_t>(maxNBitValue(tileCount));
    }

    void verifyEngineInfo(const Drm &drm) {
        GT_MULTI_TILE_ARCH_INFO &multiTileArch = rootDeviceEnvironment->getMutableHardwareInfo()->gtSystemInfo.MultiTileArchInfo;
        const uint32_t tileCount = multiTileArch.IsValid ? multiTileArch.TileCount : 1u;

        std::vector<size_t> engineCounts(tileCount);
        auto engineInfo = drm.getEngineInfo();
        for (uint32_t tile = 0; tile < tileCount; tile++) {
            std::vector<EngineClassInstance> engines = {};
            engineInfo->getListOfEnginesOnATile(tile, engines);
            engineCounts[tile] = engines.size();
            EXPECT_NE(0u, engines.size());
        }

        const bool areAllTilesEqual = std::equal(engineCounts.begin(), engineCounts.end(), engineCounts.begin());
        EXPECT_TRUE(areAllTilesEqual);
    }

    std::unique_ptr<ExecutionEnvironment> executionEnvironment;
    RootDeviceEnvironment *rootDeviceEnvironment;
};

HWTEST_F(DistanceQueryDrmTests, givenDistanceQueryNotSupportedWhenQueryingEngineInfoThen) {
    setTileCount(0);
    auto drm = createDrm(false);
    EXPECT_TRUE(drm->queryEngineInfo());
    verifyEngineInfo(*drm);

    setTileCount(1);
    drm = createDrm(false);
    EXPECT_TRUE(drm->queryEngineInfo());
    verifyEngineInfo(*drm);
}

HWTEST_F(DistanceQueryDrmTests, givenDistanceQuerySupportedAndZeroTilesWhenQueryingEngineInfoThen) {
    setTileCount(0);
    auto drm = createDrm(true);
    EXPECT_TRUE(drm->queryEngineInfo());
    verifyEngineInfo(*drm);
}

HWTEST_F(DistanceQueryDrmTests, givenDistanceQuerySupportedAndTilesDetectedWhenQueryingEngineInfoThen) {
    setTileCount(1);
    auto drm = createDrm(true);
    EXPECT_TRUE(drm->queryEngineInfo());
    verifyEngineInfo(*drm);

    setTileCount(4);
    drm = createDrm(true);
    EXPECT_TRUE(drm->queryEngineInfo());
    verifyEngineInfo(*drm);
}
