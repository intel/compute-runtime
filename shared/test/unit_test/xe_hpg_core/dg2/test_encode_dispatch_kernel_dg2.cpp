/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/kernel/dispatch_kernel_encoder_interface.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/xe_hpg_core/hw_cmds_dg2.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

using CommandEncodeStatesDg2Test = ::testing::Test;

DG2TEST_F(CommandEncodeStatesDg2Test, GivenSmallSlmTotalSizesWhenSetAdditionalInfoIsCalledThenCorrectValuesAreSet) {
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;
    using PREFERRED_SLM_ALLOCATION_SIZE = typename INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE;
    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &rootDeviceEnvironment = mockExecutionEnvironment.rootDeviceEnvironments[0];

    auto hwInfo = rootDeviceEnvironment->getMutableHardwareInfo();
    auto &revisionId = hwInfo->platform.usRevId;
    uint32_t threadsCount = 1;
    uint32_t slmTotalSize = 0;

    {
        revisionId = ProductHelper::get(productFamily)->getHwRevIdFromStepping(REVISION_A0, *hwInfo);
        INTERFACE_DESCRIPTOR_DATA idd = FamilyType::cmdInitInterfaceDescriptorData;
        EncodeDispatchKernel<FamilyType>::appendAdditionalIDDFields(&idd, *rootDeviceEnvironment, threadsCount, slmTotalSize, SlmPolicy::SlmPolicyNone);
        EXPECT_EQ(PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_128K, idd.getPreferredSlmAllocationSize());
    }
    {
        revisionId = ProductHelper::get(productFamily)->getHwRevIdFromStepping(REVISION_B, *hwInfo);
        INTERFACE_DESCRIPTOR_DATA idd = FamilyType::cmdInitInterfaceDescriptorData;
        EncodeDispatchKernel<FamilyType>::appendAdditionalIDDFields(&idd, *rootDeviceEnvironment, threadsCount, slmTotalSize, SlmPolicy::SlmPolicyNone);
        EXPECT_EQ(PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_0K, idd.getPreferredSlmAllocationSize());
    }
}

DG2TEST_F(CommandEncodeStatesDg2Test, givenNoWorkaroundNeededWhenSelectingPreferredSlmSizePerDssThenUseDssCount) {
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;
    using PREFERRED_SLM_ALLOCATION_SIZE = typename INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE;

    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &rootDeviceEnvironment = mockExecutionEnvironment.rootDeviceEnvironments[0];

    auto &hwInfo = *rootDeviceEnvironment->getMutableHardwareInfo();

    hwInfo.platform.usRevId = ProductHelper::get(productFamily)->getHwRevIdFromStepping(REVISION_B, hwInfo);
    hwInfo.gtSystemInfo.ThreadCount = 1024;
    hwInfo.gtSystemInfo.DualSubSliceCount = 8;
    hwInfo.gtSystemInfo.SubSliceCount = 2 * hwInfo.gtSystemInfo.DualSubSliceCount;

    {
        const uint32_t threadsPerThreadGroup = 7; // 18 groups will fit in one DSS
        const uint32_t slmSizePerThreadGroup = 2 * MemoryConstants::kiloByte;
        INTERFACE_DESCRIPTOR_DATA idd = FamilyType::cmdInitInterfaceDescriptorData;
        EncodeDispatchKernel<FamilyType>::appendAdditionalIDDFields(&idd, *rootDeviceEnvironment, threadsPerThreadGroup, slmSizePerThreadGroup, SlmPolicy::SlmPolicyLargeSlm);
        EXPECT_EQ(PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_64K, idd.getPreferredSlmAllocationSize());
    }
    {
        const uint32_t threadsPerThreadGroup = 8; // 16 groups will fit in one DSS
        const uint32_t slmSizePerThreadGroup = 2 * MemoryConstants::kiloByte;
        INTERFACE_DESCRIPTOR_DATA idd = FamilyType::cmdInitInterfaceDescriptorData;
        EncodeDispatchKernel<FamilyType>::appendAdditionalIDDFields(&idd, *rootDeviceEnvironment, threadsPerThreadGroup, slmSizePerThreadGroup, SlmPolicy::SlmPolicyLargeSlm);
        EXPECT_EQ(PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_32K, idd.getPreferredSlmAllocationSize());
    }
    {
        const uint32_t threadsPerThreadGroup = 9; // 14 groups will fit in one DSS
        const uint32_t slmSizePerThreadGroup = 2 * MemoryConstants::kiloByte;
        INTERFACE_DESCRIPTOR_DATA idd = FamilyType::cmdInitInterfaceDescriptorData;
        EncodeDispatchKernel<FamilyType>::appendAdditionalIDDFields(&idd, *rootDeviceEnvironment, threadsPerThreadGroup, slmSizePerThreadGroup, SlmPolicy::SlmPolicyLargeSlm);
        EXPECT_EQ(PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_32K, idd.getPreferredSlmAllocationSize());
    }
    {
        const uint32_t threadsPerThreadGroup = 50; // 2 groups will fit in one DSS
        const uint32_t slmSizePerThreadGroup = 16 * MemoryConstants::kiloByte;
        INTERFACE_DESCRIPTOR_DATA idd = FamilyType::cmdInitInterfaceDescriptorData;
        EncodeDispatchKernel<FamilyType>::appendAdditionalIDDFields(&idd, *rootDeviceEnvironment, threadsPerThreadGroup, slmSizePerThreadGroup, SlmPolicy::SlmPolicyLargeSlm);
        EXPECT_EQ(PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_32K, idd.getPreferredSlmAllocationSize());
    }
}
