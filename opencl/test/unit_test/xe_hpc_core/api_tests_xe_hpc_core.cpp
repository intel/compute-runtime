/*
 * Copyright (C) 2021-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/xe_hpc_core/hw_info_xe_hpc_core.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"

#include "opencl/source/api/api.h"
#include "opencl/test/unit_test/api/cl_api_tests.h"

using namespace NEO;

struct EnqueueKernelTestGenXeHpcCore : public ApiTests {
    void SetUp() override {
        defaultHwInfo->caps.rcsExposureDisabled = false;
        ApiTests::SetUp();
    }

    VariableBackup<HardwareInfo> backupHwInfo{defaultHwInfo.get()};
};

XE_HPC_CORETEST_F(EnqueueKernelTestGenXeHpcCore, givenCommandQueueWithCCCSEngineAndRevisionBWhenCallingClEnqueueNDCountKernelINTELThenInvalidCommandQueueIsReturned) {
    size_t workgroupCount[3] = {2, 1, 1};
    size_t localWorkSize[3] = {256, 1, 1};
    cl_int retVal = CL_SUCCESS;

    auto &hwInfo = *pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    auto &productHelper = pDevice->getProductHelper();
    hwInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(REVISION_B, hwInfo);

    pProgram->mockKernelInfo.kernelDescriptor.kernelAttributes.flags.usesSyncBuffer = true;
    pCommandQueue->getGpgpuEngine().osContext = pCommandQueue->getDevice().getEngine(aub_stream::ENGINE_CCCS, EngineUsage::regular).osContext;
    retVal = pMultiDeviceKernel->setKernelExecutionType(CL_KERNEL_EXEC_INFO_CONCURRENT_TYPE_INTEL);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clEnqueueNDCountKernelINTEL(pCommandQueue, pMultiDeviceKernel, 1, nullptr, workgroupCount, localWorkSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, retVal);
}
