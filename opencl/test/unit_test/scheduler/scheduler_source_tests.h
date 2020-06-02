/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/test_macros/test_checks_ocl.h"
#include "test.h"

#include "gtest/gtest.h"

namespace NEO {

class SchedulerSourceTest : public testing::Test {
  public:
    void SetUp() override {
        pDevice = new MockClDevice{MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr)};
        REQUIRE_DEVICE_ENQUEUE_OR_SKIP(pDevice);
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
