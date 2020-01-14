/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "unit_tests/aub_tests/command_queue/command_enqueue_fixture.h"
#include "unit_tests/fixtures/hello_world_kernel_fixture.h"

namespace NEO {
static const char programFile[] = "simple_block_kernel";
static const char kernelName[] = "kernel_reflection";

class AUBParentKernelFixture : public CommandEnqueueAUBFixture,
                               public HelloWorldKernelFixture,
                               public testing::Test {
  public:
    using HelloWorldKernelFixture::SetUp;

    void SetUp() {
        if (platformDevices[0]->capabilityTable.clVersionSupport < 20) {
            GTEST_SKIP();
        }
        CommandEnqueueAUBFixture::SetUp();
        ASSERT_NE(nullptr, pClDevice);
        HelloWorldKernelFixture::SetUp(pClDevice, programFile, kernelName, "-cl-std=CL2.0");
    }
    void TearDown() {
        if (IsSkipped()) {
            return;
        }
        HelloWorldKernelFixture::TearDown();
        CommandEnqueueAUBFixture::TearDown();
    }
};
} // namespace NEO
