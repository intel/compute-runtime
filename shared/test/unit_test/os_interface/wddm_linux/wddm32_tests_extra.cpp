/*
 * Copyright (C) 2025 Intel Corporation
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
