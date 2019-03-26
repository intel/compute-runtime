/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "cl_api_tests.h"

#include "runtime/command_queue/command_queue.h"
#include "runtime/helpers/options.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_device.h"
#include "unit_tests/mocks/mock_kernel.h"
#include "unit_tests/mocks/mock_memory_manager.h"

namespace NEO {

api_fixture::api_fixture()
    : retVal(CL_SUCCESS), retSize(0), pCommandQueue(nullptr),
      pContext(nullptr), pKernel(nullptr), pProgram(nullptr) {
}

void api_fixture::SetUp() {
    PlatformFixture::SetUp();

    ASSERT_EQ(retVal, CL_SUCCESS);
    auto pDevice = pPlatform->getDevice(0);
    ASSERT_NE(nullptr, pDevice);

    cl_device_id clDevice = pDevice;
    pContext = Context::create<MockContext>(nullptr, DeviceVector(&clDevice, 1), nullptr, nullptr, retVal);

    pCommandQueue = new CommandQueue(pContext, pDevice, 0);

    pProgram = new MockProgram(*pDevice->getExecutionEnvironment(), pContext, false);

    pKernel = new MockKernel(pProgram, *pProgram->MockProgram::getKernelInfo(), *pDevice);
    ASSERT_NE(nullptr, pKernel);
}

void api_fixture::TearDown() {
    delete pKernel;
    delete pCommandQueue;
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

    kernel = new MockKernel(prgPtr, *program->MockProgram::getKernelInfo(), *devPtr);
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
