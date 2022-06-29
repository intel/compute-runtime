/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/program/kernel_info.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/source/command_queue/command_queue_hw.h"
#include "opencl/source/helpers/hardware_commands_helper.h"
#include "opencl/source/kernel/kernel.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/mocks/mock_program.h"

using namespace NEO;

struct KernelSLMAndBarrierTest : public ClDeviceFixture,
                                 public ::testing::TestWithParam<uint32_t> {
    void SetUp() override {
        ClDeviceFixture::SetUp();
        program = std::make_unique<MockProgram>(toClDeviceVector(*pClDevice));

        kernelInfo.setCrossThreadDataSize(sizeof(crossThreadData));

        kernelInfo.setLocalIds({1, 1, 1});

        kernelInfo.heapInfo.pKernelHeap = kernelIsa;
        kernelInfo.heapInfo.KernelHeapSize = sizeof(kernelIsa);

        kernelInfo.kernelDescriptor.kernelAttributes.simdSize = 32;
    }
    void TearDown() override {
        ClDeviceFixture::TearDown();
    }

    uint32_t simd;
    uint32_t numChannels;

    std::unique_ptr<MockProgram> program;

    SKernelBinaryHeaderCommon kernelHeader;
    MockKernelInfo kernelInfo;

    uint32_t kernelIsa[32];
    uint32_t crossThreadData[32];
    uint32_t perThreadData[8];
};

static uint32_t slmSizeInKb[] = {1, 4, 8, 16, 32, 64};

HWCMDTEST_P(IGFX_GEN8_CORE, KernelSLMAndBarrierTest, GivenStaticSlmSizeWhenProgrammingSlmThenProgrammingIsCorrect) {
    ASSERT_NE(nullptr, pClDevice);
    CommandQueueHw<FamilyType> cmdQ(nullptr, pClDevice, 0, false);
    typedef typename FamilyType::INTERFACE_DESCRIPTOR_DATA INTERFACE_DESCRIPTOR_DATA;

    // define kernel info
    kernelInfo.kernelDescriptor.kernelAttributes.barrierCount = 1;
    kernelInfo.kernelDescriptor.kernelAttributes.slmInlineSize = GetParam() * KB;

    MockKernel kernel(program.get(), kernelInfo, *pClDevice);
    ASSERT_EQ(CL_SUCCESS, kernel.initialize());

    // After creating Mock Kernel now create Indirect Heap
    auto &indirectHeap = cmdQ.getIndirectHeap(IndirectHeap::Type::DYNAMIC_STATE, 8192);

    uint64_t interfaceDescriptorOffset = indirectHeap.getUsed();

    size_t offsetInterfaceDescriptorData = HardwareCommandsHelper<FamilyType>::sendInterfaceDescriptorData(
        indirectHeap,
        interfaceDescriptorOffset,
        0,
        sizeof(crossThreadData),
        sizeof(perThreadData),
        0,
        0,
        0,
        1,
        kernel,
        4u,
        pDevice->getPreemptionMode(),
        nullptr,
        *pDevice);

    // add the heap base + offset
    uint32_t *pIdData = (uint32_t *)indirectHeap.getCpuBase() + offsetInterfaceDescriptorData;

    INTERFACE_DESCRIPTOR_DATA *pSrcIDData = (INTERFACE_DESCRIPTOR_DATA *)pIdData;

    uint32_t expectedSlmSize = 0;

    if (::renderCoreFamily == IGFX_GEN8_CORE) {
        if (kernelInfo.kernelDescriptor.kernelAttributes.slmInlineSize <= (4 * 1024)) {
            expectedSlmSize = 1;
        } else if (kernelInfo.kernelDescriptor.kernelAttributes.slmInlineSize <= (8 * 1024)) {
            expectedSlmSize = 2;
        } else if (kernelInfo.kernelDescriptor.kernelAttributes.slmInlineSize <= (16 * 1024)) {
            expectedSlmSize = 4;
        } else if (kernelInfo.kernelDescriptor.kernelAttributes.slmInlineSize <= (32 * 1024)) {
            expectedSlmSize = 8;
        } else if (kernelInfo.kernelDescriptor.kernelAttributes.slmInlineSize <= (64 * 1024)) {
            expectedSlmSize = 16;
        }
    } else {
        if (kernelInfo.kernelDescriptor.kernelAttributes.slmInlineSize <= (1 * 1024)) // its a power of "2" +1 for example 1 is 2^0 ( 0+1); 2 is 2^1 is (1+1) etc.
        {
            expectedSlmSize = 1;
        } else if (kernelInfo.kernelDescriptor.kernelAttributes.slmInlineSize <= (2 * 1024)) {
            expectedSlmSize = 2;
        } else if (kernelInfo.kernelDescriptor.kernelAttributes.slmInlineSize <= (4 * 1024)) {
            expectedSlmSize = 3;
        } else if (kernelInfo.kernelDescriptor.kernelAttributes.slmInlineSize <= (8 * 1024)) {
            expectedSlmSize = 4;
        } else if (kernelInfo.kernelDescriptor.kernelAttributes.slmInlineSize <= (16 * 1024)) {
            expectedSlmSize = 5;
        } else if (kernelInfo.kernelDescriptor.kernelAttributes.slmInlineSize <= (32 * 1024)) {
            expectedSlmSize = 6;
        } else if (kernelInfo.kernelDescriptor.kernelAttributes.slmInlineSize <= (64 * 1024)) {
            expectedSlmSize = 7;
        }
    }
    ASSERT_GT(expectedSlmSize, 0u);
    EXPECT_EQ(expectedSlmSize, pSrcIDData->getSharedLocalMemorySize());
    EXPECT_EQ(kernelInfo.kernelDescriptor.kernelAttributes.usesBarriers(), pSrcIDData->getBarrierEnable());
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::DENORM_MODE_SETBYKERNEL, pSrcIDData->getDenormMode());

    if (EncodeSurfaceState<FamilyType>::doBindingTablePrefetch()) {
        EXPECT_EQ(4u, pSrcIDData->getBindingTableEntryCount());
    } else {
        EXPECT_EQ(0u, pSrcIDData->getBindingTableEntryCount());
    }
}

INSTANTIATE_TEST_CASE_P(
    SlmSizes,
    KernelSLMAndBarrierTest,
    testing::ValuesIn(slmSizeInKb));

HWTEST_F(KernelSLMAndBarrierTest, GivenInterfaceDescriptorProgrammedWhenOverrideSlmAllocationSizeIsSetThenSlmSizeIsOverwritten) {
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;

    uint32_t expectedSlmSize = 5;
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.OverrideSlmAllocationSize.set(expectedSlmSize);

    kernelInfo.kernelDescriptor.kernelAttributes.slmInlineSize = 0;

    MockKernel kernel(program.get(), kernelInfo, *pClDevice);
    ASSERT_EQ(CL_SUCCESS, kernel.initialize());

    CommandQueueHw<FamilyType> cmdQ(nullptr, pClDevice, 0, false);
    auto &indirectHeap = cmdQ.getIndirectHeap(IndirectHeap::Type::DYNAMIC_STATE, 8192);

    uint64_t interfaceDescriptorOffset = indirectHeap.getUsed();
    INTERFACE_DESCRIPTOR_DATA interfaceDescriptorData;

    HardwareCommandsHelper<FamilyType>::sendInterfaceDescriptorData(
        indirectHeap,
        interfaceDescriptorOffset,
        0,
        sizeof(crossThreadData),
        sizeof(perThreadData),
        0,
        0,
        0,
        1,
        kernel,
        4u,
        pDevice->getPreemptionMode(),
        &interfaceDescriptorData,
        *pDevice);

    auto pInterfaceDescriptor = HardwareCommandsHelper<FamilyType>::getInterfaceDescriptor(indirectHeap, interfaceDescriptorOffset, &interfaceDescriptorData);

    EXPECT_EQ(expectedSlmSize, pInterfaceDescriptor->getSharedLocalMemorySize());
}
