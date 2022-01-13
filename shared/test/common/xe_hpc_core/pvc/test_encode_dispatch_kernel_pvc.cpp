/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/test_macros/test.h"

#include "hw_cmds.h"

using namespace NEO;

using CommandEncodeStatesPvcTest = ::testing::Test;

PVCTEST_F(CommandEncodeStatesPvcTest, GivenSmallSlmTotalSizesWhenSetAdditionalInfoIsCalledThenCorrectValuesAreSet) {
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;
    using PREFERRED_SLM_ALLOCATION_SIZE = typename INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE;

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
            EXPECT_EQ(PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_16K, idd.getPreferredSlmAllocationSize());
        } else {
            EXPECT_EQ(PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_0K, idd.getPreferredSlmAllocationSize());
        }
    }
}
