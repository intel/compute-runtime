/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/test/unit_test/cmd_parse/hw_parse.h"

#include "opencl/test/unit_test/command_queue/enqueue_fixture.h"

#include "hello_world_fixture.h"

namespace NEO {

// Generates two back-to-back walkers using the same kernel for testing purposes
template <typename FactoryType>
struct TwoWalkerTest
    : public HelloWorldTest<FactoryType>,
      public HardwareParse {
    typedef HelloWorldTest<FactoryType> Parent;

    using Parent::pCmdBuffer;
    using Parent::pCmdQ;
    using Parent::pCS;
    using Parent::pKernel;

    template <typename FamilyType>
    void enqueueTwoKernels() {
        auto retVal = EnqueueKernelHelper<>::enqueueKernel(
            pCmdQ,
            pKernel);
        ASSERT_EQ(CL_SUCCESS, retVal);

        // We have to parse after each enqueue* because
        // the CSR CS may insert commands in between
        parseCommands<FamilyType>(*pCmdQ);

        retVal = EnqueueKernelHelper<>::enqueueKernel(
            pCmdQ,
            pKernel);
        ASSERT_EQ(CL_SUCCESS, retVal);

        parseCommands<FamilyType>(*pCmdQ);

        itorWalker1 = find<typename FamilyType::WALKER_TYPE *>(cmdList.begin(), cmdList.end());
        ASSERT_NE(cmdList.end(), itorWalker1);

        itorWalker2 = itorWalker1;
        ++itorWalker2;
        itorWalker2 = find<typename FamilyType::WALKER_TYPE *>(itorWalker2, cmdList.end());
        ASSERT_NE(cmdList.end(), itorWalker2);
    }

    void SetUp() override {
        Parent::SetUp();
        HardwareParse::SetUp();
    }

    void TearDown() override {
        HardwareParse::TearDown();
        Parent::TearDown();
    }

    GenCmdList::iterator itorWalker1;
    GenCmdList::iterator itorWalker2;
};
} // namespace NEO
