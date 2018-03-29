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
#include "unit_tests/fixtures/platform_fixture.h"
#include "unit_tests/fixtures/built_in_fixture.h"
#include "runtime/api/api.h"
#include "test.h"

#include <memory>

namespace OCLRT {

class CommandQueue;
class Context;
class MockKernel;
class MockProgram;
class MockAlignedMallocManagerDevice;

struct api_fixture : public PlatformFixture,
                     public BuiltInFixture {
    using BuiltInFixture::SetUp;
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

struct api_fixture_using_aligned_memory_manager : public BuiltInFixture {
    using BuiltInFixture::SetUp;

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
