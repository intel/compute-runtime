/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/gmm_helper/cache_settings_helper.h"
#include "shared/source/gmm_helper/gmm_lib.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/release_helpers/release_helper/release_helper.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_product_helper.h"
#include "shared/test/common/mocks/mock_release_helper.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "gtest/gtest.h"

using namespace NEO;

using ReleaseHelperPatIndexXe3pTests = ::testing::Test;

HWTEST2_F(ReleaseHelperPatIndexXe3pTests, givenSystemMemoryPatOverrideWhenDebugFlagIsSetThenSelectDefaultDisabledOrForcedPat, IsAtLeastXe3pCore) {
    DebugManagerStateRestore restore;
    auto releaseHelper = ReleaseHelper::create(defaultHwInfo->ipVersion);
    ASSERT_NE(nullptr, releaseHelper);
    constexpr uint64_t patIndex = 5u;

    debugManager.flags.EnableOverrideToPat19ForSystemMemory.set(-1);
    EXPECT_EQ(defaultHwInfo->capabilityTable.isIntegratedDevice ? 19u : patIndex, releaseHelper->overrideSystemMemoryPatIndex(patIndex));

    debugManager.flags.EnableOverrideToPat19ForSystemMemory.set(0);
    EXPECT_EQ(patIndex, releaseHelper->overrideSystemMemoryPatIndex(patIndex));

    debugManager.flags.EnableOverrideToPat19ForSystemMemory.set(1);
    EXPECT_EQ(19u, releaseHelper->overrideSystemMemoryPatIndex(patIndex));
}

namespace {
struct MockProductHelperWithMisalignedUserPtr2WayCoherency : MockProductHelper {
    bool isMisalignedUserPtr2WayCoherent() const override {
        return true;
    }
};
} // namespace

TEST(ReleaseHelperPatIndexTests, givenCoherentSystemMemoryWhenSelectingGmmUsageThenPreserveSafeUsageAndUseAppTransientCoherentPatIfRequired) {
    DebugManagerStateRestore restore;
    MockProductHelper productHelper;
    MockReleaseHelper releaseHelper;
    productHelper.isL3FlushAfterPostSyncSupportedResult = true;

    releaseHelper.isAppTransientCoherentPatRequiredResult = false;
    EXPECT_EQ(GMM_RESOURCE_USAGE_OCL_SYSTEM_MEMORY_BUFFER,
              CacheSettingsHelper::getGmmUsageTypeForCoherentSystemMemory(GMM_RESOURCE_USAGE_OCL_SYSTEM_MEMORY_BUFFER, productHelper, releaseHelper));
    EXPECT_EQ(GMM_RESOURCE_USAGE_HW_CONTEXT,
              CacheSettingsHelper::getGmmUsageTypeForCoherentSystemMemory(GMM_RESOURCE_USAGE_HW_CONTEXT, productHelper, releaseHelper));
    EXPECT_EQ(GMM_RESOURCE_USAGE_FINE_GRAINED_COHERENT,
              CacheSettingsHelper::getGmmUsageTypeForCoherentSystemMemory(GMM_RESOURCE_USAGE_FINE_GRAINED_COHERENT, productHelper, releaseHelper));
    EXPECT_EQ(GMM_RESOURCE_USAGE_FINE_GRAINED_COHERENT,
              CacheSettingsHelper::getGmmUsageTypeForCoherentSystemMemory(GMM_RESOURCE_USAGE_OCL_BUFFER, productHelper, releaseHelper));

    releaseHelper.isAppTransientCoherentPatRequiredResult = true;
    EXPECT_EQ(GMM_RESOURCE_USAGE_OCL_SYSTEM_MEMORY_BUFFER,
              CacheSettingsHelper::getGmmUsageTypeForCoherentSystemMemory(GMM_RESOURCE_USAGE_FINE_GRAINED_COHERENT, productHelper, releaseHelper));
}

TEST(ReleaseHelperPatIndexTests, givenNoL3FlushAfterPostSyncSupportWhenSelectingGmmUsageForCoherentSystemMemoryThenPreservePreferredUsage) {
    MockProductHelper productHelper;
    MockReleaseHelper releaseHelper;
    productHelper.isL3FlushAfterPostSyncSupportedResult = false;

    EXPECT_EQ(GMM_RESOURCE_USAGE_OCL_BUFFER,
              CacheSettingsHelper::getGmmUsageTypeForCoherentSystemMemory(GMM_RESOURCE_USAGE_OCL_BUFFER, productHelper, releaseHelper));
}

TEST(ReleaseHelperPatIndexTests, givenMisalignedUserPtrWhenSelectingGmmUsageThenUse2WayCoherencyOnlyWhenSupportedByRelease) {
    DebugManagerStateRestore restore;
    MockProductHelperWithMisalignedUserPtr2WayCoherency productHelper;
    MockReleaseHelper releaseHelper;

    const auto userPtr = reinterpret_cast<const void *>(0x1001);
    constexpr size_t size = 13u;

    releaseHelper.isAppTransientCoherentPatRequiredResult = false;
    EXPECT_EQ(GMM_RESOURCE_USAGE_HW_CONTEXT,
              CacheSettingsHelper::getGmmUsageTypeForUserPtr(true, userPtr, size, productHelper, releaseHelper));

    releaseHelper.isAppTransientCoherentPatRequiredResult = true;
    EXPECT_EQ(GMM_RESOURCE_USAGE_OCL_SYSTEM_MEMORY_BUFFER,
              CacheSettingsHelper::getGmmUsageTypeForUserPtr(true, userPtr, size, productHelper, releaseHelper));
}
