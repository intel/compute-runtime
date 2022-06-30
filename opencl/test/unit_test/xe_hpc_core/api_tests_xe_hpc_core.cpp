/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/hw_info_config.h"

#include "opencl/test/unit_test/api/cl_api_tests.h"

using namespace NEO;

using EnqueueKernelTestGenXeHpcCore = api_tests;

XE_HPC_CORETEST_F(EnqueueKernelTestGenXeHpcCore, givenCommandQueueWithCCCSEngineAndRevisionBWhenCallingClEnqueueNDCountKernelINTELThenInvalidCommandQueueIsReturned) {
    size_t workgroupCount[3] = {2, 1, 1};
    size_t localWorkSize[3] = {256, 1, 1};
    cl_int retVal = CL_SUCCESS;

    auto &hwInfo = *pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    auto &hwConfig = *HwInfoConfig::get(hwInfo.platform.eProductFamily);
    hwInfo.platform.usRevId = hwConfig.getHwRevIdFromStepping(REVISION_B, hwInfo);

    pProgram->mockKernelInfo.kernelDescriptor.kernelAttributes.flags.usesSyncBuffer = true;
    pCommandQueue->getGpgpuEngine().osContext = pCommandQueue->getDevice().getEngine(aub_stream::ENGINE_CCCS, EngineUsage::Regular).osContext;
    retVal = pMultiDeviceKernel->setKernelExecutionType(CL_KERNEL_EXEC_INFO_CONCURRENT_TYPE_INTEL);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clEnqueueNDCountKernelINTEL(pCommandQueue, pMultiDeviceKernel, 1, nullptr, workgroupCount, localWorkSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, retVal);
}
