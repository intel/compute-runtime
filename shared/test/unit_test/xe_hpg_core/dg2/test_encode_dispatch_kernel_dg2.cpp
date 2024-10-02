/*
 * Copyright (C) 2021-2024 Intel Corporation
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

    {
        const uint32_t threadsPerThreadGroup = 7; // 18 groups will fit in one DSS
        const uint32_t slmSizePerThreadGroup = 2 * MemoryConstants::kiloByte;
        INTERFACE_DESCRIPTOR_DATA idd = FamilyType::cmdInitInterfaceDescriptorData;
        EncodeDispatchKernel<FamilyType>::setupPreferredSlmSize(&idd, rootDeviceEnvironment, threadsPerThreadGroup, slmSizePerThreadGroup, SlmPolicy::slmPolicyLargeSlm);
        EXPECT_EQ(PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_64K, idd.getPreferredSlmAllocationSize());
    }
    {
        const uint32_t threadsPerThreadGroup = 8; // 16 groups will fit in one DSS
        const uint32_t slmSizePerThreadGroup = 2 * MemoryConstants::kiloByte;
        INTERFACE_DESCRIPTOR_DATA idd = FamilyType::cmdInitInterfaceDescriptorData;
        EncodeDispatchKernel<FamilyType>::setupPreferredSlmSize(&idd, rootDeviceEnvironment, threadsPerThreadGroup, slmSizePerThreadGroup, SlmPolicy::slmPolicyLargeSlm);
        EXPECT_EQ(PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_32K, idd.getPreferredSlmAllocationSize());
    }
    {
        const uint32_t threadsPerThreadGroup = 9; // 14 groups will fit in one DSS
        const uint32_t slmSizePerThreadGroup = 2 * MemoryConstants::kiloByte;
        INTERFACE_DESCRIPTOR_DATA idd = FamilyType::cmdInitInterfaceDescriptorData;
        EncodeDispatchKernel<FamilyType>::setupPreferredSlmSize(&idd, rootDeviceEnvironment, threadsPerThreadGroup, slmSizePerThreadGroup, SlmPolicy::slmPolicyLargeSlm);
        EXPECT_EQ(PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_32K, idd.getPreferredSlmAllocationSize());
    }
    {
        const uint32_t threadsPerThreadGroup = 50; // 2 groups will fit in one DSS
        const uint32_t slmSizePerThreadGroup = 16 * MemoryConstants::kiloByte;
        INTERFACE_DESCRIPTOR_DATA idd = FamilyType::cmdInitInterfaceDescriptorData;
        EncodeDispatchKernel<FamilyType>::setupPreferredSlmSize(&idd, rootDeviceEnvironment, threadsPerThreadGroup, slmSizePerThreadGroup, SlmPolicy::slmPolicyLargeSlm);
        EXPECT_EQ(PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_64K, idd.getPreferredSlmAllocationSize());
    }
}

DG2TEST_F(CommandEncodeStatesDg2Test, GivenVariousSlmTotalSizesWhenSetPreferredSlmIsCalledThenCorrectValuesAreSet) {
    using PREFERRED_SLM_ALLOCATION_SIZE = typename FamilyType::INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE;

    const std::vector<PreferredSlmTestValues<FamilyType>> valuesToTest = {
        {0, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_0K},
        {16 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_16K},
        {32 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_32K},
        // since we can't set 48KB as SLM size for workgroup, we need to ask for 64KB here.
        {64 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_64K},
    };

    MockExecutionEnvironment executionEnvironment{};
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];
    verifyPreferredSlmValues<FamilyType>(valuesToTest, rootDeviceEnvironment);
}
