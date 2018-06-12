/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#pragma once
#include "gtest/gtest.h"
#include "runtime/command_queue/command_queue.h"
#include "runtime/context/context.h"
#include "runtime/program/program.h"
#include "unit_tests/perf_tests/fixtures/device_fixture.h"
#include "unit_tests/perf_tests/fixtures/platform_fixture.h"
#include "unit_tests/perf_tests/fixtures/command_queue_fixture.h"
#include "unit_tests/perf_tests/perf_test_utils.h"

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
