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
#include "shared/test/common/mocks/mock_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/unit_test/os_interface/product_helper_tests.h"

using namespace NEO;

using PtlProductHelperWindows = ProductHelperTest;

PTLTEST_F(PtlProductHelperWindows, givenDebugFlagWhenCheckingIsResolveDependenciesByPipeControlsSupportedThenTheFlagDerivedValueIsReturned) {
    DebugManagerStateRestore restorer;

    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MockCommandStreamReceiverWithDirectSubmissionRelaxedOrdering<false> csr(*mockDevice->getExecutionEnvironment(), mockDevice->getRootDeviceIndex(), mockDevice->getDeviceBitfield());
    MockCommandStreamReceiverWithDirectSubmissionRelaxedOrdering<true> csrRelaxed(*mockDevice->getExecutionEnvironment(), mockDevice->getRootDeviceIndex(), mockDevice->getDeviceBitfield());
    csr.taskCount = 2;
    csrRelaxed.taskCount = 2;
    auto productHelper = &mockDevice->getProductHelper();

    debugManager.flags.ResolveDependenciesViaPipeControls.set(0);
    EXPECT_FALSE(productHelper->isResolveDependenciesByPipeControlsSupported(pInHwInfo, false, 2, csr));
    EXPECT_FALSE(productHelper->isResolveDependenciesByPipeControlsSupported(pInHwInfo, true, 2, csr));
    EXPECT_FALSE(productHelper->isResolveDependenciesByPipeControlsSupported(pInHwInfo, false, 3, csr));
    EXPECT_FALSE(productHelper->isResolveDependenciesByPipeControlsSupported(pInHwInfo, false, 2, csrRelaxed));

    debugManager.flags.ResolveDependenciesViaPipeControls.set(1);
    EXPECT_TRUE(productHelper->isResolveDependenciesByPipeControlsSupported(pInHwInfo, false, 2, csr));
    EXPECT_TRUE(productHelper->isResolveDependenciesByPipeControlsSupported(pInHwInfo, true, 2, csr));
    EXPECT_TRUE(productHelper->isResolveDependenciesByPipeControlsSupported(pInHwInfo, false, 3, csr));
    EXPECT_TRUE(productHelper->isResolveDependenciesByPipeControlsSupported(pInHwInfo, false, 2, csrRelaxed));
}

PTLTEST_F(PtlProductHelperWindows, givenResolveDependenciesByPipeControllsSupportedWhenCheckedThenReturnsTrue) {
    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MockCommandStreamReceiverWithDirectSubmissionRelaxedOrdering<false> csr(*mockDevice->getExecutionEnvironment(), mockDevice->getRootDeviceIndex(), mockDevice->getDeviceBitfield());
    csr.taskCount = 2;
    auto productHelper = &mockDevice->getProductHelper();

    EXPECT_TRUE(productHelper->isResolveDependenciesByPipeControlsSupported(pInHwInfo, false, 2, csr));
}

PTLTEST_F(PtlProductHelperWindows, givenResolveDependenciesByPipeControllsNotSupportedWhenCheckedThenReturnsFalse) {
    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MockCommandStreamReceiverWithDirectSubmissionRelaxedOrdering<false> csr(*mockDevice->getExecutionEnvironment(), mockDevice->getRootDeviceIndex(), mockDevice->getDeviceBitfield());
    MockCommandStreamReceiverWithDirectSubmissionRelaxedOrdering<true> csrRelaxed(*mockDevice->getExecutionEnvironment(), mockDevice->getRootDeviceIndex(), mockDevice->getDeviceBitfield());
    csr.taskCount = 2;
    csrRelaxed.taskCount = 2;
    auto productHelper = &mockDevice->getProductHelper();

    EXPECT_FALSE(productHelper->isResolveDependenciesByPipeControlsSupported(pInHwInfo, true, 2, csr));
    EXPECT_FALSE(productHelper->isResolveDependenciesByPipeControlsSupported(pInHwInfo, false, 3, csr));
    EXPECT_FALSE(productHelper->isResolveDependenciesByPipeControlsSupported(pInHwInfo, false, 2, csrRelaxed));
}
