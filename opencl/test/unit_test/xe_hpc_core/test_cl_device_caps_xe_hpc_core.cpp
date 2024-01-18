/*
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hpc_core/hw_cmds.h"
#include "shared/source/xe_hpc_core/hw_cmds_xe_hpc_core_base.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/fixtures/device_info_fixture.h"

using namespace NEO;

using XeHpcCoreClDeviceCaps = Test<ClDeviceFixture>;

XE_HPC_CORETEST_F(XeHpcCoreClDeviceCaps, givenXeHpcCoreWhenCheckExtensionsThenDeviceDoesReportClKhrSubgroupsExtension) {
    const auto &caps = pClDevice->getDeviceInfo();

    EXPECT_TRUE(hasSubstr(caps.deviceExtensions, std::string("cl_khr_subgroups")));
    EXPECT_TRUE(hasSubstr(caps.deviceExtensions, std::string("cl_intel_subgroup_matrix_multiply_accumulate")));
    EXPECT_FALSE(hasSubstr(caps.deviceExtensions, std::string("cl_intel_subgroup_split_matrix_multiply_accumulate")));
    EXPECT_TRUE(hasSubstr(caps.deviceExtensions, std::string("cl_intel_bfloat16_conversions")));
}

XE_HPC_CORETEST_F(XeHpcCoreClDeviceCaps, givenXeHpcCoreWhenCheckingCapsThenDeviceDoesNotSupportIndependentForwardProgress) {
    const auto &caps = pClDevice->getDeviceInfo();

    EXPECT_TRUE(caps.independentForwardProgress);
}

using QueueFamilyNameTestXeHpcCore = QueueFamilyNameTest;

XE_HPC_CORETEST_F(QueueFamilyNameTestXeHpcCore, givenCccsWhenGettingQueueFamilyNameThenReturnProperValue) {
    verify(EngineGroupType::renderCompute, "cccs");
}

XE_HPC_CORETEST_F(QueueFamilyNameTestXeHpcCore, givenLinkedBcsWhenGettingQueueFamilyNameThenReturnProperValue) {
    verify(EngineGroupType::linkedCopy, "linked bcs");
}
