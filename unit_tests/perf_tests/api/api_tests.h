/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "unit_tests/perf_tests/fixtures/command_queue_fixture.h"
#include "unit_tests/perf_tests/fixtures/device_fixture.h"
#include "unit_tests/perf_tests/fixtures/platform_fixture.h"
#include "unit_tests/perf_tests/perf_test_utils.h"

#include "command_queue/command_queue.h"
#include "context/context.h"
#include "gtest/gtest.h"
#include "program/program.h"

namespace NEO {

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
} // namespace NEO
