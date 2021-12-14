/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_helper.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"

#include "gmock/gmock.h"

using namespace NEO;

using PvcDeviceCapsTests = Test<ClDeviceFixture>;

PVCTEST_F(PvcDeviceCapsTests, givenPvcProductWhenCheckBlitterOperationsSupportThenReturnFalse) {
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.blitterOperationsSupported);
}

PVCTEST_F(PvcDeviceCapsTests, givenPvcProductWhenCheckingSldSupportThenReturnFalse) {
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.debuggerSupported);
}

PVCTEST_F(PvcDeviceCapsTests, givenPvcWhenAskingForCacheFlushAfterWalkerThenReturnFalse) {
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.supportCacheFlushAfterWalker);
}

PVCTEST_F(PvcDeviceCapsTests, givenPvcProductWhenCheckImagesSupportThenReturnFalse) {
    EXPECT_FALSE(PVC::hwInfo.capabilityTable.supportsImages);
}

PVCTEST_F(PvcDeviceCapsTests, givenPvcProductWhenDeviceCapsInitializedThenAddPvcExtensions) {
    const auto &dInfo = pClDevice->getDeviceInfo();
    EXPECT_THAT(dInfo.deviceExtensions, testing::HasSubstr(std::string("cl_intel_create_buffer_with_properties")));
    EXPECT_THAT(dInfo.deviceExtensions, testing::HasSubstr(std::string("cl_intel_dot_accumulate")));
    EXPECT_THAT(dInfo.deviceExtensions, testing::HasSubstr(std::string("cl_intel_subgroup_local_block_io")));
    EXPECT_THAT(dInfo.deviceExtensions, testing::HasSubstr(std::string("cl_intel_subgroup_matrix_multiply_accumulate_for_PVC")));
    EXPECT_THAT(dInfo.deviceExtensions, testing::HasSubstr(std::string("cl_khr_subgroup_named_barrier")));
    EXPECT_THAT(dInfo.deviceExtensions, testing::HasSubstr(std::string("cl_intel_subgroup_extended_block_read")));
}
