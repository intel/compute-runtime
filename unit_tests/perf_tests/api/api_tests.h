/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/command_queue/command_queue.h"
#include "runtime/context/context.h"
#include "runtime/program/program.h"
#include "unit_tests/perf_tests/fixtures/command_queue_fixture.h"
#include "unit_tests/perf_tests/fixtures/device_fixture.h"
#include "unit_tests/perf_tests/fixtures/platform_fixture.h"
#include "unit_tests/perf_tests/perf_test_utils.h"

#include "gtest/gtest.h"

namespace OCLRT {

struct api_fixture : public PlatformFixture,
                     public CommandQueueHwFixture,
                     public DeviceFixture {
  public:
    api_fixture(void);

  protected:
    virtual void SetUp();
    virtual void TearDown();

    cl_int retVal;
    size_t retSize;

    CommandQueue *pCommandQueue;
    Context *pContext;
    Kernel *pKernel;
    Program *pProgram;
};

struct api_tests : public api_fixture,
                   public ::testing::Test {
    virtual void SetUp() override {
        api_fixture::SetUp();
    }
    virtual void TearDown() override {
        api_fixture::TearDown();
    }
};
} // namespace OCLRT
