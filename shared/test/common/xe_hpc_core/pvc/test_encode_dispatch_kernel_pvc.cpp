/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/variable_backup.h"

#include "test.h"

#include "hw_cmds.h"

using namespace NEO;

using CommandEncodeStatesPvcTest = ::testing::Test;

PVCTEST_F(CommandEncodeStatesPvcTest, GivenSmallSlmTotalSizesWhenSetAdditionalInfoIsCalledThenCorrectValuesAreSet) {
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;
    using PREFERRED_SLM_SIZE_OVERRIDE = typename INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_SIZE_OVERRIDE;
    using PREFERRED_SLM_ALLOCATION_SIZE_PER_DSS = typename INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE_PER_DSS;

    HardwareInfo hwInfo = *defaultHwInfo;
    uint32_t threadsCount = 1;
    uint32_t slmTotalSize = 0;

    struct {
        unsigned short revisionId;
        bool isWaRequired;
    } revisionsToTest[] = {
        {0x0, true},
        {0x1, true},
        {0x2, true},
        {0x41, true},
        {0x3, false},
        {0x9d, false},
    };

    for (auto &revisionToTest : revisionsToTest) {
        hwInfo.platform.usRevId = revisionToTest.revisionId;
        INTERFACE_DESCRIPTOR_DATA idd = FamilyType::cmdInitInterfaceDescriptorData;
        EncodeDispatchKernel<FamilyType>::appendAdditionalIDDFields(&idd, hwInfo, threadsCount, slmTotalSize, SlmPolicy::SlmPolicyNone);
        if (revisionToTest.isWaRequired) {
            EXPECT_EQ(PREFERRED_SLM_ALLOCATION_SIZE_PER_DSS::PREFERRED_SLM_SIZE_IS_16K, idd.getPreferredSlmAllocationSizePerDss());
        } else {
            EXPECT_EQ(PREFERRED_SLM_ALLOCATION_SIZE_PER_DSS::PREFERRED_SLM_SIZE_IS_0K, idd.getPreferredSlmAllocationSizePerDss());
        }
    }
}
