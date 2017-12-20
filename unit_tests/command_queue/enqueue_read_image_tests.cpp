/*
 * Copyright (c) 2017, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "runtime/gen_common/reg_configs.h"
#include "unit_tests/command_queue/enqueue_read_image_fixture.h"
#include "test.h"

using namespace OCLRT;

HWTEST_F(EnqueueReadImageTest, gpgpuWalker) {
    typedef typename FamilyType::GPGPU_WALKER GPGPU_WALKER;
    enqueueReadImage<FamilyType>();

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

HWTEST_F(EnqueueReadImageTest, alignsToCSR_Blocking) {
    //this test case assumes IOQ
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.taskCount = pCmdQ->taskCount + 100;
    csr.taskLevel = pCmdQ->taskLevel + 50;
    auto oldCsrTaskLevel = csr.peekTaskLevel();

    enqueueReadImage<FamilyType>(CL_TRUE);
    EXPECT_EQ(csr.peekTaskCount(), pCmdQ->taskCount);
    EXPECT_EQ(oldCsrTaskLevel, pCmdQ->taskLevel);
}

HWTEST_F(EnqueueReadImageTest, alignsToCSR_NonBlocking) {
    //this test case assumes IOQ
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.taskCount = pCmdQ->taskCount + 100;
    csr.taskLevel = pCmdQ->taskLevel + 50;

    enqueueReadImage<FamilyType>(CL_FALSE);
    EXPECT_EQ(csr.peekTaskCount(), pCmdQ->taskCount);
    EXPECT_EQ(csr.peekTaskLevel(), pCmdQ->taskLevel + 1);
}

HWTEST_F(EnqueueReadImageTest, bumpsTaskLevel) {
    auto taskLevelBefore = pCmdQ->taskLevel;

    enqueueReadImage<FamilyType>();
    EXPECT_GT(pCmdQ->taskLevel, taskLevelBefore);
}

HWTEST_F(EnqueueReadImageTest, addsCommands) {
    auto usedCmdBufferBefore = pCS->getUsed();

    enqueueReadImage<FamilyType>();
    EXPECT_NE(usedCmdBufferBefore, pCS->getUsed());
}

HWTEST_F(EnqueueReadImageTest, addsIndirectData) {
    auto dshBefore = pDSH->getUsed();
    auto iohBefore = pIOH->getUsed();
    auto ihBefore = pIH->getUsed();
    auto sshBefore = pSSH->getUsed();

    enqueueReadImage<FamilyType>();
    EXPECT_NE(dshBefore, pDSH->getUsed());
    EXPECT_NE(iohBefore, pIOH->getUsed());
    EXPECT_NE(ihBefore, pIH->getUsed());
    EXPECT_NE(sshBefore, pSSH->getUsed());
}

HWTEST_F(EnqueueReadImageTest, loadRegisterImmediateL3CNTLREG) {
    typedef typename FamilyType::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;

    enqueueReadImage<FamilyType>();

    // All state should be programmed before walker
    auto itorCmd = findMmio<FamilyType>(cmdList.begin(), itorWalker, L3CNTLRegisterOffset<FamilyType>::registerOffset);
    ASSERT_NE(itorWalker, itorCmd);

    auto *cmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(*itorCmd);
    ASSERT_NE(nullptr, cmd);

    auto RegisterOffset = L3CNTLRegisterOffset<FamilyType>::registerOffset;
    EXPECT_EQ(RegisterOffset, cmd->getRegisterOffset());
    auto l3Cntlreg = cmd->getDataDword();
    auto numURBWays = (l3Cntlreg >> 1) & 0x7f;
    auto L3ClientPool = (l3Cntlreg >> 25) & 0x7f;
    EXPECT_NE(0u, numURBWays);
    EXPECT_NE(0u, L3ClientPool);
}

HWTEST_F(EnqueueReadImageTest, stateBaseAddress) {
    typedef typename FamilyType::STATE_BASE_ADDRESS STATE_BASE_ADDRESS;

    enqueueReadImage<FamilyType>();

    // All state should be programmed before walker
    auto *cmd = getCommand<STATE_BASE_ADDRESS>(itorPipelineSelect, itorWalker);

    // Verify all addresses are getting programmed
    EXPECT_TRUE(cmd->getDynamicStateBaseAddressModifyEnable());
    EXPECT_TRUE(cmd->getGeneralStateBaseAddressModifyEnable());
    EXPECT_TRUE(cmd->getSurfaceStateBaseAddressModifyEnable());
    EXPECT_TRUE(cmd->getIndirectObjectBaseAddressModifyEnable());
    EXPECT_TRUE(cmd->getInstructionBaseAddressModifyEnable());

    EXPECT_EQ((uintptr_t)pDSH->getBase(), cmd->getDynamicStateBaseAddress());
    // Stateless accesses require GSH.base to be 0.
    EXPECT_EQ(0u, cmd->getGeneralStateBaseAddress());
    EXPECT_EQ((uintptr_t)pSSH->getBase(), cmd->getSurfaceStateBaseAddress());
    EXPECT_EQ((uintptr_t)pIOH->getBase(), cmd->getIndirectObjectBaseAddress());
    EXPECT_EQ((uintptr_t)pIH->getBase(), cmd->getInstructionBaseAddress());

    // Verify all sizes are getting programmed
    EXPECT_TRUE(cmd->getDynamicStateBufferSizeModifyEnable());
    EXPECT_TRUE(cmd->getGeneralStateBufferSizeModifyEnable());
    EXPECT_TRUE(cmd->getIndirectObjectBufferSizeModifyEnable());
    EXPECT_TRUE(cmd->getInstructionBufferSizeModifyEnable());

    EXPECT_EQ(pDSH->getMaxAvailableSpace(), cmd->getDynamicStateBufferSize() * MemoryConstants::pageSize);
    EXPECT_NE(0u, cmd->getGeneralStateBufferSize());
    EXPECT_EQ(pIOH->getMaxAvailableSpace(), cmd->getIndirectObjectBufferSize() * MemoryConstants::pageSize);
    EXPECT_EQ(pIH->getMaxAvailableSpace(), cmd->getInstructionBufferSize() * MemoryConstants::pageSize);

    // Generically validate this command
    FamilyType::PARSE::template validateCommand<STATE_BASE_ADDRESS *>(cmdList.begin(), itorStateBaseAddress);
}

HWTEST_F(EnqueueReadImageTest, mediaInterfaceDescriptorLoad) {
    typedef typename FamilyType::MEDIA_INTERFACE_DESCRIPTOR_LOAD MEDIA_INTERFACE_DESCRIPTOR_LOAD;
    typedef typename FamilyType::INTERFACE_DESCRIPTOR_DATA INTERFACE_DESCRIPTOR_DATA;

    enqueueReadImage<FamilyType>();

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

HWTEST_F(EnqueueReadImageTest, interfaceDescriptorData) {
    typedef typename FamilyType::STATE_BASE_ADDRESS STATE_BASE_ADDRESS;
    typedef typename FamilyType::INTERFACE_DESCRIPTOR_DATA INTERFACE_DESCRIPTOR_DATA;

    enqueueReadImage<FamilyType>();

    // Extract the interfaceDescriptorData
    auto cmdSBA = (STATE_BASE_ADDRESS *)cmdStateBaseAddress;
    auto &interfaceDescriptorData = *(INTERFACE_DESCRIPTOR_DATA *)cmdInterfaceDescriptorData;

    // Validate the kernel start pointer.  Technically, a kernel can start at address 0 but let's force a value.
    auto kernelStartPointer = ((uint64_t)interfaceDescriptorData.getKernelStartPointerHigh() << 32) + interfaceDescriptorData.getKernelStartPointer();
    EXPECT_LE(kernelStartPointer, cmdSBA->getInstructionBufferSize());

    auto localWorkSize = 4u;
    auto simd = 32u;
    auto threadsPerThreadGroup = (localWorkSize + simd - 1) / simd;
    EXPECT_EQ(threadsPerThreadGroup, interfaceDescriptorData.getNumberOfThreadsInGpgpuThreadGroup());
    EXPECT_NE(0u, interfaceDescriptorData.getCrossThreadConstantDataReadLength());
    EXPECT_NE(0u, interfaceDescriptorData.getConstantIndirectUrbEntryReadLength());

    // We shouldn't have these pointers the same.
    EXPECT_NE(kernelStartPointer, interfaceDescriptorData.getBindingTablePointer());
}

HWTEST_F(EnqueueReadImageTest, surfaceState) {
    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;

    enqueueReadImage<FamilyType>();

    // BufferToImage kernel uses BTI=1 for destSurface
    uint32_t bindingTableIndex = 0;
    const auto &surfaceState = getSurfaceState<FamilyType>(bindingTableIndex);

    // EnqueueReadImage uses multi-byte copies depending on per-pixel-size-in-bytes
    const auto &imageDesc = srcImage->getImageDesc();
    EXPECT_EQ(imageDesc.image_width, surfaceState.getWidth());
    EXPECT_EQ(imageDesc.image_height, surfaceState.getHeight());
    EXPECT_NE(0u, surfaceState.getSurfacePitch());
    EXPECT_NE(0u, surfaceState.getSurfaceType());
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R32_UINT, surfaceState.getSurfaceFormat());
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_HORIZONTAL_ALIGNMENT_HALIGN_4, surfaceState.getSurfaceHorizontalAlignment());
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_VERTICAL_ALIGNMENT_VALIGN_4, surfaceState.getSurfaceVerticalAlignment());
    EXPECT_EQ(reinterpret_cast<uint64_t>(srcImage->getCpuAddress()), surfaceState.getSurfaceBaseAddress());
}

HWTEST_F(EnqueueReadImageTest, pipelineSelect) {
    typedef typename FamilyType::PIPELINE_SELECT PIPELINE_SELECT;

    enqueueReadImage<FamilyType>();

    auto *cmd = (PIPELINE_SELECT *)cmdPipelineSelect;

    // Verify we have a valid length
    EXPECT_EQ(PIPELINE_SELECT::PIPELINE_SELECTION_GPGPU, cmd->getPipelineSelection());

    // Specifying GPGPU mode requires setting equivalent mask bits.
    EXPECT_EQ(0x3u, cmd->getMaskBits() & 0x3);
}

HWTEST_F(EnqueueReadImageTest, mediaVFEState) {
    typedef typename FamilyType::MEDIA_VFE_STATE MEDIA_VFE_STATE;

    enqueueReadImage<FamilyType>();

    auto *cmd = (MEDIA_VFE_STATE *)cmdMediaVfeState;

    // Verify we have a valid length
    EXPECT_EQ(pDevice->getHardwareInfo().pSysInfo->ThreadCount, cmd->getMaximumNumberOfThreads());
    EXPECT_NE(0u, cmd->getNumberOfUrbEntries());
    EXPECT_NE(0u, cmd->getUrbEntryAllocationSize());

    // Generically validate this command
    FamilyType::PARSE::template validateCommand<MEDIA_VFE_STATE *>(cmdList.begin(), itorMediaVfeState);
}

HWTEST_F(EnqueueReadImageTest, blockingEnqueueRequiresPCWithDCFlushSetAfterWalker) {
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;

    bool blocking = true;
    enqueueReadImage<FamilyType>(blocking);

    auto itorWalker = find<typename FamilyType::GPGPU_WALKER *>(cmdList.begin(), cmdList.end());

    auto itorCmd = find<PIPE_CONTROL *>(itorWalker, cmdList.end());
    auto *cmd = (PIPE_CONTROL *)*itorCmd;
    EXPECT_NE(cmdList.end(), itorCmd);

    if (::renderCoreFamily != IGFX_GEN8_CORE) {
        // SKL+: two PIPE_CONTROLs following GPGPU_WALKER: first has DcFlush and second has Write HwTag
        EXPECT_TRUE(cmd->getDcFlushEnable());
        // Move to next PPC
        auto itorCmdP = ++((GenCmdList::iterator)itorCmd);
        EXPECT_NE(cmdList.end(), itorCmdP);
        auto itorCmd2 = find<PIPE_CONTROL *>(itorCmdP, cmdList.end());
        cmd = (PIPE_CONTROL *)*itorCmd2;
        EXPECT_FALSE(cmd->getDcFlushEnable());
    } else {
        // BDW: single PIPE_CONTROL following GPGPU_WALKER has DcFlush and Write HwTag
        EXPECT_TRUE(cmd->getDcFlushEnable());
    }
}

HWTEST_F(EnqueueReadImageTest, GivenImage1DarrayWhenReadImageIsCalledThenHostPtrSizeIsCalculatedProperly) {
    auto srcImage = Image1dArrayHelper<>::create(context);
    auto imageDesc = srcImage->getImageDesc();
    auto imageSize = imageDesc.image_width * imageDesc.image_array_size * 4;
    size_t origin[] = {0, 0, 0};
    size_t region[] = {imageDesc.image_width, imageDesc.image_array_size, 1};

    EnqueueReadImageHelper<>::enqueueReadImage(pCmdQ, srcImage, CL_FALSE, origin, region);

    auto memoryManager = pCmdQ->getDevice().getMemoryManager();

    auto temporaryAllocation = memoryManager->graphicsAllocations.peekHead();
    ASSERT_NE(nullptr, temporaryAllocation);

    EXPECT_EQ(temporaryAllocation->getUnderlyingBufferSize(), imageSize);

    delete srcImage;
}

HWTEST_F(EnqueueReadImageTest, GivenImage2DarrayWhenReadImageIsCalledThenHostPtrSizeIsCalculatedProperly) {
    auto srcImage = Image2dArrayHelper<>::create(context);
    auto imageDesc = srcImage->getImageDesc();
    auto imageSize = imageDesc.image_width * imageDesc.image_height * imageDesc.image_array_size * 4;
    size_t origin[] = {0, 0, 0};
    size_t region[] = {imageDesc.image_width, imageDesc.image_height, imageDesc.image_array_size};

    EnqueueReadImageHelper<>::enqueueReadImage(pCmdQ, srcImage, CL_FALSE, origin, region);

    auto memoryManager = pCmdQ->getDevice().getMemoryManager();

    auto temporaryAllocation = memoryManager->graphicsAllocations.peekHead();
    ASSERT_NE(nullptr, temporaryAllocation);

    EXPECT_EQ(temporaryAllocation->getUnderlyingBufferSize(), imageSize);

    delete srcImage;
}
