/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/kernel/dispatch_kernel_encoder_interface.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/release_helpers/release_helper/release_helper.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/unit_test/fixtures/command_container_fixture.h"

#include <algorithm>
#include <array>
#include <vector>

using namespace NEO;

class CommandEncodeStatesSlmTestXe2AndLater
    : public CommandEncodeStatesFixture,
      public ::testing::Test {
  public:
    void SetUp() override {
        CommandEncodeStatesFixture::setUp();
    }

    void TearDown() override {
        CommandEncodeStatesFixture::tearDown();
    }

    template <typename FamilyType>
    struct PreferredSlmSizeValues {
        using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
        using INTERFACE_DESCRIPTOR_DATA = typename DefaultWalkerType::InterfaceDescriptorType;
        using PREFERRED_SLM_ALLOCATION_SIZE = typename INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE;
        uint32_t slmSize;
        PREFERRED_SLM_ALLOCATION_SIZE programmableValue;
    };
    template <typename FamilyType>
    struct SharedLocalMemorySizeValues {
        using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
        using INTERFACE_DESCRIPTOR_DATA = typename DefaultWalkerType::InterfaceDescriptorType;
        using SHARED_LOCAL_MEMORY_SIZE = typename INTERFACE_DESCRIPTOR_DATA::SHARED_LOCAL_MEMORY_SIZE;
        uint32_t slmSize;
        SHARED_LOCAL_MEMORY_SIZE programmableValue;
    };
    template <typename FamilyType>
    struct SlmTestHelper {
        std::vector<SharedLocalMemorySizeValues<FamilyType>> programmableSlmSizesPerThreadGroup;
        std::vector<PreferredSlmSizeValues<FamilyType>> programmablePreferredSlmSizesPerSubslice;
    };

    uint32_t getMaxConcurrentThreadCountPerSubslice(const RootDeviceEnvironment &rootDeviceEnvironment, uint32_t grfCount) {
        auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();
        auto &gfxCoreHelper = rootDeviceEnvironment.getHelper<GfxCoreHelper>();

        return gfxCoreHelper.calculateAvailableThreadCount(hwInfo, grfCount, rootDeviceEnvironment) / hwInfo.gtSystemInfo.SubSliceCount;
    }

    template <typename FamilyType>
    void verifyPreferredSlmValues(const SlmTestHelper<FamilyType> &slmTestHelper, const RootDeviceEnvironment &rootDeviceEnvironment) {
        using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
        using INTERFACE_DESCRIPTOR_DATA = typename DefaultWalkerType::InterfaceDescriptorType;
        using PREFERRED_SLM_ALLOCATION_SIZE = typename INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE;

        auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();
        uint32_t threadsPerThreadGroupValues[] = {1, 2, 3, 4, 5, 10, 15, 20};
        uint32_t workloadThreadGroupCounts[] = {1, 2, 3, 4, 5, 8, 10, 15, 16, 20, 32, 64, 96, 128};
        const auto grfCounts = rootDeviceEnvironment.getProductHelper().getSupportedNumGrfs(rootDeviceEnvironment.getReleaseHelper());

        auto idd = FamilyType::template getInitInterfaceDescriptor<INTERFACE_DESCRIPTOR_DATA>();
        EXPECT_EQ(0u, idd.getPreferredSlmAllocationSize());

        const std::array<NEO::SlmPolicy, 3> slmPolicies = {
            NEO::SlmPolicy::slmPolicyNone,
            NEO::SlmPolicy::slmPolicyLargeSlm,
            NEO::SlmPolicy::slmPolicyLargeData};

        const auto &programmableSlmSizesPerThreadGroup = slmTestHelper.programmableSlmSizesPerThreadGroup;

        for (size_t index = 0; index < programmableSlmSizesPerThreadGroup.size(); index++) {
            const auto &programmableSlmSizePerThreadGroup = programmableSlmSizesPerThreadGroup[index];
            const bool isLastProgrammableSlmSizePerThreadGroup = (index + 1 == programmableSlmSizesPerThreadGroup.size());
            for (auto slmPolicy : slmPolicies) {
                for (auto workloadThreadGroupCount : workloadThreadGroupCounts) {
                    for (auto grfCount : grfCounts) {
                        auto maxConcurrentThreadCountPerSubslice = this->getMaxConcurrentThreadCountPerSubslice(rootDeviceEnvironment, grfCount);
                        ASSERT_NE(0u, maxConcurrentThreadCountPerSubslice) << "grfCount: " << grfCount;

                        for (auto threadsPerThreadGroup : threadsPerThreadGroupValues) {
                            if (threadsPerThreadGroup > maxConcurrentThreadCountPerSubslice) {
                                // whole thread group has to be resident in a single subslice, such thread group is not dispatchable with this grf count
                                continue;
                            }
                            auto workloadThreadGroupCountPerSubslice = this->calculateThreadGroupCountPerSubslice(hwInfo, workloadThreadGroupCount);
                            auto maxConcurrentThreadGroupCountPerSubslice = maxConcurrentThreadCountPerSubslice / threadsPerThreadGroup;
                            auto threadGroupCountSharingSubsliceSlm = std::min(workloadThreadGroupCountPerSubslice, maxConcurrentThreadGroupCountPerSubslice);
                            auto slmTotalSizePerThreadGroupEdges = this->getSlmTotalSizePerThreadGroupEdgeValues(programmableSlmSizePerThreadGroup.slmSize, threadGroupCountSharingSubsliceSlm, slmPolicy, isLastProgrammableSlmSizePerThreadGroup);

                            for (auto slmTotalSizePerThreadGroup : slmTotalSizePerThreadGroupEdges) {
                                auto expectedSlmPerSubslice = this->calculateExpectedSlmPerSubsliceFromSlmTotalSizePerThreadGroup(slmTotalSizePerThreadGroup, threadGroupCountSharingSubsliceSlm, slmPolicy, slmTestHelper);
                                auto expectedValue = this->getExpectedProgrammableValue<FamilyType>(expectedSlmPerSubslice, slmTestHelper, rootDeviceEnvironment);

                                NEO::EncodeSlmSizePerSubSliceArgs slmArgs{
                                    .threadsPerThreadGroup = threadsPerThreadGroup,
                                    .workloadThreadGroupCount = workloadThreadGroupCount,
                                    .slmTotalSizePerThreadGroup = slmTotalSizePerThreadGroup,
                                    .grfCount = grfCount,
                                    .slmPolicy = slmPolicy};

                                NEO::EncodeDispatchKernel<FamilyType>::encodeSlmSizePerSubSlice(&idd, rootDeviceEnvironment, slmArgs);

                                EXPECT_EQ(static_cast<PREFERRED_SLM_ALLOCATION_SIZE>(expectedValue), idd.getPreferredSlmAllocationSize())
                                    << ", programmableSlmSizePerThreadGroup: " << programmableSlmSizePerThreadGroup.slmSize
                                    << ", isLastProgrammableSlmSizePerThreadGroup: " << isLastProgrammableSlmSizePerThreadGroup
                                    << ", slmPolicy: " << static_cast<uint32_t>(slmPolicy)
                                    << ", availableSlmSizePerSubslice: " << rootDeviceEnvironment.getProductHelper().getAvailableSlmSizePerSubslice(rootDeviceEnvironment)
                                    << ", workloadThreadGroupCount: " << workloadThreadGroupCount
                                    << ", threadsPerThreadGroup: " << threadsPerThreadGroup
                                    << ", grfCount: " << grfCount
                                    << ", maxConcurrentThreadCountPerSubslice: " << maxConcurrentThreadCountPerSubslice
                                    << ", workloadThreadGroupCountPerSubslice: " << workloadThreadGroupCountPerSubslice
                                    << ", threadGroupCountSharingSubsliceSlm: " << threadGroupCountSharingSubsliceSlm
                                    << ", slmTotalSizePerThreadGroup: " << slmTotalSizePerThreadGroup
                                    << ", expectedSlmPerSubslice: " << expectedSlmPerSubslice
                                    << ", expectedValue: " << static_cast<uint32_t>(expectedValue)
                                    << ", actualValue: " << static_cast<uint32_t>(idd.getPreferredSlmAllocationSize());
                            }
                        }
                    }
                }
            }
        }
    }

  private:
    uint32_t calculateThreadGroupCountPerSubslice(const HardwareInfo &hwInfo, const uint32_t workloadThreadGroupCount) {
        return static_cast<uint32_t>(Math::divideAndRoundUp(workloadThreadGroupCount, hwInfo.gtSystemInfo.SubSliceCount));
    }

    std::array<uint32_t, 3> getSlmTotalSizePerThreadGroupEdgeValues(uint32_t programmableSlmSizePerThreadGroup, uint32_t threadGroupCountSharingSubsliceSlm, SlmPolicy slmPolicy, bool isMaxProgrammableSlmSizePerThreadGroup) {
        uint32_t baseSlmTotalSizePerThreadGroup = 0;
        if (slmPolicy == SlmPolicy::slmPolicyLargeData) {
            baseSlmTotalSizePerThreadGroup = programmableSlmSizePerThreadGroup;
        } else {
            baseSlmTotalSizePerThreadGroup = programmableSlmSizePerThreadGroup / threadGroupCountSharingSubsliceSlm;
        }

        const uint32_t minusOne = (baseSlmTotalSizePerThreadGroup > 0) ? baseSlmTotalSizePerThreadGroup - 1 : 0;
        const uint32_t plusOne = (!isMaxProgrammableSlmSizePerThreadGroup) ? baseSlmTotalSizePerThreadGroup + 1 : baseSlmTotalSizePerThreadGroup;
        return {minusOne, baseSlmTotalSizePerThreadGroup, plusOne};
    }

    template <typename FamilyType>
    uint32_t alignSlmSizePerThreadGroup(uint32_t slmSize, const SlmTestHelper<FamilyType> &slmTestHelper) {
        for (const auto &value : slmTestHelper.programmableSlmSizesPerThreadGroup) {
            if (slmSize <= value.slmSize) {
                return value.slmSize;
            }
        }

        return slmTestHelper.programmableSlmSizesPerThreadGroup.back().slmSize;
    }

    template <typename FamilyType>
    uint32_t calculateExpectedSlmPerSubsliceFromSlmTotalSizePerThreadGroup(uint32_t slmTotalSizePerThreadGroup, uint32_t threadGroupCountSharingSubsliceSlm, SlmPolicy slmPolicy, const SlmTestHelper<FamilyType> &slmTestHelper) {
        auto alignedSlmSizePerThreadGroup = alignSlmSizePerThreadGroup(slmTotalSizePerThreadGroup, slmTestHelper);

        if (slmPolicy == SlmPolicy::slmPolicyLargeData) {
            return alignedSlmSizePerThreadGroup;
        }

        return alignedSlmSizePerThreadGroup * threadGroupCountSharingSubsliceSlm;
    }

    template <typename FamilyType>
    auto getExpectedProgrammableValue(uint32_t expectedSlmPerSubslice, const SlmTestHelper<FamilyType> &slmTestHelper, const RootDeviceEnvironment &rootDeviceEnvironment) {
        using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
        using INTERFACE_DESCRIPTOR_DATA = typename DefaultWalkerType::InterfaceDescriptorType;
        using PREFERRED_SLM_ALLOCATION_SIZE = typename INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE;

        auto availableSlmSizePerSubslice = rootDeviceEnvironment.getProductHelper().getAvailableSlmSizePerSubslice(rootDeviceEnvironment);
        auto clampedExpectedSlmPerSubslice = std::min(expectedSlmPerSubslice, static_cast<uint32_t>(availableSlmSizePerSubslice * MemoryConstants::kiloByte));

        for (const auto &value : slmTestHelper.programmablePreferredSlmSizesPerSubslice) {
            if (clampedExpectedSlmPerSubslice <= value.slmSize) {
                return value.programmableValue;
            }
        }

        return static_cast<PREFERRED_SLM_ALLOCATION_SIZE>(0);
    }
};
