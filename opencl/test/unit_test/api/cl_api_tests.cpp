/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "cl_api_tests.h"

#include "shared/source/host_function/host_function.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/test/common/mocks/mock_device.h"

#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"

namespace NEO {
class Program;

static_assert(std::is_same_v<
              void(CL_CALLBACK *)(void *),
              void(NEO_HOST_FUNCTION_CALLBACK *)(void *)>);

void CL_CALLBACK notifyFuncProgram(
    cl_program program,
    void *userData) {
    *((char *)userData) = 'a';
}

void ApiFixture::setUp() {
    debugManager.flags.CreateMultipleRootDevices.set(numRootDevices);
    debugManager.flags.EnableCpuCacheForResources.set(true);
    debugManager.flags.ContextGroupSize.set(0);
    executionEnvironment = new ClExecutionEnvironment();
    prepareDeviceEnvironments(*executionEnvironment);
    auto platform = NEO::constructPlatform(executionEnvironment);
    for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
        executionEnvironment->rootDeviceEnvironments[i]->initGmm();
    }
    auto rootDevice = MockDevice::createWithExecutionEnvironment<MockDevice>(defaultHwInfo.get(), executionEnvironment, testedRootDeviceIndex);

    NEO::initPlatform({rootDevice});
    pDevice = static_cast<MockClDevice *>(platform->getClDevice(0u));
    ASSERT_NE(nullptr, pDevice);

    testedClDevice = pDevice;
    pContext = Context::create<MockContext>(nullptr, ClDeviceVector(&testedClDevice, 1), nullptr, nullptr, retVal);
    EXPECT_EQ(retVal, CL_SUCCESS);

    pCommandQueue = new MockCommandQueue(pContext, pDevice, nullptr, false);

    pProgram = new MockProgram(pContext, false, toClDeviceVector(*pDevice));

    pMultiDeviceKernel = MockMultiDeviceKernel::create<MockKernel>(pProgram, MockKernel::toKernelInfoContainer(pProgram->mockKernelInfo, testedRootDeviceIndex));
    pKernel = static_cast<MockKernel *>(pMultiDeviceKernel->getKernel(testedRootDeviceIndex));
    ASSERT_NE(nullptr, pKernel);
}

void ApiFixtureUsingAlignedMemoryManager::setUp() {
    retVal = CL_SUCCESS;
    retSize = 0;

    device = new MockClDevice{MockDevice::createWithNewExecutionEnvironment<MockAlignedMallocManagerDevice>(defaultHwInfo.get())};
    cl_device_id deviceId = device;

    context = Context::create<MockContext>(nullptr, ClDeviceVector(&deviceId, 1), nullptr, nullptr, retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    Context *ctxPtr = reinterpret_cast<Context *>(context);

    commandQueue = new MockCommandQueue(context, device, 0, false);

    program = new MockProgram(ctxPtr, false, toClDeviceVector(*device));
    Program *prgPtr = reinterpret_cast<Program *>(program);

    kernel = new MockKernel(prgPtr, program->mockKernelInfo, *device);
    ASSERT_NE(nullptr, kernel);
}

void ApiFixtureUsingAlignedMemoryManager::tearDown() {
    delete kernel;
    delete commandQueue;
    context->release();
    program->release();
    delete device;
}
} // namespace NEO
