/*
 * Copyright (C) 2019-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/engine_info.h"
#include "shared/source/os_interface/linux/i915.h"
#include "shared/source/os_interface/linux/memory_info.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/helpers/mock_product_helper_hw.h"
#include "shared/test/common/helpers/raii_product_helper.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(DrmTest, whenQueryingEngineInfoThenSingleIoctlIsCalled) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    std::unique_ptr<DrmMock> drm = std::make_unique<DrmMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    EXPECT_NE(nullptr, drm);
    auto ioctlCount = drm->ioctlCount.total.load();
    drm->memoryInfoQueried = true;
    drm->queryEngineInfo();
    EXPECT_EQ(1 + ioctlCount, drm->ioctlCount.total.load());
}

TEST(EngineInfoTest, givenEngineInfoQuerySupportedWhenQueryingEngineInfoThenEngineInfoIsCreatedWithEngines) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmMockEngine>(*executionEnvironment->rootDeviceEnvironments[0]);
    ASSERT_NE(nullptr, drm);

    std::vector<MemoryRegion> memRegions{
        {{0, 0}, 0, 0}};
    drm->memoryInfo.reset(new MemoryInfo(memRegions, *drm));
    drm->memoryInfoQueried = true;
    drm->queryEngineInfo();
    EXPECT_EQ(2u + drm->getBaseIoctlCalls(), drm->ioctlCallsCount);
    auto engineInfo = drm->getEngineInfo();

    ASSERT_NE(nullptr, engineInfo);
    EXPECT_EQ(2u, engineInfo->getEngineInfos().size());
}

TEST(EngineInfoTest, whenQueryingEngineInfoWithoutMemoryInfoThenEngineInfoCreated) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmMockEngine>(*executionEnvironment->rootDeviceEnvironments[0]);
    ASSERT_NE(nullptr, drm);

    drm->memoryInfoQueried = true;
    drm->queryEngineInfo();
    EXPECT_EQ(2u + drm->getBaseIoctlCalls(), drm->ioctlCallsCount);
    auto engineInfo = drm->getEngineInfo();

    ASSERT_NE(nullptr, engineInfo);
}

TEST(EngineInfoTest, whenCreateEngineInfoWithRcsThenCorrectHwInfoSet) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmMockEngine>(*executionEnvironment->rootDeviceEnvironments[0]);
    auto ioctlHelper = drm->getIoctlHelper();

    auto &hwInfo = *drm->getRootDeviceEnvironment().getHardwareInfo();
    std::vector<EngineCapabilities> engines(2);
    engines[0].engine = {static_cast<uint16_t>(ioctlHelper->getDrmParamValue(DrmParam::engineClassRender)), 0};
    engines[0].capabilities = {};
    engines[1].engine = {static_cast<uint16_t>(ioctlHelper->getDrmParamValue(DrmParam::engineClassCopy)), 0};
    engines[1].capabilities = {};
    StackVec<std::vector<EngineCapabilities>, 2> engineInfosPerTile{engines};
    auto engineInfo = std::make_unique<EngineInfo>(drm.get(), engineInfosPerTile);

    auto ccsInfo = hwInfo.gtSystemInfo.CCSInfo;
    EXPECT_FALSE(ccsInfo.IsValid);
    EXPECT_EQ_VAL(0u, ccsInfo.NumberOfCCSEnabled);
    EXPECT_EQ_VAL(0u, ccsInfo.Instances.CCSEnableMask);

    auto &productHelper = executionEnvironment->rootDeviceEnvironments[0]->getProductHelper();
    auto defaultCopyEngine = productHelper.getDefaultCopyEngine();
    if (defaultCopyEngine == aub_stream::EngineType::ENGINE_BCS) {
        EXPECT_EQ(1u, hwInfo.featureTable.ftrBcsInfo.to_ulong());
    } else {
        EXPECT_TRUE(hwInfo.featureTable.ftrBcsInfo.test(static_cast<uint32_t>(defaultCopyEngine) - static_cast<uint32_t>(aub_stream::EngineType::ENGINE_BCS1) + 1));
    }
}

TEST(EngineInfoTest, whenCallingGetEngineTileInfoCorrectValuesAreReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmMockEngine>(*executionEnvironment->rootDeviceEnvironments[0]);
    auto ioctlHelper = drm->getIoctlHelper();

    std::vector<EngineCapabilities> engines(1);
    engines[0].engine = {static_cast<uint16_t>(ioctlHelper->getDrmParamValue(DrmParam::engineClassRender)), 0};
    engines[0].capabilities = {};
    StackVec<std::vector<EngineCapabilities>, 2> engineInfosPerTile{engines};
    auto engineInfo = std::make_unique<EngineInfo>(drm.get(), engineInfosPerTile);

    auto engineTileMap = engineInfo->getEngineTileInfo();
    auto it = engineTileMap.begin();
    EXPECT_EQ(it->second.engineClass, engines[0].engine.engineClass);
    EXPECT_EQ(it->second.engineInstance, engines[0].engine.engineInstance);
}

TEST(EngineInfoTest, whenCreateEngineInfoWithCcsThenCorrectHwInfoSet) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmMockEngine>(*executionEnvironment->rootDeviceEnvironments[0]);
    auto ioctlHelper = drm->getIoctlHelper();

    auto &hwInfo = *drm->getRootDeviceEnvironment().getHardwareInfo();
    std::vector<EngineCapabilities> engines(2);
    uint16_t ccsClass = ioctlHelper->getDrmParamValue(DrmParam::engineClassCompute);
    engines[0].engine = {ccsClass, 0};
    engines[0].capabilities = {};
    engines[1].engine = {static_cast<uint16_t>(ioctlHelper->getDrmParamValue(DrmParam::engineClassCopy)), 0};
    engines[1].capabilities = {};
    StackVec<std::vector<EngineCapabilities>, 2> engineInfosPerTile{engines};
    auto engineInfo = std::make_unique<EngineInfo>(drm.get(), engineInfosPerTile);

    auto ccsInfo = hwInfo.gtSystemInfo.CCSInfo;
    EXPECT_TRUE(ccsInfo.IsValid);
    EXPECT_EQ_VAL(1u, ccsInfo.NumberOfCCSEnabled);
    EXPECT_EQ_VAL(1u, ccsInfo.Instances.CCSEnableMask);

    auto &productHelper = executionEnvironment->rootDeviceEnvironments[0]->getProductHelper();
    auto defaultCopyEngine = productHelper.getDefaultCopyEngine();
    if (defaultCopyEngine == aub_stream::EngineType::ENGINE_BCS) {
        EXPECT_EQ(1u, hwInfo.featureTable.ftrBcsInfo.to_ulong());
    } else {
        EXPECT_TRUE(hwInfo.featureTable.ftrBcsInfo.test(static_cast<uint32_t>(defaultCopyEngine) - static_cast<uint32_t>(aub_stream::EngineType::ENGINE_BCS1) + 1));
    }
}

TEST(EngineInfoTest, whenGetEngineInstanceAndTileThenCorrectValuesReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmMockEngine>(*executionEnvironment->rootDeviceEnvironments[0]);
    auto ioctlHelper = drm->getIoctlHelper();

    const auto &productHelper = executionEnvironment->rootDeviceEnvironments[0]->getProductHelper();
    const auto defaultCopyEngine = productHelper.getDefaultCopyEngine();

    std::vector<EngineCapabilities> engines(4);
    engines[0].engine = {static_cast<uint16_t>(ioctlHelper->getDrmParamValue(DrmParam::engineClassRender)), 0};
    engines[0].capabilities = {};
    engines[1].engine = {static_cast<uint16_t>(ioctlHelper->getDrmParamValue(DrmParam::engineClassCopy)), 0};
    engines[1].capabilities = {};
    engines[2].engine = {static_cast<uint16_t>(ioctlHelper->getDrmParamValue(DrmParam::engineClassRender)), 1};
    engines[2].capabilities = {};
    engines[3].engine = {static_cast<uint16_t>(ioctlHelper->getDrmParamValue(DrmParam::engineClassCopy)), 1};
    engines[3].capabilities = {};

    std::vector<DistanceInfo> distances(4);
    distances[0].engine = engines[0].engine;
    distances[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 0};
    distances[1].engine = engines[1].engine;
    distances[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 0};
    distances[2].engine = engines[2].engine;
    distances[2].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 1};
    distances[3].engine = engines[3].engine;
    distances[3].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 1};

    std::vector<QueryItem> queryItems{distances.size()};
    for (auto i = 0u; i < distances.size(); i++) {
        queryItems[i].length = sizeof(drm_i915_query_engine_info);
    }
    auto engineInfo = std::make_unique<EngineInfo>(drm.get(), 2, distances, queryItems, engines);

    auto engineType = EngineHelpers::remapEngineTypeToHwSpecific(aub_stream::EngineType::ENGINE_RCS, drm->getRootDeviceEnvironment());
    auto engine = engineInfo->getEngineInstance(0, engineType);
    EXPECT_EQ(engines[0].engine.engineClass, engine->engineClass);
    EXPECT_EQ(engines[0].engine.engineInstance, engine->engineInstance);

    engine = engineInfo->getEngineInstance(1, defaultCopyEngine);
    EXPECT_EQ(engines[3].engine.engineClass, engine->engineClass);
    EXPECT_EQ(engines[3].engine.engineInstance, engine->engineInstance);

    EXPECT_EQ(nullptr, engineInfo->getEngineInstance(3, aub_stream::EngineType::ENGINE_RCS));
    EXPECT_EQ(nullptr, engineInfo->getEngineInstance(0, aub_stream::EngineType::ENGINE_VCS));

    EXPECT_EQ(0u, engineInfo->getEngineTileIndex(engines[0].engine));
    EXPECT_EQ(1u, engineInfo->getEngineTileIndex(engines[2].engine));

    EXPECT_EQ(0u, engineInfo->getEngineTileIndex({static_cast<uint16_t>(ioctlHelper->getDrmParamValue(DrmParam::engineClassRender)), 2}));
}

TEST(EngineInfoTest, whenCreateEngineInfoAndInvalidQueryThenNoEnginesSet) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmMockEngine>(*executionEnvironment->rootDeviceEnvironments[0]);
    auto ioctlHelper = drm->getIoctlHelper();

    std::vector<EngineCapabilities> engines(4);
    engines[0].engine = {static_cast<uint16_t>(ioctlHelper->getDrmParamValue(DrmParam::engineClassRender)), 0};
    engines[0].capabilities = {};
    engines[1].engine = {static_cast<uint16_t>(ioctlHelper->getDrmParamValue(DrmParam::engineClassCopy)), 0};
    engines[1].capabilities = {};
    engines[2].engine = {static_cast<uint16_t>(ioctlHelper->getDrmParamValue(DrmParam::engineClassRender)), 1};
    engines[2].capabilities = {};
    engines[3].engine = {static_cast<uint16_t>(ioctlHelper->getDrmParamValue(DrmParam::engineClassCopy)), 1};
    engines[3].capabilities = {};

    std::vector<DistanceInfo> distances(4);
    distances[0].engine = engines[0].engine;
    distances[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 0};
    distances[1].engine = engines[1].engine;
    distances[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 0};
    distances[2].engine = engines[2].engine;
    distances[2].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 1};
    distances[3].engine = engines[3].engine;
    distances[3].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 1};

    std::vector<QueryItem> queryItems{distances.size()};
    for (auto i = 0u; i < distances.size(); i++) {
        queryItems[i].length = -1;
    }
    auto engineInfo = std::make_unique<EngineInfo>(drm.get(), 2, distances, queryItems, engines);
    EXPECT_EQ(nullptr, engineInfo->getEngineInstance(0, aub_stream::EngineType::ENGINE_RCS));
}

TEST(EngineInfoTest, whenEmptyEngineInfoCreatedThen0TileReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmMockEngine>(*executionEnvironment->rootDeviceEnvironments[0]);
    auto ioctlHelper = drm->getIoctlHelper();

    std::vector<DistanceInfo> distances;
    std::vector<EngineCapabilities> engines;
    std::vector<QueryItem> queryItems;

    auto engineInfo = std::make_unique<EngineInfo>(drm.get(), 0, distances, queryItems, engines);
    EXPECT_EQ(0u, engineInfo->getEngineTileIndex({static_cast<uint16_t>(ioctlHelper->getDrmParamValue(DrmParam::engineClassRender)), 1}));
}

using DisabledBCSEngineInfoTest = ::testing::Test;

HWTEST2_F(DisabledBCSEngineInfoTest, whenBCS0IsNotEnabledThenSkipMappingItAndSetProperBcsInfoMask, MatchAny) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto &rootDeviceEnvironment = *executionEnvironment->rootDeviceEnvironments[0];
    auto drm = std::make_unique<DrmMockEngine>(rootDeviceEnvironment);
    auto ioctlHelper = drm->getIoctlHelper();

    RAIIProductHelperFactory<MockProductHelperHw<productFamily>> raii(rootDeviceEnvironment);
    raii.mockProductHelper->mockDefaultCopyEngine = aub_stream::EngineType::ENGINE_BCS1;
    auto emplaceCopyEngine = [&ioctlHelper](std::vector<EngineCapabilities> &vec) {
        auto &emplaced = vec.emplace_back();
        emplaced.engine.engineClass = static_cast<uint16_t>(ioctlHelper->getDrmParamValue(DrmParam::engineClassCopy));
        emplaced.engine.engineInstance = 0u;
    };

    StackVec<std::vector<EngineCapabilities>, 2> enginesPerTile{};
    enginesPerTile.resize(2u);
    for (int tileIdx = 0; tileIdx < 2; tileIdx++) {
        emplaceCopyEngine(enginesPerTile[tileIdx]);
        emplaceCopyEngine(enginesPerTile[tileIdx]);
    }

    auto engineInfo = std::make_unique<EngineInfo>(drm.get(), enginesPerTile);
    EXPECT_EQ(nullptr, engineInfo->getEngineInstance(0u, aub_stream::EngineType::ENGINE_BCS));
    EXPECT_NE(nullptr, engineInfo->getEngineInstance(0u, aub_stream::EngineType::ENGINE_BCS1));
    EXPECT_NE(nullptr, engineInfo->getEngineInstance(0u, aub_stream::EngineType::ENGINE_BCS2));

    EXPECT_EQ(nullptr, engineInfo->getEngineInstance(1u, aub_stream::EngineType::ENGINE_BCS));
    EXPECT_NE(nullptr, engineInfo->getEngineInstance(1u, aub_stream::EngineType::ENGINE_BCS1));
    EXPECT_NE(nullptr, engineInfo->getEngineInstance(1u, aub_stream::EngineType::ENGINE_BCS2));

    const auto hwInfo = rootDeviceEnvironment.getHardwareInfo();
    EXPECT_EQ(false, hwInfo->featureTable.ftrBcsInfo.test(0));
}
