/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/engine_node_helper.h"

#include "test.h"

using namespace NEO;

TEST(EngineNodeHelperTests, givenValidEngineUsageWhenGettingStringRepresentationThenItIsCorrect) {
    EXPECT_EQ(std::string{"Regular"}, EngineHelpers::engineUsageToString(EngineUsage::Regular));
    EXPECT_EQ(std::string{"Internal"}, EngineHelpers::engineUsageToString(EngineUsage::Internal));
    EXPECT_EQ(std::string{"LowPriority"}, EngineHelpers::engineUsageToString(EngineUsage::LowPriority));
}

TEST(EngineNodeHelperTests, givenInValidEngineUsageWhenGettingStringRepresentationThenReturnUnknown) {
    EXPECT_EQ(std::string{"Unknown"}, EngineHelpers::engineUsageToString(EngineUsage::EngineUsageCount));
    EXPECT_EQ(std::string{"Unknown"}, EngineHelpers::engineUsageToString(static_cast<EngineUsage>(0xcc)));
}

TEST(EngineNodeHelperTests, givenValidEngineTypeWhenGettingStringRepresentationThenItIsCorrect) {
#define CHECK_ENGINE(type) EXPECT_EQ(std::string{#type}, EngineHelpers::engineTypeToString(aub_stream::EngineType::ENGINE_##type))
    CHECK_ENGINE(RCS);
    CHECK_ENGINE(BCS);
    CHECK_ENGINE(VCS);
    CHECK_ENGINE(VECS);
    CHECK_ENGINE(CCS);
    CHECK_ENGINE(CCS1);
    CHECK_ENGINE(CCS2);
    CHECK_ENGINE(CCS3);
#undef CHECK_ENGINE
}

TEST(EngineNodeHelperTests, givenCcsEngineWhenHelperIsUsedThenReturnTrue) {
    EXPECT_TRUE(EngineHelpers::isCcs(aub_stream::EngineType::ENGINE_CCS));
    EXPECT_TRUE(EngineHelpers::isCcs(aub_stream::EngineType::ENGINE_CCS1));
    EXPECT_TRUE(EngineHelpers::isCcs(aub_stream::EngineType::ENGINE_CCS2));
    EXPECT_TRUE(EngineHelpers::isCcs(aub_stream::EngineType::ENGINE_CCS3));
    EXPECT_FALSE(EngineHelpers::isCcs(aub_stream::EngineType::ENGINE_RCS));
    EXPECT_FALSE(EngineHelpers::isCcs(aub_stream::EngineType::NUM_ENGINES));
}

TEST(EngineNodeHelperTests, givenInvalidEngineTypeWhenGettingStringRepresentationThenItIsCorrect) {
    EXPECT_EQ(std::string{"Unknown"}, EngineHelpers::engineTypeToString(aub_stream::EngineType::NUM_ENGINES));
    EXPECT_EQ(std::string{"Unknown"}, EngineHelpers::engineTypeToString(static_cast<aub_stream::EngineType>(0xcc)));
}
