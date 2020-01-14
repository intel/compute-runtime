/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/mocks/mock_context.h"

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
