/*
 * Copyright (C) 2021-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/kernel/dispatch_kernel_encoder_interface.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/xe_hpg_core/hw_cmds_dg2.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/unit_test/encoders/test_encode_dispatch_kernel_dg2_and_later.h"

using namespace NEO;

using CommandEncodeStatesDg2Test = ::testing::Test;

DG2TEST_F(CommandEncodeStatesDg2Test, whenSelectingPreferredSlmSizePerDssThenUseDssCount) {
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;
    using PREFERRED_SLM_ALLOCATION_SIZE = typename INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE;

    MockExecutionEnvironment executionEnvironment{};
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];
    auto &hwInfo = *rootDeviceEnvironment.getMutableHardwareInfo();

    auto &productHelper = rootDeviceEnvironment.getHelper<ProductHelper>();

    hwInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(REVISION_B, hwInfo);
    hwInfo.gtSystemInfo.ThreadCount = 1024;
    hwInfo.gtSystemInfo.DualSubSliceCount = 8;
    hwInfo.gtSystemInfo.SubSliceCount = 2 * hwInfo.gtSystemInfo.DualSubSliceCount;

    struct PreferredSlmSizePerDssTestValues {
        uint32_t threadsPerThreadGroup;
        uint32_t slmSizePerThreadGroup;
        PREFERRED_SLM_ALLOCATION_SIZE expectedValueInIdd;
    };

    const PreferredSlmSizePerDssTestValues valuesToTest[] = {
        {7, 2 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_64KB},   // 18 groups will fit in one DSS
        {8, 2 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_32KB},   // 16 groups will fit in one DSS
        {9, 2 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_32KB},   // 14 groups will fit in one DSS
        {50, 16 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_64KB}, // 2 groups will fit in one DSS
    };

    for (const auto &valueToTest : valuesToTest) {
        INTERFACE_DESCRIPTOR_DATA idd = FamilyType::cmdInitInterfaceDescriptorData;

        EncodeSlmSizePerSubSliceArgs slmArgs{
            .threadsPerThreadGroup = valueToTest.threadsPerThreadGroup,
            .workloadThreadGroupCount = 1024,
            .slmTotalSizePerThreadGroup = valueToTest.slmSizePerThreadGroup,
            .slmPolicy = SlmPolicy::slmPolicyLargeSlm};

        EncodeDispatchKernel<FamilyType>::encodeSlmSizePerSubSlice(&idd, rootDeviceEnvironment, slmArgs);

        EXPECT_EQ(valueToTest.expectedValueInIdd, idd.getPreferredSlmAllocationSize())
            << "threadsPerThreadGroup: " << valueToTest.threadsPerThreadGroup
            << ", slmSizePerThreadGroup: " << valueToTest.slmSizePerThreadGroup;
    }
}

DG2TEST_F(CommandEncodeStatesDg2Test, GivenVariousSlmTotalSizesWhenSetPreferredSlmIsCalledThenCorrectValuesAreSet) {
    using PREFERRED_SLM_ALLOCATION_SIZE = typename FamilyType::INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE;

    const std::vector<PreferredSlmTestValuesPreXe2<FamilyType>> valuesToTest = {
        {0, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_0KB},
        {16 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_16KB},
        {32 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_32KB},
        // since we can't set 48KB as SLM size for workgroup, we need to ask for 64KB here.
        {64 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_64KB},
    };

    MockExecutionEnvironment executionEnvironment{};
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];
    verifyPreferredSlmValuesPreXe2<FamilyType>(valuesToTest, rootDeviceEnvironment);
}
