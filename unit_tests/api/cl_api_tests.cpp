/*
 * Copyright (C) 2017-2019 Intel Corporation
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
constexpr size_t ApiFixture::numRootDevices;
void ApiFixture::SetUp() {
    numDevicesBackup = numRootDevices;
    PlatformFixture::SetUp();

    auto pDevice = pPlatform->getDevice(testedRootDeviceIndex);
    ASSERT_NE(nullptr, pDevice);

    testedClDevice = pDevice;
    pContext = Context::create<MockContext>(nullptr, DeviceVector(&testedClDevice, 1), nullptr, nullptr, retVal);
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

    PlatformFixture::TearDown();
}

void api_fixture_using_aligned_memory_manager::SetUp() {
    retVal = CL_SUCCESS;
    retSize = 0;

    device = MockDevice::createWithNewExecutionEnvironment<MockAlignedMallocManagerDevice>(*platformDevices);
    Device *devPtr = reinterpret_cast<Device *>(device);
    cl_device_id clDevice = devPtr;

    context = Context::create<MockContext>(nullptr, DeviceVector(&clDevice, 1), nullptr, nullptr, retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    Context *ctxPtr = reinterpret_cast<Context *>(context);

    commandQueue = new CommandQueue(context, devPtr, 0);

    program = new MockProgram(*device->getExecutionEnvironment(), ctxPtr, false);
    Program *prgPtr = reinterpret_cast<Program *>(program);

    kernel = new MockKernel(prgPtr, program->mockKernelInfo, *devPtr);
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
