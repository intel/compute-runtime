/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/device.h"
#include "shared/source/helpers/engine_node_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

TEST(EngineNodeHelperTest, givenValidEngineUsageWhenGettingStringRepresentationThenItIsCorrect) {
    EXPECT_EQ(std::string{"Regular"}, EngineHelpers::engineUsageToString(EngineUsage::regular));
    EXPECT_EQ(std::string{"Internal"}, EngineHelpers::engineUsageToString(EngineUsage::internal));
    EXPECT_EQ(std::string{"LowPriority"}, EngineHelpers::engineUsageToString(EngineUsage::lowPriority));
    EXPECT_EQ(std::string{"HighPriority"}, EngineHelpers::engineUsageToString(EngineUsage::highPriority));
    EXPECT_EQ(std::string{"Cooperative"}, EngineHelpers::engineUsageToString(EngineUsage::cooperative));
}

TEST(EngineNodeHelperTest, givenInValidEngineUsageWhenGettingStringRepresentationThenReturnUnknown) {
    EXPECT_EQ(std::string{"Unknown"}, EngineHelpers::engineUsageToString(EngineUsage::engineUsageCount));
}

TEST(EngineNodeHelperTest, givenValidEngineTypeWhenGettingStringRepresentationThenItIsCorrect) {
#define CHECK_ENGINE(type) EXPECT_EQ(std::string{#type}, EngineHelpers::engineTypeToString(aub_stream::EngineType::ENGINE_##type))
    CHECK_ENGINE(RCS);
    CHECK_ENGINE(BCS);
    CHECK_ENGINE(VCS);
    CHECK_ENGINE(VECS);
    CHECK_ENGINE(CCS);
    CHECK_ENGINE(CCS1);
    CHECK_ENGINE(CCS2);
    CHECK_ENGINE(CCS3);
    CHECK_ENGINE(CCCS);
    CHECK_ENGINE(BCS1);
    CHECK_ENGINE(BCS2);
    CHECK_ENGINE(BCS3);
    CHECK_ENGINE(BCS4);
    CHECK_ENGINE(BCS5);
    CHECK_ENGINE(BCS6);
    CHECK_ENGINE(BCS7);
    CHECK_ENGINE(BCS8);
#undef CHECK_ENGINE
}

TEST(EngineNodeHelperTest, givenBcsWhenGettingBcsIndexThenReturnCorrectIndex) {
    EXPECT_EQ(0u, EngineHelpers::getBcsIndex(aub_stream::ENGINE_BCS));
    EXPECT_EQ(1u, EngineHelpers::getBcsIndex(aub_stream::ENGINE_BCS1));
    EXPECT_EQ(2u, EngineHelpers::getBcsIndex(aub_stream::ENGINE_BCS2));
    EXPECT_EQ(3u, EngineHelpers::getBcsIndex(aub_stream::ENGINE_BCS3));
    EXPECT_EQ(4u, EngineHelpers::getBcsIndex(aub_stream::ENGINE_BCS4));
    EXPECT_EQ(5u, EngineHelpers::getBcsIndex(aub_stream::ENGINE_BCS5));
    EXPECT_EQ(6u, EngineHelpers::getBcsIndex(aub_stream::ENGINE_BCS6));
    EXPECT_EQ(7u, EngineHelpers::getBcsIndex(aub_stream::ENGINE_BCS7));
    EXPECT_EQ(8u, EngineHelpers::getBcsIndex(aub_stream::ENGINE_BCS8));
}

TEST(EngineNodeHelperTest, givenCcsEngineWhenHelperIsUsedThenReturnTrue) {
    EXPECT_TRUE(EngineHelpers::isCcs(aub_stream::EngineType::ENGINE_CCS));
    EXPECT_TRUE(EngineHelpers::isCcs(aub_stream::EngineType::ENGINE_CCS1));
    EXPECT_TRUE(EngineHelpers::isCcs(aub_stream::EngineType::ENGINE_CCS2));
    EXPECT_TRUE(EngineHelpers::isCcs(aub_stream::EngineType::ENGINE_CCS3));
    EXPECT_FALSE(EngineHelpers::isCcs(aub_stream::EngineType::ENGINE_RCS));
    EXPECT_FALSE(EngineHelpers::isCcs(aub_stream::EngineType::NUM_ENGINES));
}

TEST(EngineNodeHelperTest, givenCcsWhenGettingCcsIndexThenReturnCorrectIndex) {
    EXPECT_EQ(0u, EngineHelpers::getCcsIndex(aub_stream::ENGINE_CCS));
    EXPECT_EQ(1u, EngineHelpers::getCcsIndex(aub_stream::ENGINE_CCS1));
    EXPECT_EQ(2u, EngineHelpers::getCcsIndex(aub_stream::ENGINE_CCS2));
    EXPECT_EQ(3u, EngineHelpers::getCcsIndex(aub_stream::ENGINE_CCS3));
}

TEST(EngineNodeHelperTest, givenInvalidEngineTypeWhenGettingStringRepresentationThenItIsCorrect) {
    EXPECT_EQ(std::string{"Unknown"}, EngineHelpers::engineTypeToString(aub_stream::EngineType::NUM_ENGINES));
}

TEST(EngineNodeHelperTest, givenLinkCopyEnginesSupportedWhenGettingBcsEngineTypeThenFirstReturnMainCopyEngineAndThenRoundRobinBetweenLinkEngines) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableCopyEngineSelector.set(1);
    SelectorCopyEngine selectorCopyEngine{};
    MockExecutionEnvironment executionEnvironment{};
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];
    auto &hwInfo = *rootDeviceEnvironment.getMutableHardwareInfo();
    DeviceBitfield deviceBitfield = 0b11;

    const auto mainBcsEngine = rootDeviceEnvironment.getProductHelper().getDefaultCopyEngine();
    const auto isBCS0Main = (aub_stream::ENGINE_BCS == mainBcsEngine);
    hwInfo.featureTable.ftrBcsInfo = isBCS0Main ? 0b00111 : 0b10111;
    const auto nextLinkCopyEngine = isBCS0Main ? aub_stream::EngineType::ENGINE_BCS1 : aub_stream::EngineType::ENGINE_BCS4;

    EXPECT_EQ(mainBcsEngine, EngineHelpers::getBcsEngineType(rootDeviceEnvironment, deviceBitfield, selectorCopyEngine, false));
    EXPECT_EQ(aub_stream::EngineType::ENGINE_BCS2, EngineHelpers::getBcsEngineType(rootDeviceEnvironment, deviceBitfield, selectorCopyEngine, false));
    EXPECT_EQ(nextLinkCopyEngine, EngineHelpers::getBcsEngineType(rootDeviceEnvironment, deviceBitfield, selectorCopyEngine, false));
    EXPECT_EQ(aub_stream::EngineType::ENGINE_BCS2, EngineHelpers::getBcsEngineType(rootDeviceEnvironment, deviceBitfield, selectorCopyEngine, false));
    EXPECT_EQ(nextLinkCopyEngine, EngineHelpers::getBcsEngineType(rootDeviceEnvironment, deviceBitfield, selectorCopyEngine, false));
    EXPECT_EQ(aub_stream::EngineType::ENGINE_BCS2, EngineHelpers::getBcsEngineType(rootDeviceEnvironment, deviceBitfield, selectorCopyEngine, false));
    EXPECT_EQ(nextLinkCopyEngine, EngineHelpers::getBcsEngineType(rootDeviceEnvironment, deviceBitfield, selectorCopyEngine, false));
}

TEST(EngineNodeHelperTest, givenMainBcsEngineIsReleasedWhenGettingBcsEngineTypeThenItCanBeReturnedAgain) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableCopyEngineSelector.set(1);
    SelectorCopyEngine selectorCopyEngine{};
    MockExecutionEnvironment executionEnvironment{};
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];
    auto &hwInfo = *rootDeviceEnvironment.getMutableHardwareInfo();
    DeviceBitfield deviceBitfield = 0b11;

    const auto mainBcsEngine = rootDeviceEnvironment.getProductHelper().getDefaultCopyEngine();
    const auto isBCS0Main = (aub_stream::ENGINE_BCS == mainBcsEngine);
    hwInfo.featureTable.ftrBcsInfo = isBCS0Main ? 0b00111 : 0b10111;
    const auto nextLinkCopyEngine = isBCS0Main ? aub_stream::EngineType::ENGINE_BCS1 : aub_stream::EngineType::ENGINE_BCS4;

    EXPECT_EQ(mainBcsEngine, EngineHelpers::getBcsEngineType(rootDeviceEnvironment, deviceBitfield, selectorCopyEngine, false));
    EXPECT_EQ(aub_stream::EngineType::ENGINE_BCS2, EngineHelpers::getBcsEngineType(rootDeviceEnvironment, deviceBitfield, selectorCopyEngine, false));
    EXPECT_EQ(nextLinkCopyEngine, EngineHelpers::getBcsEngineType(rootDeviceEnvironment, deviceBitfield, selectorCopyEngine, false));
    EXPECT_EQ(aub_stream::EngineType::ENGINE_BCS2, EngineHelpers::getBcsEngineType(rootDeviceEnvironment, deviceBitfield, selectorCopyEngine, false));

    EngineHelpers::releaseBcsEngineType(mainBcsEngine, selectorCopyEngine, rootDeviceEnvironment);
    EXPECT_EQ(mainBcsEngine, EngineHelpers::getBcsEngineType(rootDeviceEnvironment, deviceBitfield, selectorCopyEngine, false));
    EXPECT_EQ(nextLinkCopyEngine, EngineHelpers::getBcsEngineType(rootDeviceEnvironment, deviceBitfield, selectorCopyEngine, false));
    EXPECT_EQ(aub_stream::EngineType::ENGINE_BCS2, EngineHelpers::getBcsEngineType(rootDeviceEnvironment, deviceBitfield, selectorCopyEngine, false));
}

TEST(EngineNodeHelperTest, givenLinkBcsEngineIsReleasedWhenGettingBcsEngineTypeThenItDoesNotAffectFurtherSelections) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableCopyEngineSelector.set(1);
    SelectorCopyEngine selectorCopyEngine{};
    MockExecutionEnvironment executionEnvironment{};
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];
    auto &hwInfo = *rootDeviceEnvironment.getMutableHardwareInfo();
    DeviceBitfield deviceBitfield = 0b11;

    const auto mainBcsEngine = rootDeviceEnvironment.getProductHelper().getDefaultCopyEngine();
    const auto isBCS0Main = (aub_stream::ENGINE_BCS == mainBcsEngine);
    hwInfo.featureTable.ftrBcsInfo = isBCS0Main ? 0b00111 : 0b10111;
    const auto nextLinkCopyEngine = isBCS0Main ? aub_stream::EngineType::ENGINE_BCS1 : aub_stream::EngineType::ENGINE_BCS4;

    EXPECT_EQ(mainBcsEngine, EngineHelpers::getBcsEngineType(rootDeviceEnvironment, deviceBitfield, selectorCopyEngine, false));
    EXPECT_EQ(aub_stream::EngineType::ENGINE_BCS2, EngineHelpers::getBcsEngineType(rootDeviceEnvironment, deviceBitfield, selectorCopyEngine, false));
    EXPECT_EQ(nextLinkCopyEngine, EngineHelpers::getBcsEngineType(rootDeviceEnvironment, deviceBitfield, selectorCopyEngine, false));
    EXPECT_EQ(aub_stream::EngineType::ENGINE_BCS2, EngineHelpers::getBcsEngineType(rootDeviceEnvironment, deviceBitfield, selectorCopyEngine, false));

    EngineHelpers::releaseBcsEngineType(nextLinkCopyEngine, selectorCopyEngine, rootDeviceEnvironment);
    EXPECT_EQ(nextLinkCopyEngine, EngineHelpers::getBcsEngineType(rootDeviceEnvironment, deviceBitfield, selectorCopyEngine, false));
    EXPECT_EQ(aub_stream::EngineType::ENGINE_BCS2, EngineHelpers::getBcsEngineType(rootDeviceEnvironment, deviceBitfield, selectorCopyEngine, false));
    EXPECT_EQ(nextLinkCopyEngine, EngineHelpers::getBcsEngineType(rootDeviceEnvironment, deviceBitfield, selectorCopyEngine, false));
}

TEST(EngineNodeHelperTest, givenLinkCopyEnginesAndInternalUsageEnabledWhenGettingBcsEngineThenUseBcs2only) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableCopyEngineSelector.set(1);
    SelectorCopyEngine selectorCopyEngine{};
    MockExecutionEnvironment executionEnvironment{};
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];
    auto &hwInfo = *rootDeviceEnvironment.getMutableHardwareInfo();
    DeviceBitfield deviceBitfield = 0b11;
    hwInfo.featureTable.ftrBcsInfo = 0b1111;
    auto isInternalUsage = true;
    EXPECT_EQ(aub_stream::EngineType::ENGINE_BCS3, EngineHelpers::getBcsEngineType(rootDeviceEnvironment, deviceBitfield, selectorCopyEngine, isInternalUsage));
    EXPECT_EQ(aub_stream::EngineType::ENGINE_BCS3, EngineHelpers::getBcsEngineType(rootDeviceEnvironment, deviceBitfield, selectorCopyEngine, isInternalUsage));
}

TEST(EngineNodeHelperTest, givenAllEnginesWhenCheckingEngineIsComputeCapableThenReturnTrueOnlyForCompute) {
    struct EngineProperties {
        aub_stream::EngineType engineType;
        bool isCompute;
    };

    const EngineProperties engines[] = {
        {aub_stream::ENGINE_RCS, true},
        {aub_stream::ENGINE_CCS, true},
        {aub_stream::ENGINE_CCS1, true},
        {aub_stream::ENGINE_CCS2, true},
        {aub_stream::ENGINE_CCS3, true},
        {aub_stream::ENGINE_CCCS, true},

        {aub_stream::ENGINE_BCS, false},
        {aub_stream::ENGINE_BCS1, false},
        {aub_stream::ENGINE_BCS2, false},
        {aub_stream::ENGINE_BCS3, false},
        {aub_stream::ENGINE_BCS4, false},
        {aub_stream::ENGINE_BCS5, false},
        {aub_stream::ENGINE_BCS6, false},
        {aub_stream::ENGINE_BCS7, false},
        {aub_stream::ENGINE_BCS8, false}};

    const size_t numEngines = sizeof(engines) / sizeof(EngineProperties);

    for (size_t i = 0; i < numEngines; i++) {
        EXPECT_EQ(engines[i].isCompute, EngineHelpers::isComputeEngine(engines[i].engineType));
    }
}
