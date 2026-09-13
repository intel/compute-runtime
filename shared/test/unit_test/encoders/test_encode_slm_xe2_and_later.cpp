/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/encoders/test_encode_slm_xe2_and_later.h"

#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/kernel/grf_config.h"
#include "shared/test/common/test_macros/hw_test.h"

#include <algorithm>
#include <limits>

using namespace NEO;

HWTEST2_F(CommandEncodeStatesSlmTestXe2AndLater, GivenSlmSizePerSubsliceAboveAvailableSlmSizePerSubsliceWhenCallingEncodeSlmSizePerSubSliceThenPreferredSlmOfTheAvailableSizeIsProgrammed, IsAtLeastXe2HpgCore) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    using INTERFACE_DESCRIPTOR_DATA = typename DefaultWalkerType::InterfaceDescriptorType;

    auto &rootDeviceEnvironment = getRootDeviceEnvironment();
    auto &hwInfo = *rootDeviceEnvironment.getMutableHardwareInfo();
    const auto &releaseHelper = rootDeviceEnvironment.getReleaseHelper();

    auto encodePreferredSlm = [&](uint32_t slmTotalSizePerThreadGroup, uint32_t workloadThreadGroupCount, NEO::SlmPolicy slmPolicy) {
        auto idd = FamilyType::template getInitInterfaceDescriptor<INTERFACE_DESCRIPTOR_DATA>();
        NEO::EncodeSlmSizePerSubSliceArgs slmArgs{
            .threadsPerThreadGroup = 1,
            .workloadThreadGroupCount = workloadThreadGroupCount,
            .slmTotalSizePerThreadGroup = slmTotalSizePerThreadGroup,
            .grfCount = GrfConfig::defaultGrfNumber,
            .slmPolicy = slmPolicy};

        NEO::EncodeDispatchKernel<FamilyType>::encodeSlmSizePerSubSlice(&idd, rootDeviceEnvironment, slmArgs);
        return static_cast<uint32_t>(idd.getPreferredSlmAllocationSize());
    };

    auto preferredSlmValueFor = [&](uint32_t slmSizePerSubslice) {
        for (const auto &range : releaseHelper.getSizeToPreferredSlmValue()) {
            if (slmSizePerSubslice <= range.upperLimit) {
                return range.valueToProgram;
            }
        }
        return std::numeric_limits<uint32_t>::max();
    };

    const uint32_t saturatingWorkloadThreadGroupCount = hwInfo.gtSystemInfo.ThreadCount;
    uint32_t previousProgrammedValue = 0;

    for (uint32_t availableSlmSizeKb : {16u, 32u, 64u, 96u}) {
        hwInfo.gtSystemInfo.SLMSizeInKb = availableSlmSizeKb;

        const uint32_t availableSlmSizePerSubslice = rootDeviceEnvironment.getProductHelper().getAvailableSlmSizePerSubslice(rootDeviceEnvironment) * MemoryConstants::kiloByte;
        ASSERT_EQ(static_cast<uint32_t>(availableSlmSizeKb * MemoryConstants::kiloByte), availableSlmSizePerSubslice);

        const uint32_t expectedValue = preferredSlmValueFor(availableSlmSizePerSubslice);

        const uint32_t justAboveAvailable = availableSlmSizePerSubslice + 1;
        ASSERT_GT(NEO::EncodeDispatchKernel<FamilyType>::alignSlmSizePerThreadGroup(justAboveAvailable, releaseHelper), availableSlmSizePerSubslice);

        EXPECT_EQ(expectedValue, encodePreferredSlm(justAboveAvailable, 1, NEO::SlmPolicy::slmPolicyLargeData))
            << ", availableSlmSizeKb: " << availableSlmSizeKb;

        ASSERT_EQ(availableSlmSizePerSubslice, NEO::EncodeDispatchKernel<FamilyType>::alignSlmSizePerThreadGroup(availableSlmSizePerSubslice, releaseHelper));

        NEO::EncodeSlmSizePerSubSliceArgs slmArgs{
            .threadsPerThreadGroup = 1,
            .workloadThreadGroupCount = saturatingWorkloadThreadGroupCount,
            .slmTotalSizePerThreadGroup = availableSlmSizePerSubslice,
            .grfCount = GrfConfig::defaultGrfNumber,
            .slmPolicy = NEO::SlmPolicy::slmPolicyLargeSlm};
        const auto threadGroupCountSharingSubsliceSlm = NEO::EncodeDispatchKernel<FamilyType>::calculateThreadGroupCountSharingSubsliceSlm(rootDeviceEnvironment, slmArgs);
        ASSERT_LE(2u, threadGroupCountSharingSubsliceSlm) << "a single thread group per subslice cannot exceed the available slm by sharing it";

        EXPECT_EQ(expectedValue, encodePreferredSlm(availableSlmSizePerSubslice, saturatingWorkloadThreadGroupCount, NEO::SlmPolicy::slmPolicyLargeSlm))
            << ", availableSlmSizeKb: " << availableSlmSizeKb
            << ", threadGroupCountSharingSubsliceSlm: " << threadGroupCountSharingSubsliceSlm;
        EXPECT_EQ(expectedValue, encodePreferredSlm(availableSlmSizePerSubslice, saturatingWorkloadThreadGroupCount, NEO::SlmPolicy::slmPolicyNone))
            << ", availableSlmSizeKb: " << availableSlmSizeKb
            << ", threadGroupCountSharingSubsliceSlm: " << threadGroupCountSharingSubsliceSlm;

        EXPECT_LT(previousProgrammedValue, expectedValue) << ", availableSlmSizeKb: " << availableSlmSizeKb;
        previousProgrammedValue = expectedValue;
    }
}

HWTEST2_F(CommandEncodeStatesSlmTestXe2AndLater, GivenGrfCountLimitedSubsliceResidencyWhenCallingEncodeSlmSizePerSubSliceThenSameSlmIsEncodedAsForAnEquallyLimitedWorkload, IsAtLeastXe2HpgCore) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    using INTERFACE_DESCRIPTOR_DATA = typename DefaultWalkerType::InterfaceDescriptorType;

    auto &rootDeviceEnvironment = getRootDeviceEnvironment();
    auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();
    const auto grfCounts = rootDeviceEnvironment.getProductHelper().getSupportedNumGrfs(rootDeviceEnvironment.getReleaseHelper());

    ASSERT_TRUE(std::is_sorted(grfCounts.begin(), grfCounts.end()));
    const uint32_t smallestGrfCount = *grfCounts.begin();

    constexpr uint32_t threadsPerThreadGroup = 1;
    constexpr uint32_t slmTotalSizePerThreadGroup = MemoryConstants::kiloByte;
    const uint32_t saturatingWorkloadThreadGroupCount = hwInfo.gtSystemInfo.ThreadCount;

    auto encodePreferredSlm = [&](uint32_t grfCount, uint32_t workloadThreadGroupCount) {
        auto idd = FamilyType::template getInitInterfaceDescriptor<INTERFACE_DESCRIPTOR_DATA>();
        NEO::EncodeSlmSizePerSubSliceArgs slmArgs{
            .threadsPerThreadGroup = threadsPerThreadGroup,
            .workloadThreadGroupCount = workloadThreadGroupCount,
            .slmTotalSizePerThreadGroup = slmTotalSizePerThreadGroup,
            .grfCount = grfCount,
            .slmPolicy = NEO::SlmPolicy::slmPolicyLargeSlm};

        NEO::EncodeDispatchKernel<FamilyType>::encodeSlmSizePerSubSlice(&idd, rootDeviceEnvironment, slmArgs);
        return static_cast<uint32_t>(idd.getPreferredSlmAllocationSize());
    };

    for (auto grfCount : grfCounts) {
        const auto maxConcurrentThreadCountPerSubslice = NEO::EncodeDispatchKernel<FamilyType>::getMaxConcurrentThreadCountPerSubslice(rootDeviceEnvironment, grfCount);
        const auto workloadThreadGroupCountFittingInSubslices = maxConcurrentThreadCountPerSubslice * hwInfo.gtSystemInfo.SubSliceCount;

        EXPECT_EQ(encodePreferredSlm(smallestGrfCount, workloadThreadGroupCountFittingInSubslices),
                  encodePreferredSlm(grfCount, saturatingWorkloadThreadGroupCount))
            << ", grfCount: " << grfCount
            << ", maxConcurrentThreadCountPerSubslice: " << maxConcurrentThreadCountPerSubslice;
    }
}

HWTEST2_F(CommandEncodeStatesSlmTestXe2AndLater, GivenSlmPoliciesWhenCallingEncodeSlmSizePerSubSliceThenOnlyLargeDataIgnoresThreadGroupCountSharingSubsliceSlm, IsAtLeastXe2HpgCore) {
    verifySlmPolicies<FamilyType>();
}

HWTEST2_F(CommandEncodeStatesSlmTestXe2AndLater, GivenSlmSizePerSubsliceAtPreferredSlmRangeLimitsWhenCallingEncodeSlmSizePerSubSliceThenProgramsValueOfTheMatchingRange, IsAtLeastXe2HpgCore) {
    verifyPreferredSlmValueRanges<FamilyType>();
}
