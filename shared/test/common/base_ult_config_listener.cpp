/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "base_ult_config_listener.h"

#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/ult_hw_config.h"

#include "third_party/aub_stream/headers/aubstream.h"

void NEO::BaseUltConfigListener::OnTestStart(const ::testing::TestInfo &) {
    debugVarSnapshot = DebugManager.flags;
    injectFcnSnapshot = DebugManager.injectFcn;

    referencedHwInfo = *defaultHwInfo;
}

void NEO::BaseUltConfigListener::OnTestEnd(const ::testing::TestInfo &) {
    aub_stream::injectMMIOList(aub_stream::MMIOList{});

#undef DECLARE_DEBUG_VARIABLE
#define DECLARE_DEBUG_VARIABLE(dataType, variableName, defaultValue, description) \
    EXPECT_EQ(debugVarSnapshot.variableName.getRef(), DebugManager.flags.variableName.getRef());

#include "shared/source/debug_settings/release_variables.inl"

#include "debug_variables.inl"

#undef DECLARE_DEBUG_VARIABLE

    EXPECT_EQ(injectFcnSnapshot, DebugManager.injectFcn);

    // Ensure that global state is restored
    UltHwConfig expectedState{};
    static_assert(sizeof(UltHwConfig) == 13 * sizeof(bool), ""); // Ensure that there is no internal padding
    EXPECT_EQ(0, memcmp(&expectedState, &ultHwConfig, sizeof(UltHwConfig)));

    EXPECT_EQ(0, memcmp(&referencedHwInfo.platform, &defaultHwInfo->platform, sizeof(PLATFORM)));
    EXPECT_EQ(1, referencedHwInfo.featureTable.asHash() == defaultHwInfo->featureTable.asHash());
    EXPECT_EQ(1, referencedHwInfo.workaroundTable.asHash() == defaultHwInfo->workaroundTable.asHash());
    EXPECT_EQ(1, referencedHwInfo.capabilityTable == defaultHwInfo->capabilityTable);
}
