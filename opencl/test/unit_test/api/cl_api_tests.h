/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/test/unit_test/helpers/debug_manager_state_restore.h"

#include "opencl/source/api/api.h"
#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/tracing/tracing_api.h"
#include "opencl/test/unit_test/fixtures/platform_fixture.h"
#include "opencl/test/unit_test/helpers/ult_limits.h"
#include "opencl/test/unit_test/helpers/variable_backup.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "test.h"

#include "gtest/gtest.h"

#include <memory>

namespace NEO {

class Context;
class MockClDevice;
struct RootDeviceEnvironment;

template <uint32_t rootDeviceIndex = 1u>
struct ApiFixture : PlatformFixture {
    ApiFixture() = default;
    ~ApiFixture() = default;

    virtual void SetUp() {
        DebugManager.flags.CreateMultipleRootDevices.set(numRootDevices);
        PlatformFixture::SetUp();

        if (rootDeviceIndex != 0u) {
            rootDeviceEnvironmentBackup.swap(pPlatform->peekExecutionEnvironment()->rootDeviceEnvironments[0]);
        }

        auto pDevice = pPlatform->getClDevice(testedRootDeviceIndex);
        ASSERT_NE(nullptr, pDevice);

        testedClDevice = pDevice;
        pContext = Context::create<MockContext>(nullptr, ClDeviceVector(&testedClDevice, 1), nullptr, nullptr, retVal);
        EXPECT_EQ(retVal, CL_SUCCESS);

        pCommandQueue = new MockCommandQueue(pContext, pDevice, nullptr);

        pProgram = new MockProgram(*pDevice->getExecutionEnvironment(), pContext, false, &pDevice->getDevice());

        pKernel = new MockKernel(pProgram, pProgram->mockKernelInfo, *pDevice);
        ASSERT_NE(nullptr, pKernel);
    }

    virtual void TearDown() {
        pKernel->release();
        pCommandQueue->release();
        pContext->release();
        pProgram->release();

        if (rootDeviceIndex != 0u) {
            rootDeviceEnvironmentBackup.swap(pPlatform->peekExecutionEnvironment()->rootDeviceEnvironments[0]);
        }

        PlatformFixture::TearDown();
    }
    DebugManagerStateRestore restorer;
    cl_int retVal = CL_SUCCESS;
    size_t retSize = 0;

    CommandQueue *pCommandQueue = nullptr;
    Context *pContext = nullptr;
    MockKernel *pKernel = nullptr;
    MockProgram *pProgram = nullptr;
    constexpr static uint32_t numRootDevices = maxRootDeviceCount;
    constexpr static uint32_t testedRootDeviceIndex = rootDeviceIndex;
    cl_device_id testedClDevice = nullptr;
    std::unique_ptr<RootDeviceEnvironment> rootDeviceEnvironmentBackup;
};

struct api_tests : public ApiFixture<>,
                   public ::testing::Test {
    void SetUp() override {
        ApiFixture::SetUp();
    }
    void TearDown() override {
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
    MockClDevice *device;
};

using api_test_using_aligned_memory_manager = Test<api_fixture_using_aligned_memory_manager>;

} // namespace NEO
