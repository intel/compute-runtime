/*
 * Copyright (c) 2017 - 2018, Intel Corporation
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

#include "runtime/built_ins/builtins_dispatch_builder.h"
#include "runtime/command_queue/gpgpu_walker.h"
#include "runtime/command_queue/enqueue_fill_buffer.h"
#include "runtime/command_queue/enqueue_kernel.h"
#include "runtime/command_queue/enqueue_read_buffer.h"
#include "runtime/command_queue/enqueue_write_buffer.h"
#include "runtime/event/event.h"
#include "runtime/helpers/kernel_commands.h"
#include "runtime/kernel/kernel.h"

#include "unit_tests/command_queue/command_enqueue_fixture.h"
#include "unit_tests/command_queue/enqueue_fixture.h"
#include "unit_tests/fixtures/hello_world_kernel_fixture.h"
#include "unit_tests/fixtures/simple_arg_kernel_fixture.h"
#include "test.h"
#include "unit_tests/fixtures/buffer_fixture.h"
#include "unit_tests/fixtures/image_fixture.h"
using namespace OCLRT;

struct GetSizeRequiredBufferTest : public CommandEnqueueFixture,
                                   public SimpleArgKernelFixture,
                                   public HelloWorldKernelFixture,
                                   public ::testing::Test {

    using HelloWorldKernelFixture::SetUp;
    using SimpleArgKernelFixture::SetUp;

    GetSizeRequiredBufferTest() {
    }

    void SetUp() override {
        CommandEnqueueFixture::SetUp();
        SimpleArgKernelFixture::SetUp(pDevice);
        HelloWorldKernelFixture::SetUp(pDevice, "CopyBuffer_simd", "CopyBuffer");
        BufferDefaults::context = new MockContext;
        srcBuffer = BufferHelper<>::create();
        dstBuffer = BufferHelper<>::create();
        patternAllocation = context->getMemoryManager()->allocateGraphicsMemory(EnqueueFillBufferTraits::patternSize);
        pDevice->setPreemptionMode(PreemptionMode::Disabled);
    }

    void TearDown() override {
        context->getMemoryManager()->freeGraphicsMemory(patternAllocation);
        delete dstBuffer;
        delete srcBuffer;
        delete BufferDefaults::context;
        HelloWorldKernelFixture::TearDown();
        SimpleArgKernelFixture::TearDown();
        CommandEnqueueFixture::TearDown();
    }

    Buffer *srcBuffer = nullptr;
    Buffer *dstBuffer = nullptr;
    GraphicsAllocation *patternAllocation = nullptr;
};

HWCMDTEST_F(IGFX_GEN8_CORE, GetSizeRequiredBufferTest, enqueueFillBuffer) {
    typedef typename FamilyType::GPGPU_WALKER GPGPU_WALKER;
    auto &commandStream = pCmdQ->getCS();
    auto usedBeforeCS = commandStream.getUsed();
    auto &dsh = pCmdQ->getIndirectHeap(IndirectHeap::DYNAMIC_STATE, 0u);
    auto &ioh = pCmdQ->getIndirectHeap(IndirectHeap::INDIRECT_OBJECT, 0u);
    auto &ssh = pCmdQ->getIndirectHeap(IndirectHeap::SURFACE_STATE, 0u);
    auto usedBeforeDSH = dsh.getUsed();
    auto usedBeforeIOH = ioh.getUsed();
    auto usedBeforeSSH = ssh.getUsed();

    auto retVal = EnqueueFillBufferHelper<>::enqueue(pCmdQ);
    EXPECT_EQ(CL_SUCCESS, retVal);

    MultiDispatchInfo multiDispatchInfo;
    auto &builder = pCmdQ->getDevice().getBuiltIns().getBuiltinDispatchInfoBuilder(EBuiltInOps::FillBuffer,
                                                                                   pCmdQ->getContext(), pCmdQ->getDevice());
    ASSERT_NE(nullptr, &builder);

    BuiltinDispatchInfoBuilder::BuiltinOpParams dc;
    MemObj patternMemObj(this->context, 0, 0, alignUp(EnqueueFillBufferTraits::patternSize, 4), patternAllocation->getUnderlyingBuffer(),
                         patternAllocation->getUnderlyingBuffer(), patternAllocation, false, false, true);
    dc.srcMemObj = &patternMemObj;
    dc.dstMemObj = dstBuffer;
    dc.dstOffset = {EnqueueFillBufferTraits::offset, 0, 0};
    dc.size = {EnqueueFillBufferTraits::size, 0, 0};
    builder.buildDispatchInfos(multiDispatchInfo, dc);
    EXPECT_NE(0u, multiDispatchInfo.size());

    auto usedAfterCS = commandStream.getUsed();
    auto usedAfterDSH = dsh.getUsed();
    auto usedAfterIOH = ioh.getUsed();
    auto usedAfterSSH = ssh.getUsed();

    auto expectedSizeCS = EnqueueOperation<FamilyType>::getTotalSizeRequiredCS(false, false, *pCmdQ, multiDispatchInfo);
    auto expectedSizeDSH = KernelCommandsHelper<FamilyType>::getTotalSizeRequiredDSH(multiDispatchInfo);
    auto expectedSizeIOH = KernelCommandsHelper<FamilyType>::getTotalSizeRequiredIOH(multiDispatchInfo);
    auto expectedSizeSSH = KernelCommandsHelper<FamilyType>::getTotalSizeRequiredSSH(multiDispatchInfo);

    EXPECT_EQ(0u, expectedSizeIOH % GPGPU_WALKER::INDIRECTDATASTARTADDRESS_ALIGN_SIZE);
    EXPECT_EQ(0u, expectedSizeDSH % 64);

    // Since each enqueue* may flush, we may see a MI_BATCH_BUFFER_END appended.
    expectedSizeCS += sizeof(typename FamilyType::MI_BATCH_BUFFER_END);
    expectedSizeCS = alignUp(expectedSizeCS, MemoryConstants::cacheLineSize);

    EXPECT_EQ(expectedSizeCS, usedAfterCS - usedBeforeCS);
    EXPECT_GE(expectedSizeDSH, usedAfterDSH - usedBeforeDSH);
    EXPECT_GE(expectedSizeIOH, usedAfterIOH - usedBeforeIOH);
    EXPECT_GE(expectedSizeSSH, usedAfterSSH - usedBeforeSSH);
}

HWCMDTEST_F(IGFX_GEN8_CORE, GetSizeRequiredBufferTest, enqueueCopyBuffer) {
    typedef typename FamilyType::GPGPU_WALKER GPGPU_WALKER;
    auto &commandStream = pCmdQ->getCS();
    auto usedBeforeCS = commandStream.getUsed();
    auto &dsh = pCmdQ->getIndirectHeap(IndirectHeap::DYNAMIC_STATE, 0u);
    auto &ioh = pCmdQ->getIndirectHeap(IndirectHeap::INDIRECT_OBJECT, 0u);
    auto &ssh = pCmdQ->getIndirectHeap(IndirectHeap::SURFACE_STATE, 0u);
    auto usedBeforeDSH = dsh.getUsed();
    auto usedBeforeIOH = ioh.getUsed();
    auto usedBeforeSSH = ssh.getUsed();

    auto retVal = EnqueueCopyBufferHelper<>::enqueue(pCmdQ);
    EXPECT_EQ(CL_SUCCESS, retVal);

    MultiDispatchInfo multiDispatchInfo;
    auto &builder = pCmdQ->getDevice().getBuiltIns().getBuiltinDispatchInfoBuilder(EBuiltInOps::CopyBufferToBuffer,
                                                                                   pCmdQ->getContext(), pCmdQ->getDevice());
    ASSERT_NE(nullptr, &builder);

    BuiltinDispatchInfoBuilder::BuiltinOpParams dc;
    dc.srcMemObj = srcBuffer;
    dc.srcMemObj = dstBuffer;
    dc.srcOffset = {EnqueueCopyBufferTraits::srcOffset, 0, 0};
    dc.dstOffset = {EnqueueCopyBufferTraits::dstOffset, 0, 0};
    dc.size = {EnqueueCopyBufferTraits::size, 0, 0};
    builder.buildDispatchInfos(multiDispatchInfo, dc);
    EXPECT_NE(0u, multiDispatchInfo.size());

    auto usedAfterCS = commandStream.getUsed();
    auto usedAfterDSH = dsh.getUsed();
    auto usedAfterIOH = ioh.getUsed();
    auto usedAfterSSH = ssh.getUsed();

    auto expectedSizeCS = EnqueueOperation<FamilyType>::getTotalSizeRequiredCS(false, false, *pCmdQ, multiDispatchInfo);
    auto expectedSizeDSH = KernelCommandsHelper<FamilyType>::getTotalSizeRequiredDSH(multiDispatchInfo);
    auto expectedSizeIOH = KernelCommandsHelper<FamilyType>::getTotalSizeRequiredIOH(multiDispatchInfo);
    auto expectedSizeSSH = KernelCommandsHelper<FamilyType>::getTotalSizeRequiredSSH(multiDispatchInfo);

    EXPECT_EQ(0u, expectedSizeIOH % GPGPU_WALKER::INDIRECTDATASTARTADDRESS_ALIGN_SIZE);
    EXPECT_EQ(0u, expectedSizeDSH % 64);

    // Since each enqueue* may flush, we may see a MI_BATCH_BUFFER_END appended.
    expectedSizeCS += sizeof(typename FamilyType::MI_BATCH_BUFFER_END);
    expectedSizeCS = alignUp(expectedSizeCS, MemoryConstants::cacheLineSize);

    EXPECT_EQ(expectedSizeCS, usedAfterCS - usedBeforeCS);
    EXPECT_GE(expectedSizeDSH, usedAfterDSH - usedBeforeDSH);
    EXPECT_GE(expectedSizeIOH, usedAfterIOH - usedBeforeIOH);
    EXPECT_GE(expectedSizeSSH, usedAfterSSH - usedBeforeSSH);
}

HWCMDTEST_F(IGFX_GEN8_CORE, GetSizeRequiredBufferTest, enqueueReadBufferNonBlocking) {
    typedef typename FamilyType::GPGPU_WALKER GPGPU_WALKER;
    auto &commandStream = pCmdQ->getCS();
    auto usedBeforeCS = commandStream.getUsed();
    auto &dsh = pCmdQ->getIndirectHeap(IndirectHeap::DYNAMIC_STATE, 0u);
    auto &ioh = pCmdQ->getIndirectHeap(IndirectHeap::INDIRECT_OBJECT, 0u);
    auto &ssh = pCmdQ->getIndirectHeap(IndirectHeap::SURFACE_STATE, 0u);
    auto usedBeforeDSH = dsh.getUsed();
    auto usedBeforeIOH = ioh.getUsed();
    auto usedBeforeSSH = ssh.getUsed();

    EnqueueReadBufferHelper<>::enqueueReadBuffer(
        pCmdQ,
        srcBuffer,
        CL_FALSE);

    MultiDispatchInfo multiDispatchInfo;
    auto &builder = pCmdQ->getDevice().getBuiltIns().getBuiltinDispatchInfoBuilder(EBuiltInOps::CopyBufferToBuffer,
                                                                                   pCmdQ->getContext(), pCmdQ->getDevice());
    ASSERT_NE(nullptr, &builder);

    BuiltinDispatchInfoBuilder::BuiltinOpParams dc;
    dc.dstPtr = EnqueueReadBufferTraits::hostPtr;
    dc.srcMemObj = srcBuffer;
    dc.srcOffset = {EnqueueReadBufferTraits::offset, 0, 0};
    dc.size = {srcBuffer->getSize(), 0, 0};
    builder.buildDispatchInfos(multiDispatchInfo, dc);
    EXPECT_NE(0u, multiDispatchInfo.size());

    auto usedAfterCS = commandStream.getUsed();
    auto usedAfterDSH = dsh.getUsed();
    auto usedAfterIOH = ioh.getUsed();
    auto usedAfterSSH = ssh.getUsed();

    auto expectedSizeCS = EnqueueOperation<FamilyType>::getTotalSizeRequiredCS(false, false, *pCmdQ, multiDispatchInfo);
    auto expectedSizeDSH = KernelCommandsHelper<FamilyType>::getTotalSizeRequiredDSH(multiDispatchInfo);
    auto expectedSizeIOH = KernelCommandsHelper<FamilyType>::getTotalSizeRequiredIOH(multiDispatchInfo);
    auto expectedSizeSSH = KernelCommandsHelper<FamilyType>::getTotalSizeRequiredSSH(multiDispatchInfo);

    EXPECT_EQ(0u, expectedSizeIOH % GPGPU_WALKER::INDIRECTDATASTARTADDRESS_ALIGN_SIZE);
    EXPECT_EQ(0u, expectedSizeDSH % 64);

    // Since each enqueue* may flush, we may see a MI_BATCH_BUFFER_END appended.
    expectedSizeCS += sizeof(typename FamilyType::MI_BATCH_BUFFER_END);
    expectedSizeCS = alignUp(expectedSizeCS, MemoryConstants::cacheLineSize);

    EXPECT_EQ(expectedSizeCS, usedAfterCS - usedBeforeCS);
    EXPECT_GE(expectedSizeDSH, usedAfterDSH - usedBeforeDSH);
    EXPECT_GE(expectedSizeIOH, usedAfterIOH - usedBeforeIOH);
    EXPECT_GE(expectedSizeSSH, usedAfterSSH - usedBeforeSSH);
}

HWCMDTEST_F(IGFX_GEN8_CORE, GetSizeRequiredBufferTest, enqueueReadBufferBlocking) {
    typedef typename FamilyType::GPGPU_WALKER GPGPU_WALKER;
    auto &commandStream = pCmdQ->getCS();
    auto usedBeforeCS = commandStream.getUsed();
    auto &dsh = pCmdQ->getIndirectHeap(IndirectHeap::DYNAMIC_STATE, 0u);
    auto &ioh = pCmdQ->getIndirectHeap(IndirectHeap::INDIRECT_OBJECT, 0u);
    auto &ssh = pCmdQ->getIndirectHeap(IndirectHeap::SURFACE_STATE, 0u);
    auto usedBeforeDSH = dsh.getUsed();
    auto usedBeforeIOH = ioh.getUsed();
    auto usedBeforeSSH = ssh.getUsed();

    srcBuffer->forceDisallowCPUCopy = true;
    EnqueueReadBufferHelper<>::enqueueReadBuffer(
        pCmdQ,
        srcBuffer,
        CL_TRUE);

    MultiDispatchInfo multiDispatchInfo;
    auto &builder = pCmdQ->getDevice().getBuiltIns().getBuiltinDispatchInfoBuilder(EBuiltInOps::CopyBufferToBuffer,
                                                                                   pCmdQ->getContext(), pCmdQ->getDevice());
    ASSERT_NE(nullptr, &builder);

    BuiltinDispatchInfoBuilder::BuiltinOpParams dc;
    dc.dstPtr = EnqueueReadBufferTraits::hostPtr;
    dc.srcMemObj = srcBuffer;
    dc.srcOffset = {EnqueueReadBufferTraits::offset, 0, 0};
    dc.size = {srcBuffer->getSize(), 0, 0};
    builder.buildDispatchInfos(multiDispatchInfo, dc);
    EXPECT_NE(0u, multiDispatchInfo.size());

    auto usedAfterCS = commandStream.getUsed();
    auto usedAfterDSH = dsh.getUsed();
    auto usedAfterIOH = ioh.getUsed();
    auto usedAfterSSH = ssh.getUsed();

    auto expectedSizeCS = EnqueueOperation<FamilyType>::getTotalSizeRequiredCS(false, false, *pCmdQ, multiDispatchInfo);
    auto expectedSizeDSH = KernelCommandsHelper<FamilyType>::getTotalSizeRequiredDSH(multiDispatchInfo);
    auto expectedSizeIOH = KernelCommandsHelper<FamilyType>::getTotalSizeRequiredIOH(multiDispatchInfo);
    auto expectedSizeSSH = KernelCommandsHelper<FamilyType>::getTotalSizeRequiredSSH(multiDispatchInfo);

    EXPECT_EQ(0u, expectedSizeIOH % GPGPU_WALKER::INDIRECTDATASTARTADDRESS_ALIGN_SIZE);
    EXPECT_EQ(0u, expectedSizeDSH % 64);

    // Since each enqueue* may flush, we may see a MI_BATCH_BUFFER_END appended.
    expectedSizeCS += sizeof(typename FamilyType::MI_BATCH_BUFFER_END);
    expectedSizeCS = alignUp(expectedSizeCS, MemoryConstants::cacheLineSize);

    EXPECT_EQ(expectedSizeCS, usedAfterCS - usedBeforeCS);
    EXPECT_GE(expectedSizeDSH, usedAfterDSH - usedBeforeDSH);
    EXPECT_GE(expectedSizeIOH, usedAfterIOH - usedBeforeIOH);
    EXPECT_GE(expectedSizeSSH, usedAfterSSH - usedBeforeSSH);
}

HWCMDTEST_F(IGFX_GEN8_CORE, GetSizeRequiredBufferTest, enqueueWriteBufferNonBlocking) {
    typedef typename FamilyType::GPGPU_WALKER GPGPU_WALKER;
    auto &commandStream = pCmdQ->getCS();
    auto usedBeforeCS = commandStream.getUsed();
    auto &dsh = pCmdQ->getIndirectHeap(IndirectHeap::DYNAMIC_STATE, 0u);
    auto &ioh = pCmdQ->getIndirectHeap(IndirectHeap::INDIRECT_OBJECT, 0u);
    auto &ssh = pCmdQ->getIndirectHeap(IndirectHeap::SURFACE_STATE, 0u);
    auto usedBeforeDSH = dsh.getUsed();
    auto usedBeforeIOH = ioh.getUsed();
    auto usedBeforeSSH = ssh.getUsed();

    auto retVal = EnqueueWriteBufferHelper<>::enqueueWriteBuffer(
        pCmdQ,
        dstBuffer,
        CL_FALSE);
    EXPECT_EQ(CL_SUCCESS, retVal);

    MultiDispatchInfo multiDispatchInfo;
    auto &builder = pCmdQ->getDevice().getBuiltIns().getBuiltinDispatchInfoBuilder(EBuiltInOps::CopyBufferToBuffer,
                                                                                   pCmdQ->getContext(), pCmdQ->getDevice());
    ASSERT_NE(nullptr, &builder);

    BuiltinDispatchInfoBuilder::BuiltinOpParams dc;
    dc.srcPtr = EnqueueWriteBufferTraits::hostPtr;
    dc.dstMemObj = dstBuffer;
    dc.dstOffset = {EnqueueWriteBufferTraits::offset, 0, 0};
    dc.size = {dstBuffer->getSize(), 0, 0};
    builder.buildDispatchInfos(multiDispatchInfo, dc);
    EXPECT_NE(0u, multiDispatchInfo.size());

    auto usedAfterCS = commandStream.getUsed();
    auto usedAfterDSH = dsh.getUsed();
    auto usedAfterIOH = ioh.getUsed();
    auto usedAfterSSH = ssh.getUsed();

    auto expectedSizeCS = EnqueueOperation<FamilyType>::getTotalSizeRequiredCS(false, false, *pCmdQ, multiDispatchInfo);
    auto expectedSizeDSH = KernelCommandsHelper<FamilyType>::getTotalSizeRequiredDSH(multiDispatchInfo);
    auto expectedSizeIOH = KernelCommandsHelper<FamilyType>::getTotalSizeRequiredIOH(multiDispatchInfo);
    auto expectedSizeSSH = KernelCommandsHelper<FamilyType>::getTotalSizeRequiredSSH(multiDispatchInfo);

    // Since each enqueue* may flush, we may see a MI_BATCH_BUFFER_END appended.
    expectedSizeCS += sizeof(typename FamilyType::MI_BATCH_BUFFER_END);
    expectedSizeCS = alignUp(expectedSizeCS, MemoryConstants::cacheLineSize);

    EXPECT_EQ(expectedSizeCS, usedAfterCS - usedBeforeCS);
    EXPECT_GE(expectedSizeDSH, usedAfterDSH - usedBeforeDSH);
    EXPECT_GE(expectedSizeIOH, usedAfterIOH - usedBeforeIOH);
    EXPECT_GE(expectedSizeSSH, usedAfterSSH - usedBeforeSSH);
}

HWTEST_F(GetSizeRequiredBufferTest, enqueueWriteBufferBlocking) {
    auto &commandStream = pCmdQ->getCS();
    auto usedBeforeCS = commandStream.getUsed();
    auto &dsh = pCmdQ->getIndirectHeap(IndirectHeap::DYNAMIC_STATE, 0u);
    auto &ioh = pCmdQ->getIndirectHeap(IndirectHeap::INDIRECT_OBJECT, 0u);
    auto &ssh = pCmdQ->getIndirectHeap(IndirectHeap::SURFACE_STATE, 0u);
    auto usedBeforeDSH = dsh.getUsed();
    auto usedBeforeIOH = ioh.getUsed();
    auto usedBeforeSSH = ssh.getUsed();

    dstBuffer->forceDisallowCPUCopy = true;
    auto retVal = EnqueueWriteBufferHelper<>::enqueueWriteBuffer(
        pCmdQ,
        dstBuffer,
        CL_TRUE);
    EXPECT_EQ(CL_SUCCESS, retVal);

    MultiDispatchInfo multiDispatchInfo;
    auto &builder = pCmdQ->getDevice().getBuiltIns().getBuiltinDispatchInfoBuilder(EBuiltInOps::CopyBufferToBuffer,
                                                                                   pCmdQ->getContext(), pCmdQ->getDevice());
    ASSERT_NE(nullptr, &builder);

    BuiltinDispatchInfoBuilder::BuiltinOpParams dc;
    dc.srcPtr = EnqueueWriteBufferTraits::hostPtr;
    dc.dstMemObj = dstBuffer;
    dc.dstOffset = {EnqueueWriteBufferTraits::offset, 0, 0};
    dc.size = {dstBuffer->getSize(), 0, 0};
    builder.buildDispatchInfos(multiDispatchInfo, dc);
    EXPECT_NE(0u, multiDispatchInfo.size());

    auto usedAfterCS = commandStream.getUsed();
    auto usedAfterDSH = dsh.getUsed();
    auto usedAfterIOH = ioh.getUsed();
    auto usedAfterSSH = ssh.getUsed();

    auto expectedSizeCS = EnqueueOperation<FamilyType>::getTotalSizeRequiredCS(false, false, *pCmdQ, multiDispatchInfo);
    auto expectedSizeDSH = KernelCommandsHelper<FamilyType>::getTotalSizeRequiredDSH(multiDispatchInfo);
    auto expectedSizeIOH = KernelCommandsHelper<FamilyType>::getTotalSizeRequiredIOH(multiDispatchInfo);
    auto expectedSizeSSH = KernelCommandsHelper<FamilyType>::getTotalSizeRequiredSSH(multiDispatchInfo);

    // Since each enqueue* may flush, we may see a MI_BATCH_BUFFER_END appended.
    expectedSizeCS += sizeof(typename FamilyType::MI_BATCH_BUFFER_END);
    expectedSizeCS = alignUp(expectedSizeCS, MemoryConstants::cacheLineSize);

    EXPECT_EQ(expectedSizeCS, usedAfterCS - usedBeforeCS);
    EXPECT_GE(expectedSizeDSH, usedAfterDSH - usedBeforeDSH);
    EXPECT_GE(expectedSizeIOH, usedAfterIOH - usedBeforeIOH);
    EXPECT_GE(expectedSizeSSH, usedAfterSSH - usedBeforeSSH);
}

HWTEST_F(GetSizeRequiredBufferTest, enqueueKernelHelloWorld) {
    typedef HelloWorldKernelFixture KernelFixture;
    auto &commandStream = pCmdQ->getCS();
    auto usedBeforeCS = commandStream.getUsed();
    auto dshBefore = pDSH->getUsed();
    auto iohBefore = pIOH->getUsed();
    auto sshBefore = pSSH->getUsed();

    size_t workSize[] = {256};
    auto retVal = EnqueueKernelHelper<>::enqueueKernel(
        pCmdQ,
        KernelFixture::pKernel,
        1,
        nullptr,
        workSize,
        workSize);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto usedAfterCS = commandStream.getUsed();
    auto dshAfter = pDSH->getUsed();
    auto iohAfter = pIOH->getUsed();
    auto sshAfter = pSSH->getUsed();

    auto expectedSizeCS = EnqueueOperation<FamilyType>::getSizeRequiredCS(CL_COMMAND_NDRANGE_KERNEL, false, false, *pCmdQ, nullptr);
    auto expectedSizeDSH = KernelCommandsHelper<FamilyType>::getSizeRequiredDSH(*KernelFixture::pKernel);
    auto expectedSizeIOH = KernelCommandsHelper<FamilyType>::getSizeRequiredIOH(*KernelFixture::pKernel, workSize[0]);
    auto expectedSizeSSH = KernelCommandsHelper<FamilyType>::getSizeRequiredSSH(*KernelFixture::pKernel);

    // Since each enqueue* may flush, we may see a MI_BATCH_BUFFER_END appended.
    expectedSizeCS += sizeof(typename FamilyType::MI_BATCH_BUFFER_END);
    expectedSizeCS = alignUp(expectedSizeCS, MemoryConstants::cacheLineSize);

    EXPECT_EQ(expectedSizeCS, usedAfterCS - usedBeforeCS);
    EXPECT_GE(expectedSizeDSH, dshAfter - dshBefore);
    EXPECT_GE(expectedSizeIOH, iohAfter - iohBefore);
    EXPECT_GE(expectedSizeSSH, sshAfter - sshBefore);
}

HWCMDTEST_F(IGFX_GEN8_CORE, GetSizeRequiredBufferTest, enqueueKernelSimpleArg) {
    typedef typename FamilyType::GPGPU_WALKER GPGPU_WALKER;
    typedef SimpleArgKernelFixture KernelFixture;
    auto &commandStream = pCmdQ->getCS();
    auto usedBeforeCS = commandStream.getUsed();
    auto dshBefore = pDSH->getUsed();
    auto iohBefore = pIOH->getUsed();
    auto sshBefore = pSSH->getUsed();

    size_t workSize[] = {256};
    auto retVal = EnqueueKernelHelper<>::enqueueKernel(
        pCmdQ,
        KernelFixture::pKernel,
        1,
        nullptr,
        workSize,
        workSize);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto usedAfterCS = commandStream.getUsed();
    auto dshAfter = pDSH->getUsed();
    auto iohAfter = pIOH->getUsed();
    auto sshAfter = pSSH->getUsed();

    auto expectedSizeCS = EnqueueOperation<FamilyType>::getSizeRequiredCS(CL_COMMAND_NDRANGE_KERNEL, false, false, *pCmdQ, nullptr);
    auto expectedSizeDSH = KernelCommandsHelper<FamilyType>::getSizeRequiredDSH(*KernelFixture::pKernel);
    auto expectedSizeIOH = KernelCommandsHelper<FamilyType>::getSizeRequiredIOH(*KernelFixture::pKernel, workSize[0]);
    auto expectedSizeSSH = KernelCommandsHelper<FamilyType>::getSizeRequiredSSH(*KernelFixture::pKernel);

    EXPECT_EQ(0u, expectedSizeIOH % GPGPU_WALKER::INDIRECTDATASTARTADDRESS_ALIGN_SIZE);
    EXPECT_EQ(0u, expectedSizeDSH % 64);

    // Since each enqueue* may flush, we may see a MI_BATCH_BUFFER_END appended.
    expectedSizeCS += sizeof(typename FamilyType::MI_BATCH_BUFFER_END);
    expectedSizeCS = alignUp(expectedSizeCS, MemoryConstants::cacheLineSize);

    EXPECT_EQ(expectedSizeCS, usedAfterCS - usedBeforeCS);
    EXPECT_GE(expectedSizeDSH, dshAfter - dshBefore);
    EXPECT_GE(expectedSizeIOH, iohAfter - iohBefore);
    EXPECT_GE(expectedSizeSSH, sshAfter - sshBefore);
}
