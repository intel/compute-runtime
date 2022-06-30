/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/local_work_size.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/source/command_queue/gpgpu_walker.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"

#include "patch_shared.h"

using namespace NEO;

struct WorkGroupSizeBase {
    template <typename FamilyType>
    size_t computeWalkerWorkItems(typename FamilyType::GPGPU_WALKER &pCmd) {
        typedef typename FamilyType::GPGPU_WALKER GPGPU_WALKER;

        // Compute the SIMD lane mask
        size_t simd =
            pCmd.getSimdSize() == GPGPU_WALKER::SIMD_SIZE_SIMD32 ? 32 : pCmd.getSimdSize() == GPGPU_WALKER::SIMD_SIZE_SIMD16 ? 16
                                                                                                                             : 8;
        uint64_t simdMask = maxNBitValue(simd);

        // Mask off lanes based on the execution masks
        auto laneMaskRight = pCmd.getRightExecutionMask() & simdMask;
        auto lanesPerThreadX = 0;
        while (laneMaskRight) {
            lanesPerThreadX += laneMaskRight & 1;
            laneMaskRight >>= 1;
        }

        auto numWorkItems = ((pCmd.getThreadWidthCounterMaximum() - 1) * simd + lanesPerThreadX) * pCmd.getThreadGroupIdXDimension();
        numWorkItems *= pCmd.getThreadGroupIdYDimension();
        numWorkItems *= pCmd.getThreadGroupIdZDimension();

        return numWorkItems;
    }

    template <typename FamilyType>
    void verify(uint32_t simdSize, size_t dimX, size_t dimY, size_t dimZ) {
        size_t globalOffsets[] = {0, 0, 0};
        size_t workItems[] = {
            dimX,
            dimY,
            dimZ};
        int dims = (dimX > 1 ? 1 : 0) +
                   (dimY > 1 ? 1 : 0) +
                   (dimZ > 1 ? 1 : 0);

        size_t workGroupSize[3];
        auto maxWorkGroupSize = 256u;
        if (DebugManager.flags.EnableComputeWorkSizeND.get()) {
            WorkSizeInfo wsInfo(maxWorkGroupSize, 0u, simdSize, 0u, ::defaultHwInfo.get(), 32u, 0u, false, false, false);
            computeWorkgroupSizeND(wsInfo, workGroupSize, workItems, dims);
        } else {
            if (dims == 1) {
                computeWorkgroupSize1D(maxWorkGroupSize, workGroupSize, workItems, simdSize);
            } else {
                computeWorkgroupSize2D(maxWorkGroupSize, workGroupSize, workItems, simdSize);
            }
        }
        auto totalWorkItems = workItems[0] * workItems[1] * workItems[2];
        auto localWorkItems = workGroupSize[0] * workGroupSize[1] * workGroupSize[2];

        EXPECT_GT(localWorkItems, 0u);
        EXPECT_LE(localWorkItems, 256u);

        auto xRemainder = workItems[0] % workGroupSize[0];
        auto yRemainder = workItems[1] % workGroupSize[1];
        auto zRemainder = workItems[2] % workGroupSize[2];

        //No remainders
        EXPECT_EQ(0u, xRemainder);
        EXPECT_EQ(0u, yRemainder);
        EXPECT_EQ(0u, zRemainder);

        //Now setup GPGPU Walker
        typedef typename FamilyType::GPGPU_WALKER GPGPU_WALKER;
        GPGPU_WALKER pCmd = FamilyType::cmdInitGpgpuWalker;

        const size_t workGroupsStart[3] = {0, 0, 0};
        const size_t workGroupsNum[3] = {
            Math::divideAndRoundUp(workItems[0], workGroupSize[0]),
            Math::divideAndRoundUp(workItems[1], workGroupSize[1]),
            Math::divideAndRoundUp(workItems[2], workGroupSize[2])};
        KernelDescriptor kd;
        GpgpuWalkerHelper<FamilyType>::setGpgpuWalkerThreadData(&pCmd, kd, globalOffsets, workGroupsStart, workGroupsNum,
                                                                workGroupSize, simdSize, dims, true, false, 0u);

        //And check if it is programmed correctly
        auto numWorkItems = computeWalkerWorkItems<FamilyType>(pCmd);
        EXPECT_EQ(totalWorkItems, numWorkItems);

        if (xRemainder | yRemainder | zRemainder | (totalWorkItems != numWorkItems)) {
            std::stringstream regionString;
            regionString << "workItems = <" << workItems[0] << ", " << workItems[1] << ", " << workItems[2] << ">; ";
            regionString << "LWS = <" << workGroupSize[0] << ", " << workGroupSize[1] << ", " << workGroupSize[2] << ">; ";
            regionString << "thread = <"
                         << pCmd.getThreadGroupIdXDimension() << ", "
                         << pCmd.getThreadGroupIdYDimension() << ", "
                         << pCmd.getThreadGroupIdZDimension() << ">; ";
            regionString << "threadWidth = " << std::dec << pCmd.getThreadWidthCounterMaximum() << std::dec << "; ";
            regionString << "rightMask = " << std::hex << pCmd.getRightExecutionMask() << std::dec << "; ";
            EXPECT_FALSE(true) << regionString.str();
        }
    }
};

struct WorkGroupSizeChannels : public WorkGroupSizeBase,
                               public ::testing::TestWithParam<std::tuple<uint32_t, size_t>> {
};

HWCMDTEST_P(IGFX_GEN8_CORE, WorkGroupSizeChannels, GivenAllChannelsWhenEnablingComputeWorkSizeNDDefaultThenSizeAndDimAreCorrect) {
    uint32_t simdSize;
    size_t workDim;
    std::tie(simdSize, workDim) = GetParam();

    verify<FamilyType>(simdSize, workDim, workDim, workDim);
}

HWCMDTEST_P(IGFX_GEN8_CORE, WorkGroupSizeChannels, GivenAllChannelsWhenEnablingComputeWorkSizeNDEnabledThenSizeAndDimAreCorrect) {
    bool isWorkGroupSizeEnabled = DebugManager.flags.EnableComputeWorkSizeND.get();
    DebugManager.flags.EnableComputeWorkSizeND.set(true);
    uint32_t simdSize;
    size_t workDim;
    std::tie(simdSize, workDim) = GetParam();

    verify<FamilyType>(simdSize, workDim, workDim, workDim);

    DebugManager.flags.EnableComputeWorkSizeND.set(isWorkGroupSizeEnabled);
}

HWCMDTEST_P(IGFX_GEN8_CORE, WorkGroupSizeChannels, GivenAllChannelsWhenEnablingComputeWorkSizeNDDisabledThenSizeAndDimAreCorrect) {
    bool isWorkGroupSizeEnabled = DebugManager.flags.EnableComputeWorkSizeND.get();
    DebugManager.flags.EnableComputeWorkSizeND.set(false);
    uint32_t simdSize;
    size_t workDim;
    std::tie(simdSize, workDim) = GetParam();

    verify<FamilyType>(simdSize, workDim, workDim, workDim);

    DebugManager.flags.EnableComputeWorkSizeND.set(isWorkGroupSizeEnabled);
}

HWCMDTEST_P(IGFX_GEN8_CORE, WorkGroupSizeChannels, GivenAllChannelsWhenEnablingComputeWorkSizeSquaredDefaultThenSizeAndDimAreCorrect) {
    uint32_t simdSize;
    size_t workDim;
    std::tie(simdSize, workDim) = GetParam();

    verify<FamilyType>(simdSize, workDim, workDim, workDim);
}

HWCMDTEST_P(IGFX_GEN8_CORE, WorkGroupSizeChannels, GivenAllChannelsWhenEnablingComputeWorkSizeSquaredEnabledThenSizeAndDimAreCorrect) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableComputeWorkSizeSquared.set(true);
    DebugManager.flags.EnableComputeWorkSizeND.set(false);
    uint32_t simdSize;
    size_t workDim;
    std::tie(simdSize, workDim) = GetParam();

    verify<FamilyType>(simdSize, workDim, workDim, workDim);
}

HWCMDTEST_P(IGFX_GEN8_CORE, WorkGroupSizeChannels, GivenAllChannelsWhenEnablingComputeWorkSizeSquaredDisabledThenSizeAndDimAreCorrect) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableComputeWorkSizeSquared.set(false);
    DebugManager.flags.EnableComputeWorkSizeND.set(false);
    uint32_t simdSize;
    size_t workDim;
    std::tie(simdSize, workDim) = GetParam();

    verify<FamilyType>(simdSize, workDim, workDim, workDim);
}

HWCMDTEST_P(IGFX_GEN8_CORE, WorkGroupSizeChannels, GivenOnlyXWhenEnablingComputeWorkSizeNDDefaultThenSizeAndDimAreCorrect) {
    uint32_t simdSize;
    size_t workDim;
    std::tie(simdSize, workDim) = GetParam();

    verify<FamilyType>(simdSize, workDim, 1, 1);
}

HWCMDTEST_P(IGFX_GEN8_CORE, WorkGroupSizeChannels, GivenOnlyXWhenEnablingComputeWorkSizeNDEnabledThenSizeAndDimAreCorrect) {
    bool isWorkGroupSizeEnabled = DebugManager.flags.EnableComputeWorkSizeND.get();
    DebugManager.flags.EnableComputeWorkSizeND.set(true);
    uint32_t simdSize;
    size_t workDim;
    std::tie(simdSize, workDim) = GetParam();

    verify<FamilyType>(simdSize, workDim, 1, 1);

    DebugManager.flags.EnableComputeWorkSizeND.set(isWorkGroupSizeEnabled);
}

HWCMDTEST_P(IGFX_GEN8_CORE, WorkGroupSizeChannels, GivenOnlyXWhenEnablingComputeWorkSizeNDDisabledThenSizeAndDimAreCorrect) {
    bool isWorkGroupSizeEnabled = DebugManager.flags.EnableComputeWorkSizeND.get();
    DebugManager.flags.EnableComputeWorkSizeND.set(false);
    uint32_t simdSize;
    size_t workDim;
    std::tie(simdSize, workDim) = GetParam();

    verify<FamilyType>(simdSize, workDim, 1, 1);

    DebugManager.flags.EnableComputeWorkSizeND.set(isWorkGroupSizeEnabled);
}

HWCMDTEST_P(IGFX_GEN8_CORE, WorkGroupSizeChannels, GivenOnlyYWhenEnablingComputeWorkSizeNDDefaultThenSizeAndDimAreCorrect) {
    uint32_t simdSize;
    size_t workDim;
    std::tie(simdSize, workDim) = GetParam();

    verify<FamilyType>(simdSize, 1, workDim, 1);
}

HWCMDTEST_P(IGFX_GEN8_CORE, WorkGroupSizeChannels, GivenOnlyYWhenEnablingComputeWorkSizeNDEnabledThenSizeAndDimAreCorrect) {
    bool isWorkGroupSizeEnabled = DebugManager.flags.EnableComputeWorkSizeND.get();
    DebugManager.flags.EnableComputeWorkSizeND.set(true);
    uint32_t simdSize;
    size_t workDim;
    std::tie(simdSize, workDim) = GetParam();

    verify<FamilyType>(simdSize, 1, workDim, 1);

    DebugManager.flags.EnableComputeWorkSizeND.set(isWorkGroupSizeEnabled);
}

HWCMDTEST_P(IGFX_GEN8_CORE, WorkGroupSizeChannels, GivenOnlyYWhenEnablingComputeWorkSizeNDDisabledThenSizeAndDimAreCorrect) {
    bool isWorkGroupSizeEnabled = DebugManager.flags.EnableComputeWorkSizeND.get();
    DebugManager.flags.EnableComputeWorkSizeND.set(false);
    uint32_t simdSize;
    size_t workDim;
    std::tie(simdSize, workDim) = GetParam();

    verify<FamilyType>(simdSize, 1, workDim, 1);

    DebugManager.flags.EnableComputeWorkSizeND.set(isWorkGroupSizeEnabled);
}

HWCMDTEST_P(IGFX_GEN8_CORE, WorkGroupSizeChannels, GivenOnlyZWhenEnablingComputeWorkSizeNDDefaultThenSizeAndDimAreCorrect) {
    uint32_t simdSize;
    size_t workDim;
    std::tie(simdSize, workDim) = GetParam();

    verify<FamilyType>(simdSize, 1, 1, workDim);
}

HWCMDTEST_P(IGFX_GEN8_CORE, WorkGroupSizeChannels, GivenOnlyZWhenEnablingComputeWorkSizeNDEnabledThenSizeAndDimAreCorrect) {
    bool isWorkGroupSizeEnabled = DebugManager.flags.EnableComputeWorkSizeND.get();
    DebugManager.flags.EnableComputeWorkSizeND.set(true);
    uint32_t simdSize;
    size_t workDim;
    std::tie(simdSize, workDim) = GetParam();

    verify<FamilyType>(simdSize, 1, 1, workDim);

    DebugManager.flags.EnableComputeWorkSizeND.set(isWorkGroupSizeEnabled);
}

HWCMDTEST_P(IGFX_GEN8_CORE, WorkGroupSizeChannels, GivenOnlyZWhenEnablingComputeWorkSizeNDDisabledThenSizeAndDimAreCorrect) {
    bool isWorkGroupSizeEnabled = DebugManager.flags.EnableComputeWorkSizeND.get();
    DebugManager.flags.EnableComputeWorkSizeND.set(false);
    uint32_t simdSize;
    size_t workDim;
    std::tie(simdSize, workDim) = GetParam();

    verify<FamilyType>(simdSize, 1, 1, workDim);

    DebugManager.flags.EnableComputeWorkSizeND.set(isWorkGroupSizeEnabled);
}

static uint32_t simdSizes[] = {
    8,
    16,
    32};

static size_t workItemCases1D[] = {
    1, 2,
    3, 4, 5,
    7, 8, 9,
    15, 16, 17,
    31, 32, 33,
    63, 64, 65,
    127, 128, 129,
    189, 190, 191,
    255, 256, 257,
    399, 400, 401,
    511, 512, 513,
    1007, 1008, 1009,
    1023, 1024, 1025,
    1400, 1401, 1402};

INSTANTIATE_TEST_CASE_P(wgs,
                        WorkGroupSizeChannels,
                        ::testing::Combine(
                            ::testing::ValuesIn(simdSizes),
                            ::testing::ValuesIn(workItemCases1D)));

// ==== ==== ==== ==== ==== ==== ==== ==== ==== ==== ==== ==== ==== ==== ==== ====
// ==== ==== ==== ==== ==== ==== ==== ==== ==== ==== ==== ==== ==== ==== ==== ====
struct WorkGroupSize2D : public WorkGroupSizeBase,
                         public ::testing::TestWithParam<std::tuple<uint32_t, size_t, size_t>> {
};

HWCMDTEST_P(IGFX_GEN8_CORE, WorkGroupSize2D, GivenUsingXandYWorkGroupDimensionsWhenComputingWorkgroupSizeThenSizeAndDimAreCorrect) {
    uint32_t simdSize;
    size_t dimX, dimY;
    std::tie(simdSize, dimX, dimY) = GetParam();

    verify<FamilyType>(simdSize, dimX, dimY, 1);
}

static size_t workItemCases2D[] = {1, 2, 3, 7, 15, 31, 63, 127, 255, 511, 1007, 1023, 2047};

INSTANTIATE_TEST_CASE_P(wgs,
                        WorkGroupSize2D,
                        ::testing::Combine(
                            ::testing::ValuesIn(simdSizes),
                            ::testing::ValuesIn(workItemCases2D),
                            ::testing::ValuesIn(workItemCases2D)));
// ==== ==== ==== ==== ==== ==== ==== ==== ==== ==== ==== ==== ==== ==== ==== ====
// ==== ==== ==== ==== ==== ==== ==== ==== ==== ==== ==== ==== ==== ==== ==== ====
struct Region {
    size_t r[3];
};

struct WorkGroupSizeRegion : public WorkGroupSizeBase,
                             public ::testing::TestWithParam<std::tuple<uint32_t, Region>> {
};

HWCMDTEST_P(IGFX_GEN8_CORE, WorkGroupSizeRegion, GivenUsingAllChannelsWhenComputingWorkgroupSizeThenSizeAndDimAreCorrect) {
    uint32_t simdSize;
    Region region;
    std::tie(simdSize, region) = GetParam();

    verify<FamilyType>(simdSize, region.r[0], region.r[1], region.r[2]);
}

Region regionCases[] = {
    {{1, 1, 1}}, // Trivial case
    {{9, 9, 10}} // This test case was hit by some AUBCopyBufferRect regressions
};

INSTANTIATE_TEST_CASE_P(wgs,
                        WorkGroupSizeRegion,
                        ::testing::Combine(
                            ::testing::ValuesIn(simdSizes),
                            ::testing::ValuesIn(regionCases)));
