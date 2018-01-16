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

#include "unit_tests/fixtures/hello_world_fixture.h"
#include "unit_tests/command_queue/enqueue_fixture.h"
#include "unit_tests/helpers/hw_parse.h"
#include "runtime/helpers/preamble.h"

using namespace OCLRT;

template <typename FactoryType>
struct MediaKernelCommandQueueTestFactory : public HelloWorldFixture<FactoryType>,
                                            public HardwareParse,
                                            public ::testing::Test {
    typedef HelloWorldFixture<FactoryType> Parent;

    using Parent::pCmdQ;
    using Parent::pCS;
    using Parent::pKernel;
    using Parent::pCmdBuffer;
    using Parent::pContext;
    using Parent::pDevice;
    using Parent::pProgram;
    using Parent::retVal;

    MediaKernelCommandQueueTestFactory() {}

    template <typename FamilyType>
    void enqueueVmeThenRegularKernel() {
        auto retVal = EnqueueKernelHelper<>::enqueueKernel(
            pCmdQ,
            pVmeKernel);
        ASSERT_EQ(CL_SUCCESS, retVal);

        // We have to parse after each enqueue* because
        // the CSR CS may insert commands in between
        parseCommands<FamilyType>(*pCmdQ);

        retVal = EnqueueKernelHelper<>::enqueueKernel(
            pCmdQ,
            pKernel);
        ASSERT_EQ(CL_SUCCESS, retVal);

        parseCommands<FamilyType>(*pCmdQ);

        itorWalker1 = find<typename FamilyType::GPGPU_WALKER *>(cmdList.begin(), cmdList.end());
        ASSERT_NE(cmdList.end(), itorWalker1);

        itorWalker2 = itorWalker1;
        ++itorWalker2;
        itorWalker2 = find<typename FamilyType::GPGPU_WALKER *>(itorWalker2, cmdList.end());
        ASSERT_NE(cmdList.end(), itorWalker2);
    }

    template <typename FamilyType>
    void enqueueVmeKernel() {
        auto retVal = EnqueueKernelHelper<>::enqueueKernel(
            pCmdQ,
            pVmeKernel);
        ASSERT_EQ(CL_SUCCESS, retVal);

        parseCommands<FamilyType>(*pCmdQ);

        itorWalker1 = find<typename FamilyType::GPGPU_WALKER *>(cmdList.begin(), cmdList.end());
        ASSERT_NE(cmdList.end(), itorWalker1);
    }

    void SetUp() override {
        Parent::kernelFilename = "vme_kernels";
        Parent::kernelName = "non_vme_kernel";

        Parent::SetUp();
        HardwareParse::SetUp();

        ASSERT_NE(nullptr, pKernel);
        ASSERT_EQ(false, pKernel->isVmeKernel());

        cl_int retVal;

        // create the VME kernel
        pVmeKernel = Kernel::create<MockKernel>(
            pProgram,
            *pProgram->getKernelInfo("device_side_block_motion_estimate_intel"),
            &retVal);

        ASSERT_NE(nullptr, pVmeKernel);
        ASSERT_EQ(true, pVmeKernel->isVmeKernel());
    }

    void TearDown() override {
        delete pVmeKernel;
        pVmeKernel = nullptr;

        HardwareParse::TearDown();
        Parent::TearDown();
    }

    GenCmdList::iterator itorWalker1;
    GenCmdList::iterator itorWalker2;

    Kernel *pVmeKernel = nullptr;
};

typedef MediaKernelCommandQueueTestFactory<HelloWorldFixtureFactory> MediaKernelCommandQueueTest;

TEST_F(MediaKernelCommandQueueTest, VmeKernelProperlyIdentifiesItself) {
    ASSERT_NE(true, pKernel->isVmeKernel());
    ASSERT_EQ(true, pVmeKernel->isVmeKernel());
}

// Test for the "enabling" pipeline select preceeding the VME kernel
HWTEST_F(MediaKernelCommandQueueTest,
         HavePipelineSelectToDisableMediaSamplerDopClockGate) {
    typedef typename FamilyType::PIPELINE_SELECT PIPELINE_SELECT;

    if (::renderCoreFamily == IGFX_GEN8_CORE) {
        return;
    }

    enqueueVmeKernel<FamilyType>();

    auto itorCmd1 = find<typename FamilyType::PIPELINE_SELECT *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itorCmd1);

    auto pPreceedingPipelineSelect = genCmdCast<PIPELINE_SELECT *>(*itorCmd1);

    bool samplerClockGateEnable = PreambleHelper<FamilyType>::getMediaSamplerDopClockGateEnable(
        reinterpret_cast<LinearStream *>(pPreceedingPipelineSelect));

    EXPECT_EQ(PreambleHelper<FamilyType>::getPipelineSelectMaskBits(), pPreceedingPipelineSelect->getMaskBits());
    EXPECT_EQ(false, samplerClockGateEnable);
    EXPECT_EQ(0u, pPreceedingPipelineSelect->getPipelineSelection());
}

// Test for the "restoring" pipeline select following the VME kernel (that
// disables the media sampler)
HWTEST_F(MediaKernelCommandQueueTest,
         HavePipelineSelectToEnableMediaSamplerDopClockGate) {
    typedef typename FamilyType::PIPELINE_SELECT PIPELINE_SELECT;

    if (::renderCoreFamily == IGFX_GEN8_CORE) {
        return;
    }

    enqueueVmeThenRegularKernel<FamilyType>();

    auto itorCmd1 = find<typename FamilyType::PIPELINE_SELECT *>(
        cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itorCmd1);

    auto itorCmd2 = itorCmd1;
    ++itorCmd2;

    itorCmd2 = find<typename FamilyType::PIPELINE_SELECT *>(
        itorCmd2, cmdList.end());
    EXPECT_NE(cmdList.end(), itorCmd2);

    auto itorCmd3 = itorCmd2;
    ++itorCmd3;

    itorCmd3 = find<typename FamilyType::PIPELINE_SELECT *>(
        itorCmd3, cmdList.end());
    EXPECT_NE(cmdList.end(), itorCmd3);

    auto pFollowingPipelineSelect = genCmdCast<PIPELINE_SELECT *>(*itorCmd3);

    bool samplerClockGateEnable = PreambleHelper<FamilyType>::getMediaSamplerDopClockGateEnable(
        reinterpret_cast<LinearStream *>(pFollowingPipelineSelect));

    EXPECT_EQ(PreambleHelper<FamilyType>::getPipelineSelectMaskBits(), pFollowingPipelineSelect->getMaskBits());
    EXPECT_EQ(true, samplerClockGateEnable);
    EXPECT_EQ(0u, pFollowingPipelineSelect->getPipelineSelection());
}
