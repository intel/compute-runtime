/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_queue/command_queue_hw.h"
#include "runtime/helpers/kernel_commands.h"
#include "runtime/kernel/kernel.h"
#include "runtime/program/kernel_info.h"
#include "test.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/mocks/mock_kernel.h"
#include "unit_tests/mocks/mock_program.h"

#include "hw_cmds.h"

using namespace OCLRT;

struct KernelSLMAndBarrierTest : public DeviceFixture,
                                 public ::testing::TestWithParam<uint32_t> {
    void SetUp() override {
        DeviceFixture::SetUp();
        program = std::make_unique<MockProgram>(*pDevice->getExecutionEnvironment());

        memset(&kernelHeader, 0, sizeof(kernelHeader));
        kernelHeader.KernelHeapSize = sizeof(kernelIsa);

        memset(&dataParameterStream, 0, sizeof(dataParameterStream));
        dataParameterStream.DataParameterStreamSize = sizeof(crossThreadData);

        executionEnvironment = {};
        memset(&executionEnvironment, 0, sizeof(executionEnvironment));
        executionEnvironment.CompiledSIMD32 = 1;
        executionEnvironment.LargestCompiledSIMDSize = 32;

        memset(&threadPayload, 0, sizeof(threadPayload));
        threadPayload.LocalIDXPresent = 1;
        threadPayload.LocalIDYPresent = 1;
        threadPayload.LocalIDZPresent = 1;

        kernelInfo.heapInfo.pKernelHeap = kernelIsa;
        kernelInfo.heapInfo.pKernelHeader = &kernelHeader;
        kernelInfo.patchInfo.dataParameterStream = &dataParameterStream;
        kernelInfo.patchInfo.executionEnvironment = &executionEnvironment;
        kernelInfo.patchInfo.threadPayload = &threadPayload;
    }
    void TearDown() override {
        DeviceFixture::TearDown();
    }

    uint32_t simd;
    uint32_t numChannels;

    std::unique_ptr<MockProgram> program;

    SKernelBinaryHeaderCommon kernelHeader;
    SPatchDataParameterStream dataParameterStream;
    SPatchExecutionEnvironment executionEnvironment;
    SPatchThreadPayload threadPayload;
    KernelInfo kernelInfo;

    uint32_t kernelIsa[32];
    uint32_t crossThreadData[32];
    uint32_t perThreadData[8];
};

static uint32_t slmSizeInKb[] = {1, 4, 8, 16, 32, 64};

HWCMDTEST_P(IGFX_GEN8_CORE, KernelSLMAndBarrierTest, test_SLMProgramming) {
    ASSERT_NE(nullptr, pDevice);
    CommandQueueHw<FamilyType> cmdQ(nullptr, pDevice, 0);
    typedef typename FamilyType::INTERFACE_DESCRIPTOR_DATA INTERFACE_DESCRIPTOR_DATA;

    // define kernel info
    executionEnvironment.HasBarriers = 1;
    kernelInfo.workloadInfo.slmStaticSize = GetParam() * KB;

    MockKernel kernel(program.get(), kernelInfo, *pDevice);
    ASSERT_EQ(CL_SUCCESS, kernel.initialize());

    // After creating Mock Kernel now create Indirect Heap
    auto &indirectHeap = cmdQ.getIndirectHeap(IndirectHeap::DYNAMIC_STATE, 8192);

    uint64_t interfaceDescriptorOffset = indirectHeap.getUsed();

    size_t offsetInterfaceDescriptorData = KernelCommandsHelper<FamilyType>::sendInterfaceDescriptorData(
        indirectHeap,
        interfaceDescriptorOffset,
        0,
        sizeof(crossThreadData),
        sizeof(perThreadData),
        0,
        0,
        0,
        1,
        kernelInfo.workloadInfo.slmStaticSize,
        4u,
        !!executionEnvironment.HasBarriers, // Barriers Enabled
        pDevice->getPreemptionMode(),
        nullptr);

    // add the heap base + offset
    uint32_t *pIdData = (uint32_t *)indirectHeap.getCpuBase() + offsetInterfaceDescriptorData;

    INTERFACE_DESCRIPTOR_DATA *pSrcIDData = (INTERFACE_DESCRIPTOR_DATA *)pIdData;

    uint32_t ExpectedSLMSize = 0;

    if (::renderCoreFamily == IGFX_GEN8_CORE) {
        if (kernelInfo.workloadInfo.slmStaticSize <= (4 * 1024)) {
            ExpectedSLMSize = 1;
        } else if (kernelInfo.workloadInfo.slmStaticSize <= (8 * 1024)) {
            ExpectedSLMSize = 2;
        } else if (kernelInfo.workloadInfo.slmStaticSize <= (16 * 1024)) {
            ExpectedSLMSize = 4;
        } else if (kernelInfo.workloadInfo.slmStaticSize <= (32 * 1024)) {
            ExpectedSLMSize = 8;
        } else if (kernelInfo.workloadInfo.slmStaticSize <= (64 * 1024)) {
            ExpectedSLMSize = 16;
        }
    } else {
        if (kernelInfo.workloadInfo.slmStaticSize <= (1 * 1024)) // its a power of "2" +1 for example 1 is 2^0 ( 0+1); 2 is 2^1 is (1+1) etc.
        {
            ExpectedSLMSize = 1;
        } else if (kernelInfo.workloadInfo.slmStaticSize <= (2 * 1024)) {
            ExpectedSLMSize = 2;
        } else if (kernelInfo.workloadInfo.slmStaticSize <= (4 * 1024)) {
            ExpectedSLMSize = 3;
        } else if (kernelInfo.workloadInfo.slmStaticSize <= (8 * 1024)) {
            ExpectedSLMSize = 4;
        } else if (kernelInfo.workloadInfo.slmStaticSize <= (16 * 1024)) {
            ExpectedSLMSize = 5;
        } else if (kernelInfo.workloadInfo.slmStaticSize <= (32 * 1024)) {
            ExpectedSLMSize = 6;
        } else if (kernelInfo.workloadInfo.slmStaticSize <= (64 * 1024)) {
            ExpectedSLMSize = 7;
        }
    }
    ASSERT_GT(ExpectedSLMSize, 0u);
    EXPECT_EQ(ExpectedSLMSize, pSrcIDData->getSharedLocalMemorySize());
    EXPECT_EQ(!!executionEnvironment.HasBarriers, pSrcIDData->getBarrierEnable());
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::DENORM_MODE_SETBYKERNEL, pSrcIDData->getDenormMode());
    EXPECT_EQ(4u, pSrcIDData->getBindingTableEntryCount());
}

INSTANTIATE_TEST_CASE_P(
    SlmSizes,
    KernelSLMAndBarrierTest,
    testing::ValuesIn(slmSizeInKb));
