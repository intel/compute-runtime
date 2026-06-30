/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/kernel/dispatch_kernel_encoder_interface.h"
#include "shared/source/os_interface/product_helper.h"
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

    template <typename FamilyType>
    void verifyPreferredSlmValues(const SlmTestHelper<FamilyType> &slmTestHelper, const RootDeviceEnvironment &rootDeviceEnvironment) {
        using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
        using INTERFACE_DESCRIPTOR_DATA = typename DefaultWalkerType::InterfaceDescriptorType;
        using PREFERRED_SLM_ALLOCATION_SIZE = typename INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE;

        auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();
        auto threadCountPerSubslice = hwInfo.gtSystemInfo.ThreadCount / hwInfo.gtSystemInfo.SubSliceCount;
        uint32_t threadsPerThreadGroupValues[] = {1, 2, 3, 4, 5, 10, 15, 20};
        uint32_t totalDispatchedThreadGroupCounts[] = {1, 2, 3, 4, 5, 8, 10, 15, 16, 20, 32, 64, 96, 128};

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
                for (auto totalDispatchedThreadGroupCount : totalDispatchedThreadGroupCounts) {
                    for (auto threadsPerThreadGroup : threadsPerThreadGroupValues) {
                        auto threadGroupCountPerSubsliceFromWorkload = this->calculateThreadGroupCountPerSubslice(hwInfo, totalDispatchedThreadGroupCount);
                        auto maxThreadGroupCountPerSubslice = threadCountPerSubslice / threadsPerThreadGroup;
                        auto threadGroupCountPerSubsliceRequired = std::min(threadGroupCountPerSubsliceFromWorkload, maxThreadGroupCountPerSubslice);
                        auto slmTotalSizePerThreadGroupEdges = this->getSlmTotalSizePerThreadGroupEdgeValues(programmableSlmSizePerThreadGroup.slmSize, threadGroupCountPerSubsliceRequired, slmPolicy, isLastProgrammableSlmSizePerThreadGroup);

                        for (auto slmTotalSizePerThreadGroup : slmTotalSizePerThreadGroupEdges) {
                            auto expectedSlmPerSubslice = this->calculateExpectedSlmPerSubsliceFromSlmTotalSizePerThreadGroup(slmTotalSizePerThreadGroup, threadGroupCountPerSubsliceRequired, slmPolicy, slmTestHelper);
                            auto expectedValue = this->getExpectedProgrammableValue<FamilyType>(expectedSlmPerSubslice, slmTestHelper, rootDeviceEnvironment);

                            NEO::EncodeDispatchKernel<FamilyType>::encodeSlmSizePerSubSlice(&idd,
                                                                                            rootDeviceEnvironment,
                                                                                            threadsPerThreadGroup,
                                                                                            totalDispatchedThreadGroupCount,
                                                                                            slmTotalSizePerThreadGroup,
                                                                                            slmPolicy);

                            EXPECT_EQ(static_cast<PREFERRED_SLM_ALLOCATION_SIZE>(expectedValue), idd.getPreferredSlmAllocationSize())
                                << ", programmableSlmSizePerThreadGroup: " << programmableSlmSizePerThreadGroup.slmSize
                                << ", isLastProgrammableSlmSizePerThreadGroup: " << isLastProgrammableSlmSizePerThreadGroup
                                << ", slmPolicy: " << static_cast<uint32_t>(slmPolicy)
                                << ", availableSlmSizePerSubslice: " << rootDeviceEnvironment.getProductHelper().getAvailableSlmSizePerSubslice(rootDeviceEnvironment)
                                << ", totalDispatchedThreadGroupCount: " << totalDispatchedThreadGroupCount
                                << ", threadsPerThreadGroup: " << threadsPerThreadGroup
                                << ", threadCountPerSubslice: " << threadCountPerSubslice
                                << ", threadGroupCountPerSubsliceFromWorkload: " << threadGroupCountPerSubsliceFromWorkload
                                << ", threadGroupCountPerSubsliceRequired: " << threadGroupCountPerSubsliceRequired
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

  private:
    uint32_t calculateThreadGroupCountPerSubslice(const HardwareInfo &hwInfo, const uint32_t totalDispatchedThreadGroupCount) {
        return static_cast<uint32_t>(Math::divideAndRoundUp(totalDispatchedThreadGroupCount, hwInfo.gtSystemInfo.SubSliceCount));
    }

    std::array<uint32_t, 3> getSlmTotalSizePerThreadGroupEdgeValues(uint32_t programmableSlmSizePerThreadGroup, uint32_t threadGroupCountPerSubsliceRequired, SlmPolicy slmPolicy, bool isMaxProgrammableSlmSizePerThreadGroup) {
        uint32_t baseSlmTotalSizePerThreadGroup = 0;
        if (slmPolicy == SlmPolicy::slmPolicyLargeData) {
            baseSlmTotalSizePerThreadGroup = programmableSlmSizePerThreadGroup;
        } else {
            baseSlmTotalSizePerThreadGroup = programmableSlmSizePerThreadGroup / threadGroupCountPerSubsliceRequired;
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
    uint32_t calculateExpectedSlmPerSubsliceFromSlmTotalSizePerThreadGroup(uint32_t slmTotalSizePerThreadGroup, uint32_t threadGroupCountPerSubsliceRequired, SlmPolicy slmPolicy, const SlmTestHelper<FamilyType> &slmTestHelper) {
        auto alignedSlmSizePerThreadGroup = alignSlmSizePerThreadGroup(slmTotalSizePerThreadGroup, slmTestHelper);

        if (slmPolicy == SlmPolicy::slmPolicyLargeData) {
            return alignedSlmSizePerThreadGroup;
        }

        return alignedSlmSizePerThreadGroup * threadGroupCountPerSubsliceRequired;
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
