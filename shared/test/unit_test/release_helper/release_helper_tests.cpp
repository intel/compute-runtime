/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/kernel/kernel_properties.h"
#include "shared/source/release_helper/release_helper.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_release_helper.h"

#include "gtest/gtest.h"

using namespace NEO;

using ReleaseHelperKernelCapabilitiesTests = ::testing::Test;

TEST(ReleaseHelperKernelCapabilitiesTests, GivenRequestForKernelFp16CapabilitiesThenReturnMinMaxAndLoadStoreCapabilitiesWithCapsFromReleaseHelper) {
    MockReleaseHelper releaseHelper;
    releaseHelper.getAdditionalFp16CapsResult = 0u;

    uint32_t fp16Caps = 0u;
    releaseHelper.getKernelFp16AtomicCapabilities(fp16Caps);

    EXPECT_EQ(1u, releaseHelper.getAdditionalFp16CapsCalled);
    EXPECT_EQ(FpAtomicExtFlags::minMaxAtomicCaps | FpAtomicExtFlags::loadStoreAtomicCaps, fp16Caps);
}

TEST(ReleaseHelperKernelCapabilitiesTests, givenAdditionalFp16CapsWhenGettingKernelFp16AtomicCapabilitiesThenAdditionalCapsAreOredIn) {
    MockReleaseHelper releaseHelper;
    releaseHelper.getAdditionalFp16CapsResult = FpAtomicExtFlags::addAtomicCaps;

    uint32_t fp16Caps = 0u;
    releaseHelper.getKernelFp16AtomicCapabilities(fp16Caps);

    EXPECT_EQ(1u, releaseHelper.getAdditionalFp16CapsCalled);
    EXPECT_EQ(FpAtomicExtFlags::addAtomicCaps | FpAtomicExtFlags::minMaxAtomicCaps | FpAtomicExtFlags::loadStoreAtomicCaps, fp16Caps);
}
