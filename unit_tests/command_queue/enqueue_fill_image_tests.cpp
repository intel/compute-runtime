/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include <algorithm>
#include "reg_configs_common.h"
#include "runtime/helpers/convert_color.h"
#include "unit_tests/command_queue/enqueue_fill_image_fixture.h"
#include "unit_tests/gen_common/gen_commands_common_validation.h"
#include "unit_tests/helpers/unit_test_helper.h"
#include "test.h"

using namespace OCLRT;

class EnqueueFillImageTest : public EnqueueFillImageTestFixture,
                             public ::testing::Test {
    void SetUp(void) override {
        EnqueueFillImageTestFixture::SetUp();
    }

    void TearDown(void) override {
        EnqueueFillImageTestFixture::TearDown();
    }
};

HWTEST_F(EnqueueFillImageTest, alignsToCSR) {
    //this test case assumes IOQ
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.taskCount = pCmdQ->taskCount + 100;
    csr.taskLevel = pCmdQ->taskLevel + 50;

    EnqueueFillImageHelper<>::enqueueFillImage(pCmdQ, image);
    EXPECT_EQ(csr.peekTaskCount(), pCmdQ->taskCount);
    EXPECT_EQ(csr.peekTaskLevel(), pCmdQ->taskLevel + 1);
}

HWCMDTEST_F(IGFX_GEN8_CORE, EnqueueFillImageTest, gpgpuWalker) {
    typedef typename FamilyType::GPGPU_WALKER GPGPU_WALKER;
    enqueueFillImage<FamilyType>();

    auto *cmd = reinterpret_cast<GPGPU_WALKER *>(cmdWalker);
    ASSERT_NE(nullptr, cmd);

    // Verify GPGPU_WALKER parameters
    EXPECT_NE(0u, cmd->getThreadGroupIdXDimension());
    EXPECT_NE(0u, cmd->getThreadGroupIdYDimension());
    EXPECT_NE(0u, cmd->getThreadGroupIdZDimension());
    EXPECT_NE(0u, cmd->getRightExecutionMask());
    EXPECT_NE(0u, cmd->getBottomExecutionMask());
    EXPECT_EQ(GPGPU_WALKER::SIMD_SIZE_SIMD32, cmd->getSimdSize());
    EXPECT_NE(0u, cmd->getIndirectDataLength());
    EXPECT_FALSE(cmd->getIndirectParameterEnable());

    // Compute the SIMD lane mask
    size_t simd =
        cmd->getSimdSize() == GPGPU_WALKER::SIMD_SIZE_SIMD32 ? 32 : cmd->getSimdSize() == GPGPU_WALKER::SIMD_SIZE_SIMD16 ? 16 : 8;
    uint64_t simdMask = (1ull << simd) - 1;

    // Mask off lanes based on the execution masks
    auto laneMaskRight = cmd->getRightExecutionMask() & simdMask;
    auto lanesPerThreadX = 0;
    while (laneMaskRight) {
        lanesPerThreadX += laneMaskRight & 1;
        laneMaskRight >>= 1;
    }
}

HWTEST_F(EnqueueFillImageTest, bumpsTaskLevel) {
    auto taskLevelBefore = pCmdQ->taskLevel;

    EnqueueFillImageHelper<>::enqueueFillImage(pCmdQ, image);
    EXPECT_GT(pCmdQ->taskLevel, taskLevelBefore);
}

HWTEST_F(EnqueueFillImageTest, addsCommands) {
    auto usedCmdBufferBefore = pCS->getUsed();

    EnqueueFillImageHelper<>::enqueueFillImage(pCmdQ, image);
    EXPECT_NE(usedCmdBufferBefore, pCS->getUsed());
}

HWTEST_F(EnqueueFillImageTest, addsIndirectData) {
    auto dshBefore = pDSH->getUsed();
    auto iohBefore = pIOH->getUsed();
    auto sshBefore = pSSH->getUsed();

    EnqueueFillImageHelper<>::enqueueFillImage(pCmdQ, image);
    EXPECT_TRUE(UnitTestHelper<FamilyType>::evaluateDshUsage(dshBefore, pDSH->getUsed(), nullptr));
    EXPECT_NE(iohBefore, pIOH->getUsed());
    EXPECT_NE(sshBefore, pSSH->getUsed());
}

HWTEST_F(EnqueueFillImageTest, loadRegisterImmediateL3CNTLREG) {
    enqueueFillImage<FamilyType>();
    validateL3Programming<FamilyType>(cmdList, itorWalker);
}

HWCMDTEST_F(IGFX_GEN8_CORE, EnqueueFillImageTest, WhenEnqueueIsDoneThenStateBaseAddressIsProperlyProgrammed) {
    enqueueFillImage<FamilyType>();
    validateStateBaseAddress<FamilyType>(this->pCmdQ->getCommandStreamReceiver().getMemoryManager()->getInternalHeapBaseAddress(),
                                         pDSH, pIOH, pSSH, itorPipelineSelect, itorWalker, cmdList, 0llu);
}

HWCMDTEST_F(IGFX_GEN8_CORE, EnqueueFillImageTest, mediaInterfaceDescriptorLoad) {
    typedef typename FamilyType::MEDIA_INTERFACE_DESCRIPTOR_LOAD MEDIA_INTERFACE_DESCRIPTOR_LOAD;
    typedef typename FamilyType::INTERFACE_DESCRIPTOR_DATA INTERFACE_DESCRIPTOR_DATA;

    enqueueFillImage<FamilyType>();

    // All state should be programmed before walker
    auto cmd = reinterpret_cast<MEDIA_INTERFACE_DESCRIPTOR_LOAD *>(cmdMediaInterfaceDescriptorLoad);
    ASSERT_NE(nullptr, cmd);

    // Verify we have a valid length -- multiple of INTERFACE_DESCRIPTOR_DATAs
    EXPECT_EQ(0u, cmd->getInterfaceDescriptorTotalLength() % sizeof(INTERFACE_DESCRIPTOR_DATA));

    // Validate the start address
    size_t alignmentStartAddress = 64 * sizeof(uint8_t);
    EXPECT_EQ(0u, cmd->getInterfaceDescriptorDataStartAddress() % alignmentStartAddress);

    // Validate the length
    EXPECT_NE(0u, cmd->getInterfaceDescriptorTotalLength());
    size_t alignmentTotalLength = 32 * sizeof(uint8_t);
    EXPECT_EQ(0u, cmd->getInterfaceDescriptorTotalLength() % alignmentTotalLength);

    // Generically validate this command
    FamilyType::PARSE::template validateCommand<MEDIA_INTERFACE_DESCRIPTOR_LOAD *>(cmdList.begin(), itorMediaInterfaceDescriptorLoad);
}

HWCMDTEST_F(IGFX_GEN8_CORE, EnqueueFillImageTest, interfaceDescriptorData) {
    typedef typename FamilyType::STATE_BASE_ADDRESS STATE_BASE_ADDRESS;
    typedef typename FamilyType::INTERFACE_DESCRIPTOR_DATA INTERFACE_DESCRIPTOR_DATA;

    enqueueFillImage<FamilyType>();

    // Extract the interfaceDescriptorData
    auto cmdSBA = (STATE_BASE_ADDRESS *)cmdStateBaseAddress;
    auto &interfaceDescriptorData = *(INTERFACE_DESCRIPTOR_DATA *)cmdInterfaceDescriptorData;

    // Validate the kernel start pointer.  Technically, a kernel can start at address 0 but let's force a value.
    auto kernelStartPointer = ((uint64_t)interfaceDescriptorData.getKernelStartPointerHigh() << 32) + interfaceDescriptorData.getKernelStartPointer();
    EXPECT_LE(kernelStartPointer, cmdSBA->getInstructionBufferSize() * MemoryConstants::pageSize);

    size_t maxLocalSize = 256u;
    auto localWorkSize = std::min(maxLocalSize,
                                  Image2dDefaults::imageDesc.image_width * Image2dDefaults::imageDesc.image_height);
    auto simd = 32u;
    auto threadsPerThreadGroup = (localWorkSize + simd - 1) / simd;
    EXPECT_EQ(threadsPerThreadGroup, interfaceDescriptorData.getNumberOfThreadsInGpgpuThreadGroup());
    EXPECT_NE(0u, interfaceDescriptorData.getCrossThreadConstantDataReadLength());
    EXPECT_NE(0u, interfaceDescriptorData.getConstantIndirectUrbEntryReadLength());

    // We shouldn't have these pointers the same.
    EXPECT_NE(kernelStartPointer, interfaceDescriptorData.getBindingTablePointer());
}

HWTEST_F(EnqueueFillImageTest, surfaceState) {
    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;

    enqueueFillImage<FamilyType>();

    const auto &surfaceState = getSurfaceState<FamilyType>(0);
    const auto &imageDesc = image->getImageDesc();
    EXPECT_EQ(imageDesc.image_width, surfaceState.getWidth());
    EXPECT_EQ(imageDesc.image_height, surfaceState.getHeight());
    EXPECT_NE(0u, surfaceState.getSurfacePitch());
    EXPECT_NE(0u, surfaceState.getSurfaceType());
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_HORIZONTAL_ALIGNMENT_HALIGN_4, surfaceState.getSurfaceHorizontalAlignment());
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_VERTICAL_ALIGNMENT_VALIGN_4, surfaceState.getSurfaceVerticalAlignment());

    const auto &srcSurfaceState = getSurfaceState<FamilyType>(0);
    EXPECT_EQ(reinterpret_cast<uint64_t>(image->getCpuAddress()), srcSurfaceState.getSurfaceBaseAddress());
}

HWTEST_F(EnqueueFillImageTest, pipelineSelect) {
    enqueueFillImage<FamilyType>();
    int numCommands = getNumberOfPipelineSelectsThatEnablePipelineSelect<FamilyType>();
    EXPECT_EQ(1, numCommands);
}

HWCMDTEST_F(IGFX_GEN8_CORE, EnqueueFillImageTest, mediaVFEState) {
    enqueueFillImage<FamilyType>();
    validateMediaVFEState<FamilyType>(&pDevice->getHardwareInfo(), cmdMediaVfeState, cmdList, itorMediaVfeState);
}

TEST_F(EnqueueFillImageTest, sRGBConvert) {
    float *fillColor;
    int iFillColor[4] = {0};
    float LessThanZeroArray[4] = {-1.0f, -1.0f, -1.0f, 1.0f};
    float MoreThanOneArray[4] = {2.0f, 2.0f, 2.0f, 1.0f};
    float NaNArray[4] = {NAN, NAN, NAN, 1.0f};
    float distance;

    cl_image_format oldImageFormat = {CL_sRGBA, CL_UNORM_INT8};
    cl_image_format newImageFormat = {CL_RGBA, CL_UNSIGNED_INT8};

    fillColor = LessThanZeroArray;

    convertFillColor(static_cast<const void *>(fillColor), iFillColor, oldImageFormat, newImageFormat);

    for (int i = 0; i < 3; i++) {
        distance = std::fabs(0.0f - static_cast<float>(iFillColor[i]));
        EXPECT_GE(0.6f, distance);
    }
    EXPECT_EQ(255, iFillColor[3]);

    fillColor = MoreThanOneArray;

    convertFillColor(static_cast<const void *>(fillColor), iFillColor, oldImageFormat, newImageFormat);

    for (int i = 0; i < 3; i++) {
        distance = std::fabs(255.0f - static_cast<float>(iFillColor[i]));
        EXPECT_GE(0.6f, distance);
    }
    EXPECT_EQ(255, iFillColor[3]);

    fillColor = NaNArray;

    convertFillColor(static_cast<const void *>(fillColor), iFillColor, oldImageFormat, newImageFormat);

    for (int i = 0; i < 3; i++) {
        distance = std::fabs(0.0f - static_cast<float>(iFillColor[i]));
        EXPECT_GE(0.6f, distance);
    }
    EXPECT_EQ(255, iFillColor[3]);
}

TEST(ColorConvertTest, givenSnorm8FormatWhenConvertingThenUseNormalizingFactor) {
    float fFillColor[4] = {0.3f, -0.3f, 0.0f, 1.0f};
    int32_t iFillColor[4] = {};
    int32_t expectedIFillColor[4] = {};

    cl_image_format oldFormat = {CL_R, CL_SNORM_INT8};
    cl_image_format newFormat = {CL_R, CL_UNSIGNED_INT8};
    auto normalizingFactor = selectNormalizingFactor(oldFormat.image_channel_data_type);
    for (size_t i = 0; i < 4; i++) {
        expectedIFillColor[i] = static_cast<int32_t>(normalizingFactor * fFillColor[i]);
        expectedIFillColor[i] = expectedIFillColor[i] & 0xFF;
    }

    convertFillColor(static_cast<const void *>(fFillColor), iFillColor, oldFormat, newFormat);

    EXPECT_TRUE(memcmp(expectedIFillColor, iFillColor, 4 * sizeof(int32_t)) == 0);
}

TEST(ColorConvertTest, givenSnorm16FormatWhenConvertingThenUseNormalizingFactor) {
    float fFillColor[4] = {0.3f, -0.3f, 0.0f, 1.0f};
    int32_t iFillColor[4] = {};
    int32_t expectedIFillColor[4] = {};

    cl_image_format oldFormat = {CL_R, CL_SNORM_INT16};
    cl_image_format newFormat = {CL_R, CL_UNSIGNED_INT16};
    auto normalizingFactor = selectNormalizingFactor(oldFormat.image_channel_data_type);
    for (size_t i = 0; i < 4; i++) {
        expectedIFillColor[i] = static_cast<int32_t>(normalizingFactor * fFillColor[i]);
        expectedIFillColor[i] = expectedIFillColor[i] & 0xFFFF;
    }

    convertFillColor(static_cast<const void *>(fFillColor), iFillColor, oldFormat, newFormat);

    EXPECT_TRUE(memcmp(expectedIFillColor, iFillColor, 4 * sizeof(int32_t)) == 0);
}
