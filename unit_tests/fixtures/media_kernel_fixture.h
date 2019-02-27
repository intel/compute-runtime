/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "unit_tests/command_queue/enqueue_fixture.h"
#include "unit_tests/fixtures/hello_world_fixture.h"
#include "unit_tests/helpers/hw_parse.h"

namespace OCLRT {

template <typename FactoryType>
struct MediaKernelFixture : public HelloWorldFixture<FactoryType>,
                            public HardwareParse,
                            public ::testing::Test {
    typedef HelloWorldFixture<FactoryType> Parent;

    using Parent::pCmdBuffer;
    using Parent::pCmdQ;
    using Parent::pContext;
    using Parent::pCS;
    using Parent::pDevice;
    using Parent::pKernel;
    using Parent::pProgram;
    using Parent::retVal;

    MediaKernelFixture() {}

    template <typename FamilyType>
    void enqueueRegularKernel() {
        auto retVal = EnqueueKernelHelper<>::enqueueKernel(
            pCmdQ,
            pKernel);
        ASSERT_EQ(CL_SUCCESS, retVal);

        parseCommands<FamilyType>(*pCmdQ);

        itorWalker1 = find<typename FamilyType::WALKER_TYPE *>(cmdList.begin(), cmdList.end());
        ASSERT_NE(cmdList.end(), itorWalker1);
    }

    template <typename FamilyType>
    void enqueueVmeKernel() {
        auto retVal = EnqueueKernelHelper<>::enqueueKernel(
            pCmdQ,
            pVmeKernel);
        ASSERT_EQ(CL_SUCCESS, retVal);

        parseCommands<FamilyType>(*pCmdQ);

        itorWalker1 = find<typename FamilyType::WALKER_TYPE *>(cmdList.begin(), cmdList.end());
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
        pVmeKernel->release();

        HardwareParse::TearDown();
        Parent::TearDown();
    }

    GenCmdList::iterator itorWalker1;
    GenCmdList::iterator itorWalker2;

    Kernel *pVmeKernel = nullptr;
};
} // namespace OCLRT
