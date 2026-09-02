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

#include <limits>

using namespace NEO;

HWTEST2_F(CommandEncodeStatesSlmTestXe2AndLater, GivenSlmTotalSizeAboveActualHwLimitWhenCallingEncodeSlmSizePerSubSliceThenPreferredSlmIsClampedToActualHwSlmSizeKb, IsAtLeastXe2HpgCore) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    using INTERFACE_DESCRIPTOR_DATA = typename DefaultWalkerType::InterfaceDescriptorType;

    auto &rootDeviceEnvironment = pDevice->getRootDeviceEnvironment();
    auto &hwInfo = *rootDeviceEnvironment.getMutableHardwareInfo();

    hwInfo.gtSystemInfo.SLMSizeInKb = 32;

    const uint32_t actualHwSlmSizeKb = rootDeviceEnvironment.getProductHelper().getAvailableSlmSizePerSubslice(rootDeviceEnvironment);
    const uint32_t slmAtLimit = actualHwSlmSizeKb * MemoryConstants::kiloByte;
    const uint32_t slmAboveLimit = slmAtLimit + 1;

    auto idd = FamilyType::template getInitInterfaceDescriptor<INTERFACE_DESCRIPTOR_DATA>();

    NEO::EncodeSlmSizePerSubSliceArgs slmArgs{
        .threadsPerThreadGroup = 1,
        .workloadThreadGroupCount = 128,
        .slmTotalSizePerThreadGroup = slmAtLimit,
        .grfCount = GrfConfig::defaultGrfNumber,
        .slmPolicy = NEO::SlmPolicy::slmPolicyLargeData};

    NEO::EncodeDispatchKernel<FamilyType>::encodeSlmSizePerSubSlice(&idd, rootDeviceEnvironment, slmArgs);
    const auto valueAtLimit = idd.getPreferredSlmAllocationSize();

    slmArgs.slmTotalSizePerThreadGroup = slmAboveLimit;

    NEO::EncodeDispatchKernel<FamilyType>::encodeSlmSizePerSubSlice(&idd, rootDeviceEnvironment, slmArgs);

    EXPECT_EQ(valueAtLimit, idd.getPreferredSlmAllocationSize());
}

HWTEST2_F(CommandEncodeStatesSlmTestXe2AndLater, GivenLargestDispatchableThreadGroupWhenGettingMaxConcurrentThreadCountPerSubsliceThenWholeThreadGroupIsResident, IsAtLeastXe2HpgCore) {
    auto &rootDeviceEnvironment = pDevice->getRootDeviceEnvironment();
    auto &gfxCoreHelper = rootDeviceEnvironment.getHelper<GfxCoreHelper>();
    const auto grfCounts = rootDeviceEnvironment.getProductHelper().getSupportedNumGrfs(rootDeviceEnvironment.getReleaseHelper());

    for (auto grfCount : grfCounts) {
        auto maxConcurrentThreadCountPerSubslice = this->getMaxConcurrentThreadCountPerSubslice(rootDeviceEnvironment, grfCount);
        ASSERT_NE(0u, maxConcurrentThreadCountPerSubslice) << ", grfCount: " << grfCount;

        for (auto simd : {16u, 32u}) {
            auto maxThreadsPerThreadGroup = gfxCoreHelper.calculateNumThreadsPerThreadGroup(simd, CommonConstants::maxWorkgroupSize, grfCount, rootDeviceEnvironment);

            EXPECT_LE(maxThreadsPerThreadGroup, maxConcurrentThreadCountPerSubslice)
                << ", grfCount: " << grfCount
                << ", simd: " << simd
                << ", maxThreadsPerThreadGroup: " << maxThreadsPerThreadGroup
                << ", maxConcurrentThreadCountPerSubslice: " << maxConcurrentThreadCountPerSubslice;
        }
    }
}

HWTEST2_F(CommandEncodeStatesSlmTestXe2AndLater, GivenIncreasingGrfCountWhenCallingEncodeSlmSizePerSubSliceThenFewerThreadGroupsShareSubsliceSlm, IsAtLeastXe2HpgCore) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    using INTERFACE_DESCRIPTOR_DATA = typename DefaultWalkerType::InterfaceDescriptorType;

    auto &rootDeviceEnvironment = pDevice->getRootDeviceEnvironment();
    auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();
    const auto grfCounts = rootDeviceEnvironment.getProductHelper().getSupportedNumGrfs(rootDeviceEnvironment.getReleaseHelper());

    constexpr uint32_t threadsPerThreadGroup = 1;
    constexpr uint32_t slmTotalSizePerThreadGroup = MemoryConstants::kiloByte;
    const uint32_t unlimitedWorkloadThreadGroupCount = hwInfo.gtSystemInfo.ThreadCount;

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

    const uint32_t smallestGrfCount = *grfCounts.begin();
    const uint32_t largestGrfCount = *(grfCounts.end() - 1);

    uint32_t previousPreferredSlm = std::numeric_limits<uint32_t>::max();
    for (auto grfCount : grfCounts) {
        auto preferredSlm = encodePreferredSlm(grfCount, unlimitedWorkloadThreadGroupCount);

        EXPECT_GE(previousPreferredSlm, preferredSlm) << ", grfCount: " << grfCount;

        auto maxConcurrentThreadCountPerSubslice = this->getMaxConcurrentThreadCountPerSubslice(rootDeviceEnvironment, grfCount);
        auto workloadThreadGroupCountFittingInSubslices = maxConcurrentThreadCountPerSubslice * hwInfo.gtSystemInfo.SubSliceCount;
        EXPECT_EQ(encodePreferredSlm(smallestGrfCount, workloadThreadGroupCountFittingInSubslices), preferredSlm)
            << ", grfCount: " << grfCount
            << ", maxConcurrentThreadCountPerSubslice: " << maxConcurrentThreadCountPerSubslice;

        previousPreferredSlm = preferredSlm;
    }

    EXPECT_LT(this->getMaxConcurrentThreadCountPerSubslice(rootDeviceEnvironment, largestGrfCount),
              this->getMaxConcurrentThreadCountPerSubslice(rootDeviceEnvironment, smallestGrfCount));
    EXPECT_LT(encodePreferredSlm(largestGrfCount, unlimitedWorkloadThreadGroupCount),
              encodePreferredSlm(smallestGrfCount, unlimitedWorkloadThreadGroupCount));
}

HWTEST2_F(CommandEncodeStatesSlmTestXe2AndLater, GivenSlmEdgeValuesWhenCallingEncodeSlmSizePerSubSliceThenProgramsExpectedPreferredSlm, IsBMG) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    using INTERFACE_DESCRIPTOR_DATA = typename DefaultWalkerType::InterfaceDescriptorType;
    using PREFERRED_SLM_ALLOCATION_SIZE = typename INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE;
    using SHARED_LOCAL_MEMORY_SIZE = typename INTERFACE_DESCRIPTOR_DATA::SHARED_LOCAL_MEMORY_SIZE;

    // clang-format off
    const SlmTestHelper<FamilyType> slmHelperBmg{
        .programmableSlmSizesPerThreadGroup = {
            {0 * MemoryConstants::kiloByte, SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_0K},
            {1 * MemoryConstants::kiloByte, SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_1K},
            {2 * MemoryConstants::kiloByte, SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_2K},
            {4 * MemoryConstants::kiloByte, SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_4K},
            {8 * MemoryConstants::kiloByte, SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_8K},
            {16 * MemoryConstants::kiloByte, SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_16K},
            {24 * MemoryConstants::kiloByte, SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_24K},
            {32 * MemoryConstants::kiloByte, SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_32K},
            {48 * MemoryConstants::kiloByte, SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_48K},
            {64 * MemoryConstants::kiloByte, SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_64K},
            {96 * MemoryConstants::kiloByte, SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_96K},
            {128 * MemoryConstants::kiloByte, SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_128K}},
        .programmablePreferredSlmSizesPerSubslice = {
            {0 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_0K},
            {16 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_16K},
            {32 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_32K},
            {64 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_64K},
            {96 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_96K},
            {128 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_128K},
            {std::numeric_limits<uint32_t>::max(), PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_160K}}
    };
    // clang-format on

    verifyPreferredSlmValues<FamilyType>(slmHelperBmg, pDevice->getRootDeviceEnvironment());
}

HWTEST2_F(CommandEncodeStatesSlmTestXe2AndLater, GivenSlmEdgeValuesWhenCallingEncodeSlmSizePerSubSliceThenProgramsExpectedPreferredSlm, IsLNL) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    using INTERFACE_DESCRIPTOR_DATA = typename DefaultWalkerType::InterfaceDescriptorType;
    using PREFERRED_SLM_ALLOCATION_SIZE = typename INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE;
    using SHARED_LOCAL_MEMORY_SIZE = typename INTERFACE_DESCRIPTOR_DATA::SHARED_LOCAL_MEMORY_SIZE;

    // clang-format off
    const SlmTestHelper<FamilyType> slmHelperLnl{
        .programmableSlmSizesPerThreadGroup = {
            {0 * MemoryConstants::kiloByte, SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_0K},
            {1 * MemoryConstants::kiloByte, SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_1K},
            {2 * MemoryConstants::kiloByte, SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_2K},
            {4 * MemoryConstants::kiloByte, SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_4K},
            {8 * MemoryConstants::kiloByte, SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_8K},
            {16 * MemoryConstants::kiloByte, SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_16K},
            {24 * MemoryConstants::kiloByte, SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_24K},
            {32 * MemoryConstants::kiloByte, SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_32K},
            {48 * MemoryConstants::kiloByte, SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_48K},
            {64 * MemoryConstants::kiloByte, SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_64K},
            {96 * MemoryConstants::kiloByte, SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_96K},
            {128 * MemoryConstants::kiloByte, SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_128K}},
        .programmablePreferredSlmSizesPerSubslice = {
            {0 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_0K},
            {16 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_16K},
            {32 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_32K},
            {64 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_64K},
            {96 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_96K},
            {std::numeric_limits<uint32_t>::max(), PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_128K}}
    };
    // clang-format on

    verifyPreferredSlmValues<FamilyType>(slmHelperLnl, pDevice->getRootDeviceEnvironment());
}

HWTEST2_F(CommandEncodeStatesSlmTestXe2AndLater, GivenSlmEdgeValuesWhenCallingEncodeSlmSizePerSubSliceThenProgramsExpectedPreferredSlm, IsXe3Core) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    using INTERFACE_DESCRIPTOR_DATA = typename DefaultWalkerType::InterfaceDescriptorType;
    using PREFERRED_SLM_ALLOCATION_SIZE = typename INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE;
    using SHARED_LOCAL_MEMORY_SIZE = typename INTERFACE_DESCRIPTOR_DATA::SHARED_LOCAL_MEMORY_SIZE;

    // clang-format off
    const SlmTestHelper<FamilyType> slmHelperXe3Core{
        .programmableSlmSizesPerThreadGroup = {
            {0 * MemoryConstants::kiloByte, SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_0K},
            {1 * MemoryConstants::kiloByte, SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_1K},
            {2 * MemoryConstants::kiloByte, SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_2K},
            {4 * MemoryConstants::kiloByte, SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_4K},
            {8 * MemoryConstants::kiloByte, SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_8K},
            {16 * MemoryConstants::kiloByte, SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_16K},
            {24 * MemoryConstants::kiloByte, SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_24K},
            {32 * MemoryConstants::kiloByte, SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_32K},
            {48 * MemoryConstants::kiloByte, SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_48K},
            {64 * MemoryConstants::kiloByte, SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_64K},
            {96 * MemoryConstants::kiloByte, SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_96K},
            {128 * MemoryConstants::kiloByte, SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_128K}},
        .programmablePreferredSlmSizesPerSubslice = {
            {0 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_0K},
            {16 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_16K},
            {32 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_32K},
            {64 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_64K},
            {96 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_96K},
            {128 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_128K},
            {160 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_160K},
            {std::numeric_limits<uint32_t>::max(), PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_192K}}
    };
    // clang-format on

    verifyPreferredSlmValues<FamilyType>(slmHelperXe3Core, pDevice->getRootDeviceEnvironment());
}

HWTEST2_F(CommandEncodeStatesSlmTestXe2AndLater, GivenSlmEdgeValuesWhenCallingEncodeSlmSizePerSubSliceThenProgramsExpectedPreferredSlm, IsXe3pLpg) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    using INTERFACE_DESCRIPTOR_DATA = typename DefaultWalkerType::InterfaceDescriptorType;
    using PREFERRED_SLM_ALLOCATION_SIZE = typename INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE;
    using SHARED_LOCAL_MEMORY_SIZE = typename INTERFACE_DESCRIPTOR_DATA::SHARED_LOCAL_MEMORY_SIZE;

    // clang-format off
    const SlmTestHelper<FamilyType> slmHelperXe3pCore{
        .programmableSlmSizesPerThreadGroup = {
            {0 * MemoryConstants::kiloByte, SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_0K},
            {1 * MemoryConstants::kiloByte, SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_1K},
            {2 * MemoryConstants::kiloByte, SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_2K},
            {3 * MemoryConstants::kiloByte, SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_3K},
            {4 * MemoryConstants::kiloByte, SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_4K},
            {5 * MemoryConstants::kiloByte, SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_5K},
            {6 * MemoryConstants::kiloByte, SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_6K},
            {7 * MemoryConstants::kiloByte, SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_7K},
            {8 * MemoryConstants::kiloByte, SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_8K},
            {9 * MemoryConstants::kiloByte, SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_9K},
            {10 * MemoryConstants::kiloByte, SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_10K},
            {11 * MemoryConstants::kiloByte, SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_11K},
            {12 * MemoryConstants::kiloByte, SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_12K},
            {13 * MemoryConstants::kiloByte, SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_13K},
            {14 * MemoryConstants::kiloByte, SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_14K},
            {15 * MemoryConstants::kiloByte, SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_15K},
            {16 * MemoryConstants::kiloByte, SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_16K},
            {24 * MemoryConstants::kiloByte, SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_24K},
            {32 * MemoryConstants::kiloByte, SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_32K},
            {48 * MemoryConstants::kiloByte, SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_48K},
            {64 * MemoryConstants::kiloByte, SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_64K},
            {96 * MemoryConstants::kiloByte, SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_96K},
            {128 * MemoryConstants::kiloByte, SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_128K},
            {192 * MemoryConstants::kiloByte, SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_192K}},
        .programmablePreferredSlmSizesPerSubslice = {
            {0 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_0K},
            {16 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_16K},
            {32 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_32K},
            {64 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_64K},
            {96 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_96K},
            {128 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_128K},
            {160 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_160K},
            {std::numeric_limits<uint32_t>::max(), PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_192K}}
    };
    // clang-format on

    verifyPreferredSlmValues<FamilyType>(slmHelperXe3pCore, pDevice->getRootDeviceEnvironment());
}

HWTEST2_F(CommandEncodeStatesSlmTestXe2AndLater, GivenSlmEdgeValuesWhenCallingEncodeSlmSizePerSubSliceThenProgramsExpectedPreferredSlm, IsCRI) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    using INTERFACE_DESCRIPTOR_DATA = typename DefaultWalkerType::InterfaceDescriptorType;
    using PREFERRED_SLM_ALLOCATION_SIZE = typename INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE;
    using SHARED_LOCAL_MEMORY_SIZE = typename INTERFACE_DESCRIPTOR_DATA::SHARED_LOCAL_MEMORY_SIZE;

    // clang-format off
    const SlmTestHelper<FamilyType> slmHelperCri{
        .programmableSlmSizesPerThreadGroup = {
            {0 * MemoryConstants::kiloByte, SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_0K},
            {1 * MemoryConstants::kiloByte, SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_1K},
            {2 * MemoryConstants::kiloByte, SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_2K},
            {3 * MemoryConstants::kiloByte, SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_3K},
            {4 * MemoryConstants::kiloByte, SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_4K},
            {5 * MemoryConstants::kiloByte, SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_5K},
            {6 * MemoryConstants::kiloByte, SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_6K},
            {7 * MemoryConstants::kiloByte, SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_7K},
            {8 * MemoryConstants::kiloByte, SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_8K},
            {9 * MemoryConstants::kiloByte, SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_9K},
            {10 * MemoryConstants::kiloByte, SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_10K},
            {11 * MemoryConstants::kiloByte, SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_11K},
            {12 * MemoryConstants::kiloByte, SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_12K},
            {13 * MemoryConstants::kiloByte, SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_13K},
            {14 * MemoryConstants::kiloByte, SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_14K},
            {15 * MemoryConstants::kiloByte, SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_15K},
            {16 * MemoryConstants::kiloByte, SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_16K},
            {24 * MemoryConstants::kiloByte, SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_24K},
            {32 * MemoryConstants::kiloByte, SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_32K},
            {48 * MemoryConstants::kiloByte, SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_48K},
            {64 * MemoryConstants::kiloByte, SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_64K},
            {96 * MemoryConstants::kiloByte, SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_96K},
            {128 * MemoryConstants::kiloByte, SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_128K},
            {192 * MemoryConstants::kiloByte, SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_192K},
            {256 * MemoryConstants::kiloByte, SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_256K},
            {320 * MemoryConstants::kiloByte, SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_320K},
            {384 * MemoryConstants::kiloByte, SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_384K}},
        .programmablePreferredSlmSizesPerSubslice = {
            {0 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_0K},
            {16 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_16K},
            {32 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_32K},
            {64 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_64K},
            {96 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_96K},
            {128 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_128K},
            {160 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_160K},
            {192 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_192K},
            {256 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_256K},
            {320 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_320K},
            {std::numeric_limits<uint32_t>::max(), PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_384K}},
    };
    // clang-format on

    verifyPreferredSlmValues<FamilyType>(slmHelperCri, pDevice->getRootDeviceEnvironment());
}
