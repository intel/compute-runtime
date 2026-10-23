/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "base_ult_config_listener.h"

#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/utilities/wait_util.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/ult_hw_config.h"
#include "shared/test/common/libult/signal_utils.h"
#include "shared/test/common/mocks/mock_sip.h"
#include "shared/test/common/test_macros/test_base.h"
#include "shared/test/common/test_macros/test_matcher_registry.h"

#include "aubstream/aubstream.h"

extern bool enableAlarm;

namespace NEO {

extern unsigned int testCaseMaxTimeInMs;

void BaseUltConfigListener::snapshotHwInfo() {
    referencedHwInfo = *defaultHwInfo;
    referencedFeatureTableHash = referencedHwInfo.featureTable.asHash();
    referencedWorkaroundTableHash = referencedHwInfo.workaroundTable.asHash();
}

void BaseUltConfigListener::OnTestIterationStart(const ::testing::UnitTest &, int) {
    lastTest.reserve(maxTestNameLength);
    snapshotHwInfo();
    markIterationStart();
    resetAlarm(enableAlarm);
}

void BaseUltConfigListener::OnTestIterationEnd(const ::testing::UnitTest &, int) {
    pauseAlarm(enableAlarm);
}

void BaseUltConfigListener::OnTestStart(const ::testing::TestInfo &testInfo) {
    testExcluded = !NEO::TestMatcherRegistry::willRunForCurrentProduct(testInfo, ::productFamily);
    if (testExcluded) {
        GTEST_SKIP();
    }

    lastTest.assign(testInfo.test_suite_name()).append(".").append(testInfo.name());
    WaitUtils::waitpkgUse = WaitUtils::WaitpkgUse::uninitialized;
    WaitUtils::waitPkgThresholdInMicroSeconds = WaitUtils::defaultWaitPkgThresholdInMicroSeconds;
    WaitUtils::waitPkgThresholdForEventHostSyncInMicroSeconds = WaitUtils::defaultWaitPkgThresholdForEventHostSyncInMicroSeconds;
    WaitUtils::waitpkgCounterValue = WaitUtils::defaultCounterValue;
    WaitUtils::counterValueForEventHostSync = WaitUtils::defaultCounterValueForEventHostSync;
    WaitUtils::waitpkgControlValue = WaitUtils::defaultControlValue;
    WaitUtils::waitCount = WaitUtils::defaultWaitCount;

    maxOsContextCountBackup = MemoryManager::maxOsContextCount;

    stateSaveAreaHeaderSnapshot = MockSipData::mockSipKernel->getStateSaveAreaHeader();

    testStart = std::chrono::steady_clock::now();
}

void BaseUltConfigListener::OnTestEnd(const ::testing::TestInfo &) {
    if (testExcluded) {
        return;
    }

    auto testEnd = std::chrono::steady_clock::now();

    if (enableAlarm) {
        EXPECT_LT(std::chrono::duration_cast<std::chrono::milliseconds>(testEnd - testStart).count(), testCaseMaxTimeInMs);
    }
    aub_stream::injectMMIOListLegacy(aub_stream::MMIOList{});

    // Ensure that global state is restored
    UltHwConfig expectedState{};
    static_assert(sizeof(UltHwConfig) == (20 * sizeof(bool) + sizeof(const char *) + sizeof(ExecutionEnvironment *) + sizeof(UltHwConfig::padding)), ""); // Ensure that there is no internal padding
    EXPECT_EQ(0, memcmp(&expectedState, &ultHwConfig, sizeof(UltHwConfig)));

    const bool platformRestored = 0 == memcmp(&referencedHwInfo.platform, &defaultHwInfo->platform, sizeof(PLATFORM));
    const bool featureTableRestored = referencedFeatureTableHash == defaultHwInfo->featureTable.asHash();
    const bool workaroundTableRestored = referencedWorkaroundTableHash == defaultHwInfo->workaroundTable.asHash();
    const bool capabilityTableRestored = referencedHwInfo.capabilityTable == defaultHwInfo->capabilityTable;
    const bool capsRestored = referencedHwInfo.caps == defaultHwInfo->caps;
    const bool ipVersionRestored = referencedHwInfo.ipVersion.value == defaultHwInfo->ipVersion.value;
    EXPECT_TRUE(platformRestored);
    EXPECT_TRUE(featureTableRestored);
    EXPECT_TRUE(workaroundTableRestored);
    EXPECT_TRUE(capabilityTableRestored);
    EXPECT_TRUE(capsRestored);
    EXPECT_TRUE(ipVersionRestored);
    if (!platformRestored || !featureTableRestored || !workaroundTableRestored || !capabilityTableRestored || !capsRestored || !ipVersionRestored) {
        snapshotHwInfo();
    }
    MemoryManager::maxOsContextCount = maxOsContextCountBackup;

    EXPECT_EQ(stateSaveAreaHeaderSnapshot.size(), MockSipData::mockSipKernel->getStateSaveAreaHeader().size());
    EXPECT_EQ(0, memcmp(stateSaveAreaHeaderSnapshot.data(), MockSipData::mockSipKernel->getStateSaveAreaHeader().data(), stateSaveAreaHeaderSnapshot.size()));
}
} // namespace NEO
