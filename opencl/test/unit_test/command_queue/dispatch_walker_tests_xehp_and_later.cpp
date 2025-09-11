/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/command_container/walker_partition_xehp_and_later.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/simd_helper.h"
#include "shared/source/helpers/timestamp_packet.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/utilities/hw_timestamps.h"
#include "shared/source/utilities/tag_allocator.h"
#include "shared/test/common/fixtures/linear_stream_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_timestamp_container.h"

#include "opencl/source/built_ins/builtins_dispatch_builder.h"
#include "opencl/source/command_queue/gpgpu_walker.h"
#include "opencl/source/command_queue/hardware_interface.h"
#include "opencl/source/helpers/dispatch_info_builder.h"
#include "opencl/source/kernel/kernel.h"
#include "opencl/test/unit_test/command_queue/hardware_interface_helper.h"
#include "opencl/test/unit_test/helpers/cl_hw_parse.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/mocks/mock_mdi.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"

#include "test_traits_common.h"

using namespace NEO;

using WalkerDispatchTest = ::testing::Test;

struct XeHPAndLaterDispatchWalkerBasicFixture : public LinearStreamFixture {
    void setUp() {
        LinearStreamFixture::setUp();
        memset(startWorkGroups, 0, sizeof(startWorkGroups));

        localWorkSizesIn[0] = 16;
        localWorkSizesIn[1] = localWorkSizesIn[2] = 1;
        numWorkGroups[0] = numWorkGroups[1] = numWorkGroups[2] = 1;
        simd = 16;

        device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get(), rootDeviceIndex));
        context = std::make_unique<MockContext>(device.get());
        kernel = std::make_unique<MockKernelWithInternals>(*device, context.get());
        sizeGrf = device->getHardwareInfo().capabilityTable.grfSize;
        sizeGrfDwords = sizeGrf / sizeof(uint32_t);

        for (uint32_t i = 0; i < sizeGrfDwords; i++) {
            crossThreadDataGrf[i] = i;
            crossThreadDataTwoGrf[i] = i + 2;
        }
        for (uint32_t i = sizeGrfDwords; i < sizeGrfDwords * 2; i++) {
            crossThreadDataTwoGrf[i] = i + 2;
        }

        auto &compilerProductHelper = device->getCompilerProductHelper();
        heaplessEnabled = compilerProductHelper.isHeaplessModeEnabled(*defaultHwInfo);
    }

    DebugManagerStateRestore restore;

    size_t startWorkGroups[3];
    size_t numWorkGroups[3];
    size_t localWorkSizesIn[3];
    uint32_t simd;
    uint32_t sizeGrf;
    uint32_t sizeInlineData;
    uint32_t sizeGrfDwords;
    uint32_t crossThreadDataGrf[16];
    uint32_t crossThreadDataTwoGrf[32];

    const uint32_t rootDeviceIndex = 1u;
    std::unique_ptr<MockClDevice> device;
    std::unique_ptr<MockContext> context;
    std::unique_ptr<MockKernelWithInternals> kernel;

    bool heaplessEnabled = false;
};

using XeHPAndLaterDispatchWalkerBasicTest = Test<XeHPAndLaterDispatchWalkerBasicFixture>;

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterDispatchWalkerBasicTest, whenWorkDimOneThenLocalWorkSizeEqualsLocalXDim) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    DefaultWalkerType *computeWalker = static_cast<DefaultWalkerType *>(linearStream.getSpace(sizeof(DefaultWalkerType)));
    *computeWalker = FamilyType::template getInitGpuWalker<DefaultWalkerType>();

    auto localWorkSize = GpgpuWalkerHelper<FamilyType>::setGpgpuWalkerThreadData(
        computeWalker, kernel->kernelInfo.kernelDescriptor, startWorkGroups, numWorkGroups, localWorkSizesIn, simd, 3, true, false, 5u);
    EXPECT_EQ(localWorkSizesIn[0], localWorkSize);

    EXPECT_EQ(0u, computeWalker->getLocalXMaximum());
    EXPECT_EQ(0u, computeWalker->getLocalYMaximum());
    EXPECT_EQ(0u, computeWalker->getLocalZMaximum());

    EXPECT_EQ(0u, computeWalker->getEmitLocalId());
    EXPECT_EQ(0u, computeWalker->getGenerateLocalId());
    EXPECT_EQ(0u, computeWalker->getEmitInlineParameter());
    EXPECT_EQ(0u, computeWalker->getWalkOrder());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterDispatchWalkerBasicTest, whenWorkDimTwoThenLocalWorkSizeEqualsProductLocalXandYDim) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    DefaultWalkerType *computeWalker = static_cast<DefaultWalkerType *>(linearStream.getSpace(sizeof(DefaultWalkerType)));
    *computeWalker = FamilyType::template getInitGpuWalker<DefaultWalkerType>();

    localWorkSizesIn[1] = 8;

    auto localWorkSize = GpgpuWalkerHelper<FamilyType>::setGpgpuWalkerThreadData(
        computeWalker, kernel->kernelInfo.kernelDescriptor, startWorkGroups, numWorkGroups, localWorkSizesIn, simd, 3, true, false, 0u);
    EXPECT_EQ(localWorkSizesIn[0] * localWorkSizesIn[1], localWorkSize);

    EXPECT_EQ(0u, computeWalker->getLocalXMaximum());
    EXPECT_EQ(0u, computeWalker->getLocalYMaximum());
    EXPECT_EQ(0u, computeWalker->getLocalZMaximum());

    EXPECT_EQ(0u, computeWalker->getEmitLocalId());
    EXPECT_EQ(0u, computeWalker->getGenerateLocalId());
    EXPECT_EQ(0u, computeWalker->getEmitInlineParameter());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterDispatchWalkerBasicTest, whenWorkDimThreeThenLocalWorkSizeEqualsProductLocalXandYandZDim) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    DefaultWalkerType *computeWalker = static_cast<DefaultWalkerType *>(linearStream.getSpace(sizeof(DefaultWalkerType)));
    *computeWalker = FamilyType::template getInitGpuWalker<DefaultWalkerType>();

    localWorkSizesIn[1] = 8;
    localWorkSizesIn[2] = 2;

    auto localWorkSize = GpgpuWalkerHelper<FamilyType>::setGpgpuWalkerThreadData(
        computeWalker, kernel->kernelInfo.kernelDescriptor, startWorkGroups, numWorkGroups, localWorkSizesIn, simd, 3, true, false, 0u);
    EXPECT_EQ(localWorkSizesIn[0] * localWorkSizesIn[1] * localWorkSizesIn[2], localWorkSize);

    EXPECT_EQ(0u, computeWalker->getLocalXMaximum());
    EXPECT_EQ(0u, computeWalker->getLocalYMaximum());
    EXPECT_EQ(0u, computeWalker->getLocalZMaximum());

    EXPECT_EQ(0u, computeWalker->getEmitLocalId());
    EXPECT_EQ(0u, computeWalker->getGenerateLocalId());
    EXPECT_EQ(0u, computeWalker->getEmitInlineParameter());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterDispatchWalkerBasicTest, givenWorkDimOneWhenAskHwForLocalIdsThenExpectGenerationFieldsSet) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    DefaultWalkerType *computeWalker = static_cast<DefaultWalkerType *>(linearStream.getSpace(sizeof(DefaultWalkerType)));
    *computeWalker = FamilyType::template getInitGpuWalker<DefaultWalkerType>();

    kernel->kernelInfo.setLocalIds({1, 0, 0});

    GpgpuWalkerHelper<FamilyType>::setGpgpuWalkerThreadData(computeWalker, kernel->kernelInfo.kernelDescriptor, startWorkGroups, numWorkGroups,
                                                            localWorkSizesIn, simd, 1, false, false, 4u);

    auto localX = static_cast<size_t>(computeWalker->getLocalXMaximum() + 1);
    auto localY = static_cast<size_t>(computeWalker->getLocalYMaximum() + 1);
    auto localZ = static_cast<size_t>(computeWalker->getLocalZMaximum() + 1);
    EXPECT_EQ(localWorkSizesIn[0], localX);
    EXPECT_EQ(localWorkSizesIn[1], localY);
    EXPECT_EQ(localWorkSizesIn[2], localZ);

    constexpr uint32_t expectedEmit = (1 << 0);
    EXPECT_EQ(expectedEmit, computeWalker->getEmitLocalId());
    EXPECT_EQ(1u, computeWalker->getGenerateLocalId());
    EXPECT_EQ(0u, computeWalker->getEmitInlineParameter());
    EXPECT_EQ(4u, computeWalker->getWalkOrder());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterDispatchWalkerBasicTest, givenWorkDimTwoWhenOnlyYIdPresentAskHwForLocalIdsThenExpectGenerationFieldsSet) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    DefaultWalkerType *computeWalker = static_cast<DefaultWalkerType *>(linearStream.getSpace(sizeof(DefaultWalkerType)));
    *computeWalker = FamilyType::template getInitGpuWalker<DefaultWalkerType>();

    kernel->kernelInfo.setLocalIds({0, 1, 0});
    localWorkSizesIn[1] = 16;
    localWorkSizesIn[0] = localWorkSizesIn[2] = 1;

    GpgpuWalkerHelper<FamilyType>::setGpgpuWalkerThreadData(computeWalker, kernel->kernelInfo.kernelDescriptor, startWorkGroups, numWorkGroups,
                                                            localWorkSizesIn, simd, 2, false, false, 0u);

    auto localX = static_cast<size_t>(computeWalker->getLocalXMaximum() + 1);
    auto localY = static_cast<size_t>(computeWalker->getLocalYMaximum() + 1);
    auto localZ = static_cast<size_t>(computeWalker->getLocalZMaximum() + 1);
    EXPECT_EQ(localWorkSizesIn[0], localX);
    EXPECT_EQ(localWorkSizesIn[1], localY);
    EXPECT_EQ(localWorkSizesIn[2], localZ);

    constexpr uint32_t expectedEmit = (1 << 1);
    EXPECT_EQ(expectedEmit, computeWalker->getEmitLocalId());
    EXPECT_EQ(1u, computeWalker->getGenerateLocalId());
    EXPECT_EQ(0u, computeWalker->getEmitInlineParameter());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterDispatchWalkerBasicTest, givenWorkThreeTwoWhenOnlyZIdPresentAskHwForLocalIdsThenExpectGenerationFieldsSet) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    DefaultWalkerType *computeWalker = static_cast<DefaultWalkerType *>(linearStream.getSpace(sizeof(DefaultWalkerType)));
    *computeWalker = FamilyType::template getInitGpuWalker<DefaultWalkerType>();

    kernel->kernelInfo.setLocalIds({0, 0, 1});
    localWorkSizesIn[2] = 16;
    localWorkSizesIn[0] = localWorkSizesIn[1] = 1;

    GpgpuWalkerHelper<FamilyType>::setGpgpuWalkerThreadData(computeWalker, kernel->kernelInfo.kernelDescriptor, startWorkGroups, numWorkGroups,
                                                            localWorkSizesIn, simd, 2, false, false, 0u);

    auto localX = static_cast<size_t>(computeWalker->getLocalXMaximum() + 1);
    auto localY = static_cast<size_t>(computeWalker->getLocalYMaximum() + 1);
    auto localZ = static_cast<size_t>(computeWalker->getLocalZMaximum() + 1);
    EXPECT_EQ(localWorkSizesIn[0], localX);
    EXPECT_EQ(localWorkSizesIn[1], localY);
    EXPECT_EQ(localWorkSizesIn[2], localZ);

    constexpr uint32_t expectedEmit = (1 << 2);
    EXPECT_EQ(expectedEmit, computeWalker->getEmitLocalId());
    EXPECT_EQ(1u, computeWalker->getGenerateLocalId());
    EXPECT_EQ(0u, computeWalker->getEmitInlineParameter());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterDispatchWalkerBasicTest, givenDifferentSIMDsizesWhenLocalIdsGeneratedThenMessageSizeIsSetToProperValue) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    DefaultWalkerType *computeWalker = static_cast<DefaultWalkerType *>(linearStream.getSpace(sizeof(DefaultWalkerType)));
    *computeWalker = FamilyType::template getInitGpuWalker<DefaultWalkerType>();

    kernel->kernelInfo.setLocalIds({0, 0, 1});
    localWorkSizesIn[2] = 16;
    localWorkSizesIn[0] = localWorkSizesIn[1] = 1;

    uint32_t simdProgramming[3][2] = {{32, 2}, {16, 1}, {8, 0}};                           // {given, expected}
    bool walkerInput[4][2] = {{false, false}, {true, false}, {false, true}, {true, true}}; // {runtime local ids, inline data}

    for (uint32_t i = 0; i < 4; i++) {
        for (uint32_t j = 0; j < 3; j++) {
            *computeWalker = FamilyType::template getInitGpuWalker<DefaultWalkerType>();
            GpgpuWalkerHelper<FamilyType>::setGpgpuWalkerThreadData(computeWalker, kernel->kernelInfo.kernelDescriptor, startWorkGroups, numWorkGroups,
                                                                    localWorkSizesIn, simdProgramming[j][0], 2,
                                                                    walkerInput[i][0], walkerInput[i][1], 0u);
            EXPECT_EQ(simdProgramming[j][1], computeWalker->getMessageSimd());
        }
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterDispatchWalkerBasicTest, givenDebugFlagWhenItIsSetThenMessageSimdIsOverwritten) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ForceSimdMessageSizeInWalker.set(1);
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    DefaultWalkerType *computeWalker = static_cast<DefaultWalkerType *>(linearStream.getSpace(sizeof(DefaultWalkerType)));
    *computeWalker = FamilyType::template getInitGpuWalker<DefaultWalkerType>();

    kernel->kernelInfo.setLocalIds({0, 0, 1});
    localWorkSizesIn[2] = 16;
    localWorkSizesIn[0] = localWorkSizesIn[1] = 1;

    uint32_t simdProgramming[3] = {32, 16, 8};
    bool walkerInput[4][2] = {{false, false}, {true, false}, {false, true}, {true, true}}; // {runtime local ids, inline data}

    for (uint32_t i = 0; i < 4; i++) {
        for (uint32_t j = 0; j < 3; j++) {
            *computeWalker = FamilyType::template getInitGpuWalker<DefaultWalkerType>();
            GpgpuWalkerHelper<FamilyType>::setGpgpuWalkerThreadData(computeWalker, kernel->kernelInfo.kernelDescriptor, startWorkGroups, numWorkGroups,
                                                                    localWorkSizesIn, simdProgramming[j], 2,
                                                                    walkerInput[i][0], walkerInput[i][1], 0u);
            EXPECT_EQ(1u, computeWalker->getMessageSimd());
        }
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterDispatchWalkerBasicTest, givenWorkDimTwoWhenAskHwForLocalIdsThenExpectGenerationFieldsSet) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    DefaultWalkerType *computeWalker = static_cast<DefaultWalkerType *>(linearStream.getSpace(sizeof(DefaultWalkerType)));

    *computeWalker = FamilyType::template getInitGpuWalker<DefaultWalkerType>();

    kernel->kernelInfo.setLocalIds({1, 1, 0});
    localWorkSizesIn[1] = 8;

    GpgpuWalkerHelper<FamilyType>::setGpgpuWalkerThreadData(computeWalker, kernel->kernelInfo.kernelDescriptor, startWorkGroups, numWorkGroups,
                                                            localWorkSizesIn, simd, 2, false, false, 0u);

    auto localX = static_cast<size_t>(computeWalker->getLocalXMaximum() + 1);
    auto localY = static_cast<size_t>(computeWalker->getLocalYMaximum() + 1);
    auto localZ = static_cast<size_t>(computeWalker->getLocalZMaximum() + 1);
    EXPECT_EQ(localWorkSizesIn[0], localX);
    EXPECT_EQ(localWorkSizesIn[1], localY);
    EXPECT_EQ(localWorkSizesIn[2], localZ);

    constexpr uint32_t expectedEmit = (1 << 0) | (1 << 1);
    EXPECT_EQ(expectedEmit, computeWalker->getEmitLocalId());
    EXPECT_EQ(1u, computeWalker->getGenerateLocalId());
    EXPECT_EQ(0u, computeWalker->getEmitInlineParameter());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterDispatchWalkerBasicTest, givenWorkDimThreeWhenAskHwForLocalIdsThenExpectGenerationFieldsSet) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    DefaultWalkerType *computeWalker = static_cast<DefaultWalkerType *>(linearStream.getSpace(sizeof(DefaultWalkerType)));
    *computeWalker = FamilyType::template getInitGpuWalker<DefaultWalkerType>();

    kernel->kernelInfo.setLocalIds({1, 1, 1});
    localWorkSizesIn[1] = 8;
    localWorkSizesIn[2] = 2;

    GpgpuWalkerHelper<FamilyType>::setGpgpuWalkerThreadData(computeWalker, kernel->kernelInfo.kernelDescriptor, startWorkGroups, numWorkGroups,
                                                            localWorkSizesIn, simd, 3, false, false, 0u);
    auto localX = static_cast<size_t>(computeWalker->getLocalXMaximum() + 1);
    auto localY = static_cast<size_t>(computeWalker->getLocalYMaximum() + 1);
    auto localZ = static_cast<size_t>(computeWalker->getLocalZMaximum() + 1);
    EXPECT_EQ(localWorkSizesIn[0], localX);
    EXPECT_EQ(localWorkSizesIn[1], localY);
    EXPECT_EQ(localWorkSizesIn[2], localZ);

    constexpr uint32_t expectedEmit = (1 << 0) | (1 << 1) | (1 << 2);
    EXPECT_EQ(expectedEmit, computeWalker->getEmitLocalId());
    EXPECT_EQ(1u, computeWalker->getGenerateLocalId());
    EXPECT_EQ(0u, computeWalker->getEmitInlineParameter());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterDispatchWalkerBasicTest, givenWorkDimThreeWhenAskHwForLocalIdsAndNoLocalIdsUsedThenExpectNoGenerationFieldsSet) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    DefaultWalkerType *computeWalker = static_cast<DefaultWalkerType *>(linearStream.getSpace(sizeof(DefaultWalkerType)));
    *computeWalker = FamilyType::template getInitGpuWalker<DefaultWalkerType>();

    kernel->kernelInfo.setLocalIds({0, 0, 0});
    localWorkSizesIn[1] = 8;
    localWorkSizesIn[2] = 2;

    GpgpuWalkerHelper<FamilyType>::setGpgpuWalkerThreadData(computeWalker, kernel->kernelInfo.kernelDescriptor, startWorkGroups, numWorkGroups,
                                                            localWorkSizesIn, simd, 3, false, false, 0u);

    EXPECT_EQ(0u, computeWalker->getLocalXMaximum());
    EXPECT_EQ(0u, computeWalker->getLocalYMaximum());
    EXPECT_EQ(0u, computeWalker->getLocalZMaximum());

    constexpr uint32_t expectedEmit = 0;
    EXPECT_EQ(expectedEmit, computeWalker->getEmitLocalId());
    EXPECT_EQ(0u, computeWalker->getGenerateLocalId());
    EXPECT_EQ(0u, computeWalker->getEmitInlineParameter());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterDispatchWalkerBasicTest, givenWorkDimThreeWhenNotAskHwForLocalIdsAndLocalIdsUsedThenExpectNoGenerationFieldsSet) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    DefaultWalkerType *computeWalker = static_cast<DefaultWalkerType *>(linearStream.getSpace(sizeof(DefaultWalkerType)));
    *computeWalker = FamilyType::template getInitGpuWalker<DefaultWalkerType>();

    kernel->kernelInfo.setLocalIds({1, 1, 1});
    localWorkSizesIn[1] = 8;
    localWorkSizesIn[2] = 2;

    GpgpuWalkerHelper<FamilyType>::setGpgpuWalkerThreadData(computeWalker, kernel->kernelInfo.kernelDescriptor, startWorkGroups, numWorkGroups,
                                                            localWorkSizesIn, simd, 3, true, false, 0u);

    EXPECT_EQ(0u, computeWalker->getLocalXMaximum());
    EXPECT_EQ(0u, computeWalker->getLocalYMaximum());
    EXPECT_EQ(0u, computeWalker->getLocalZMaximum());

    constexpr uint32_t expectedEmit = 0;
    EXPECT_EQ(expectedEmit, computeWalker->getEmitLocalId());
    EXPECT_EQ(0u, computeWalker->getGenerateLocalId());
    EXPECT_EQ(0u, computeWalker->getEmitInlineParameter());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterDispatchWalkerBasicTest, givenWorkDimOneWhenAskForInlineDataAndNoLocalIdsPresentThenExpectOnlyInlineFieldSet) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    DefaultWalkerType *computeWalker = static_cast<DefaultWalkerType *>(linearStream.getSpace(sizeof(DefaultWalkerType)));
    *computeWalker = FamilyType::template getInitGpuWalker<DefaultWalkerType>();

    GpgpuWalkerHelper<FamilyType>::setGpgpuWalkerThreadData(computeWalker, kernel->kernelInfo.kernelDescriptor, startWorkGroups, numWorkGroups,
                                                            localWorkSizesIn, simd, 1, true, true, 0u);

    EXPECT_EQ(0u, computeWalker->getLocalXMaximum());
    EXPECT_EQ(0u, computeWalker->getLocalYMaximum());
    EXPECT_EQ(0u, computeWalker->getLocalZMaximum());

    EXPECT_EQ(0u, computeWalker->getEmitLocalId());
    EXPECT_EQ(0u, computeWalker->getGenerateLocalId());
    EXPECT_EQ(1u, computeWalker->getEmitInlineParameter());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterDispatchWalkerBasicTest, givenWorkDimOneWhenAskForInlineDataAndLocalIdsPresentThenExpectInlineAndDoNotExpectEmitLocalIdFieldSet) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    DefaultWalkerType *computeWalker = static_cast<DefaultWalkerType *>(linearStream.getSpace(sizeof(DefaultWalkerType)));
    *computeWalker = FamilyType::template getInitGpuWalker<DefaultWalkerType>();

    kernel->kernelInfo.setLocalIds({1, 0, 0});

    GpgpuWalkerHelper<FamilyType>::setGpgpuWalkerThreadData(computeWalker, kernel->kernelInfo.kernelDescriptor, startWorkGroups, numWorkGroups,
                                                            localWorkSizesIn, simd, 1, true, true, 0u);

    EXPECT_EQ(0u, computeWalker->getLocalXMaximum());
    EXPECT_EQ(0u, computeWalker->getLocalYMaximum());
    EXPECT_EQ(0u, computeWalker->getLocalZMaximum());

    constexpr uint32_t expectedEmit = 0u;
    EXPECT_EQ(expectedEmit, computeWalker->getEmitLocalId());
    EXPECT_EQ(0u, computeWalker->getGenerateLocalId());
    EXPECT_EQ(1u, computeWalker->getEmitInlineParameter());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterDispatchWalkerBasicTest, givenWorkDimThreeWhenAskForInlineDataAndLocalIdsPresentThenDoNotExpectEmitLocalIdFieldSetButExpectInlineSet) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    DefaultWalkerType *computeWalker = static_cast<DefaultWalkerType *>(linearStream.getSpace(sizeof(DefaultWalkerType)));
    *computeWalker = FamilyType::template getInitGpuWalker<DefaultWalkerType>();

    kernel->kernelInfo.setLocalIds({1, 1, 1});
    GpgpuWalkerHelper<FamilyType>::setGpgpuWalkerThreadData(computeWalker, kernel->kernelInfo.kernelDescriptor, startWorkGroups, numWorkGroups,
                                                            localWorkSizesIn, simd, 3, true, true, 0u);

    EXPECT_EQ(0u, computeWalker->getLocalXMaximum());
    EXPECT_EQ(0u, computeWalker->getLocalYMaximum());
    EXPECT_EQ(0u, computeWalker->getLocalZMaximum());

    constexpr uint32_t expectedEmit = 0u;
    EXPECT_EQ(expectedEmit, computeWalker->getEmitLocalId());
    EXPECT_EQ(0u, computeWalker->getGenerateLocalId());
    EXPECT_EQ(1u, computeWalker->getEmitInlineParameter());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterDispatchWalkerBasicTest, givenWorkDimThreeWhenAskHwForLocalIdsAndInlineDataThenExpectGenerationFieldsSet) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    DefaultWalkerType *computeWalker = static_cast<DefaultWalkerType *>(linearStream.getSpace(sizeof(DefaultWalkerType)));
    *computeWalker = FamilyType::template getInitGpuWalker<DefaultWalkerType>();

    kernel->kernelInfo.setLocalIds({1, 1, 1});
    localWorkSizesIn[1] = 8;
    localWorkSizesIn[2] = 2;

    GpgpuWalkerHelper<FamilyType>::setGpgpuWalkerThreadData(computeWalker, kernel->kernelInfo.kernelDescriptor, startWorkGroups, numWorkGroups,
                                                            localWorkSizesIn, simd, 3, false, true, 5u);
    auto localX = static_cast<size_t>(computeWalker->getLocalXMaximum() + 1);
    auto localY = static_cast<size_t>(computeWalker->getLocalYMaximum() + 1);
    auto localZ = static_cast<size_t>(computeWalker->getLocalZMaximum() + 1);
    EXPECT_EQ(localWorkSizesIn[0], localX);
    EXPECT_EQ(localWorkSizesIn[1], localY);
    EXPECT_EQ(localWorkSizesIn[2], localZ);

    constexpr uint32_t expectedEmit = (1 << 0) | (1 << 1) | (1 << 2);
    EXPECT_EQ(expectedEmit, computeWalker->getEmitLocalId());
    EXPECT_EQ(1u, computeWalker->getGenerateLocalId());
    EXPECT_EQ(1u, computeWalker->getEmitInlineParameter());
    EXPECT_EQ(5u, computeWalker->getWalkOrder());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterDispatchWalkerBasicTest, givenTimestampPacketWhenDispatchingThenProgramPostSyncData) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    using PostSyncType = decltype(FamilyType::template getPostSyncType<DefaultWalkerType>());

    MockKernelWithInternals kernel1(*device);
    MockKernelWithInternals kernel2(*device);

    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;

    TimestampPacketContainer timestampPacketContainer;
    timestampPacketContainer.add(device->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator()->getTag());
    timestampPacketContainer.add(device->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator()->getTag());

    MockMultiDispatchInfo multiDispatchInfo(device.get(), std::vector<Kernel *>({kernel1.mockKernel, kernel2.mockKernel}));

    MockCommandQueue cmdQ(context.get(), device.get(), nullptr, false);
    auto &cmdStream = cmdQ.getCS(0);

    HardwareInterfaceWalkerArgs walkerArgs = createHardwareInterfaceWalkerArgs(CL_COMMAND_NDRANGE_KERNEL);
    walkerArgs.currentTimestampPacketNodes = &timestampPacketContainer;
    HardwareInterface<FamilyType>::template dispatchWalker<DefaultWalkerType>(
        cmdQ,
        multiDispatchInfo,
        CsrDependencies(),
        walkerArgs);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(cmdStream, 0);
    hwParser.findHardwareCommands<FamilyType>();
    EXPECT_NE(hwParser.itorWalker, hwParser.cmdList.end());

    auto gmmHelper = device->getGmmHelper();
    auto expectedMocs = MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, device->getRootDeviceEnvironment()) ? gmmHelper->getUncachedMOCS() : gmmHelper->getL3EnabledMOCS();

    auto walker = genCmdCast<DefaultWalkerType *>(*hwParser.itorWalker);
    EXPECT_EQ(PostSyncType::OPERATION::OPERATION_WRITE_TIMESTAMP, walker->getPostSync().getOperation());
    EXPECT_TRUE(walker->getPostSync().getDataportPipelineFlush());
    EXPECT_EQ(expectedMocs, walker->getPostSync().getMocs());
    auto contextStartAddress = TimestampPacketHelper::getContextStartGpuAddress(*timestampPacketContainer.peekNodes()[0]);
    EXPECT_EQ(contextStartAddress, walker->getPostSync().getDestinationAddress());

    auto secondWalkerItor = NEO::UnitTestHelper<FamilyType>::findWalkerTypeCmd(++hwParser.itorWalker, hwParser.cmdList.end());
    auto secondWalker = genCmdCast<DefaultWalkerType *>(*secondWalkerItor);

    EXPECT_EQ(PostSyncType::OPERATION::OPERATION_WRITE_TIMESTAMP, secondWalker->getPostSync().getOperation());
    EXPECT_TRUE(secondWalker->getPostSync().getDataportPipelineFlush());
    EXPECT_EQ(expectedMocs, walker->getPostSync().getMocs());
    contextStartAddress = TimestampPacketHelper::getContextStartGpuAddress(*timestampPacketContainer.peekNodes()[1]);
    EXPECT_EQ(contextStartAddress, secondWalker->getPostSync().getDestinationAddress());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterDispatchWalkerBasicTest, givenDebugVariableEnabledWhenEnqueueingThenWriteWalkerStamp) {
    using WalkerType = typename FamilyType::DefaultWalkerType;
    using PostSyncType = decltype(FamilyType::template getPostSyncType<WalkerType>());
    debugManager.flags.EnableTimestampPacket.set(true);

    auto testDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    MockContext testContext(testDevice.get());
    auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(&testContext, testDevice.get(), nullptr);
    MockKernelWithInternals testKernel(*testDevice, &testContext);

    size_t gws[] = {1, 1, 1};
    cmdQ->enqueueKernel(testKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    EXPECT_NE(nullptr, cmdQ->timestampPacketContainer.get());

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(cmdQ->getCS(0), 0);
    hwParser.findHardwareCommands<FamilyType>();
    EXPECT_NE(hwParser.itorWalker, hwParser.cmdList.end());

    auto gmmHelper = device->getGmmHelper();
    auto expectedMocs = MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, device->getRootDeviceEnvironment()) ? gmmHelper->getUncachedMOCS() : gmmHelper->getL3EnabledMOCS();

    auto walker = genCmdCast<WalkerType *>(*hwParser.itorWalker);

    auto &postSyncData = walker->getPostSync();
    EXPECT_EQ(PostSyncType::OPERATION::OPERATION_WRITE_TIMESTAMP, postSyncData.getOperation());
    EXPECT_TRUE(postSyncData.getDataportPipelineFlush());
    EXPECT_EQ(expectedMocs, postSyncData.getMocs());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterDispatchWalkerBasicTest, givenDebugVariableEnabledWhenMocsValueIsOverwrittenThenPostSyncContainsProperSetting) {
    using WalkerType = typename FamilyType::DefaultWalkerType;

    auto mocsValue = 8u;
    debugManager.flags.EnableTimestampPacket.set(true);
    debugManager.flags.OverridePostSyncMocs.set(mocsValue);

    auto testDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    MockContext testContext(testDevice.get());
    auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(&testContext, testDevice.get(), nullptr);
    MockKernelWithInternals testKernel(*testDevice, &testContext);

    size_t gws[] = {1, 1, 1};
    cmdQ->enqueueKernel(testKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    EXPECT_NE(nullptr, cmdQ->timestampPacketContainer.get());

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(cmdQ->getCS(0), 0);
    hwParser.findHardwareCommands<FamilyType>();
    EXPECT_NE(hwParser.itorWalker, hwParser.cmdList.end());

    auto walker = genCmdCast<WalkerType *>(*hwParser.itorWalker);
    auto &postSyncData = walker->getPostSync();
    EXPECT_EQ(mocsValue, postSyncData.getMocs());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterDispatchWalkerBasicTest, givenTimestampPacketWriteEnabledWhenEstimatingStreamSizeThenAddEnoughSpace) {
    MockCommandQueueHw<FamilyType> cmdQ(context.get(), device.get(), nullptr);
    MockKernelWithInternals kernel1(*device);
    MockKernelWithInternals kernel2(*device);
    MockMultiDispatchInfo multiDispatchInfo(device.get(), std::vector<Kernel *>({kernel1.mockKernel, kernel2.mockKernel}));
    const auto &hwInfo = device->getHardwareInfo();
    const auto &productHelper = device->getProductHelper();
    const bool isResolveDependenciesByPipeControlsEnabled = productHelper.isResolveDependenciesByPipeControlsSupported(hwInfo, cmdQ.isOOQEnabled(), cmdQ.taskCount, cmdQ.getGpgpuCommandStreamReceiver());

    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = false;
    getCommandStream<FamilyType, CL_COMMAND_NDRANGE_KERNEL>(cmdQ, CsrDependencies(), false, false, false, multiDispatchInfo, nullptr, 0, false, false, isResolveDependenciesByPipeControlsEnabled, nullptr);
    size_t sizeWithDisabled = cmdQ.requestedCmdStreamSize;

    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;
    getCommandStream<FamilyType, CL_COMMAND_NDRANGE_KERNEL>(cmdQ, CsrDependencies(), false, false, false, multiDispatchInfo, nullptr, 0, false, false, isResolveDependenciesByPipeControlsEnabled, nullptr);
    size_t sizeWithEnabled = cmdQ.requestedCmdStreamSize;

    size_t additionalSize = 0u;

    if (isResolveDependenciesByPipeControlsEnabled) {
        additionalSize = MemorySynchronizationCommands<FamilyType>::getSizeForStallingBarrier();
    }

    EXPECT_EQ(sizeWithEnabled, sizeWithDisabled + additionalSize);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterDispatchWalkerBasicTest, givenDebugVariableEnabledWhenEnqueueingThenWritePostsyncOperationInImmWriteMode) {
    using WalkerType = typename FamilyType::DefaultWalkerType;
    using PostSyncType = decltype(FamilyType::template getPostSyncType<WalkerType>());

    debugManager.flags.UseImmDataWriteModeOnPostSyncOperation.set(true);

    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;
    auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context.get(), device.get(), nullptr);

    size_t gws[] = {1, 1, 1};
    cmdQ->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(cmdQ->getCS(0), 0);
    hwParser.findHardwareCommands<FamilyType>();
    EXPECT_NE(hwParser.itorWalker, hwParser.cmdList.end());

    auto contextEndAddress = TimestampPacketHelper::getContextEndGpuAddress(*cmdQ->timestampPacketContainer->peekNodes()[0]);

    auto walker = genCmdCast<WalkerType *>(*hwParser.itorWalker);
    ASSERT_NE(nullptr, walker);

    auto &postSyncData = walker->getPostSync();
    EXPECT_EQ(PostSyncType::OPERATION::OPERATION_WRITE_IMMEDIATE_DATA, postSyncData.getOperation());
    EXPECT_EQ(contextEndAddress, postSyncData.getDestinationAddress());
    EXPECT_EQ(0x2'0000'0002u, postSyncData.getImmediateData());
}

HWTEST2_F(XeHPAndLaterDispatchWalkerBasicTest, givenDebugVariableEnabledWhenEnqueueingThenSystolicIsProgrammed, IsXeCore) {
    debugManager.flags.OverrideSystolicInComputeWalker.set(true);

    auto &compilerProductHelper = device->getCompilerProductHelper();
    auto heaplessEnabled = compilerProductHelper.isHeaplessModeEnabled(*defaultHwInfo);
    if (heaplessEnabled) {
        GTEST_SKIP();
    }

    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;
    auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context.get(), device.get(), nullptr);

    size_t gws[] = {1, 1, 1};
    cmdQ->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(cmdQ->getCS(0), 0);
    hwParser.findHardwareCommands<FamilyType>();
    EXPECT_NE(hwParser.itorWalker, hwParser.cmdList.end());

    auto walker = genCmdCast<typename FamilyType::COMPUTE_WALKER *>(*hwParser.itorWalker);
    EXPECT_TRUE(walker->getSystolicModeEnable());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterDispatchWalkerBasicTest, givenAutoLocalIdsGenerationEnabledWhenDispatchMeetCriteriaThenExpectNoLocalIdsAndProperIsaAddress) {
    if constexpr (FamilyType::isHeaplessRequired()) {
        GTEST_SKIP();
    } else {
        using WalkerType = typename FamilyType::DefaultWalkerType;
        using COMPUTE_WALKER = typename FamilyType::COMPUTE_WALKER;

        debugManager.flags.EnableHwGenerationLocalIds.set(1);

        auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context.get(), device.get(), nullptr);
        auto &commandStream = cmdQ->getCS(1024);
        auto usedBeforeCS = commandStream.getUsed();

        auto &kd = kernel->kernelInfo.kernelDescriptor;
        kd.entryPoints.skipPerThreadDataLoad = 128;
        kd.kernelAttributes.localId[0] = 1;
        kd.kernelAttributes.localId[1] = 0;
        kd.kernelAttributes.localId[2] = 0;
        kd.kernelAttributes.numLocalIdChannels = 1;

        auto memoryManager = device->getUltCommandStreamReceiver<FamilyType>().getMemoryManager();
        kernel->kernelInfo.kernelAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{device->getRootDeviceIndex(), MemoryConstants::pageSize});

        size_t gws[] = {16, 1, 1};
        size_t lws[] = {16, 1, 1};
        size_t globalOffsets[] = {0, 0, 0};

        MultiDispatchInfo multiDispatchInfo(kernel->mockKernel);
        DispatchInfoBuilder<SplitDispatch::Dim::d1D, SplitDispatch::SplitMode::noSplit> builder(*device);
        builder.setDispatchGeometry(1, gws, lws, globalOffsets);
        builder.setKernel(kernel->mockKernel);
        builder.bake(multiDispatchInfo);

        cmdQ->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, lws, 0, nullptr, nullptr);
        auto usedAfterCS = commandStream.getUsed();

        HardwareParse hwParser;
        hwParser.parseCommands<FamilyType>(cmdQ->getCS(0), 0);
        hwParser.findHardwareCommands<FamilyType>();
        EXPECT_NE(hwParser.itorWalker, hwParser.cmdList.end());

        auto walker = genCmdCast<WalkerType *>(*hwParser.itorWalker);

        EXPECT_EQ(WalkerType::DWORD_LENGTH_FIXED_SIZE, walker->getDwordLength());
        EXPECT_EQ(0u, walker->getEmitInlineParameter());

        EXPECT_EQ(1u, walker->getGenerateLocalId());
        EXPECT_EQ(1u, walker->getEmitLocalId());

        if constexpr (std::is_same_v<WalkerType, COMPUTE_WALKER>) {
            uint32_t expectedIndirectDataLength = alignUp(this->kernel->mockKernel->getCrossThreadDataSize(), FamilyType::indirectDataAlignment);
            EXPECT_EQ(expectedIndirectDataLength, walker->getIndirectDataLength());
        }

        auto &idd = walker->getInterfaceDescriptor();
        uint64_t expectedKernelStartOffset = this->kernel->kernelInfo.kernelDescriptor.entryPoints.skipPerThreadDataLoad;
        if constexpr (std::is_same_v<WalkerType, COMPUTE_WALKER>) {
            expectedKernelStartOffset += this->kernel->mockKernel->getKernelInfo().getGraphicsAllocation()->getGpuAddressToPatch();
            using KernelStartPointerType = uint32_t;
            EXPECT_EQ(static_cast<KernelStartPointerType>(expectedKernelStartOffset), static_cast<KernelStartPointerType>(idd.getKernelStartPointer()));
        } else {
            expectedKernelStartOffset += this->kernel->mockKernel->getKernelInfo().getGraphicsAllocation()->getGpuAddress();
            using KernelStartPointerType = decltype(idd.getKernelStartPointer());
            EXPECT_EQ(static_cast<KernelStartPointerType>(expectedKernelStartOffset), idd.getKernelStartPointer());
        }

        auto expectedSizeCS = EnqueueOperation<FamilyType>::getTotalSizeRequiredCS(CL_COMMAND_NDRANGE_KERNEL, CsrDependencies(), false, false,
                                                                                   false, *cmdQ.get(), multiDispatchInfo, false, false, false, nullptr);
        expectedSizeCS += sizeof(typename FamilyType::MI_BATCH_BUFFER_END);
        expectedSizeCS = alignUp(expectedSizeCS, MemoryConstants::cacheLineSize);
        EXPECT_GE(expectedSizeCS, usedAfterCS - usedBeforeCS);

        memoryManager->freeGraphicsMemory(kernel->kernelInfo.kernelAllocation);
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterDispatchWalkerBasicTest, givenPassInlineDataEnabledWhenLocalIdsUsedThenDoNotExpectCrossThreadDataInWalkerEmitLocalFieldSet) {
    if constexpr (FamilyType::isHeaplessRequired()) {
        GTEST_SKIP();
    } else {
        using WalkerType = typename FamilyType::DefaultWalkerType;
        using COMPUTE_WALKER = typename FamilyType::COMPUTE_WALKER;

        debugManager.flags.EnablePassInlineData.set(true);
        debugManager.flags.EnableHwGenerationLocalIds.set(0);

        auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context.get(), device.get(), nullptr);
        auto &commandStream = cmdQ->getCS(1024);
        auto usedBeforeCS = commandStream.getUsed();
        auto inlineDataSize = UnitTestHelper<FamilyType>::getInlineDataSize(cmdQ->heaplessModeEnabled);

        auto &kd = kernel->kernelInfo.kernelDescriptor;
        kd.kernelAttributes.flags.passInlineData = true;

        kernel->mockKernel->setCrossThreadData(crossThreadDataGrf, inlineDataSize);

        auto memoryManager = device->getUltCommandStreamReceiver<FamilyType>().getMemoryManager();
        kernel->kernelInfo.kernelAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{device->getRootDeviceIndex(), MemoryConstants::pageSize});

        size_t gws[] = {16, 1, 1};
        size_t lws[] = {16, 1, 1};
        size_t globalOffsets[] = {0, 0, 0};

        MultiDispatchInfo multiDispatchInfo(kernel->mockKernel);
        DispatchInfoBuilder<SplitDispatch::Dim::d1D, SplitDispatch::SplitMode::noSplit> builder(*device);
        builder.setDispatchGeometry(1, gws, lws, globalOffsets);
        builder.setKernel(kernel->mockKernel);
        builder.bake(multiDispatchInfo);

        cmdQ->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, lws, 0, nullptr, nullptr);
        auto usedAfterCS = commandStream.getUsed();

        HardwareParse hwParser;
        hwParser.parseCommands<FamilyType>(cmdQ->getCS(0), 0);
        hwParser.findHardwareCommands<FamilyType>();
        EXPECT_NE(hwParser.itorWalker, hwParser.cmdList.end());

        auto walker = genCmdCast<WalkerType *>(*hwParser.itorWalker);

        EXPECT_EQ(1u, walker->getEmitInlineParameter());

        EXPECT_EQ(0u, walker->getGenerateLocalId());
        constexpr uint32_t expectedEmit = 0u;
        EXPECT_EQ(expectedEmit, walker->getEmitLocalId());
        EXPECT_EQ(0, memcmp(walker->getInlineDataPointer(), crossThreadDataGrf, inlineDataSize));

        uint32_t simd = this->kernel->mockKernel->getKernelInfo().getMaxSimdSize();
        // only X is present
        auto sizePerThreadData = getPerThreadSizeLocalIDs(simd, sizeGrf);
        sizePerThreadData = std::max(sizePerThreadData, sizeGrf);
        size_t perThreadTotalDataSize = getThreadsPerWG(simd, static_cast<uint32_t>(lws[0])) * sizePerThreadData;

        if constexpr (std::is_same_v<WalkerType, COMPUTE_WALKER>) {
            uint32_t expectedIndirectDataLength = alignUp(static_cast<uint32_t>(perThreadTotalDataSize), FamilyType::indirectDataAlignment);
            EXPECT_EQ(expectedIndirectDataLength, walker->getIndirectDataLength());
        }

        auto &idd = walker->getInterfaceDescriptor();

        if constexpr (std::is_same_v<WalkerType, COMPUTE_WALKER>) {
            auto expectedKernelStartOffset = this->kernel->mockKernel->getKernelInfo().getGraphicsAllocation()->getGpuAddressToPatch();
            using KernelStartPointerType = uint32_t;
            EXPECT_EQ(static_cast<KernelStartPointerType>(expectedKernelStartOffset), static_cast<KernelStartPointerType>(idd.getKernelStartPointer()));
        } else {
            auto expectedKernelStartOffset = this->kernel->mockKernel->getKernelInfo().getGraphicsAllocation()->getGpuAddress();
            using KernelStartPointerType = decltype(idd.getKernelStartPointer());
            EXPECT_EQ(static_cast<KernelStartPointerType>(expectedKernelStartOffset), idd.getKernelStartPointer());
        }

        auto expectedSizeCS = EnqueueOperation<FamilyType>::getTotalSizeRequiredCS(CL_COMMAND_NDRANGE_KERNEL, CsrDependencies(), false, false,
                                                                                   false, *cmdQ.get(), multiDispatchInfo, false, false, false, nullptr);
        expectedSizeCS += sizeof(typename FamilyType::MI_BATCH_BUFFER_END);
        expectedSizeCS = alignUp(expectedSizeCS, MemoryConstants::cacheLineSize);
        EXPECT_GE(expectedSizeCS, usedAfterCS - usedBeforeCS);
        memoryManager->freeGraphicsMemory(kernel->kernelInfo.kernelAllocation);
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterDispatchWalkerBasicTest, givenExecutionMaskWithoutReminderWhenProgrammingWalkerThenSetValidNumberOfBitsInMask) {
    using WalkerType = typename FamilyType::DefaultWalkerType;

    auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context.get(), device.get(), nullptr);
    std::array<uint32_t, 4> testedSimd = {{1, 8, 16, 32}};

    for (auto simd : testedSimd) {
        kernel->kernelInfo.kernelDescriptor.kernelAttributes.simdSize = simd;

        auto kernelSimd = kernel->mockKernel->getKernelInfo().getMaxSimdSize();
        EXPECT_EQ(simd, kernelSimd);

        size_t gws[] = {kernelSimd, 1, 1};
        size_t lws[] = {kernelSimd, 1, 1};

        auto streamOffset = cmdQ->getCS(0).getUsed();
        cmdQ->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, lws, 0, nullptr, nullptr);

        HardwareParse hwParser;
        hwParser.parseCommands<FamilyType>(cmdQ->getCS(0), streamOffset);
        hwParser.findHardwareCommands<FamilyType>();

        auto walker = genCmdCast<WalkerType *>(*hwParser.itorWalker);
        if (isSimd1(simd)) {
            EXPECT_EQ(maxNBitValue(32), walker->getExecutionMask());
        } else {
            EXPECT_EQ(maxNBitValue(simd), walker->getExecutionMask());
        }
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterDispatchWalkerBasicTest, givenPassInlineDataEnabledWhenLocalIdsUsedAndCrossThreadIsTwoGrfsThenExpectFirstCrossThreadDataInWalkerSecondInPayloadWithPerThread) {
    if constexpr (FamilyType::isHeaplessRequired()) {
        GTEST_SKIP();
    } else {
        using WalkerType = typename FamilyType::DefaultWalkerType;
        using COMPUTE_WALKER = typename FamilyType::COMPUTE_WALKER;

        debugManager.flags.EnablePassInlineData.set(true);
        debugManager.flags.EnableHwGenerationLocalIds.set(false);

        auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context.get(), device.get(), nullptr);

        IndirectHeap &ih = cmdQ->getIndirectHeap(IndirectHeap::Type::indirectObject, 2048);

        auto &kd = kernel->kernelInfo.kernelDescriptor;
        kd.kernelAttributes.flags.passInlineData = true;
        kd.kernelAttributes.localId[0] = 1;
        kd.kernelAttributes.localId[1] = 0;
        kd.kernelAttributes.localId[2] = 0;
        kd.kernelAttributes.numLocalIdChannels = 1;

        auto inlineDataSize = UnitTestHelper<FamilyType>::getInlineDataSize(cmdQ->heaplessModeEnabled);

        kernel->mockKernel->setCrossThreadData(crossThreadDataTwoGrf, inlineDataSize * 2);

        auto memoryManager = device->getUltCommandStreamReceiver<FamilyType>().getMemoryManager();
        kernel->kernelInfo.kernelAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{device->getRootDeviceIndex(), MemoryConstants::pageSize});

        size_t gws[] = {16, 1, 1};
        size_t lws[] = {16, 1, 1};
        size_t globalOffsets[] = {0, 0, 0};

        MultiDispatchInfo multiDispatchInfo(kernel->mockKernel);
        DispatchInfoBuilder<SplitDispatch::Dim::d1D, SplitDispatch::SplitMode::noSplit> builder(*device);
        builder.setDispatchGeometry(1, gws, lws, globalOffsets);
        builder.setKernel(kernel->mockKernel);
        builder.bake(multiDispatchInfo);

        cmdQ->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, lws, 0, nullptr, nullptr);

        HardwareParse hwParser;
        hwParser.parseCommands<FamilyType>(cmdQ->getCS(0), 0);
        hwParser.findHardwareCommands<FamilyType>();
        EXPECT_NE(hwParser.itorWalker, hwParser.cmdList.end());
        void *payloadData = ih.getCpuBase();
        auto lwsX = static_cast<uint32_t>(lws[0]);

        auto walker = genCmdCast<WalkerType *>(*hwParser.itorWalker);
        EXPECT_EQ(1u, walker->getEmitInlineParameter());

        EXPECT_EQ(0u, walker->getGenerateLocalId());
        constexpr uint32_t expectedEmit = 0u;
        EXPECT_EQ(expectedEmit, walker->getEmitLocalId());
        EXPECT_EQ(0, memcmp(walker->getInlineDataPointer(), crossThreadDataTwoGrf, inlineDataSize));
        EXPECT_EQ(0, memcmp(payloadData, &crossThreadDataTwoGrf[inlineDataSize / sizeof(uint32_t)], inlineDataSize));

        uint32_t simd = kernel->mockKernel->getKernelInfo().getMaxSimdSize();
        // only X is present
        uint32_t localIdSizePerThread = getPerThreadSizeLocalIDs(simd, sizeGrf);
        localIdSizePerThread = std::max(localIdSizePerThread, sizeGrf);
        auto sizePerThreadData = getThreadsPerWG(simd, lwsX) * localIdSizePerThread;

        auto crossThreadDataSize = kernel->mockKernel->getCrossThreadDataSize();
        crossThreadDataSize -= std::min(static_cast<uint32_t>(inlineDataSize), crossThreadDataSize);

        // second GRF in indirect
        if constexpr (std::is_same_v<WalkerType, COMPUTE_WALKER>) {
            uint32_t expectedIndirectDataLength = sizePerThreadData + crossThreadDataSize;
            expectedIndirectDataLength = alignUp(expectedIndirectDataLength, FamilyType::indirectDataAlignment);
            EXPECT_EQ(expectedIndirectDataLength, walker->getIndirectDataLength());
        }

        memoryManager->freeGraphicsMemory(kernel->kernelInfo.kernelAllocation);
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterDispatchWalkerBasicTest, givenKernelWithoutLocalIdsAndPassInlineDataEnabledWhenNoHWGenerationOfLocalIdsUsedThenExpectCrossThreadDataInWalkerAndNoEmitLocalFieldSet) {
    if constexpr (FamilyType::isHeaplessRequired()) {
        GTEST_SKIP();
    } else {
        using WalkerType = typename FamilyType::DefaultWalkerType;
        using COMPUTE_WALKER = typename FamilyType::COMPUTE_WALKER;

        debugManager.flags.EnablePassInlineData.set(true);
        debugManager.flags.EnableHwGenerationLocalIds.set(false);

        auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context.get(), device.get(), nullptr);

        auto &kd = kernel->kernelInfo.kernelDescriptor;
        auto inlineDataSize = UnitTestHelper<FamilyType>::getInlineDataSize(cmdQ->heaplessModeEnabled);
        kd.kernelAttributes.flags.passInlineData = true;
        kd.kernelAttributes.numLocalIdChannels = 0;

        kernel->mockKernel->setCrossThreadData(crossThreadDataGrf, inlineDataSize);

        auto memoryManager = device->getUltCommandStreamReceiver<FamilyType>().getMemoryManager();
        kernel->kernelInfo.kernelAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{device->getRootDeviceIndex(), MemoryConstants::pageSize});

        size_t gws[] = {16, 1, 1};
        size_t lws[] = {16, 1, 1};
        cmdQ->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, lws, 0, nullptr, nullptr);

        HardwareParse hwParser;
        hwParser.parseCommands<FamilyType>(cmdQ->getCS(0), 0);
        hwParser.findHardwareCommands<FamilyType>();
        EXPECT_NE(hwParser.itorWalker, hwParser.cmdList.end());

        auto walker = genCmdCast<WalkerType *>(*hwParser.itorWalker);
        EXPECT_EQ(1u, walker->getEmitInlineParameter());

        EXPECT_EQ(0u, walker->getGenerateLocalId());
        EXPECT_EQ(0u, walker->getEmitLocalId());

        EXPECT_EQ(0, memcmp(walker->getInlineDataPointer(), this->crossThreadDataGrf, inlineDataSize));

        if constexpr (std::is_same_v<WalkerType, COMPUTE_WALKER>) {
            size_t perThreadTotalDataSize = 0U;
            uint32_t expectedIndirectDataLength = static_cast<uint32_t>(perThreadTotalDataSize);
            expectedIndirectDataLength = alignUp(expectedIndirectDataLength, FamilyType::indirectDataAlignment);
            EXPECT_EQ(expectedIndirectDataLength, walker->getIndirectDataLength());
        }

        memoryManager->freeGraphicsMemory(kernel->kernelInfo.kernelAllocation);
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterDispatchWalkerBasicTest, givenPassInlineDataEnabledWhenNoLocalIdsUsedAndCrossThreadIsTwoGrfsThenExpectFirstCrossThreadDataInWalkerSecondInPayload) {
    if constexpr (FamilyType::isHeaplessRequired()) {
        GTEST_SKIP();
    } else {
        using WalkerType = typename FamilyType::DefaultWalkerType;
        using COMPUTE_WALKER = typename FamilyType::COMPUTE_WALKER;

        debugManager.flags.EnablePassInlineData.set(true);
        debugManager.flags.EnableHwGenerationLocalIds.set(false);

        auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context.get(), device.get(), nullptr);
        IndirectHeap &ih = cmdQ->getIndirectHeap(IndirectHeap::Type::indirectObject, 2048);

        auto &kd = kernel->kernelInfo.kernelDescriptor;
        kd.kernelAttributes.flags.passInlineData = true;
        kd.kernelAttributes.numLocalIdChannels = 0;

        auto inlineDataSize = UnitTestHelper<FamilyType>::getInlineDataSize(cmdQ->heaplessModeEnabled);

        kernel->mockKernel->setCrossThreadData(crossThreadDataTwoGrf, inlineDataSize * 2);

        auto memoryManager = device->getUltCommandStreamReceiver<FamilyType>().getMemoryManager();
        kernel->kernelInfo.kernelAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{device->getRootDeviceIndex(), MemoryConstants::pageSize});

        size_t gws[] = {16, 1, 1};
        size_t lws[] = {16, 1, 1};
        cmdQ->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, lws, 0, nullptr, nullptr);

        HardwareParse hwParser;
        hwParser.parseCommands<FamilyType>(cmdQ->getCS(0), 0);
        hwParser.findHardwareCommands<FamilyType>();
        EXPECT_NE(hwParser.itorWalker, hwParser.cmdList.end());

        auto walker = genCmdCast<WalkerType *>(*hwParser.itorWalker);
        EXPECT_EQ(1u, walker->getEmitInlineParameter());

        EXPECT_EQ(0u, walker->getGenerateLocalId());
        EXPECT_EQ(0u, walker->getEmitLocalId());

        EXPECT_EQ(0, memcmp(walker->getInlineDataPointer(), crossThreadDataTwoGrf, inlineDataSize));
        void *payloadData = ih.getCpuBase();
        EXPECT_EQ(0, memcmp(payloadData, &crossThreadDataTwoGrf[inlineDataSize / sizeof(uint32_t)], inlineDataSize));

        if constexpr (std::is_same_v<WalkerType, COMPUTE_WALKER>) {
            size_t perThreadTotalDataSize = 0;
            // second GRF in indirect
            uint32_t expectedIndirectDataLength = static_cast<uint32_t>(perThreadTotalDataSize + inlineDataSize);
            expectedIndirectDataLength = alignUp(expectedIndirectDataLength, FamilyType::indirectDataAlignment);
            EXPECT_EQ(expectedIndirectDataLength, walker->getIndirectDataLength());
        }

        memoryManager->freeGraphicsMemory(kernel->kernelInfo.kernelAllocation);
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterDispatchWalkerBasicTest, givenAllChannelsActiveWithWorkDimOneDimensionThenHwGenerationIsEnabledWithOverwrittenWalkOrder) {
    using WalkerType = typename FamilyType::DefaultWalkerType;

    debugManager.flags.EnableHwGenerationLocalIds.set(true);

    auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context.get(), device.get(), nullptr);

    auto &kd = kernel->kernelInfo.kernelDescriptor;
    kd.kernelAttributes.flags.passInlineData = true;
    kd.kernelAttributes.localId[0] = 1;
    kd.kernelAttributes.localId[1] = 1;
    kd.kernelAttributes.localId[2] = 1;
    kd.kernelAttributes.numLocalIdChannels = 3;

    kernel->mockKernel->setCrossThreadData(crossThreadDataTwoGrf, sizeGrf * 2);

    auto memoryManager = device->getUltCommandStreamReceiver<FamilyType>().getMemoryManager();
    kernel->kernelInfo.kernelAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{device->getRootDeviceIndex(), MemoryConstants::pageSize});

    size_t gws[] = {4000, 1, 1};
    size_t lws[] = {40, 1, 1};
    cmdQ->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, lws, 0, nullptr, nullptr);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(cmdQ->getCS(0), 0);
    hwParser.findHardwareCommands<FamilyType>();
    EXPECT_NE(hwParser.itorWalker, hwParser.cmdList.end());

    auto walker = genCmdCast<WalkerType *>(*hwParser.itorWalker);
    EXPECT_EQ(1u, walker->getGenerateLocalId());
    EXPECT_EQ(7u, walker->getEmitLocalId());
    EXPECT_EQ(4u, walker->getWalkOrder());

    memoryManager->freeGraphicsMemory(kernel->kernelInfo.kernelAllocation);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterDispatchWalkerBasicTest, givenPassInlineDataAndHwLocalIdsGenerationEnabledWhenLocalIdsUsedThenExpectCrossThreadDataInWalkerAndEmitFields) {
    if constexpr (FamilyType::isHeaplessRequired()) {
        GTEST_SKIP();
    } else {
        using WalkerType = typename FamilyType::DefaultWalkerType;
        using COMPUTE_WALKER = typename FamilyType::COMPUTE_WALKER;

        debugManager.flags.EnablePassInlineData.set(true);
        debugManager.flags.EnableHwGenerationLocalIds.set(1);

        auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context.get(), device.get(), nullptr);
        auto inlineDataSize = UnitTestHelper<FamilyType>::getInlineDataSize(cmdQ->heaplessModeEnabled);
        auto &kd = kernel->kernelInfo.kernelDescriptor;
        kd.entryPoints.skipPerThreadDataLoad = 128;
        kd.kernelAttributes.flags.passInlineData = true;
        kd.kernelAttributes.localId[0] = 1;
        kd.kernelAttributes.localId[1] = 0;
        kd.kernelAttributes.localId[2] = 0;
        kd.kernelAttributes.numLocalIdChannels = 1;

        kernel->mockKernel->setCrossThreadData(crossThreadDataGrf, inlineDataSize);

        auto memoryManager = device->getUltCommandStreamReceiver<FamilyType>().getMemoryManager();
        kernel->kernelInfo.kernelAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{device->getRootDeviceIndex(), MemoryConstants::pageSize});

        size_t gws[] = {16, 1, 1};
        size_t lws[] = {16, 1, 1};
        cmdQ->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, lws, 0, nullptr, nullptr);

        HardwareParse hwParser;
        hwParser.parseCommands<FamilyType>(cmdQ->getCS(0), 0);
        hwParser.findHardwareCommands<FamilyType>();
        EXPECT_NE(hwParser.itorWalker, hwParser.cmdList.end());

        auto walker = genCmdCast<WalkerType *>(*hwParser.itorWalker);
        EXPECT_EQ(1u, walker->getEmitInlineParameter());

        EXPECT_EQ(1u, walker->getGenerateLocalId());
        constexpr uint32_t expectedEmit = (1 << 0);
        EXPECT_EQ(expectedEmit, walker->getEmitLocalId());

        EXPECT_EQ(0, memcmp(walker->getInlineDataPointer(), this->crossThreadDataGrf, inlineDataSize));

        if constexpr (std::is_same_v<WalkerType, COMPUTE_WALKER>) {
            constexpr uint32_t expectedIndirectDataLength = 0;
            EXPECT_EQ(expectedIndirectDataLength, walker->getIndirectDataLength());
        }

        auto &idd = walker->getInterfaceDescriptor();
        uint64_t expectedKernelStartOffset = this->kernel->kernelInfo.kernelDescriptor.entryPoints.skipPerThreadDataLoad;
        if constexpr (std::is_same_v<WalkerType, COMPUTE_WALKER>) {
            expectedKernelStartOffset += this->kernel->mockKernel->getKernelInfo().getGraphicsAllocation()->getGpuAddressToPatch();
            using KernelStartPointerType = uint32_t;
            EXPECT_EQ(static_cast<KernelStartPointerType>(expectedKernelStartOffset), static_cast<KernelStartPointerType>(idd.getKernelStartPointer()));
        } else {
            expectedKernelStartOffset += this->kernel->mockKernel->getKernelInfo().getGraphicsAllocation()->getGpuAddress();
            using KernelStartPointerType = decltype(idd.getKernelStartPointer());
            EXPECT_EQ(static_cast<KernelStartPointerType>(expectedKernelStartOffset), idd.getKernelStartPointer());
        }

        memoryManager->freeGraphicsMemory(kernel->kernelInfo.kernelAllocation);
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterDispatchWalkerBasicTest, givenPassInlineDataAndHwLocalIdsGenerationEnabledWhenLocalIdsNotUsedThenExpectCrossThreadDataInWalkerAndNoHwLocalIdGeneration) {
    if constexpr (FamilyType::isHeaplessRequired()) {
        GTEST_SKIP();
    } else {
        using WalkerType = typename FamilyType::DefaultWalkerType;
        using COMPUTE_WALKER = typename FamilyType::COMPUTE_WALKER;

        debugManager.flags.EnablePassInlineData.set(true);
        debugManager.flags.EnableHwGenerationLocalIds.set(1);

        auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context.get(), device.get(), nullptr);

        auto &kd = kernel->kernelInfo.kernelDescriptor;
        auto inlineDataSize = UnitTestHelper<FamilyType>::getInlineDataSize(cmdQ->heaplessModeEnabled);
        kd.entryPoints.skipPerThreadDataLoad = 128;
        kd.kernelAttributes.flags.passInlineData = true;
        kd.kernelAttributes.localId[0] = 0;
        kd.kernelAttributes.localId[1] = 0;
        kd.kernelAttributes.localId[2] = 0;
        kd.kernelAttributes.numLocalIdChannels = 0;

        kernel->mockKernel->setCrossThreadData(crossThreadDataGrf, inlineDataSize);

        auto memoryManager = device->getUltCommandStreamReceiver<FamilyType>().getMemoryManager();
        kernel->kernelInfo.kernelAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{device->getRootDeviceIndex(), MemoryConstants::pageSize});

        size_t gws[] = {16, 1, 1};
        size_t lws[] = {16, 1, 1};
        cmdQ->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, lws, 0, nullptr, nullptr);

        HardwareParse hwParser;
        hwParser.parseCommands<FamilyType>(cmdQ->getCS(0), 0);
        hwParser.findHardwareCommands<FamilyType>();
        EXPECT_NE(hwParser.itorWalker, hwParser.cmdList.end());

        auto walker = genCmdCast<WalkerType *>(*hwParser.itorWalker);
        EXPECT_EQ(1u, walker->getEmitInlineParameter());

        EXPECT_EQ(0u, walker->getGenerateLocalId());
        constexpr uint32_t expectedEmit = 0;
        EXPECT_EQ(expectedEmit, walker->getEmitLocalId());

        EXPECT_EQ(0, memcmp(walker->getInlineDataPointer(), this->crossThreadDataGrf, inlineDataSize));

        if constexpr (std::is_same_v<WalkerType, COMPUTE_WALKER>) {
            constexpr uint32_t expectedIndirectDataLength = 0;
            EXPECT_EQ(expectedIndirectDataLength, walker->getIndirectDataLength());
        }

        auto &idd = walker->getInterfaceDescriptor();
        if constexpr (std::is_same_v<WalkerType, COMPUTE_WALKER>) {
            auto expectedKernelStartOffset = this->kernel->mockKernel->getKernelInfo().getGraphicsAllocation()->getGpuAddressToPatch();
            using KernelStartPointerType = uint32_t;
            EXPECT_EQ(static_cast<KernelStartPointerType>(expectedKernelStartOffset), static_cast<KernelStartPointerType>(idd.getKernelStartPointer()));
        } else {
            auto expectedKernelStartOffset = this->kernel->mockKernel->getKernelInfo().getGraphicsAllocation()->getGpuAddress();
            using KernelStartPointerType = decltype(idd.getKernelStartPointer());
            EXPECT_EQ(static_cast<KernelStartPointerType>(expectedKernelStartOffset), idd.getKernelStartPointer());
        }
        memoryManager->freeGraphicsMemory(kernel->kernelInfo.kernelAllocation);
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterDispatchWalkerBasicTest, GivenPipeControlIsRequiredWhenWalkerPartitionIsOnThenSizeIsProperlyEstimated) {
    debugManager.flags.EnableWalkerPartition.set(1u);
    VariableBackup<bool> pipeControlConfigBackup(&ImplicitScalingDispatch<FamilyType>::getPipeControlStallRequired(), true);
    UltClDeviceFactory deviceFactory{1, 2};
    MockClDevice *device = deviceFactory.rootDevices[0];
    MockContext context{device};

    auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(&context, device, nullptr);
    auto &csr = cmdQ->getUltCommandStreamReceiver();

    size_t numPipeControls = MemorySynchronizationCommands<FamilyType>::isBarrierWaRequired(device->getRootDeviceEnvironment()) ? 2 : 1;

    auto baseSize = UnitTestHelper<FamilyType>::getWalkerSize(this->heaplessEnabled) +
                    (sizeof(typename FamilyType::PIPE_CONTROL) * numPipeControls) +
                    HardwareCommandsHelper<FamilyType>::getSizeRequiredCS() +
                    EncodeMemoryPrefetch<FamilyType>::getSizeForMemoryPrefetch(kernel->kernelInfo.heapInfo.kernelHeapSize, device->getRootDeviceEnvironment());

    DispatchInfo dispatchInfo{};
    dispatchInfo.setNumberOfWorkgroups({32, 1, 1});

    WalkerPartition::WalkerPartitionArgs testArgs = {};
    testArgs.initializeWparidRegister = false;
    testArgs.crossTileAtomicSynchronization = true;
    testArgs.emitPipeControlStall = true;
    testArgs.partitionCount = 2u;
    testArgs.dcFlushEnable = csr.getDcFlushSupport();
    testArgs.tileCount = static_cast<uint32_t>(device->getDeviceBitfield().count());

    debugManager.flags.SynchronizeWalkerInWparidMode.set(0);
    testArgs.staticPartitioning = false;
    testArgs.synchronizeBeforeExecution = false;
    csr.staticWorkPartitioningEnabled = false;

    auto partitionSize = UnitTestHelper<FamilyType>::getWalkerPartitionEstimateSpaceRequiredInCommandBuffer(this->heaplessEnabled, testArgs);
    auto returnedSize = EnqueueOperation<FamilyType>::getSizeRequiredCS(CL_COMMAND_NDRANGE_KERNEL, false, false, *cmdQ.get(), kernel->mockKernel, dispatchInfo);
    EXPECT_EQ(returnedSize, partitionSize + baseSize);

    testArgs.staticPartitioning = true;
    csr.staticWorkPartitioningEnabled = true;
    partitionSize = UnitTestHelper<FamilyType>::getWalkerPartitionEstimateSpaceRequiredInCommandBuffer(this->heaplessEnabled, testArgs);
    returnedSize = EnqueueOperation<FamilyType>::getSizeRequiredCS(CL_COMMAND_NDRANGE_KERNEL, false, false, *cmdQ.get(), kernel->mockKernel, dispatchInfo);
    EXPECT_EQ(returnedSize, partitionSize + baseSize);

    debugManager.flags.SynchronizeWalkerInWparidMode.set(1);
    testArgs.synchronizeBeforeExecution = true;
    testArgs.staticPartitioning = false;
    csr.staticWorkPartitioningEnabled = false;
    partitionSize = UnitTestHelper<FamilyType>::getWalkerPartitionEstimateSpaceRequiredInCommandBuffer(this->heaplessEnabled, testArgs);
    returnedSize = EnqueueOperation<FamilyType>::getSizeRequiredCS(CL_COMMAND_NDRANGE_KERNEL, false, false, *cmdQ.get(), kernel->mockKernel, dispatchInfo);
    EXPECT_EQ(returnedSize, partitionSize + baseSize);

    testArgs.synchronizeBeforeExecution = true;
    testArgs.staticPartitioning = true;
    csr.staticWorkPartitioningEnabled = true;
    partitionSize = UnitTestHelper<FamilyType>::getWalkerPartitionEstimateSpaceRequiredInCommandBuffer(this->heaplessEnabled, testArgs);
    returnedSize = EnqueueOperation<FamilyType>::getSizeRequiredCS(CL_COMMAND_NDRANGE_KERNEL, false, false, *cmdQ.get(), kernel->mockKernel, dispatchInfo);
    EXPECT_EQ(returnedSize, partitionSize + baseSize);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterDispatchWalkerBasicTest, GivenPipeControlIsNotRequiredWhenWalkerPartitionIsOnThenSizeIsProperlyEstimated) {
    debugManager.flags.EnableWalkerPartition.set(1u);
    VariableBackup<bool> pipeControlConfigBackup(&ImplicitScalingDispatch<FamilyType>::getPipeControlStallRequired(), false);
    UltClDeviceFactory deviceFactory{1, 2};
    MockClDevice *device = deviceFactory.rootDevices[0];
    MockContext context{device};

    auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(&context, device, nullptr);
    auto &csr = cmdQ->getUltCommandStreamReceiver();

    size_t numPipeControls = MemorySynchronizationCommands<FamilyType>::isBarrierWaRequired(device->getRootDeviceEnvironment()) ? 2 : 1;

    auto baseSize = UnitTestHelper<FamilyType>::getWalkerSize(this->heaplessEnabled) +
                    (sizeof(typename FamilyType::PIPE_CONTROL) * numPipeControls) +
                    HardwareCommandsHelper<FamilyType>::getSizeRequiredCS() +
                    EncodeMemoryPrefetch<FamilyType>::getSizeForMemoryPrefetch(kernel->kernelInfo.heapInfo.kernelHeapSize, device->getRootDeviceEnvironment());

    DispatchInfo dispatchInfo{};
    dispatchInfo.setNumberOfWorkgroups({32, 1, 1});

    WalkerPartition::WalkerPartitionArgs testArgs = {};
    testArgs.initializeWparidRegister = false;
    testArgs.crossTileAtomicSynchronization = false;
    testArgs.emitPipeControlStall = false;
    testArgs.partitionCount = 2u;
    testArgs.dcFlushEnable = csr.getDcFlushSupport();
    testArgs.tileCount = static_cast<uint32_t>(device->getDeviceBitfield().count());

    debugManager.flags.SynchronizeWalkerInWparidMode.set(0);
    testArgs.staticPartitioning = false;
    testArgs.synchronizeBeforeExecution = false;
    csr.staticWorkPartitioningEnabled = false;
    auto partitionSize = UnitTestHelper<FamilyType>::getWalkerPartitionEstimateSpaceRequiredInCommandBuffer(this->heaplessEnabled, testArgs);
    auto returnedSize = EnqueueOperation<FamilyType>::getSizeRequiredCS(CL_COMMAND_NDRANGE_KERNEL, false, false, *cmdQ.get(), kernel->mockKernel, dispatchInfo);
    EXPECT_EQ(returnedSize, partitionSize + baseSize);

    testArgs.staticPartitioning = true;
    csr.staticWorkPartitioningEnabled = true;
    partitionSize = UnitTestHelper<FamilyType>::getWalkerPartitionEstimateSpaceRequiredInCommandBuffer(this->heaplessEnabled, testArgs);
    returnedSize = EnqueueOperation<FamilyType>::getSizeRequiredCS(CL_COMMAND_NDRANGE_KERNEL, false, false, *cmdQ.get(), kernel->mockKernel, dispatchInfo);
    EXPECT_EQ(returnedSize, partitionSize + baseSize);

    debugManager.flags.SynchronizeWalkerInWparidMode.set(1);
    testArgs.synchronizeBeforeExecution = true;
    testArgs.staticPartitioning = false;
    csr.staticWorkPartitioningEnabled = false;
    partitionSize = UnitTestHelper<FamilyType>::getWalkerPartitionEstimateSpaceRequiredInCommandBuffer(this->heaplessEnabled, testArgs);
    returnedSize = EnqueueOperation<FamilyType>::getSizeRequiredCS(CL_COMMAND_NDRANGE_KERNEL, false, false, *cmdQ.get(), kernel->mockKernel, dispatchInfo);
    EXPECT_EQ(returnedSize, partitionSize + baseSize);

    testArgs.synchronizeBeforeExecution = true;
    testArgs.staticPartitioning = true;
    csr.staticWorkPartitioningEnabled = true;
    partitionSize = UnitTestHelper<FamilyType>::getWalkerPartitionEstimateSpaceRequiredInCommandBuffer(this->heaplessEnabled, testArgs);
    returnedSize = EnqueueOperation<FamilyType>::getSizeRequiredCS(CL_COMMAND_NDRANGE_KERNEL, false, false, *cmdQ.get(), kernel->mockKernel, dispatchInfo);
    EXPECT_EQ(returnedSize, partitionSize + baseSize);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterDispatchWalkerBasicTest, whenWalkerPartitionIsDisabledThenSizeIsProperlyEstimated) {
    debugManager.flags.EnableWalkerPartition.set(0u);
    auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context.get(), device.get(), nullptr);

    size_t numPipeControls = MemorySynchronizationCommands<FamilyType>::isBarrierWaRequired(device->getRootDeviceEnvironment()) ? 2 : 1;

    DispatchInfo dispatchInfo{};
    dispatchInfo.setNumberOfWorkgroups({32, 1, 1});

    auto baseSize = UnitTestHelper<FamilyType>::getWalkerSize(this->heaplessEnabled) +
                    (sizeof(typename FamilyType::PIPE_CONTROL) * numPipeControls) +
                    HardwareCommandsHelper<FamilyType>::getSizeRequiredCS() +
                    EncodeMemoryPrefetch<FamilyType>::getSizeForMemoryPrefetch(kernel->kernelInfo.heapInfo.kernelHeapSize, device->getRootDeviceEnvironment());

    auto returnedSize = EnqueueOperation<FamilyType>::getSizeRequiredCS(CL_COMMAND_NDRANGE_KERNEL, false, false, *cmdQ.get(), kernel->mockKernel, dispatchInfo);
    EXPECT_EQ(returnedSize, baseSize);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterDispatchWalkerBasicTest, whenPipeControlPrecedingPostSyncCommandIsDisabledAndLocalMemoryIsEnabledThenSizeIsProperlyEstimated) {
    debugManager.flags.DisablePipeControlPrecedingPostSyncCommand.set(1);
    auto &hwInfo = *device->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo.featureTable.flags.ftrLocalMemory = true;
    auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context.get(), device.get(), nullptr);

    size_t numPipeControls = MemorySynchronizationCommands<FamilyType>::isBarrierWaRequired(device->getRootDeviceEnvironment()) ? 2 : 1;

    auto baseSize = UnitTestHelper<FamilyType>::getWalkerSize(this->heaplessEnabled) +
                    (sizeof(typename FamilyType::PIPE_CONTROL) * numPipeControls) +
                    HardwareCommandsHelper<FamilyType>::getSizeRequiredCS() +
                    EncodeMemoryPrefetch<FamilyType>::getSizeForMemoryPrefetch(kernel->kernelInfo.heapInfo.kernelHeapSize, device->getRootDeviceEnvironment());

    auto returnedSize = EnqueueOperation<FamilyType>::getSizeRequiredCS(CL_COMMAND_NDRANGE_KERNEL, false, false, *cmdQ.get(), kernel->mockKernel, {});
    EXPECT_EQ(returnedSize, baseSize);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterDispatchWalkerBasicTest, GivenPipeControlIsRequiredWhenQueueIsMultiEngineCapableThenWalkerPartitionsAreEstimated) {
    debugManager.flags.EnableWalkerPartition.set(1u);
    VariableBackup<bool> pipeControlConfigBackup(&ImplicitScalingDispatch<FamilyType>::getPipeControlStallRequired(), true);

    auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context.get(), device.get(), nullptr);
    auto &csr = cmdQ->getUltCommandStreamReceiver();

    size_t numPipeControls = MemorySynchronizationCommands<FamilyType>::isBarrierWaRequired(device->getRootDeviceEnvironment()) ? 2 : 1;

    auto baseSize = UnitTestHelper<FamilyType>::getWalkerSize(this->heaplessEnabled) +
                    (sizeof(typename FamilyType::PIPE_CONTROL) * numPipeControls) +
                    HardwareCommandsHelper<FamilyType>::getSizeRequiredCS() +
                    EncodeMemoryPrefetch<FamilyType>::getSizeForMemoryPrefetch(kernel->kernelInfo.heapInfo.kernelHeapSize, device->getRootDeviceEnvironment());

    WalkerPartition::WalkerPartitionArgs testArgs = {};
    testArgs.initializeWparidRegister = true;
    testArgs.emitPipeControlStall = true;
    testArgs.crossTileAtomicSynchronization = true;
    testArgs.partitionCount = 16u;
    testArgs.dcFlushEnable = csr.getDcFlushSupport();
    testArgs.tileCount = static_cast<uint32_t>(device->getDeviceBitfield().count());

    auto partitionSize = UnitTestHelper<FamilyType>::getWalkerPartitionEstimateSpaceRequiredInCommandBuffer(this->heaplessEnabled, testArgs);

    DispatchInfo dispatchInfo{};
    dispatchInfo.setNumberOfWorkgroups({32, 1, 1});

    auto returnedSize = EnqueueOperation<FamilyType>::getSizeRequiredCS(CL_COMMAND_NDRANGE_KERNEL, false, false, *cmdQ.get(), kernel->mockKernel, dispatchInfo);
    EXPECT_EQ(returnedSize, partitionSize + baseSize);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterDispatchWalkerBasicTest, GivenPipeControlIsNotRequiredWhenQueueIsMultiEngineCapableThenWalkerPartitionsAreEstimated) {
    debugManager.flags.EnableWalkerPartition.set(1u);
    VariableBackup<bool> pipeControlConfigBackup(&ImplicitScalingDispatch<FamilyType>::getPipeControlStallRequired(), false);

    auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context.get(), device.get(), nullptr);
    auto &csr = cmdQ->getUltCommandStreamReceiver();

    size_t numPipeControls = MemorySynchronizationCommands<FamilyType>::isBarrierWaRequired(device->getRootDeviceEnvironment()) ? 2 : 1;

    auto baseSize = UnitTestHelper<FamilyType>::getWalkerSize(this->heaplessEnabled) +
                    (sizeof(typename FamilyType::PIPE_CONTROL) * numPipeControls) +
                    HardwareCommandsHelper<FamilyType>::getSizeRequiredCS() +
                    EncodeMemoryPrefetch<FamilyType>::getSizeForMemoryPrefetch(kernel->kernelInfo.heapInfo.kernelHeapSize, device->getRootDeviceEnvironment());

    WalkerPartition::WalkerPartitionArgs testArgs = {};
    testArgs.initializeWparidRegister = true;
    testArgs.emitPipeControlStall = false;
    testArgs.crossTileAtomicSynchronization = false;
    testArgs.partitionCount = 16u;
    testArgs.dcFlushEnable = csr.getDcFlushSupport();
    testArgs.tileCount = static_cast<uint32_t>(device->getDeviceBitfield().count());

    auto partitionSize = UnitTestHelper<FamilyType>::getWalkerPartitionEstimateSpaceRequiredInCommandBuffer(this->heaplessEnabled, testArgs);

    DispatchInfo dispatchInfo{};
    dispatchInfo.setNumberOfWorkgroups({32, 1, 1});

    auto returnedSize = EnqueueOperation<FamilyType>::getSizeRequiredCS(CL_COMMAND_NDRANGE_KERNEL, false, false, *cmdQ.get(), kernel->mockKernel, dispatchInfo);
    EXPECT_EQ(returnedSize, partitionSize + baseSize);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterDispatchWalkerBasicTest, whenProgramWalkerIsCalledThenWalkerPartitionLogicIsExecuted) {
    using WalkerType = typename FamilyType::DefaultWalkerType;

    debugManager.flags.EnableWalkerPartition.set(1u);
    auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context.get(), device.get(), nullptr);
    size_t gws[] = {2, 1, 1};
    size_t lws[] = {1, 1, 1};
    cmdQ->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, lws, 0, nullptr, nullptr);

    ClHardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(*cmdQ);

    auto walker = genCmdCast<WalkerType *>(hwParser.cmdWalker);
    ASSERT_NE(nullptr, walker);
    EXPECT_EQ(WalkerType::PARTITION_TYPE::PARTITION_TYPE_X, walker->getPartitionType());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterDispatchWalkerBasicTest, whenProgramWalkerIsCalledAndForceSynchronizeWalkerInWpariModeThenWalkerPartitionLogicIsExecuted) {
    using WalkerType = typename FamilyType::DefaultWalkerType;

    debugManager.flags.EnableWalkerPartition.set(1u);
    debugManager.flags.SynchronizeWalkerInWparidMode.set(1);
    auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context.get(), device.get(), nullptr);
    size_t gws[] = {2, 1, 1};
    size_t lws[] = {1, 1, 1};
    cmdQ->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, lws, 0, nullptr, nullptr);

    ClHardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(*cmdQ);

    auto walker = genCmdCast<WalkerType *>(hwParser.cmdWalker);
    ASSERT_NE(nullptr, walker);
    EXPECT_EQ(WalkerType::PARTITION_TYPE::PARTITION_TYPE_X, walker->getPartitionType());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterDispatchWalkerBasicTest, whenProgramWalkerIsCalledWithPartitionLogicDisabledThenWalkerPartitionLogicIsNotExecuted) {
    using WalkerType = typename FamilyType::DefaultWalkerType;

    debugManager.flags.EnableWalkerPartition.set(0u);
    auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context.get(), device.get(), nullptr);
    size_t gws[] = {2, 1, 1};
    size_t lws[] = {1, 1, 1};
    cmdQ->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, lws, 0, nullptr, nullptr);

    ClHardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(*cmdQ);

    auto walker = genCmdCast<WalkerType *>(hwParser.cmdWalker);
    ASSERT_NE(nullptr, walker);
    EXPECT_EQ(WalkerType::PARTITION_TYPE::PARTITION_TYPE_DISABLED, walker->getPartitionType());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterDispatchWalkerBasicTest, whenQueueIsCreatedWithMultiEngineSupportAndEnqueueIsDoneThenWalkerIsPartitioned) {
    using WalkerType = typename FamilyType::DefaultWalkerType;

    debugManager.flags.EnableWalkerPartition.set(1u);

    auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context.get(), device.get(), nullptr);
    size_t gws[] = {128, 1, 1};
    size_t lws[] = {1, 1, 1};
    cmdQ->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, lws, 0, nullptr, nullptr);

    ClHardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(*cmdQ);

    auto walker = genCmdCast<WalkerType *>(hwParser.cmdWalker);
    ASSERT_NE(nullptr, walker);
    EXPECT_EQ(WalkerType::PARTITION_TYPE::PARTITION_TYPE_X, walker->getPartitionType());
    EXPECT_EQ(64u, walker->getPartitionSize());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterDispatchWalkerBasicTest, whenProgramWalkerIsCalledWithDebugRegistryOverridesThenWalkerContainsProperParameters) {
    debugManager.flags.EnableWalkerPartition.set(1u);
    debugManager.flags.ExperimentalSetWalkerPartitionCount.set(2u);
    debugManager.flags.ExperimentalSetWalkerPartitionType.set(2u);
    using WalkerType = typename FamilyType::DefaultWalkerType;

    auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context.get(), device.get(), nullptr);
    size_t gws[] = {1, 1, 1};
    cmdQ->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    ClHardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(*cmdQ);

    auto walker = genCmdCast<WalkerType *>(hwParser.cmdWalker);
    ASSERT_NE(nullptr, walker);
    EXPECT_EQ(WalkerType::PARTITION_TYPE::PARTITION_TYPE_Y, walker->getPartitionType());
    EXPECT_EQ(1u, walker->getPartitionSize());

    auto timestampPacket = cmdQ->timestampPacketContainer->peekNodes().at(0);
    auto expectedPartitionCount = timestampPacket->getPacketsUsed();

    EXPECT_EQ(expectedPartitionCount, static_cast<unsigned int>(debugManager.flags.ExperimentalSetWalkerPartitionCount.get()));
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterDispatchWalkerBasicTest, whenProgramWalkerIsCalledWithDebugRegistryOverridesToPartitionCountOneThenProgramProperParameters) {
    debugManager.flags.EnableWalkerPartition.set(1u);
    debugManager.flags.ExperimentalSetWalkerPartitionCount.set(1u);
    debugManager.flags.ExperimentalSetWalkerPartitionType.set(2u);
    using WalkerType = typename FamilyType::DefaultWalkerType;

    auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context.get(), device.get(), nullptr);
    size_t gws[] = {1, 1, 1};
    cmdQ->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    ClHardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(*cmdQ);

    auto walker = genCmdCast<WalkerType *>(hwParser.cmdWalker);
    ASSERT_NE(nullptr, walker);
    EXPECT_EQ(WalkerType::PARTITION_TYPE::PARTITION_TYPE_DISABLED, walker->getPartitionType());
    EXPECT_EQ(0u, walker->getPartitionSize());
    EXPECT_FALSE(walker->getWorkloadPartitionEnable());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterDispatchWalkerBasicTest, whenEnqueueIsBlockedOnUserEventThenDoNotPartition) {
    debugManager.flags.EnableWalkerPartition.set(1u);
    debugManager.flags.ExperimentalSetWalkerPartitionCount.set(2u);
    debugManager.flags.ExperimentalSetWalkerPartitionType.set(2u);
    using WalkerType = typename FamilyType::DefaultWalkerType;

    cl_event userEvent = clCreateUserEvent(context.get(), nullptr);

    auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context.get(), device.get(), nullptr);
    size_t gws[] = {1, 1, 1};
    cmdQ->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, nullptr, 1, &userEvent, nullptr);
    clSetUserEventStatus(userEvent, 0u);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(*cmdQ->getUltCommandStreamReceiver().lastFlushedCommandStream);
    hwParser.findHardwareCommands<FamilyType>(&cmdQ->getGpgpuCommandStreamReceiver().getIndirectHeap(IndirectHeap::Type::dynamicState, 0));

    auto walker = genCmdCast<WalkerType *>(hwParser.cmdWalker);
    ASSERT_NE(nullptr, walker);
    EXPECT_EQ(WalkerType::PARTITION_TYPE::PARTITION_TYPE_Y, walker->getPartitionType());
    EXPECT_EQ(1u, walker->getPartitionSize());
    EXPECT_TRUE(walker->getWorkloadPartitionEnable());

    clReleaseEvent(userEvent);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterDispatchWalkerBasicTest, whenDispatchProfilingCalledThenDoNothing) {
    MockCommandQueue cmdQ(context.get(), device.get(), nullptr, false);

    auto &cmdStream = cmdQ.getCS(0);
    MockTagAllocator<HwTimeStamps> timeStampAllocator(device->getRootDeviceIndex(), device->getMemoryManager(), 10,
                                                      MemoryConstants::cacheLineSize, sizeof(HwTimeStamps), false, device->getDeviceBitfield());

    auto hwTimeStamp1 = timeStampAllocator.getTag();

    GpgpuWalkerHelper<FamilyType>::dispatchProfilingCommandsStart(*hwTimeStamp1, &cmdStream, device->getRootDeviceEnvironment());

    GpgpuWalkerHelper<FamilyType>::dispatchProfilingCommandsEnd(*hwTimeStamp1, &cmdStream, device->getRootDeviceEnvironment());

    EXPECT_EQ(0u, cmdStream.getUsed());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterDispatchWalkerBasicTest, givenOpenClWhenEnqueuePartitionWalkerThenExpectNoSelfCleanupSection) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    debugManager.flags.EnableWalkerPartition.set(1u);
    using WalkerType = typename FamilyType::DefaultWalkerType;

    auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context.get(), device.get(), nullptr);
    size_t gws[] = {128, 1, 1};
    size_t lws[] = {8, 1, 1};
    cmdQ->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, lws, 0, nullptr, nullptr);

    ClHardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(*cmdQ);

    auto walker = genCmdCast<WalkerType *>(hwParser.cmdWalker);
    ASSERT_NE(nullptr, walker);
    EXPECT_EQ(WalkerType::PARTITION_TYPE::PARTITION_TYPE_X, walker->getPartitionType());
    EXPECT_EQ(8u, walker->getPartitionSize());

    GenCmdList storeDataImmList = hwParser.getCommandsList<MI_STORE_DATA_IMM>();
    EXPECT_EQ(0u, storeDataImmList.size());
}

struct XeHPAndLaterDispatchWalkerBasicTestDynamicPartition : public XeHPAndLaterDispatchWalkerBasicTest {
    void SetUp() override {
        debugManager.flags.CreateMultipleSubDevices.set(2);
        debugManager.flags.EnableStaticPartitioning.set(0);
        debugManager.flags.EnableWalkerPartition.set(1u);

        XeHPAndLaterDispatchWalkerBasicTest::SetUp();
    }
    void TearDown() override {
        XeHPAndLaterDispatchWalkerBasicTest::TearDown();
    }
};

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterDispatchWalkerBasicTestDynamicPartition, givenDynamicPartitioningWhenEnqueueingKernelThenExpectNoMultipleActivePartitionsSetInCsr) {
    auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context.get(), device.get(), nullptr);
    size_t gws[] = {128, 1, 1};
    size_t lws[] = {8, 1, 1};
    auto &commandStreamReceiver = cmdQ->getUltCommandStreamReceiver();
    using WalkerType = typename FamilyType::DefaultWalkerType;

    EXPECT_EQ(1u, commandStreamReceiver.activePartitions);
    cmdQ->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, lws, 0, nullptr, nullptr);
    EXPECT_EQ(1u, commandStreamReceiver.activePartitions);

    ClHardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(*cmdQ);
    auto walker = genCmdCast<WalkerType *>(hwParser.cmdWalker);
    ASSERT_NE(nullptr, walker);
    EXPECT_EQ(WalkerType::PARTITION_TYPE::PARTITION_TYPE_X, walker->getPartitionType());
    EXPECT_EQ(8u, walker->getPartitionSize());
}

struct XeHPAndLaterDispatchWalkerBasicTestStaticPartition : public XeHPAndLaterDispatchWalkerBasicTest {
    void SetUp() override {
        debugManager.flags.CreateMultipleSubDevices.set(2);
        debugManager.flags.EnableStaticPartitioning.set(1);
        debugManager.flags.EnableWalkerPartition.set(1u);

        XeHPAndLaterDispatchWalkerBasicTest::SetUp();
    }
    void TearDown() override {
        XeHPAndLaterDispatchWalkerBasicTest::TearDown();
    }
};

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterDispatchWalkerBasicTestStaticPartition, givenStaticPartitioningWhenEnqueueingKernelThenMultipleActivePartitionsAreSetInCsr) {
    auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context.get(), device.get(), nullptr);
    size_t gws[] = {128, 1, 1};
    size_t lws[] = {8, 1, 1};
    auto &commandStreamReceiver = cmdQ->getUltCommandStreamReceiver();
    using WalkerType = typename FamilyType::DefaultWalkerType;

    EXPECT_EQ(2u, commandStreamReceiver.activePartitions);
    cmdQ->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, lws, 0, nullptr, nullptr);
    EXPECT_EQ(2u, commandStreamReceiver.activePartitions);

    ClHardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(*cmdQ);
    auto walker = genCmdCast<WalkerType *>(hwParser.cmdWalker);
    ASSERT_NE(nullptr, walker);
    EXPECT_EQ(WalkerType::PARTITION_TYPE::PARTITION_TYPE_X, walker->getPartitionType());
    EXPECT_EQ(8u, walker->getPartitionSize());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterDispatchWalkerBasicTestStaticPartition,
            givenStaticPartitioningWhenEnqueueingNonUnifromKernelThenMultipleActivePartitionsAreSetInCsrAndWparidRegisterIsReconfiguredToStatic) {
    using WalkerType = typename FamilyType::DefaultWalkerType;

    using MI_LOAD_REGISTER_MEM = typename FamilyType::MI_LOAD_REGISTER_MEM;
    auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context.get(), device.get(), nullptr);
    size_t gws[] = {129, 1, 1};
    size_t lws[] = {8, 1, 1};
    auto &commandStreamReceiver = cmdQ->getUltCommandStreamReceiver();

    EXPECT_EQ(2u, commandStreamReceiver.activePartitions);
    kernel->mockProgram->allowNonUniform = true;
    cmdQ->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, lws, 0, nullptr, nullptr);
    EXPECT_EQ(2u, commandStreamReceiver.activePartitions);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(*cmdQ->commandStream);

    auto firstComputeWalkerItor = NEO::UnitTestHelper<FamilyType>::findWalkerTypeCmd(hwParser.cmdList.begin(), hwParser.cmdList.end());
    ASSERT_NE(hwParser.cmdList.end(), firstComputeWalkerItor);
    auto nextCmdItor = firstComputeWalkerItor;
    ++nextCmdItor;

    auto walker1 = genCmdCast<WalkerType *>(*firstComputeWalkerItor);
    EXPECT_EQ(WalkerType::PARTITION_TYPE::PARTITION_TYPE_X, walker1->getPartitionType());
    EXPECT_EQ(8u, walker1->getPartitionSize());

    auto secondComputeWalkerItor = NEO::UnitTestHelper<FamilyType>::findWalkerTypeCmd(nextCmdItor, hwParser.cmdList.end());
    ASSERT_NE(hwParser.cmdList.end(), secondComputeWalkerItor);

    auto walker2 = genCmdCast<WalkerType *>(*secondComputeWalkerItor);

    EXPECT_EQ(WalkerType::PARTITION_TYPE::PARTITION_TYPE_DISABLED, walker2->getPartitionType());

    auto workPartitionAllocationGpuVa = commandStreamReceiver.getWorkPartitionAllocationGpuAddress();
    auto expectedRegister = 0x221Cu;
    auto loadRegisterMem = hwParser.getCommand<MI_LOAD_REGISTER_MEM>(firstComputeWalkerItor, secondComputeWalkerItor);
    ASSERT_NE(nullptr, loadRegisterMem);
    EXPECT_EQ(workPartitionAllocationGpuVa, loadRegisterMem->getMemoryAddress());
    EXPECT_EQ(expectedRegister, loadRegisterMem->getRegisterAddress());
}

using NonDefaultPlatformGpuWalkerTest = XeHPAndLaterDispatchWalkerBasicTest;

struct NonDefaultPlatformGpuWalkerTestNonHeaplessMatcher {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        return IsAtLeastXeCore::isMatched<productFamily>() && !(TestTraits<NEO::ToGfxCoreFamily<productFamily>::get()>::heaplessRequired);
    }
};

HWTEST2_F(NonDefaultPlatformGpuWalkerTest, givenNonDefaultPlatformWhenSetupTimestampPacketThenGmmHelperIsTakenFromNonDefaultPlatform, NonDefaultPlatformGpuWalkerTestNonHeaplessMatcher) {

    using WalkerType = typename FamilyType::DefaultWalkerType;

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto rootDeviceEnvironment = executionEnvironment->rootDeviceEnvironments[0].get();
    rootDeviceEnvironment->initGmm();
    auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context.get(), device.get(), nullptr);
    size_t gws[] = {1, 1, 1};
    cmdQ->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    auto &cmdStream = cmdQ->getCS(0);
    TagNode<TimestampPackets<uint32_t, TimestampPacketConstants::preferredPacketCount>> timestamp;
    ClHardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(*cmdQ);

    auto walker = genCmdCast<WalkerType *>(hwParser.cmdWalker);
    ASSERT_NE(nullptr, walker);
    platformsImpl->clear();
    EXPECT_EQ(platform(), nullptr);
    GpgpuWalkerHelper<FamilyType>::setupTimestampPacket(&cmdStream, walker, static_cast<TagNodeBase *>(&timestamp), *rootDeviceEnvironment);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, WalkerDispatchTest, givenDefaultLocalIdsGenerationWhenPassingFittingParametersThenReturnFalse) {
    uint32_t workDim = 1;
    uint32_t simd = 8;
    size_t lws[3] = {16, 1, 1};
    std::array<uint8_t, 3> walkOrder = {};
    uint32_t requiredWalkOrder = 0u;

    EXPECT_FALSE(EncodeDispatchKernel<FamilyType>::isRuntimeLocalIdsGenerationRequired(
        workDim, lws, walkOrder, true, requiredWalkOrder, simd));
}

HWCMDTEST_F(IGFX_XE_HP_CORE, WalkerDispatchTest, givenEnabledLocalIdsGenerationWhenPassingFittingOneDimParametersThenReturnFalse) {
    DebugManagerStateRestore restore;
    debugManager.flags.EnableHwGenerationLocalIds.set(1);

    uint32_t workDim = 1;
    uint32_t simd = 8;
    size_t lws[3] = {16, 1, 1};
    std::array<uint8_t, 3> walkOrder = {{0, 1, 2}};
    uint32_t requiredWalkOrder = 4u;

    EXPECT_FALSE(EncodeDispatchKernel<FamilyType>::isRuntimeLocalIdsGenerationRequired(
        workDim, lws, walkOrder, true, requiredWalkOrder, simd));
    EXPECT_EQ(0u, requiredWalkOrder);

    lws[0] = 15;
    EXPECT_FALSE(EncodeDispatchKernel<FamilyType>::isRuntimeLocalIdsGenerationRequired(
        workDim, lws, walkOrder, false, requiredWalkOrder, simd));
    EXPECT_EQ(0u, requiredWalkOrder);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, WalkerDispatchTest, givenEnabledLocalIdsGenerationWhenPassingFittingTwoDimParametersThenReturnFalse) {
    DebugManagerStateRestore restore;
    debugManager.flags.EnableHwGenerationLocalIds.set(1);

    uint32_t workDim = 2;
    uint32_t simd = 8;
    size_t lws[3] = {16, 16, 1};
    std::array<uint8_t, 3> walkOrder = {{1, 0, 2}};
    uint32_t requiredWalkOrder = 77u;

    EXPECT_FALSE(EncodeDispatchKernel<FamilyType>::isRuntimeLocalIdsGenerationRequired(workDim, lws, walkOrder, true, requiredWalkOrder, simd));
    EXPECT_EQ(2u, requiredWalkOrder);

    lws[0] = 15;
    EXPECT_FALSE(EncodeDispatchKernel<FamilyType>::isRuntimeLocalIdsGenerationRequired(
        workDim, lws, walkOrder, true, requiredWalkOrder, simd));
    EXPECT_EQ(2u, requiredWalkOrder);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, WalkerDispatchTest, givenWalkOrderThatNeedsToBeFollowedWithCompatibleDimSizesArePassedThenRuntimeGenerationIsNotRequired) {
    DebugManagerStateRestore restore;
    debugManager.flags.EnableHwGenerationLocalIds.set(1);

    uint32_t workDim = 3;
    uint32_t simd = 8;
    size_t lws[3] = {200, 1, 1};
    std::array<uint8_t, 3> walkOrder = {{2, 1, 0}};
    uint32_t requiredWalkOrder = 77u;

    EXPECT_FALSE(EncodeDispatchKernel<FamilyType>::isRuntimeLocalIdsGenerationRequired(
        workDim, lws, walkOrder, true, requiredWalkOrder, simd));
    EXPECT_EQ(5u, requiredWalkOrder);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, WalkerDispatchTest, givenLocalWorkgroupSizeGreaterThen1024ThenRuntimeMustGenerateLocalIds) {
    DebugManagerStateRestore restore;
    debugManager.flags.EnableHwGenerationLocalIds.set(1);

    uint32_t workDim = 3;
    uint32_t simd = 8;
    std::array<size_t, 3> lws = {1025, 1, 1};

    std::array<uint8_t, 3> walkOrder = {{0, 1, 2}};
    uint32_t requiredWalkOrder = 77u;

    EXPECT_TRUE(EncodeDispatchKernel<FamilyType>::isRuntimeLocalIdsGenerationRequired(
        workDim, lws.data(), walkOrder, false, requiredWalkOrder, simd));

    lws = {1, 1, 1025};

    EXPECT_TRUE(EncodeDispatchKernel<FamilyType>::isRuntimeLocalIdsGenerationRequired(
        workDim, lws.data(), walkOrder, false, requiredWalkOrder, simd));

    lws = {32, 32, 4};

    EXPECT_TRUE(EncodeDispatchKernel<FamilyType>::isRuntimeLocalIdsGenerationRequired(
        workDim, lws.data(), walkOrder, false, requiredWalkOrder, simd));

    workDim = 2;

    EXPECT_FALSE(EncodeDispatchKernel<FamilyType>::isRuntimeLocalIdsGenerationRequired(
        workDim, lws.data(), walkOrder, false, requiredWalkOrder, simd));
}

HWCMDTEST_F(IGFX_XE_HP_CORE, WalkerDispatchTest, givenWalkOrderThatDoesntNeedToBeFollowedWhenIncompatibleDimSizesArePassedThenRuntimeGenerationIsReuqired) {
    DebugManagerStateRestore restore;
    debugManager.flags.EnableHwGenerationLocalIds.set(1);

    uint32_t workDim = 3;
    uint32_t simd = 8;
    std::array<size_t, 3> lws = {200, 1, 1};

    std::array<uint8_t, 3> walkOrder = {{0, 2, 1}};
    uint32_t requiredWalkOrder = 77u;

    EXPECT_FALSE(EncodeDispatchKernel<FamilyType>::isRuntimeLocalIdsGenerationRequired(
        workDim, lws.data(), walkOrder, false, requiredWalkOrder, simd));
    EXPECT_EQ(4u, requiredWalkOrder);

    lws = {16, 17, 2};
    EXPECT_FALSE(EncodeDispatchKernel<FamilyType>::isRuntimeLocalIdsGenerationRequired(
        workDim, lws.data(), walkOrder, false, requiredWalkOrder, simd));
    EXPECT_EQ(1u, requiredWalkOrder);

    lws = {16, 2, 17};
    EXPECT_FALSE(EncodeDispatchKernel<FamilyType>::isRuntimeLocalIdsGenerationRequired(
        workDim, lws.data(), walkOrder, false, requiredWalkOrder, simd));
    EXPECT_EQ(0u, requiredWalkOrder);

    lws = {17, 2, 17};
    EXPECT_TRUE(EncodeDispatchKernel<FamilyType>::isRuntimeLocalIdsGenerationRequired(
        workDim, lws.data(), walkOrder, false, requiredWalkOrder, simd));

    lws = {3, 4, 32};
    EXPECT_FALSE(EncodeDispatchKernel<FamilyType>::isRuntimeLocalIdsGenerationRequired(
        workDim, lws.data(), walkOrder, false, requiredWalkOrder, simd));
    EXPECT_EQ(4u, requiredWalkOrder);

    workDim = 2;
    lws = {17, 2, 17};
    EXPECT_FALSE(EncodeDispatchKernel<FamilyType>::isRuntimeLocalIdsGenerationRequired(
        workDim, lws.data(), walkOrder, false, requiredWalkOrder, simd));
    EXPECT_EQ(2u, requiredWalkOrder);

    lws = {2, 17, 17};
    EXPECT_FALSE(EncodeDispatchKernel<FamilyType>::isRuntimeLocalIdsGenerationRequired(
        workDim, lws.data(), walkOrder, false, requiredWalkOrder, simd));
    EXPECT_EQ(0u, requiredWalkOrder);

    lws = {2, 4, 17};
    EXPECT_FALSE(EncodeDispatchKernel<FamilyType>::isRuntimeLocalIdsGenerationRequired(
        workDim, lws.data(), walkOrder, false, requiredWalkOrder, simd));
    EXPECT_EQ(0u, requiredWalkOrder);

    workDim = 1;
    lws = {17, 2, 17};
    EXPECT_FALSE(EncodeDispatchKernel<FamilyType>::isRuntimeLocalIdsGenerationRequired(
        workDim, lws.data(), walkOrder, false, requiredWalkOrder, simd));
    EXPECT_EQ(0u, requiredWalkOrder);

    workDim = 1;
    lws = {2, 17, 17};
    EXPECT_FALSE(EncodeDispatchKernel<FamilyType>::isRuntimeLocalIdsGenerationRequired(
        workDim, lws.data(), walkOrder, false, requiredWalkOrder, simd));
    EXPECT_EQ(0u, requiredWalkOrder);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, WalkerDispatchTest, givenDisabledLocalIdsGenerationWhenPassingFittingThreeDimParametersThenReturnTrue) {
    DebugManagerStateRestore restore;
    debugManager.flags.EnableHwGenerationLocalIds.set(0);

    uint32_t workDim = 3;
    uint32_t simd = 8;
    size_t lws[3] = {16, 16, 4};

    std::array<uint8_t, 3> walkOrder = {{1, 0, 2}};
    uint32_t requiredWalkOrder = 77u;

    EXPECT_TRUE(EncodeDispatchKernel<FamilyType>::isRuntimeLocalIdsGenerationRequired(
        workDim, lws, walkOrder, true, requiredWalkOrder, simd));
}

HWCMDTEST_F(IGFX_XE_HP_CORE, WalkerDispatchTest, givenEnabledLocalIdsGenerationWhenPassingFittingThreeDimParametersThenReturnFalseAndProperWalkOrder) {
    DebugManagerStateRestore restore;
    debugManager.flags.EnableHwGenerationLocalIds.set(1);

    uint32_t workDim = 3;
    uint32_t simd = 8;
    size_t lws[3] = {16, 16, 2};
    std::array<uint8_t, 3> walkOrder = {{2, 1, 0}};
    uint32_t requiredWalkOrder = 77u;

    EXPECT_FALSE(EncodeDispatchKernel<FamilyType>::isRuntimeLocalIdsGenerationRequired(
        workDim, lws, walkOrder, true, requiredWalkOrder, simd));
    EXPECT_EQ(5u, requiredWalkOrder);

    walkOrder = {2, 0, 1};

    EXPECT_FALSE(EncodeDispatchKernel<FamilyType>::isRuntimeLocalIdsGenerationRequired(
        workDim, lws, walkOrder, true, requiredWalkOrder, simd));
    EXPECT_EQ(3u, requiredWalkOrder);

    walkOrder = {1, 2, 0};
    EXPECT_FALSE(EncodeDispatchKernel<FamilyType>::isRuntimeLocalIdsGenerationRequired(
        workDim, lws, walkOrder, true, requiredWalkOrder, simd));
    EXPECT_EQ(4u, requiredWalkOrder);

    walkOrder = {1, 0, 2};
    EXPECT_FALSE(EncodeDispatchKernel<FamilyType>::isRuntimeLocalIdsGenerationRequired(
        workDim, lws, walkOrder, true, requiredWalkOrder, simd));
    EXPECT_EQ(2u, requiredWalkOrder);

    walkOrder = {0, 2, 1};
    EXPECT_FALSE(EncodeDispatchKernel<FamilyType>::isRuntimeLocalIdsGenerationRequired(
        workDim, lws, walkOrder, true, requiredWalkOrder, simd));
    EXPECT_EQ(1u, requiredWalkOrder);

    walkOrder = {0, 1, 2};
    EXPECT_FALSE(EncodeDispatchKernel<FamilyType>::isRuntimeLocalIdsGenerationRequired(
        workDim, lws, walkOrder, true, requiredWalkOrder, simd));
    EXPECT_EQ(0u, requiredWalkOrder);

    // incorrect walkOrder returns 6
    walkOrder = {2, 2, 0};
    EXPECT_FALSE(EncodeDispatchKernel<FamilyType>::isRuntimeLocalIdsGenerationRequired(
        workDim, lws, walkOrder, true, requiredWalkOrder, simd));
    EXPECT_EQ(6u, requiredWalkOrder);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, WalkerDispatchTest, givenEnabledLocalIdsGenerationWhenPassingInvalidLwsTwoDimParametersThenReturnTrue) {
    DebugManagerStateRestore restore;
    debugManager.flags.EnableHwGenerationLocalIds.set(1);

    uint32_t workDim = 2;
    uint32_t simd = 8;
    size_t lws[3] = {15, 15, 1};

    std::array<uint8_t, 3> walkOrder = {{0, 1, 2}};
    uint32_t requiredWalkOrder = 4u;

    EXPECT_TRUE(EncodeDispatchKernel<FamilyType>::isRuntimeLocalIdsGenerationRequired(
        workDim, lws, walkOrder, true, requiredWalkOrder, simd));
}

HWCMDTEST_F(IGFX_XE_HP_CORE, WalkerDispatchTest, givenEnabledLocalIdsGenerationWhenPassingInvalidLwsThreeDimParametersThenReturnTrue) {
    DebugManagerStateRestore restore;
    debugManager.flags.EnableHwGenerationLocalIds.set(1);

    uint32_t workDim = 3;
    uint32_t simd = 8;
    size_t lws[3] = {16, 15, 15};
    std::array<uint8_t, 3> walkOrder = {{0, 1, 2}};
    uint32_t requiredWalkOrder = 4u;

    EXPECT_TRUE(EncodeDispatchKernel<FamilyType>::isRuntimeLocalIdsGenerationRequired(
        workDim, lws, walkOrder, true, requiredWalkOrder, simd));

    lws[0] = 15;
    EXPECT_TRUE(EncodeDispatchKernel<FamilyType>::isRuntimeLocalIdsGenerationRequired(
        workDim, lws, walkOrder, true, requiredWalkOrder, simd));
}

HWCMDTEST_F(IGFX_XE_HP_CORE, WalkerDispatchTest, givenSimdSize1TWhenCheckToGeneratHwIdsThenReturnedFalse) {
    DebugManagerStateRestore restore;
    debugManager.flags.EnableHwGenerationLocalIds.set(1);

    uint32_t workDim = 3;
    uint32_t simd = 8;
    std::array<size_t, 3> lws = {200, 1, 1};

    std::array<uint8_t, 3> walkOrder = {{0, 2, 1}};
    uint32_t requiredWalkOrder = 77u;

    EXPECT_FALSE(EncodeDispatchKernel<FamilyType>::isRuntimeLocalIdsGenerationRequired(
        workDim, lws.data(), walkOrder, false, requiredWalkOrder, simd));
    simd = 1;
    EXPECT_TRUE(EncodeDispatchKernel<FamilyType>::isRuntimeLocalIdsGenerationRequired(
        workDim, lws.data(), walkOrder, false, requiredWalkOrder, simd));
}

struct XeHPAndLaterDispatchWalkerTestMultiTileDevice : public XeHPAndLaterDispatchWalkerBasicTest {
    void SetUp() override {
        debugManager.flags.CreateMultipleSubDevices.set(2u);

        XeHPAndLaterDispatchWalkerBasicTest::SetUp();
    }
    void TearDown() override {
        XeHPAndLaterDispatchWalkerBasicTest::TearDown();
    }
};

struct KernelWithSingleSubdevicePreferences : public MockKernel {
    using MockKernel::MockKernel;
    bool isSingleSubdevicePreferred() const override { return singleSubdevicePreferred; }

    bool singleSubdevicePreferred = true;
};

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterDispatchWalkerTestMultiTileDevice, givenKernelThatPrefersSingleSubdeviceWhenProgramWalkerThenKernelIsExecutedOnSingleTile) {
    using WalkerType = typename FamilyType::DefaultWalkerType;

    auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context.get(), device.get(), nullptr);
    size_t gws[] = {2, 1, 1};
    size_t lws[] = {1, 1, 1};

    KernelWithSingleSubdevicePreferences subdeviceKernel(kernel->mockProgram, kernel->kernelInfo, *device);
    subdeviceKernel.singleSubdevicePreferred = true;
    cmdQ->enqueueKernel(&subdeviceKernel, 1, nullptr, gws, lws, 0, nullptr, nullptr);

    ClHardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(*cmdQ);
    auto walker = genCmdCast<WalkerType *>(hwParser.cmdWalker);
    ASSERT_NE(nullptr, walker);
    EXPECT_EQ(WalkerType::PARTITION_TYPE::PARTITION_TYPE_X, walker->getPartitionType());
    EXPECT_EQ(2u, walker->getPartitionSize());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterDispatchWalkerTestMultiTileDevice, givenKernelThatDoesntPreferSingleSubdeviceWhenProgramWalkerThenKernelIsExecutedOnAllTiles) {
    using WalkerType = typename FamilyType::DefaultWalkerType;

    auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context.get(), device.get(), nullptr);
    size_t gws[] = {2, 1, 1};
    size_t lws[] = {1, 1, 1};

    KernelWithSingleSubdevicePreferences subdeviceKernel(kernel->mockProgram, kernel->kernelInfo, *device);
    subdeviceKernel.singleSubdevicePreferred = false;
    cmdQ->enqueueKernel(&subdeviceKernel, 1, nullptr, gws, lws, 0, nullptr, nullptr);

    ClHardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(*cmdQ);

    auto walker = genCmdCast<WalkerType *>(hwParser.cmdWalker);
    ASSERT_NE(nullptr, walker);
    EXPECT_EQ(WalkerType::PARTITION_TYPE::PARTITION_TYPE_X, walker->getPartitionType());
    EXPECT_EQ(1u, walker->getPartitionSize());
}
