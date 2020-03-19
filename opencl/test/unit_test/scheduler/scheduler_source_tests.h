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

namespace NEO {

class SchedulerSourceTest : public testing::Test {
  public:
    void SetUp() override {
        pDevice = new MockClDevice{MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr)};
    }
    void TearDown() override {
        delete pDevice;
    }

    MockClDevice *pDevice;
    MockContext context;

    template <typename GfxFamily>
    void givenDeviceQueueThenNumberOfEnqueuesEqualsNumberOfEnqueuesInSchedulerKernelCodeTest();
    template <typename GfxFamily>
    void givenDeviceQueueWhenCommandsSizeIsCalculatedThenItEqualsSpaceForEachEnqueueInSchedulerKernelCodeTest();
    template <typename GfxFamily>
    void givenDeviceQueueWhenSlbDummyCommandsAreBuildThenSizeUsedIsCorrectTest();
};

} // namespace NEO
