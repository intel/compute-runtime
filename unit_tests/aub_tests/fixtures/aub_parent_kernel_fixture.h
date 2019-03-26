/*
 * Copyright (C) 2017-2019 Intel Corporation
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
        CommandEnqueueAUBFixture::SetUp();
        ASSERT_NE(nullptr, pDevice);

        std::string options("");
        if (pDevice->getSupportedClVersion() >= 20) {
            options = "-cl-std=CL2.0";
        } else {
            return;
        }
        HelloWorldKernelFixture::SetUp(pDevice, programFile, kernelName, options.c_str());
    }
    void TearDown() {
        if (pDevice->getSupportedClVersion() >= 20) {
            HelloWorldKernelFixture::TearDown();
        }
        CommandEnqueueAUBFixture::TearDown();
    }
};
} // namespace NEO
