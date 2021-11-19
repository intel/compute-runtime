/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/variable_backup.h"

#include "test.h"

#include "hw_cmds.h"

using namespace NEO;

using CommandEncodeStatesDg2Test = ::testing::Test;

DG2TEST_F(CommandEncodeStatesDg2Test, GivenSmallSlmTotalSizesWhenSetAdditionalInfoIsCalledThenCorrectValuesAreSet) {
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;
    using PREFERRED_SLM_SIZE_OVERRIDE = typename INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_SIZE_OVERRIDE;
    using PREFERRED_SLM_ALLOCATION_SIZE_PER_DSS = typename INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE_PER_DSS;

    VariableBackup<unsigned short> revisionId(&defaultHwInfo->platform.usRevId);
    uint32_t threadsCount = 1;
    uint32_t slmTotalSize = 0;

    {
        revisionId = HwInfoConfig::get(productFamily)->getHwRevIdFromStepping(REVISION_A0, *defaultHwInfo);
        INTERFACE_DESCRIPTOR_DATA idd = FamilyType::cmdInitInterfaceDescriptorData;
        EncodeDispatchKernel<FamilyType>::appendAdditionalIDDFields(&idd, *defaultHwInfo, threadsCount, slmTotalSize, SlmPolicy::SlmPolicyNone);
        EXPECT_EQ(PREFERRED_SLM_ALLOCATION_SIZE_PER_DSS::PREFERRED_SLM_SIZE_IS_128K, idd.getPreferredSlmAllocationSizePerDss());
    }
    {
        revisionId = HwInfoConfig::get(productFamily)->getHwRevIdFromStepping(REVISION_B, *defaultHwInfo);
        INTERFACE_DESCRIPTOR_DATA idd = FamilyType::cmdInitInterfaceDescriptorData;
        EncodeDispatchKernel<FamilyType>::appendAdditionalIDDFields(&idd, *defaultHwInfo, threadsCount, slmTotalSize, SlmPolicy::SlmPolicyNone);
        EXPECT_EQ(PREFERRED_SLM_ALLOCATION_SIZE_PER_DSS::PREFERRED_SLM_SIZE_IS_0K, idd.getPreferredSlmAllocationSizePerDss());
    }
}

DG2TEST_F(CommandEncodeStatesDg2Test, givenNoWorkaroundNeededWhenSelectingPreferredSlmSizePerDssThenUseDssCount) {
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;
    using PREFERRED_SLM_ALLOCATION_SIZE_PER_DSS = typename INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE_PER_DSS;

    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.platform.usRevId = HwInfoConfig::get(productFamily)->getHwRevIdFromStepping(REVISION_B, *defaultHwInfo);
    hwInfo.gtSystemInfo.ThreadCount = 1024;
    hwInfo.gtSystemInfo.DualSubSliceCount = 8;
    hwInfo.gtSystemInfo.SubSliceCount = 2 * hwInfo.gtSystemInfo.DualSubSliceCount;

    {
        const uint32_t threadsPerThreadGroup = 7; // 18 groups will fit in one DSS
        const uint32_t slmSizePerThreadGroup = 2 * MemoryConstants::kiloByte;
        INTERFACE_DESCRIPTOR_DATA idd = FamilyType::cmdInitInterfaceDescriptorData;
        EncodeDispatchKernel<FamilyType>::appendAdditionalIDDFields(&idd, hwInfo, threadsPerThreadGroup, slmSizePerThreadGroup, SlmPolicy::SlmPolicyLargeSlm);
        EXPECT_EQ(PREFERRED_SLM_ALLOCATION_SIZE_PER_DSS::PREFERRED_SLM_SIZE_IS_64K, idd.getPreferredSlmAllocationSizePerDss());
    }
    {
        const uint32_t threadsPerThreadGroup = 8; // 16 groups will fit in one DSS
        const uint32_t slmSizePerThreadGroup = 2 * MemoryConstants::kiloByte;
        INTERFACE_DESCRIPTOR_DATA idd = FamilyType::cmdInitInterfaceDescriptorData;
        EncodeDispatchKernel<FamilyType>::appendAdditionalIDDFields(&idd, hwInfo, threadsPerThreadGroup, slmSizePerThreadGroup, SlmPolicy::SlmPolicyLargeSlm);
        EXPECT_EQ(PREFERRED_SLM_ALLOCATION_SIZE_PER_DSS::PREFERRED_SLM_SIZE_IS_32K, idd.getPreferredSlmAllocationSizePerDss());
    }
    {
        const uint32_t threadsPerThreadGroup = 9; // 14 groups will fit in one DSS
        const uint32_t slmSizePerThreadGroup = 2 * MemoryConstants::kiloByte;
        INTERFACE_DESCRIPTOR_DATA idd = FamilyType::cmdInitInterfaceDescriptorData;
        EncodeDispatchKernel<FamilyType>::appendAdditionalIDDFields(&idd, hwInfo, threadsPerThreadGroup, slmSizePerThreadGroup, SlmPolicy::SlmPolicyLargeSlm);
        EXPECT_EQ(PREFERRED_SLM_ALLOCATION_SIZE_PER_DSS::PREFERRED_SLM_SIZE_IS_32K, idd.getPreferredSlmAllocationSizePerDss());
    }
    {
        const uint32_t threadsPerThreadGroup = 50; // 2 groups will fit in one DSS
        const uint32_t slmSizePerThreadGroup = 16 * MemoryConstants::kiloByte;
        INTERFACE_DESCRIPTOR_DATA idd = FamilyType::cmdInitInterfaceDescriptorData;
        EncodeDispatchKernel<FamilyType>::appendAdditionalIDDFields(&idd, hwInfo, threadsPerThreadGroup, slmSizePerThreadGroup, SlmPolicy::SlmPolicyLargeSlm);
        EXPECT_EQ(PREFERRED_SLM_ALLOCATION_SIZE_PER_DSS::PREFERRED_SLM_SIZE_IS_32K, idd.getPreferredSlmAllocationSizePerDss());
    }
}
