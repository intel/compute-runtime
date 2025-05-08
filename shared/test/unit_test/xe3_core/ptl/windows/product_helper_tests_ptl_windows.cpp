/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/product_helper.h"
#include "shared/source/xe3_core/hw_info_xe3_core.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/unit_test/os_interface/product_helper_tests.h"

using namespace NEO;

using PtlProductHelperWindows = ProductHelperTest;

PTLTEST_F(PtlProductHelperWindows, givenOverrideDirectSubmissionTimeoutsCalledThenTimeoutsAreOverridden) {
    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    auto productHelper = &mockDevice->getProductHelper();

    auto timeout = std::chrono::microseconds{5'000};
    auto maxTimeout = timeout;
    productHelper->overrideDirectSubmissionTimeouts(timeout, maxTimeout);
    EXPECT_EQ(timeout.count(), 1'000);
    EXPECT_EQ(maxTimeout.count(), 1'000);

    DebugManagerStateRestore restorer{};
    debugManager.flags.DirectSubmissionControllerTimeout.set(2'000);
    debugManager.flags.DirectSubmissionControllerMaxTimeout.set(3'000);

    productHelper->overrideDirectSubmissionTimeouts(timeout, maxTimeout);
    EXPECT_EQ(timeout.count(), 2'000);
    EXPECT_EQ(maxTimeout.count(), 3'000);
}