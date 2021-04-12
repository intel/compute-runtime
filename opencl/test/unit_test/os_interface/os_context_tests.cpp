/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/os_context.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(OSContext, whenCreatingDefaultOsContextThenExpectInitializedAlways) {
    OsContext *osContext = OsContext::create(nullptr, 0, 0, EngineTypeUsage{aub_stream::ENGINE_RCS, EngineUsage::Regular}, PreemptionMode::Disabled, false);
    EXPECT_FALSE(osContext->isLowPriority());
    EXPECT_FALSE(osContext->isInternalEngine());
    EXPECT_FALSE(osContext->isRootDevice());
    delete osContext;
}

TEST(OSContext, givenInternalAndRootDeviceAreTrueWhenCreatingDefaultOsContextThenExpectGettersTrue) {
    OsContext *osContext = OsContext::create(nullptr, 0, 0, EngineTypeUsage{aub_stream::ENGINE_RCS, EngineUsage::Internal}, PreemptionMode::Disabled, true);
    EXPECT_FALSE(osContext->isLowPriority());
    EXPECT_TRUE(osContext->isInternalEngine());
    EXPECT_TRUE(osContext->isRootDevice());
    delete osContext;
}

TEST(OSContext, givenLowPriorityAndRootDeviceAreTrueWhenCreatingDefaultOsContextThenExpectGettersTrue) {
    OsContext *osContext = OsContext::create(nullptr, 0, 0, EngineTypeUsage{aub_stream::ENGINE_RCS, EngineUsage::LowPriority}, PreemptionMode::Disabled, true);
    EXPECT_TRUE(osContext->isLowPriority());
    EXPECT_FALSE(osContext->isInternalEngine());
    EXPECT_TRUE(osContext->isRootDevice());
    delete osContext;
}

TEST(OSContext, givenOsContextCreatedDefaultIsFalseWhenSettingTrueThenFlagTrueReturned) {
    OsContext *osContext = OsContext::create(nullptr, 0, 0, EngineTypeUsage{aub_stream::ENGINE_RCS, EngineUsage::Regular}, PreemptionMode::Disabled, false);
    EXPECT_FALSE(osContext->isDefaultContext());
    osContext->setDefaultContext(true);
    EXPECT_TRUE(osContext->isDefaultContext());
    delete osContext;
}
