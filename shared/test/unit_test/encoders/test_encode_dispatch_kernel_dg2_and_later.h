/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/kernel/dispatch_kernel_encoder_interface.h"
#include "shared/test/common/test_macros/test.h"

template <typename FamilyType>
struct PreferredSlmTestValues {

    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    using INTERFACE_DESCRIPTOR_DATA = typename DefaultWalkerType::InterfaceDescriptorType;
    using PREFERRED_SLM_ALLOCATION_SIZE = typename INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE;

    uint32_t preferredSlmAllocationSizePerDss;
    PREFERRED_SLM_ALLOCATION_SIZE expectedValueInIdd;
};

template <typename FamilyType>
void verifyPreferredSlmValues(std::vector<PreferredSlmTestValues<FamilyType>> valuesToTest, const NEO::RootDeviceEnvironment &rootDeviceEnvironment) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    using INTERFACE_DESCRIPTOR_DATA = typename DefaultWalkerType::InterfaceDescriptorType;

    auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();
    auto threadsPerDssCount = hwInfo.gtSystemInfo.ThreadCount / hwInfo.gtSystemInfo.SubSliceCount;
    uint32_t localWorkGroupsPerDssCounts[] = {1, 2, 4};

    auto idd = FamilyType::template getInitInterfaceDescriptor<INTERFACE_DESCRIPTOR_DATA>();

    EXPECT_EQ(0u, idd.getPreferredSlmAllocationSize());

    const std::array<NEO::SlmPolicy, 3> slmPolicies = {
        NEO::SlmPolicy::slmPolicyNone,
        NEO::SlmPolicy::slmPolicyLargeSlm,
        NEO::SlmPolicy::slmPolicyLargeData};

    for (auto localWorkGroupsPerDssCount : localWorkGroupsPerDssCounts) {
        for (auto &valueToTest : valuesToTest) {
            for (auto slmPolicy : slmPolicies) {
                auto threadsPerThreadGroup = threadsPerDssCount / localWorkGroupsPerDssCount;
                auto slmTotalSize = (slmPolicy == NEO::SlmPolicy::slmPolicyLargeData)
                                        ? valueToTest.preferredSlmAllocationSizePerDss
                                        : valueToTest.preferredSlmAllocationSizePerDss / localWorkGroupsPerDssCount;

                NEO::EncodeDispatchKernel<FamilyType>::setupPreferredSlmSize(&idd,
                                                                             rootDeviceEnvironment,
                                                                             threadsPerThreadGroup,
                                                                             slmTotalSize,
                                                                             slmPolicy);

                EXPECT_EQ(valueToTest.expectedValueInIdd, idd.getPreferredSlmAllocationSize());
            }
        }
    }
}
