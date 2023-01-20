/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hpc_core/hw_cmds.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/fixtures/device_info_fixture.h"

#include "hw_cmds_xe_hpc_core_base.h"

using namespace NEO;

using XeHpcCoreClDeviceCaps = Test<ClDeviceFixture>;

XE_HPC_CORETEST_F(XeHpcCoreClDeviceCaps, givenXeHpcCoreWhenCheckExtensionsThenDeviceDoesNotReportClKhrSubgroupsExtension) {
    const auto &caps = pClDevice->getDeviceInfo();

    EXPECT_TRUE(hasSubstr(caps.deviceExtensions, std::string("cl_khr_subgroups")));
    EXPECT_TRUE(hasSubstr(caps.deviceExtensions, std::string("cl_intel_subgroup_matrix_multiply_accumulate")));
    EXPECT_TRUE(hasSubstr(caps.deviceExtensions, std::string("cl_intel_subgroup_split_matrix_multiply_accumulate")));
    EXPECT_TRUE(hasSubstr(caps.deviceExtensions, std::string("cl_intel_bfloat16_conversions")));
}

XE_HPC_CORETEST_F(XeHpcCoreClDeviceCaps, givenXeHpcCoreWhenCheckingCapsThenDeviceDoesNotSupportIndependentForwardProgress) {
    const auto &caps = pClDevice->getDeviceInfo();

    EXPECT_TRUE(caps.independentForwardProgress);
}

using QueueFamilyNameTestXeHpcCore = QueueFamilyNameTest;

XE_HPC_CORETEST_F(QueueFamilyNameTestXeHpcCore, givenCccsWhenGettingQueueFamilyNameThenReturnProperValue) {
    verify(EngineGroupType::RenderCompute, "cccs");
}

XE_HPC_CORETEST_F(QueueFamilyNameTestXeHpcCore, givenLinkedBcsWhenGettingQueueFamilyNameThenReturnProperValue) {
    verify(EngineGroupType::LinkedCopy, "linked bcs");
}
