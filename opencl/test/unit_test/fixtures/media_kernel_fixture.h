/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/test/common/helpers/default_hw_info.h"

#include "opencl/test/unit_test/command_queue/enqueue_fixture.h"
#include "opencl/test/unit_test/fixtures/hello_world_fixture.h"
#include "opencl/test/unit_test/helpers/cl_hw_parse.h"

namespace NEO {

template <typename FactoryType>
struct MediaKernelFixture : public HelloWorldFixture<FactoryType>,
                            public ClHardwareParse,
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
        skipVmeTest = !defaultHwInfo->capabilityTable.supportsVme;
        if (skipVmeTest) {
            GTEST_SKIP();
        }
        Parent::kernelFilename = "vme_kernels";
        Parent::kernelName = "non_vme_kernel";

        Parent::setUp();
        ClHardwareParse::setUp();

        ASSERT_NE(nullptr, pKernel);
        ASSERT_EQ(false, pKernel->isVmeKernel());

        cl_int retVal;

        // create the VME kernel
        pMultiDeviceVmeKernel = MultiDeviceKernel::create<MockKernel>(
            pProgram,
            pProgram->getKernelInfosForKernel("device_side_block_motion_estimate_intel"),
            &retVal);

        pVmeKernel = pMultiDeviceVmeKernel->getKernel(pDevice->getRootDeviceIndex());
        ASSERT_NE(nullptr, pVmeKernel);
        ASSERT_EQ(true, pVmeKernel->isVmeKernel());
    }

    void TearDown() override {
        if (skipVmeTest) {
            return;
        }
        pMultiDeviceVmeKernel->release();

        ClHardwareParse::tearDown();
        Parent::tearDown();
    }

    GenCmdList::iterator itorWalker1;
    GenCmdList::iterator itorWalker2;

    MultiDeviceKernel *pMultiDeviceVmeKernel = nullptr;
    Kernel *pVmeKernel = nullptr;
    bool skipVmeTest = false;
};
} // namespace NEO
