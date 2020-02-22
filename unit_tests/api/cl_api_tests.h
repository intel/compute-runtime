/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/execution_environment/root_device_environment.h"
#include "core/unit_tests/helpers/debug_manager_state_restore.h"
#include "test.h"
#include "unit_tests/fixtures/platform_fixture.h"
#include "unit_tests/helpers/ult_limits.h"
#include "unit_tests/helpers/variable_backup.h"
#include "unit_tests/mocks/mock_kernel.h"

#include "api/api.h"
#include "command_queue/command_queue.h"
#include "gtest/gtest.h"
#include "tracing/tracing_api.h"

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

        pCommandQueue = new CommandQueue(pContext, pDevice, nullptr);

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
    MockClDevice *device;
};

using api_test_using_aligned_memory_manager = Test<api_fixture_using_aligned_memory_manager>;

} // namespace NEO
