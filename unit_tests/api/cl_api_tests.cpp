/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "cl_api_tests.h"

#include "core/helpers/options.h"
#include "runtime/command_queue/command_queue.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_device.h"
#include "unit_tests/mocks/mock_kernel.h"
#include "unit_tests/mocks/mock_memory_manager.h"

namespace NEO {
constexpr uint32_t ApiFixture::numRootDevices;
constexpr uint32_t ApiFixture::testedRootDeviceIndex;
ApiFixture::ApiFixture() = default;
ApiFixture::~ApiFixture() = default;
void ApiFixture::SetUp() {
    numDevicesBackup = numRootDevices;
    PlatformFixture::SetUp();

    EXPECT_LT(0u, testedRootDeviceIndex);
    rootDeviceEnvironmentBackup.swap(pPlatform->peekExecutionEnvironment()->rootDeviceEnvironments[0]);
    auto pDevice = pPlatform->getClDevice(testedRootDeviceIndex);
    ASSERT_NE(nullptr, pDevice);

    testedClDevice = pDevice;
    pContext = Context::create<MockContext>(nullptr, ClDeviceVector(&testedClDevice, 1), nullptr, nullptr, retVal);
    EXPECT_EQ(retVal, CL_SUCCESS);

    pCommandQueue = new CommandQueue(pContext, pDevice, nullptr);

    pProgram = new MockProgram(*pDevice->getExecutionEnvironment(), pContext, false);

    pKernel = new MockKernel(pProgram, pProgram->mockKernelInfo, *pDevice);
    ASSERT_NE(nullptr, pKernel);
}

void ApiFixture::TearDown() {
    pKernel->release();
    pCommandQueue->release();
    pContext->release();
    pProgram->release();
    rootDeviceEnvironmentBackup.swap(pPlatform->peekExecutionEnvironment()->rootDeviceEnvironments[0]);
    PlatformFixture::TearDown();
}

void api_fixture_using_aligned_memory_manager::SetUp() {
    retVal = CL_SUCCESS;
    retSize = 0;

    device = new MockClDevice{MockDevice::createWithNewExecutionEnvironment<MockAlignedMallocManagerDevice>(*platformDevices)};
    cl_device_id deviceId = device;

    context = Context::create<MockContext>(nullptr, ClDeviceVector(&deviceId, 1), nullptr, nullptr, retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    Context *ctxPtr = reinterpret_cast<Context *>(context);

    commandQueue = new CommandQueue(context, device, 0);

    program = new MockProgram(*device->getExecutionEnvironment(), ctxPtr, false);
    Program *prgPtr = reinterpret_cast<Program *>(program);

    kernel = new MockKernel(prgPtr, program->mockKernelInfo, *device);
    ASSERT_NE(nullptr, kernel);
}

void api_fixture_using_aligned_memory_manager::TearDown() {
    delete kernel;
    delete commandQueue;
    context->release();
    program->release();
    delete device;
}
} // namespace NEO
