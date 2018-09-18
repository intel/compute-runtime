/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/mocks/mock_context.h"
#include "gtest/gtest.h"
#include "test.h"

class SchedulerSourceTest : public testing::Test {
  public:
    void SetUp() override {
        pDevice = OCLRT::MockDevice::createWithNewExecutionEnvironment<OCLRT::MockDevice>(nullptr);
    }
    void TearDown() override {
        delete pDevice;
    }

    OCLRT::Device *pDevice;
    OCLRT::MockContext context;

    template <typename GfxFamily>
    void givenDeviceQueueThenNumberOfEnqueuesEqualsNumberOfEnqueuesInSchedulerKernelCodeTest();
    template <typename GfxFamily>
    void givenDeviceQueueWhenCommandsSizeIsCalculatedThenItEqualsSpaceForEachEnqueueInSchedulerKernelCodeTest();
    template <typename GfxFamily>
    void givenDeviceQueueWhenSlbDummyCommandsAreBuildThenSizeUsedIsCorrectTest();
};
