/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/fixtures/device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "test.h"

#include "gtest/gtest.h"

class SchedulerSourceTest : public testing::Test {
  public:
    void SetUp() override {
        pDevice = new MockClDevice{NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(nullptr)};
    }
    void TearDown() override {
        delete pDevice;
    }

    NEO::MockClDevice *pDevice;
    NEO::MockContext context;

    template <typename GfxFamily>
    void givenDeviceQueueThenNumberOfEnqueuesEqualsNumberOfEnqueuesInSchedulerKernelCodeTest();
    template <typename GfxFamily>
    void givenDeviceQueueWhenCommandsSizeIsCalculatedThenItEqualsSpaceForEachEnqueueInSchedulerKernelCodeTest();
    template <typename GfxFamily>
    void givenDeviceQueueWhenSlbDummyCommandsAreBuildThenSizeUsedIsCorrectTest();
};
