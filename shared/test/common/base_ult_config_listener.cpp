/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "base_ult_config_listener.h"

#include "shared/source/memory_manager/memory_manager.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/ult_hw_config.h"

#include "aubstream/aubstream.h"

extern bool enableAlarm;

namespace NEO {

extern unsigned int testCaseMaxTimeInMs;

void BaseUltConfigListener::OnTestStart(const ::testing::TestInfo &) {
    maxOsContextCountBackup = MemoryManager::maxOsContextCount;
    debugVarSnapshot = debugManager.flags;
    injectFcnSnapshot = debugManager.injectFcn;

    referencedHwInfo = *defaultHwInfo;
    testStart = std::chrono::steady_clock::now();
}

void BaseUltConfigListener::OnTestEnd(const ::testing::TestInfo &) {
    auto testEnd = std::chrono::steady_clock::now();

    if (enableAlarm) {
        EXPECT_LT(std::chrono::duration_cast<std::chrono::milliseconds>(testEnd - testStart).count(), testCaseMaxTimeInMs);
    }
    aub_stream::injectMMIOListLegacy(aub_stream::MMIOList{});

#undef DECLARE_DEBUG_VARIABLE
#define DECLARE_DEBUG_VARIABLE(dataType, variableName, defaultValue, description) \
    EXPECT_EQ(debugVarSnapshot.variableName.getRef(), debugManager.flags.variableName.getRef());

#include "shared/source/debug_settings/release_variables.inl"

#include "debug_variables.inl"

#undef DECLARE_DEBUG_VARIABLE

    EXPECT_EQ(injectFcnSnapshot, debugManager.injectFcn);

    // Ensure that global state is restored
    UltHwConfig expectedState{};
    static_assert(sizeof(UltHwConfig) == (16 * sizeof(bool) + sizeof(const char *)), ""); // Ensure that there is no internal padding
    EXPECT_EQ(0, memcmp(&expectedState, &ultHwConfig, sizeof(UltHwConfig)));

    EXPECT_EQ(0, memcmp(&referencedHwInfo.platform, &defaultHwInfo->platform, sizeof(PLATFORM)));
    EXPECT_EQ(1, referencedHwInfo.featureTable.asHash() == defaultHwInfo->featureTable.asHash());
    EXPECT_EQ(1, referencedHwInfo.workaroundTable.asHash() == defaultHwInfo->workaroundTable.asHash());
    EXPECT_EQ(1, referencedHwInfo.capabilityTable == defaultHwInfo->capabilityTable);
    MemoryManager::maxOsContextCount = maxOsContextCountBackup;
}
} // namespace NEO
