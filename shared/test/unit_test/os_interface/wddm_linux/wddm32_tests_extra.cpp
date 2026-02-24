/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/os_interface/windows/wddm32_tests.h"

using namespace NEO;

TEST_F(Wddm32Tests, whencreateMonitoredFenceForDirectSubmissionThenObtainHwQueueFenceAndReplaceResidencyControllerWithNewFence) {
    MonitoredFence fence{};
    EXPECT_THROW(wddm->getWddmInterface()->createFenceForDirectSubmission(fence, *osContext), std::exception);
}

TEST_F(Wddm32Tests, givenWslWhenCreateNativeFenceThenFail) {
    WddmSyncFence syncFence;
    syncFence.setFenceValue(0u);
    EXPECT_EQ(syncFence.getFence()->currentFenceValue, 0u);

    EXPECT_FALSE(wddm->getWddmInterface()->createNativeFence(*syncFence.getFence(), false));
    EXPECT_EQ(syncFence.getCpuAddress(), nullptr);
    EXPECT_EQ(syncFence.getGpuAddress(), 0u);
    EXPECT_EQ(syncFence.getFence()->currentFenceValue, 0u);
}
