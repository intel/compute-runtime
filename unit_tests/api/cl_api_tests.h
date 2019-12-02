/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/api/api.h"
#include "runtime/tracing/tracing_api.h"
#include "test.h"
#include "unit_tests/fixtures/platform_fixture.h"
#include "unit_tests/helpers/variable_backup.h"

#include "gtest/gtest.h"

#include <memory>

namespace NEO {

class CommandQueue;
class Context;
class MockKernel;
class MockProgram;
class MockAlignedMallocManagerDevice;
struct RootDeviceEnvironment;
extern size_t numPlatformDevices;

struct ApiFixture : public PlatformFixture {
    virtual void SetUp();
    virtual void TearDown();

    cl_int retVal = CL_SUCCESS;
    size_t retSize = 0;

    CommandQueue *pCommandQueue = nullptr;
    Context *pContext = nullptr;
    MockKernel *pKernel = nullptr;
    MockProgram *pProgram = nullptr;
    constexpr static size_t numRootDevices = 1u;
    VariableBackup<size_t> numDevicesBackup{&numPlatformDevices};
    const uint32_t testedRootDeviceIndex = 0u;
    cl_device_id testedClDevice = nullptr;
};

struct api_tests : public ApiFixture,
                   public ::testing::Test {
    virtual void SetUp() override {
        ApiFixture::SetUp();
    }
    virtual void TearDown() override {
        ApiFixture::TearDown();
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

} // namespace NEO
