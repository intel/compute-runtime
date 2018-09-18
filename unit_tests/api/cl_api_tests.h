/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "gtest/gtest.h"
#include "unit_tests/fixtures/platform_fixture.h"
#include "runtime/api/api.h"
#include "test.h"

#include <memory>

namespace OCLRT {

class CommandQueue;
class Context;
class MockKernel;
class MockProgram;
class MockAlignedMallocManagerDevice;

struct api_fixture : public PlatformFixture {
    using PlatformFixture::SetUp;

  public:
    api_fixture();

  protected:
    virtual void SetUp();
    virtual void TearDown();

    cl_int retVal;
    size_t retSize;

    CommandQueue *pCommandQueue;
    Context *pContext;
    MockKernel *pKernel;
    MockProgram *pProgram;
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

struct api_fixture_using_aligned_memory_manager {
  public:
    virtual void SetUp();
    virtual void TearDown();

    cl_int retVal;
    size_t retSize;

    CommandQueue *commandQueue;
    Context *context;
    MockKernel *kernel;
    MockProgram *program;
    MockAlignedMallocManagerDevice *device;
};

using api_test_using_aligned_memory_manager = Test<api_fixture_using_aligned_memory_manager>;

} // namespace OCLRT
