/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/kernel/dispatch_kernel_encoder_interface.h"

#include "test.h"

#include "hw_cmds.h"

template <typename FamilyType>
struct PreferredSlmTestValues {
    uint32_t preferredSlmAllocationSizePerDss;
    typename FamilyType::INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE_PER_DSS expectedValueInIdd;
};

template <typename FamilyType>
void verifyPreferredSlmValues(std::vector<PreferredSlmTestValues<FamilyType>> valuesToTest, NEO::HardwareInfo &hwInfo) {
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;
    using PREFERRED_SLM_SIZE_OVERRIDE = typename INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_SIZE_OVERRIDE;
    using PREFERRED_SLM_ALLOCATION_SIZE_PER_DSS = typename INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE_PER_DSS;

    auto threadsPerDssCount = hwInfo.gtSystemInfo.ThreadCount / hwInfo.gtSystemInfo.SubSliceCount;
    uint32_t localWorkGroupsPerDssCounts[] = {1, 2, 4};

    INTERFACE_DESCRIPTOR_DATA idd = FamilyType::cmdInitInterfaceDescriptorData;
    EXPECT_EQ(0u, idd.getPreferredSlmAllocationSizePerDss());
    EXPECT_EQ(PREFERRED_SLM_SIZE_OVERRIDE::PREFERRED_SLM_SIZE_OVERRIDE_IS_DISABLED, idd.getPreferredSlmSizeOverride());

    const std::array<NEO::SlmPolicy, 3> slmPolicies = {
        NEO::SlmPolicy::SlmPolicyNone,
        NEO::SlmPolicy::SlmPolicyLargeSlm,
        NEO::SlmPolicy::SlmPolicyLargeData};

    for (auto localWorkGroupsPerDssCount : localWorkGroupsPerDssCounts) {
        for (auto &valueToTest : valuesToTest) {
            for (auto slmPolicy : slmPolicies) {
                auto threadsPerThreadGroup = threadsPerDssCount / localWorkGroupsPerDssCount;
                auto slmTotalSize = (slmPolicy == NEO::SlmPolicy::SlmPolicyLargeData)
                                        ? valueToTest.preferredSlmAllocationSizePerDss
                                        : valueToTest.preferredSlmAllocationSizePerDss / localWorkGroupsPerDssCount;

                NEO::EncodeDispatchKernel<FamilyType>::appendAdditionalIDDFields(&idd,
                                                                                 hwInfo,
                                                                                 threadsPerThreadGroup,
                                                                                 slmTotalSize,
                                                                                 slmPolicy);

                EXPECT_EQ(valueToTest.expectedValueInIdd, idd.getPreferredSlmAllocationSizePerDss());
                EXPECT_EQ(PREFERRED_SLM_SIZE_OVERRIDE::PREFERRED_SLM_SIZE_OVERRIDE_IS_ENABLED, idd.getPreferredSlmSizeOverride());
            }
        }
    }
}
