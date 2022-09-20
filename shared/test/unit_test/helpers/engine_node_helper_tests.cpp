/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/device.h"
#include "shared/source/helpers/engine_node_helper.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

TEST(EngineNodeHelperTest, givenValidEngineUsageWhenGettingStringRepresentationThenItIsCorrect) {
    EXPECT_EQ(std::string{"Regular"}, EngineHelpers::engineUsageToString(EngineUsage::Regular));
    EXPECT_EQ(std::string{"Internal"}, EngineHelpers::engineUsageToString(EngineUsage::Internal));
    EXPECT_EQ(std::string{"LowPriority"}, EngineHelpers::engineUsageToString(EngineUsage::LowPriority));
    EXPECT_EQ(std::string{"Cooperative"}, EngineHelpers::engineUsageToString(EngineUsage::Cooperative));
}

TEST(EngineNodeHelperTest, givenInValidEngineUsageWhenGettingStringRepresentationThenReturnUnknown) {
    EXPECT_EQ(std::string{"Unknown"}, EngineHelpers::engineUsageToString(EngineUsage::EngineUsageCount));
    EXPECT_EQ(std::string{"Unknown"}, EngineHelpers::engineUsageToString(static_cast<EngineUsage>(0xcc)));
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

TEST(EngineNodeHelperTest, givenInvalidEngineTypeWhenGettingStringRepresentationThenItIsCorrect) {
    EXPECT_EQ(std::string{"Unknown"}, EngineHelpers::engineTypeToString(aub_stream::EngineType::NUM_ENGINES));
    EXPECT_EQ(std::string{"Unknown"}, EngineHelpers::engineTypeToString(static_cast<aub_stream::EngineType>(0xcc)));
}

TEST(EngineNodeHelperTest, givenLinkCopyEnginesSupportedWhenGettingBcsEngineTypeThenFirstReturnMainCopyEngineAndThenRoundRobinBetweenLinkEngines) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableCopyEngineSelector.set(1);
    SelectorCopyEngine selectorCopyEngine{};
    HardwareInfo hwInfo = *::defaultHwInfo;
    DeviceBitfield deviceBitfield = 0b11;
    hwInfo.featureTable.ftrBcsInfo = 0b111;

    EXPECT_EQ(aub_stream::EngineType::ENGINE_BCS, EngineHelpers::getBcsEngineType(hwInfo, deviceBitfield, selectorCopyEngine, false));
    EXPECT_EQ(aub_stream::EngineType::ENGINE_BCS2, EngineHelpers::getBcsEngineType(hwInfo, deviceBitfield, selectorCopyEngine, false));
    EXPECT_EQ(aub_stream::EngineType::ENGINE_BCS1, EngineHelpers::getBcsEngineType(hwInfo, deviceBitfield, selectorCopyEngine, false));
    EXPECT_EQ(aub_stream::EngineType::ENGINE_BCS2, EngineHelpers::getBcsEngineType(hwInfo, deviceBitfield, selectorCopyEngine, false));
    EXPECT_EQ(aub_stream::EngineType::ENGINE_BCS1, EngineHelpers::getBcsEngineType(hwInfo, deviceBitfield, selectorCopyEngine, false));
    EXPECT_EQ(aub_stream::EngineType::ENGINE_BCS2, EngineHelpers::getBcsEngineType(hwInfo, deviceBitfield, selectorCopyEngine, false));
    EXPECT_EQ(aub_stream::EngineType::ENGINE_BCS1, EngineHelpers::getBcsEngineType(hwInfo, deviceBitfield, selectorCopyEngine, false));
}

TEST(EngineNodeHelperTest, givenMainBcsEngineIsReleasedWhenGettingBcsEngineTypeThenItCanBeReturnedAgain) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableCopyEngineSelector.set(1);
    SelectorCopyEngine selectorCopyEngine{};
    HardwareInfo hwInfo = *::defaultHwInfo;
    DeviceBitfield deviceBitfield = 0b11;
    hwInfo.featureTable.ftrBcsInfo = 0b111;

    EXPECT_EQ(aub_stream::EngineType::ENGINE_BCS, EngineHelpers::getBcsEngineType(hwInfo, deviceBitfield, selectorCopyEngine, false));
    EXPECT_EQ(aub_stream::EngineType::ENGINE_BCS2, EngineHelpers::getBcsEngineType(hwInfo, deviceBitfield, selectorCopyEngine, false));
    EXPECT_EQ(aub_stream::EngineType::ENGINE_BCS1, EngineHelpers::getBcsEngineType(hwInfo, deviceBitfield, selectorCopyEngine, false));
    EXPECT_EQ(aub_stream::EngineType::ENGINE_BCS2, EngineHelpers::getBcsEngineType(hwInfo, deviceBitfield, selectorCopyEngine, false));

    EngineHelpers::releaseBcsEngineType(aub_stream::EngineType::ENGINE_BCS, selectorCopyEngine);
    EXPECT_EQ(aub_stream::EngineType::ENGINE_BCS, EngineHelpers::getBcsEngineType(hwInfo, deviceBitfield, selectorCopyEngine, false));
    EXPECT_EQ(aub_stream::EngineType::ENGINE_BCS1, EngineHelpers::getBcsEngineType(hwInfo, deviceBitfield, selectorCopyEngine, false));
    EXPECT_EQ(aub_stream::EngineType::ENGINE_BCS2, EngineHelpers::getBcsEngineType(hwInfo, deviceBitfield, selectorCopyEngine, false));
}

TEST(EngineNodeHelperTest, givenLinkBcsEngineIsReleasedWhenGettingBcsEngineTypeThenItDoesNotAffectFurtherSelections) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableCopyEngineSelector.set(1);
    SelectorCopyEngine selectorCopyEngine{};
    HardwareInfo hwInfo = *::defaultHwInfo;
    DeviceBitfield deviceBitfield = 0b11;
    hwInfo.featureTable.ftrBcsInfo = 0b111;

    EXPECT_EQ(aub_stream::EngineType::ENGINE_BCS, EngineHelpers::getBcsEngineType(hwInfo, deviceBitfield, selectorCopyEngine, false));
    EXPECT_EQ(aub_stream::EngineType::ENGINE_BCS2, EngineHelpers::getBcsEngineType(hwInfo, deviceBitfield, selectorCopyEngine, false));
    EXPECT_EQ(aub_stream::EngineType::ENGINE_BCS1, EngineHelpers::getBcsEngineType(hwInfo, deviceBitfield, selectorCopyEngine, false));
    EXPECT_EQ(aub_stream::EngineType::ENGINE_BCS2, EngineHelpers::getBcsEngineType(hwInfo, deviceBitfield, selectorCopyEngine, false));

    EngineHelpers::releaseBcsEngineType(aub_stream::EngineType::ENGINE_BCS1, selectorCopyEngine);
    EXPECT_EQ(aub_stream::EngineType::ENGINE_BCS1, EngineHelpers::getBcsEngineType(hwInfo, deviceBitfield, selectorCopyEngine, false));
    EXPECT_EQ(aub_stream::EngineType::ENGINE_BCS2, EngineHelpers::getBcsEngineType(hwInfo, deviceBitfield, selectorCopyEngine, false));
    EXPECT_EQ(aub_stream::EngineType::ENGINE_BCS1, EngineHelpers::getBcsEngineType(hwInfo, deviceBitfield, selectorCopyEngine, false));
}

TEST(EngineNodeHelperTest, givenLinkCopyEnginesAndInternalUsageEnabledWhenGettingBcsEngineThenUseBcs2only) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableCopyEngineSelector.set(1);
    SelectorCopyEngine selectorCopyEngine{};
    HardwareInfo hwInfo = *::defaultHwInfo;
    DeviceBitfield deviceBitfield = 0b11;
    hwInfo.featureTable.ftrBcsInfo = 0b111;
    auto isInternalUsage = true;
    EXPECT_EQ(aub_stream::EngineType::ENGINE_BCS3, EngineHelpers::getBcsEngineType(hwInfo, deviceBitfield, selectorCopyEngine, isInternalUsage));
    EXPECT_EQ(aub_stream::EngineType::ENGINE_BCS3, EngineHelpers::getBcsEngineType(hwInfo, deviceBitfield, selectorCopyEngine, isInternalUsage));
}
