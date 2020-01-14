/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/built_ins/built_ins.h"
#include "runtime/built_ins/builtins_dispatch_builder.h"
#include "runtime/command_queue/command_queue_hw.h"
#include "runtime/command_queue/enqueue_copy_image.h"
#include "runtime/command_queue/enqueue_fill_image.h"
#include "runtime/command_queue/enqueue_read_image.h"
#include "runtime/command_queue/enqueue_write_image.h"
#include "runtime/command_queue/gpgpu_walker.h"
#include "runtime/event/event.h"
#include "runtime/event/perf_counter.h"
#include "runtime/helpers/hardware_commands_helper.h"
#include "runtime/kernel/kernel.h"
#include "test.h"
#include "unit_tests/command_queue/command_enqueue_fixture.h"
#include "unit_tests/command_queue/enqueue_fixture.h"
#include "unit_tests/command_queue/enqueue_write_image_fixture.h"
#include "unit_tests/fixtures/built_in_fixture.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/mocks/mock_kernel.h"

using namespace NEO;

struct GetSizeRequiredImageTest : public CommandEnqueueFixture,
                                  public ::testing::Test {

    GetSizeRequiredImageTest() {
    }

    void SetUp() override {
        CommandEnqueueFixture::SetUp();

        srcImage = Image2dHelper<>::create(context);
        dstImage = Image2dHelper<>::create(context);

        pDevice->setPreemptionMode(PreemptionMode::Disabled);
    }

    void TearDown() override {
        delete dstImage;
        delete srcImage;

        CommandEnqueueFixture::TearDown();
    }

    Image *srcImage = nullptr;
    Image *dstImage = nullptr;
};

HWTEST_F(GetSizeRequiredImageTest, WhenCopyingImageThenHeapsAndCommandBufferConsumedMinimumRequiredSize) {
    auto &commandStream = pCmdQ->getCS(1024);
    auto usedBeforeCS = commandStream.getUsed();
    auto &dsh = pCmdQ->getIndirectHeap(IndirectHeap::DYNAMIC_STATE, 0u);
    auto &ioh = pCmdQ->getIndirectHeap(IndirectHeap::INDIRECT_OBJECT, 0u);
    auto &ssh = pCmdQ->getIndirectHeap(IndirectHeap::SURFACE_STATE, 0u);
    auto usedBeforeDSH = dsh.getUsed();
    auto usedBeforeIOH = ioh.getUsed();
    auto usedBeforeSSH = ssh.getUsed();

    auto retVal = EnqueueCopyImageHelper<>::enqueueCopyImage(pCmdQ);
    EXPECT_EQ(CL_SUCCESS, retVal);

    MultiDispatchInfo multiDispatchInfo;
    auto &builder = pCmdQ->getDevice().getExecutionEnvironment()->getBuiltIns()->getBuiltinDispatchInfoBuilder(EBuiltInOps::CopyImageToImage3d,
                                                                                                               pCmdQ->getContext(), pCmdQ->getDevice());
    ASSERT_NE(nullptr, &builder);

    BuiltinOpParams dc;
    dc.srcMemObj = srcImage;
    dc.dstMemObj = dstImage;
    dc.srcOffset = EnqueueCopyImageTraits::srcOrigin;
    dc.dstOffset = EnqueueCopyImageTraits::dstOrigin;
    dc.size = {1, 1, 1};
    builder.buildDispatchInfos(multiDispatchInfo, dc);
    EXPECT_NE(0u, multiDispatchInfo.size());

    auto kernel = multiDispatchInfo.begin()->getKernel();
    ASSERT_NE(nullptr, kernel);

    auto usedAfterCS = commandStream.getUsed();
    auto usedAfterDSH = dsh.getUsed();
    auto usedAfterIOH = ioh.getUsed();
    auto usedAfterSSH = ssh.getUsed();

    auto expectedSizeCS = EnqueueOperation<FamilyType>::getSizeRequiredCS(CL_COMMAND_COPY_IMAGE, false, false, *pCmdQ, kernel);
    auto expectedSizeDSH = HardwareCommandsHelper<FamilyType>::getSizeRequiredDSH(*kernel);
    auto expectedSizeIOH = HardwareCommandsHelper<FamilyType>::getSizeRequiredIOH(*kernel);
    auto expectedSizeSSH = HardwareCommandsHelper<FamilyType>::getSizeRequiredSSH(*kernel);

    // Since each enqueue* may flush, we may see a MI_BATCH_BUFFER_END appended.
    expectedSizeCS += sizeof(typename FamilyType::MI_BATCH_BUFFER_END);
    expectedSizeCS = alignUp(expectedSizeCS, MemoryConstants::cacheLineSize);

    EXPECT_GE(expectedSizeCS, usedAfterCS - usedBeforeCS);
    EXPECT_GE(expectedSizeDSH, usedAfterDSH - usedBeforeDSH);
    EXPECT_GE(expectedSizeIOH, usedAfterIOH - usedBeforeIOH);
    EXPECT_GE(expectedSizeSSH, usedAfterSSH - usedBeforeSSH);
}

HWTEST_F(GetSizeRequiredImageTest, WhenCopyingReadWriteImageThenHeapsAndCommandBufferConsumedMinimumRequiredSize) {
    auto &commandStream = pCmdQ->getCS(1024);
    auto usedBeforeCS = commandStream.getUsed();
    auto &dsh = pCmdQ->getIndirectHeap(IndirectHeap::DYNAMIC_STATE, 0u);
    auto &ioh = pCmdQ->getIndirectHeap(IndirectHeap::INDIRECT_OBJECT, 0u);
    auto &ssh = pCmdQ->getIndirectHeap(IndirectHeap::SURFACE_STATE, 0u);
    auto usedBeforeDSH = dsh.getUsed();
    auto usedBeforeIOH = ioh.getUsed();
    auto usedBeforeSSH = ssh.getUsed();

    std::unique_ptr<Program> program(Program::create("CopyImageToImage3d", context, *pClDevice, true, nullptr));
    cl_device_id device = pClDevice;
    program->build(1, &device, nullptr, nullptr, nullptr, false);
    std::unique_ptr<Kernel> kernel(Kernel::create<MockKernel>(program.get(), *program->getKernelInfo("CopyImageToImage3d"), nullptr));

    EXPECT_NE(nullptr, kernel);
    // This kernel does not operate on OpenCL 2.0 Read and Write images
    EXPECT_EQ(kernel->getKernelInfo().patchInfo.executionEnvironment->UsesFencesForReadWriteImages, (uint32_t) false);
    // Simulate that the kernel actually operates on OpenCL 2.0 Read and Write images.
    // Such kernel may require special WA DisableLSQCROPERFforOCL during construction of Command Buffer
    struct SPatchExecutionEnvironment *pExecEnv = (struct SPatchExecutionEnvironment *)kernel->getKernelInfo().patchInfo.executionEnvironment;
    pExecEnv->UsesFencesForReadWriteImages = (uint32_t) true;
    EXPECT_EQ(kernel->getKernelInfo().patchInfo.executionEnvironment->UsesFencesForReadWriteImages, (uint32_t) true);

    // Enqueue kernel that may require special WA DisableLSQCROPERFforOCL
    auto retVal = EnqueueKernelHelper<>::enqueueKernel(pCmdQ, kernel.get());
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto usedAfterCS = commandStream.getUsed();
    auto usedAfterDSH = dsh.getUsed();
    auto usedAfterIOH = ioh.getUsed();
    auto usedAfterSSH = ssh.getUsed();

    auto expectedSizeCS = EnqueueOperation<FamilyType>::getSizeRequiredCS(CL_COMMAND_COPY_IMAGE, false, false, *pCmdQ, kernel.get());
    auto expectedSizeDSH = HardwareCommandsHelper<FamilyType>::getSizeRequiredDSH(*kernel.get());
    auto expectedSizeIOH = HardwareCommandsHelper<FamilyType>::getSizeRequiredIOH(*kernel.get());
    auto expectedSizeSSH = HardwareCommandsHelper<FamilyType>::getSizeRequiredSSH(*kernel.get());

    // Since each enqueue* may flush, we may see a MI_BATCH_BUFFER_END appended.
    expectedSizeCS += sizeof(typename FamilyType::MI_BATCH_BUFFER_END);
    expectedSizeCS = alignUp(expectedSizeCS, MemoryConstants::cacheLineSize);

    pExecEnv->UsesFencesForReadWriteImages = (uint32_t) false;

    EXPECT_GE(expectedSizeCS, usedAfterCS - usedBeforeCS);
    EXPECT_GE(expectedSizeDSH, usedAfterDSH - usedBeforeDSH);
    EXPECT_GE(expectedSizeIOH, usedAfterIOH - usedBeforeIOH);
    EXPECT_GE(expectedSizeSSH, usedAfterSSH - usedBeforeSSH);
}

HWTEST_F(GetSizeRequiredImageTest, WhenReadingImageNonBlockingThenHeapsAndCommandBufferConsumedMinimumRequiredSize) {
    auto &commandStream = pCmdQ->getCS(1024);
    auto usedBeforeCS = commandStream.getUsed();
    auto &dsh = pCmdQ->getIndirectHeap(IndirectHeap::DYNAMIC_STATE, 0u);
    auto &ioh = pCmdQ->getIndirectHeap(IndirectHeap::INDIRECT_OBJECT, 0u);
    auto &ssh = pCmdQ->getIndirectHeap(IndirectHeap::SURFACE_STATE, 0u);
    auto usedBeforeDSH = dsh.getUsed();
    auto usedBeforeIOH = ioh.getUsed();
    auto usedBeforeSSH = ssh.getUsed();

    auto retVal = EnqueueReadImageHelper<>::enqueueReadImage(
        pCmdQ,
        srcImage,
        CL_FALSE);
    EXPECT_EQ(CL_SUCCESS, retVal);

    MultiDispatchInfo multiDispatchInfo;
    auto &builder = pCmdQ->getDevice().getExecutionEnvironment()->getBuiltIns()->getBuiltinDispatchInfoBuilder(EBuiltInOps::CopyImage3dToBuffer,
                                                                                                               pCmdQ->getContext(), pCmdQ->getDevice());
    ASSERT_NE(nullptr, &builder);

    BuiltinOpParams dc;
    dc.srcMemObj = srcImage;
    dc.dstPtr = EnqueueReadImageTraits::hostPtr;
    dc.srcOffset = EnqueueReadImageTraits::origin;
    dc.size = EnqueueReadImageTraits::region;
    dc.srcRowPitch = EnqueueReadImageTraits::rowPitch;
    dc.srcSlicePitch = EnqueueReadImageTraits::slicePitch;
    builder.buildDispatchInfos(multiDispatchInfo, dc);
    EXPECT_NE(0u, multiDispatchInfo.size());

    auto kernel = multiDispatchInfo.begin()->getKernel();
    ASSERT_NE(nullptr, kernel);

    auto usedAfterCS = commandStream.getUsed();
    auto usedAfterDSH = dsh.getUsed();
    auto usedAfterIOH = ioh.getUsed();
    auto usedAfterSSH = ssh.getUsed();

    auto expectedSizeCS = EnqueueOperation<FamilyType>::getSizeRequiredCS(CL_COMMAND_READ_IMAGE, false, false, *pCmdQ, kernel);
    auto expectedSizeDSH = HardwareCommandsHelper<FamilyType>::getSizeRequiredDSH(*kernel);
    auto expectedSizeIOH = HardwareCommandsHelper<FamilyType>::getSizeRequiredIOH(*kernel);
    auto expectedSizeSSH = HardwareCommandsHelper<FamilyType>::getSizeRequiredSSH(*kernel);

    // Since each enqueue* may flush, we may see a MI_BATCH_BUFFER_END appended.
    expectedSizeCS += sizeof(typename FamilyType::MI_BATCH_BUFFER_END);
    expectedSizeCS = alignUp(expectedSizeCS, MemoryConstants::cacheLineSize);

    EXPECT_GE(expectedSizeCS, usedAfterCS - usedBeforeCS);
    EXPECT_GE(expectedSizeDSH, usedAfterDSH - usedBeforeDSH);
    EXPECT_GE(expectedSizeIOH, usedAfterIOH - usedBeforeIOH);
    EXPECT_GE(expectedSizeSSH, usedAfterSSH - usedBeforeSSH);
}

HWTEST_F(GetSizeRequiredImageTest, WhenReadingImageBlockingThenHeapsAndCommandBufferConsumedMinimumRequiredSize) {
    auto &commandStream = pCmdQ->getCS(1024);
    auto usedBeforeCS = commandStream.getUsed();
    auto &dsh = pCmdQ->getIndirectHeap(IndirectHeap::DYNAMIC_STATE, 0u);
    auto &ioh = pCmdQ->getIndirectHeap(IndirectHeap::INDIRECT_OBJECT, 0u);
    auto &ssh = pCmdQ->getIndirectHeap(IndirectHeap::SURFACE_STATE, 0u);
    auto usedBeforeDSH = dsh.getUsed();
    auto usedBeforeIOH = ioh.getUsed();
    auto usedBeforeSSH = ssh.getUsed();

    auto retVal = EnqueueReadImageHelper<>::enqueueReadImage(
        pCmdQ,
        srcImage,
        CL_TRUE);
    EXPECT_EQ(CL_SUCCESS, retVal);

    MultiDispatchInfo multiDispatchInfo;
    auto &builder = pCmdQ->getDevice().getExecutionEnvironment()->getBuiltIns()->getBuiltinDispatchInfoBuilder(EBuiltInOps::CopyImage3dToBuffer,
                                                                                                               pCmdQ->getContext(), pCmdQ->getDevice());
    ASSERT_NE(nullptr, &builder);

    BuiltinOpParams dc;
    dc.srcMemObj = srcImage;
    dc.dstPtr = EnqueueReadImageTraits::hostPtr;
    dc.srcOffset = EnqueueReadImageTraits::origin;
    dc.size = EnqueueReadImageTraits::region;
    dc.srcRowPitch = EnqueueReadImageTraits::rowPitch;
    dc.srcSlicePitch = EnqueueReadImageTraits::slicePitch;
    builder.buildDispatchInfos(multiDispatchInfo, dc);
    EXPECT_NE(0u, multiDispatchInfo.size());

    auto kernel = multiDispatchInfo.begin()->getKernel();
    ASSERT_NE(nullptr, kernel);

    auto usedAfterCS = commandStream.getUsed();
    auto usedAfterDSH = dsh.getUsed();
    auto usedAfterIOH = ioh.getUsed();
    auto usedAfterSSH = ssh.getUsed();

    auto expectedSizeCS = EnqueueOperation<FamilyType>::getSizeRequiredCS(CL_COMMAND_READ_IMAGE, false, false, *pCmdQ, kernel);
    auto expectedSizeDSH = HardwareCommandsHelper<FamilyType>::getSizeRequiredDSH(*kernel);
    auto expectedSizeIOH = HardwareCommandsHelper<FamilyType>::getSizeRequiredIOH(*kernel);
    auto expectedSizeSSH = HardwareCommandsHelper<FamilyType>::getSizeRequiredSSH(*kernel);

    // Since each enqueue* may flush, we may see a MI_BATCH_BUFFER_END appended.
    expectedSizeCS += sizeof(typename FamilyType::MI_BATCH_BUFFER_END);
    expectedSizeCS = alignUp(expectedSizeCS, MemoryConstants::cacheLineSize);

    EXPECT_GE(expectedSizeCS, usedAfterCS - usedBeforeCS);
    EXPECT_GE(expectedSizeDSH, usedAfterDSH - usedBeforeDSH);
    EXPECT_GE(expectedSizeIOH, usedAfterIOH - usedBeforeIOH);
    EXPECT_GE(expectedSizeSSH, usedAfterSSH - usedBeforeSSH);
}

HWTEST_F(GetSizeRequiredImageTest, WhenWritingImageNonBlockingThenHeapsAndCommandBufferConsumedMinimumRequiredSize) {
    auto &commandStream = pCmdQ->getCS(1024);
    auto usedBeforeCS = commandStream.getUsed();
    auto &dsh = pCmdQ->getIndirectHeap(IndirectHeap::DYNAMIC_STATE, 0u);
    auto &ioh = pCmdQ->getIndirectHeap(IndirectHeap::INDIRECT_OBJECT, 0u);
    auto &ssh = pCmdQ->getIndirectHeap(IndirectHeap::SURFACE_STATE, 0u);
    auto usedBeforeDSH = dsh.getUsed();
    auto usedBeforeIOH = ioh.getUsed();
    auto usedBeforeSSH = ssh.getUsed();

    auto retVal = EnqueueWriteImageHelper<>::enqueueWriteImage(
        pCmdQ,
        dstImage,
        CL_FALSE);
    EXPECT_EQ(CL_SUCCESS, retVal);

    MultiDispatchInfo multiDispatchInfo;
    auto &builder = pCmdQ->getDevice().getExecutionEnvironment()->getBuiltIns()->getBuiltinDispatchInfoBuilder(EBuiltInOps::CopyBufferToImage3d,
                                                                                                               pCmdQ->getContext(), pCmdQ->getDevice());
    ASSERT_NE(nullptr, &builder);

    BuiltinOpParams dc;
    dc.srcPtr = EnqueueWriteImageTraits::hostPtr;
    dc.dstMemObj = dstImage;
    dc.dstOffset = EnqueueWriteImageTraits::origin;
    dc.size = EnqueueWriteImageTraits::region;
    dc.dstRowPitch = EnqueueWriteImageTraits::rowPitch;
    dc.dstSlicePitch = EnqueueWriteImageTraits::slicePitch;
    builder.buildDispatchInfos(multiDispatchInfo, dc);
    EXPECT_NE(0u, multiDispatchInfo.size());

    auto kernel = multiDispatchInfo.begin()->getKernel();
    ASSERT_NE(nullptr, kernel);

    auto usedAfterCS = commandStream.getUsed();
    auto usedAfterDSH = dsh.getUsed();
    auto usedAfterIOH = ioh.getUsed();
    auto usedAfterSSH = ssh.getUsed();

    auto expectedSizeCS = EnqueueOperation<FamilyType>::getSizeRequiredCS(CL_COMMAND_WRITE_IMAGE, false, false, *pCmdQ, kernel);
    auto expectedSizeDSH = HardwareCommandsHelper<FamilyType>::getSizeRequiredDSH(*kernel);
    auto expectedSizeIOH = HardwareCommandsHelper<FamilyType>::getSizeRequiredIOH(*kernel);
    auto expectedSizeSSH = HardwareCommandsHelper<FamilyType>::getSizeRequiredSSH(*kernel);

    // Since each enqueue* may flush, we may see a MI_BATCH_BUFFER_END appended.
    expectedSizeCS += sizeof(typename FamilyType::MI_BATCH_BUFFER_END);
    expectedSizeCS = alignUp(expectedSizeCS, MemoryConstants::cacheLineSize);

    EXPECT_GE(expectedSizeCS, usedAfterCS - usedBeforeCS);
    EXPECT_GE(expectedSizeDSH, usedAfterDSH - usedBeforeDSH);
    EXPECT_GE(expectedSizeIOH, usedAfterIOH - usedBeforeIOH);
    EXPECT_GE(expectedSizeSSH, usedAfterSSH - usedBeforeSSH);
}

HWTEST_F(GetSizeRequiredImageTest, WhenWritingImageBlockingThenHeapsAndCommandBufferConsumedMinimumRequiredSize) {
    auto &commandStream = pCmdQ->getCS(1024);
    auto usedBeforeCS = commandStream.getUsed();
    auto &dsh = pCmdQ->getIndirectHeap(IndirectHeap::DYNAMIC_STATE, 0u);
    auto &ioh = pCmdQ->getIndirectHeap(IndirectHeap::INDIRECT_OBJECT, 0u);
    auto &ssh = pCmdQ->getIndirectHeap(IndirectHeap::SURFACE_STATE, 0u);
    auto usedBeforeDSH = dsh.getUsed();
    auto usedBeforeIOH = ioh.getUsed();
    auto usedBeforeSSH = ssh.getUsed();

    auto retVal = EnqueueWriteImageHelper<>::enqueueWriteImage(
        pCmdQ,
        dstImage,
        CL_TRUE);
    EXPECT_EQ(CL_SUCCESS, retVal);

    MultiDispatchInfo multiDispatchInfo;
    auto &builder = pCmdQ->getDevice().getExecutionEnvironment()->getBuiltIns()->getBuiltinDispatchInfoBuilder(EBuiltInOps::CopyBufferToImage3d,
                                                                                                               pCmdQ->getContext(), pCmdQ->getDevice());
    ASSERT_NE(nullptr, &builder);

    BuiltinOpParams dc;
    dc.srcPtr = EnqueueWriteImageTraits::hostPtr;
    dc.dstMemObj = dstImage;
    dc.dstOffset = EnqueueWriteImageTraits::origin;
    dc.size = EnqueueWriteImageTraits::region;
    dc.dstRowPitch = EnqueueWriteImageTraits::rowPitch;
    dc.dstSlicePitch = EnqueueWriteImageTraits::slicePitch;
    builder.buildDispatchInfos(multiDispatchInfo, dc);
    EXPECT_NE(0u, multiDispatchInfo.size());

    auto kernel = multiDispatchInfo.begin()->getKernel();
    ASSERT_NE(nullptr, kernel);

    auto usedAfterCS = commandStream.getUsed();
    auto usedAfterDSH = dsh.getUsed();
    auto usedAfterIOH = ioh.getUsed();
    auto usedAfterSSH = ssh.getUsed();

    auto expectedSizeCS = EnqueueOperation<FamilyType>::getSizeRequiredCS(CL_COMMAND_WRITE_IMAGE, false, false, *pCmdQ, kernel);
    auto expectedSizeDSH = HardwareCommandsHelper<FamilyType>::getSizeRequiredDSH(*kernel);
    auto expectedSizeIOH = HardwareCommandsHelper<FamilyType>::getSizeRequiredIOH(*kernel);
    auto expectedSizeSSH = HardwareCommandsHelper<FamilyType>::getSizeRequiredSSH(*kernel);

    // Since each enqueue* may flush, we may see a MI_BATCH_BUFFER_END appended.
    expectedSizeCS += sizeof(typename FamilyType::MI_BATCH_BUFFER_END);
    expectedSizeCS = alignUp(expectedSizeCS, MemoryConstants::cacheLineSize);

    EXPECT_GE(expectedSizeCS, usedAfterCS - usedBeforeCS);
    EXPECT_GE(expectedSizeDSH, usedAfterDSH - usedBeforeDSH);
    EXPECT_GE(expectedSizeIOH, usedAfterIOH - usedBeforeIOH);
    EXPECT_GE(expectedSizeSSH, usedAfterSSH - usedBeforeSSH);
}
