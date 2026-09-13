/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/kernel/dispatch_kernel_encoder_interface.h"
#include "shared/source/kernel/grf_config.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/release_helpers/release_helper/release_helper.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/test.h"

#include <algorithm>
#include <limits>
#include <vector>

using namespace NEO;

inline std::vector<uint32_t> slmSizesInBytes(std::initializer_list<uint32_t> sizesInKb) {
    std::vector<uint32_t> sizesInBytes;
    sizesInBytes.reserve(sizesInKb.size());
    for (auto sizeInKb : sizesInKb) {
        sizesInBytes.push_back(sizeInKb * MemoryConstants::kiloByte);
    }
    return sizesInBytes;
}

class CommandEncodeStatesSlmTestXe2AndLater : public ::testing::Test {
  public:
    NEO::RootDeviceEnvironment &getRootDeviceEnvironment() {
        return *mockExecutionEnvironment.rootDeviceEnvironments[0];
    }

    template <typename FamilyType>
    void verifySlmSizePerThreadGroupAlignment(const std::vector<uint32_t> &programmableSlmSizesPerThreadGroup) {
        const auto &releaseHelper = getRootDeviceEnvironment().getReleaseHelper();
        ASSERT_LE(2u, programmableSlmSizesPerThreadGroup.size());

        auto alignSlmSizePerThreadGroup = [&](uint32_t slmTotalSizePerThreadGroup) {
            return NEO::EncodeDispatchKernel<FamilyType>::alignSlmSizePerThreadGroup(slmTotalSizePerThreadGroup, releaseHelper);
        };

        for (size_t index = 0; index < programmableSlmSizesPerThreadGroup.size(); index++) {
            const uint32_t programmableSlmSize = programmableSlmSizesPerThreadGroup[index];

            EXPECT_EQ(programmableSlmSize, alignSlmSizePerThreadGroup(programmableSlmSize))
                << "a programmable size has to be left as it is, index: " << index;

            if (programmableSlmSize > 0) {
                EXPECT_EQ(programmableSlmSize, alignSlmSizePerThreadGroup(programmableSlmSize - 1))
                    << "a size just below a programmable one has to be aligned up to it, index: " << index;
            }

            if (index + 1 < programmableSlmSizesPerThreadGroup.size()) {
                EXPECT_EQ(programmableSlmSizesPerThreadGroup[index + 1], alignSlmSizePerThreadGroup(programmableSlmSize + 1))
                    << "a size just above a programmable one has to be aligned up to the next one, index: " << index;
            }
        }
    }

    template <typename FamilyType>
    void verifySlmPolicies() {
        auto &rootDeviceEnvironment = getRootDeviceEnvironment();
        auto &hwInfo = *rootDeviceEnvironment.getMutableHardwareInfo();
        const auto &releaseHelper = rootDeviceEnvironment.getReleaseHelper();

        constexpr uint32_t threadsPerThreadGroup = 1;
        constexpr uint32_t slmTotalSizePerThreadGroup = 16 * MemoryConstants::kiloByte;
        ASSERT_EQ(slmTotalSizePerThreadGroup, NEO::EncodeDispatchKernel<FamilyType>::alignSlmSizePerThreadGroup(slmTotalSizePerThreadGroup, releaseHelper))
            << "the size has to be already aligned, so that only the policy decides the slm size of a subslice";

        const uint32_t maxThreadGroupCountSharingSubsliceSlm = NEO::EncodeDispatchKernel<FamilyType>::getMaxConcurrentThreadCountPerSubslice(rootDeviceEnvironment, GrfConfig::defaultGrfNumber) / threadsPerThreadGroup;
        ASSERT_LE(2u, maxThreadGroupCountSharingSubsliceSlm);
        hwInfo.gtSystemInfo.SLMSizeInKb = static_cast<uint32_t>(slmTotalSizePerThreadGroup / MemoryConstants::kiloByte) * maxThreadGroupCountSharingSubsliceSlm;
        const uint32_t availableSlmSizePerSubslice = rootDeviceEnvironment.getProductHelper().getAvailableSlmSizePerSubslice(rootDeviceEnvironment) * MemoryConstants::kiloByte;

        auto makeSlmArgs = [&](uint32_t workloadThreadGroupCount, NEO::SlmPolicy slmPolicy) {
            return NEO::EncodeSlmSizePerSubSliceArgs{
                .threadsPerThreadGroup = threadsPerThreadGroup,
                .workloadThreadGroupCount = workloadThreadGroupCount,
                .slmTotalSizePerThreadGroup = slmTotalSizePerThreadGroup,
                .grfCount = GrfConfig::defaultGrfNumber,
                .slmPolicy = slmPolicy};
        };

        const uint32_t saturatingWorkloadThreadGroupCount = hwInfo.gtSystemInfo.ThreadCount;
        for (auto workloadThreadGroupCount : {1u, saturatingWorkloadThreadGroupCount}) {
            const auto threadGroupCountSharingSubsliceSlm = NEO::EncodeDispatchKernel<FamilyType>::calculateThreadGroupCountSharingSubsliceSlm(rootDeviceEnvironment, makeSlmArgs(workloadThreadGroupCount, NEO::SlmPolicy::slmPolicyNone));
            ASSERT_NE(0u, threadGroupCountSharingSubsliceSlm);

            const auto expectedSharedValue = this->getExpectedProgrammableValue(std::min(slmTotalSizePerThreadGroup * threadGroupCountSharingSubsliceSlm, availableSlmSizePerSubslice), releaseHelper);
            const auto expectedLargeDataValue = this->getExpectedProgrammableValue(std::min(slmTotalSizePerThreadGroup, availableSlmSizePerSubslice), releaseHelper);

            EXPECT_EQ(expectedSharedValue, this->encodePreferredSlm<FamilyType>(rootDeviceEnvironment, makeSlmArgs(workloadThreadGroupCount, NEO::SlmPolicy::slmPolicyNone)))
                << ", slmTotalSizePerThreadGroup: " << slmTotalSizePerThreadGroup
                << ", threadGroupCountSharingSubsliceSlm: " << threadGroupCountSharingSubsliceSlm;
            EXPECT_EQ(expectedSharedValue, this->encodePreferredSlm<FamilyType>(rootDeviceEnvironment, makeSlmArgs(workloadThreadGroupCount, NEO::SlmPolicy::slmPolicyLargeSlm)))
                << ", slmTotalSizePerThreadGroup: " << slmTotalSizePerThreadGroup
                << ", threadGroupCountSharingSubsliceSlm: " << threadGroupCountSharingSubsliceSlm;
            EXPECT_EQ(expectedLargeDataValue, this->encodePreferredSlm<FamilyType>(rootDeviceEnvironment, makeSlmArgs(workloadThreadGroupCount, NEO::SlmPolicy::slmPolicyLargeData)))
                << ", slmTotalSizePerThreadGroup: " << slmTotalSizePerThreadGroup
                << ", threadGroupCountSharingSubsliceSlm: " << threadGroupCountSharingSubsliceSlm;

            if (threadGroupCountSharingSubsliceSlm > 1) {
                EXPECT_NE(expectedLargeDataValue, expectedSharedValue)
                    << "the thread group count has to move the size into another preferred slm range, otherwise the policies are indistinguishable";
            } else {
                EXPECT_EQ(expectedLargeDataValue, expectedSharedValue)
                    << "a single thread group per subslice makes all the policies ask for the same size";
            }
        }
    }

    template <typename FamilyType>
    void verifyPreferredSlmValueRanges() {
        auto &rootDeviceEnvironment = getRootDeviceEnvironment();
        auto &hwInfo = *rootDeviceEnvironment.getMutableHardwareInfo();
        const auto &releaseHelper = rootDeviceEnvironment.getReleaseHelper();
        const auto &preferredSlmValueRanges = releaseHelper.getSizeToPreferredSlmValue();

        const size_t rangeCount = this->getPreferredSlmValueRangeCount(preferredSlmValueRanges);
        ASSERT_LE(2u, rangeCount);

        const uint32_t largestProbedSlmSizePerSubslice = preferredSlmValueRanges[rangeCount - 2].upperLimit + static_cast<uint32_t>(MemoryConstants::kiloByte);
        const uint32_t slmTotalSizePerThreadGroup = NEO::EncodeDispatchKernel<FamilyType>::alignSlmSizePerThreadGroup(preferredSlmValueRanges[rangeCount - 2].upperLimit, releaseHelper);

        auto makeSlmArgs = [&]() {
            return NEO::EncodeSlmSizePerSubSliceArgs{
                .threadsPerThreadGroup = 1,
                .workloadThreadGroupCount = hwInfo.gtSystemInfo.ThreadCount,
                .slmTotalSizePerThreadGroup = slmTotalSizePerThreadGroup,
                .grfCount = GrfConfig::defaultGrfNumber,
                .slmPolicy = NEO::SlmPolicy::slmPolicyLargeSlm};
        };

        const auto threadGroupCountSharingSubsliceSlm = NEO::EncodeDispatchKernel<FamilyType>::calculateThreadGroupCountSharingSubsliceSlm(rootDeviceEnvironment, makeSlmArgs());
        ASSERT_NE(0u, threadGroupCountSharingSubsliceSlm);
        ASSERT_LE(largestProbedSlmSizePerSubslice, slmTotalSizePerThreadGroup * threadGroupCountSharingSubsliceSlm);

        auto encodeWithAvailableSlmSizePerSubslice = [&](uint32_t availableSlmSizePerSubslice) {
            hwInfo.gtSystemInfo.SLMSizeInKb = static_cast<uint32_t>(availableSlmSizePerSubslice / MemoryConstants::kiloByte);
            return this->encodePreferredSlm<FamilyType>(rootDeviceEnvironment, makeSlmArgs());
        };

        for (size_t index = 0; index + 1 < rangeCount; index++) {
            const uint32_t upperLimit = preferredSlmValueRanges[index].upperLimit;
            ASSERT_EQ(0u, upperLimit % MemoryConstants::kiloByte);

            EXPECT_EQ(preferredSlmValueRanges[index].valueToProgram, encodeWithAvailableSlmSizePerSubslice(upperLimit))
                << "the upper limit of a range belongs to that range, upperLimit: " << upperLimit;
            EXPECT_EQ(preferredSlmValueRanges[index + 1].valueToProgram, encodeWithAvailableSlmSizePerSubslice(upperLimit + static_cast<uint32_t>(MemoryConstants::kiloByte)))
                << "a size above the upper limit of a range belongs to the next range, upperLimit: " << upperLimit;
        }
    }

  private:
    template <typename FamilyType>
    uint32_t encodePreferredSlm(const RootDeviceEnvironment &rootDeviceEnvironment, const NEO::EncodeSlmSizePerSubSliceArgs &slmArgs) {
        using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
        using INTERFACE_DESCRIPTOR_DATA = typename DefaultWalkerType::InterfaceDescriptorType;

        auto idd = FamilyType::template getInitInterfaceDescriptor<INTERFACE_DESCRIPTOR_DATA>();
        NEO::EncodeDispatchKernel<FamilyType>::encodeSlmSizePerSubSlice(&idd, rootDeviceEnvironment, slmArgs);
        return static_cast<uint32_t>(idd.getPreferredSlmAllocationSize());
    }

    size_t getPreferredSlmValueRangeCount(const NEO::SizeToPreferredSlmValueArray &preferredSlmValueRanges) {
        for (size_t index = 0; index < preferredSlmValueRanges.size(); index++) {
            if (preferredSlmValueRanges[index].upperLimit == std::numeric_limits<uint32_t>::max()) {
                return index + 1;
            }
        }

        return 0;
    }

    uint32_t getExpectedProgrammableValue(uint32_t expectedSlmSizePerSubslice, const NEO::ReleaseHelper &releaseHelper) {
        for (const auto &range : releaseHelper.getSizeToPreferredSlmValue()) {
            if (expectedSlmSizePerSubslice <= range.upperLimit) {
                return range.valueToProgram;
            }
        }

        return std::numeric_limits<uint32_t>::max();
    }

    NEO::MockExecutionEnvironment mockExecutionEnvironment{};
};
