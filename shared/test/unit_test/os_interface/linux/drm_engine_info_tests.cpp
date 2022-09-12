/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/engine_info.h"
#include "shared/source/os_interface/linux/i915.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/mocks/mock_execution_environment.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(DrmTest, whenQueryingEngineInfoThenSingleIoctlIsCalled) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    std::unique_ptr<DrmMock> drm = std::make_unique<DrmMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    EXPECT_NE(nullptr, drm);

    drm->queryEngineInfo();
    EXPECT_EQ(1u + drm->getBaseIoctlCalls(), drm->ioctlCallsCount);
}

TEST(EngineInfoTest, givenEngineInfoQuerySupportedWhenQueryingEngineInfoThenEngineInfoIsCreatedWithEngines) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmMockEngine>(*executionEnvironment->rootDeviceEnvironments[0]);
    ASSERT_NE(nullptr, drm);

    std::vector<MemoryRegion> memRegions{
        {{0, 0}, 0, 0}};
    drm->memoryInfo.reset(new MemoryInfo(memRegions, *drm));
    drm->queryEngineInfo();
    EXPECT_EQ(2u + drm->getBaseIoctlCalls(), drm->ioctlCallsCount);
    auto engineInfo = drm->getEngineInfo();

    ASSERT_NE(nullptr, engineInfo);
    EXPECT_EQ(2u, engineInfo->engines.size());
}

TEST(EngineInfoTest, whenQueryingEngineInfoWithoutMemoryInfoThenEngineInfoCreated) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmMockEngine>(*executionEnvironment->rootDeviceEnvironments[0]);
    ASSERT_NE(nullptr, drm);

    drm->queryEngineInfo();
    EXPECT_EQ(2u + drm->getBaseIoctlCalls(), drm->ioctlCallsCount);
    auto engineInfo = drm->getEngineInfo();

    ASSERT_NE(nullptr, engineInfo);
}

TEST(EngineInfoTest, whenCreateEngineInfoWithRcsThenCorrectHwInfoSet) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmMockEngine>(*executionEnvironment->rootDeviceEnvironments[0]);
    auto ioctlHelper = drm->getIoctlHelper();

    auto hwInfo = *defaultHwInfo.get();
    std::vector<EngineCapabilities> engines(2);
    engines[0].engine = {static_cast<uint16_t>(ioctlHelper->getDrmParamValue(DrmParam::EngineClassRender)), 0};
    engines[0].capabilities = 0;
    engines[1].engine = {static_cast<uint16_t>(ioctlHelper->getDrmParamValue(DrmParam::EngineClassCopy)), 0};
    engines[1].capabilities = 0;
    auto engineInfo = std::make_unique<EngineInfo>(drm.get(), &hwInfo, engines);

    auto ccsInfo = hwInfo.gtSystemInfo.CCSInfo;
    EXPECT_FALSE(ccsInfo.IsValid);
    EXPECT_EQ(0u, ccsInfo.NumberOfCCSEnabled);
    EXPECT_EQ(0u, ccsInfo.Instances.CCSEnableMask);
    EXPECT_EQ(1u, hwInfo.featureTable.ftrBcsInfo.to_ulong());
}

TEST(EngineInfoTest, whenCreateEngineInfoWithCcsThenCorrectHwInfoSet) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmMockEngine>(*executionEnvironment->rootDeviceEnvironments[0]);
    auto ioctlHelper = drm->getIoctlHelper();

    auto hwInfo = *defaultHwInfo.get();
    std::vector<EngineCapabilities> engines(2);
    uint16_t ccsClass = ioctlHelper->getDrmParamValue(DrmParam::EngineClassCompute);
    engines[0].engine = {ccsClass, 0};
    engines[0].capabilities = 0;
    engines[1].engine = {static_cast<uint16_t>(ioctlHelper->getDrmParamValue(DrmParam::EngineClassCopy)), 0};
    engines[1].capabilities = 0;
    auto engineInfo = std::make_unique<EngineInfo>(drm.get(), &hwInfo, engines);

    auto ccsInfo = hwInfo.gtSystemInfo.CCSInfo;
    EXPECT_TRUE(ccsInfo.IsValid);
    EXPECT_EQ(1u, ccsInfo.NumberOfCCSEnabled);
    EXPECT_EQ(1u, ccsInfo.Instances.CCSEnableMask);
    EXPECT_EQ(1u, hwInfo.featureTable.ftrBcsInfo.to_ulong());
}

TEST(EngineInfoTest, whenGetEngineInstanceAndTileThenCorrectValuesReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmMockEngine>(*executionEnvironment->rootDeviceEnvironments[0]);
    auto ioctlHelper = drm->getIoctlHelper();

    auto hwInfo = *defaultHwInfo.get();
    std::vector<EngineCapabilities> engines(4);
    engines[0].engine = {static_cast<uint16_t>(ioctlHelper->getDrmParamValue(DrmParam::EngineClassRender)), 0};
    engines[0].capabilities = 0;
    engines[1].engine = {static_cast<uint16_t>(ioctlHelper->getDrmParamValue(DrmParam::EngineClassCopy)), 0};
    engines[1].capabilities = 0;
    engines[2].engine = {static_cast<uint16_t>(ioctlHelper->getDrmParamValue(DrmParam::EngineClassRender)), 1};
    engines[2].capabilities = 0;
    engines[3].engine = {static_cast<uint16_t>(ioctlHelper->getDrmParamValue(DrmParam::EngineClassCopy)), 1};
    engines[3].capabilities = 0;

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
    auto engineInfo = std::make_unique<EngineInfo>(drm.get(), &hwInfo, 2, distances, queryItems, engines);

    auto engineType = EngineHelpers::remapEngineTypeToHwSpecific(aub_stream::EngineType::ENGINE_RCS, hwInfo);
    auto engine = engineInfo->getEngineInstance(0, engineType);
    EXPECT_EQ(engines[0].engine.engineClass, engine->engineClass);
    EXPECT_EQ(engines[0].engine.engineInstance, engine->engineInstance);

    engine = engineInfo->getEngineInstance(1, aub_stream::EngineType::ENGINE_BCS);
    EXPECT_EQ(engines[3].engine.engineClass, engine->engineClass);
    EXPECT_EQ(engines[3].engine.engineInstance, engine->engineInstance);

    EXPECT_EQ(nullptr, engineInfo->getEngineInstance(3, aub_stream::EngineType::ENGINE_RCS));
    EXPECT_EQ(nullptr, engineInfo->getEngineInstance(0, aub_stream::EngineType::ENGINE_VCS));

    EXPECT_EQ(0u, engineInfo->getEngineTileIndex(engines[0].engine));
    EXPECT_EQ(1u, engineInfo->getEngineTileIndex(engines[2].engine));

    EXPECT_EQ(0u, engineInfo->getEngineTileIndex({static_cast<uint16_t>(ioctlHelper->getDrmParamValue(DrmParam::EngineClassRender)), 2}));
}

TEST(EngineInfoTest, whenCreateEngineInfoAndInvalidQueryThenNoEnginesSet) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmMockEngine>(*executionEnvironment->rootDeviceEnvironments[0]);
    auto ioctlHelper = drm->getIoctlHelper();

    auto hwInfo = *defaultHwInfo.get();
    std::vector<EngineCapabilities> engines(4);
    engines[0].engine = {static_cast<uint16_t>(ioctlHelper->getDrmParamValue(DrmParam::EngineClassRender)), 0};
    engines[0].capabilities = 0;
    engines[1].engine = {static_cast<uint16_t>(ioctlHelper->getDrmParamValue(DrmParam::EngineClassCopy)), 0};
    engines[1].capabilities = 0;
    engines[2].engine = {static_cast<uint16_t>(ioctlHelper->getDrmParamValue(DrmParam::EngineClassRender)), 1};
    engines[2].capabilities = 0;
    engines[3].engine = {static_cast<uint16_t>(ioctlHelper->getDrmParamValue(DrmParam::EngineClassCopy)), 1};
    engines[3].capabilities = 0;

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
    auto engineInfo = std::make_unique<EngineInfo>(drm.get(), &hwInfo, 2, distances, queryItems, engines);
    EXPECT_EQ(nullptr, engineInfo->getEngineInstance(0, aub_stream::EngineType::ENGINE_RCS));
}

TEST(EngineInfoTest, whenEmptyEngineInfoCreatedThen0TileReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmMockEngine>(*executionEnvironment->rootDeviceEnvironments[0]);
    auto ioctlHelper = drm->getIoctlHelper();

    auto hwInfo = *defaultHwInfo.get();
    std::vector<DistanceInfo> distances;
    std::vector<EngineCapabilities> engines;
    std::vector<QueryItem> queryItems;

    auto engineInfo = std::make_unique<EngineInfo>(drm.get(), &hwInfo, 0, distances, queryItems, engines);
    EXPECT_EQ(0u, engineInfo->getEngineTileIndex({static_cast<uint16_t>(ioctlHelper->getDrmParamValue(DrmParam::EngineClassRender)), 1}));
}
