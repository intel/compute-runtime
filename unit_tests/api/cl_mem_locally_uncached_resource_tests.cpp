/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "public/cl_ext_private.h"
#include "runtime/api/api.h"
#include "runtime/command_queue/command_queue_hw.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/device/device.h"
#include "runtime/gen_common/hw_cmds.h"
#include "runtime/gmm_helper/gmm_helper.h"
#include "runtime/helpers/state_base_address.h"
#include "runtime/kernel/kernel.h"
#include "test.h"
#include "unit_tests/fixtures/hello_world_fixture.h"
#include "unit_tests/helpers/hw_parse.h"

using namespace OCLRT;

namespace clMemLocallyUncachedResourceTests {

struct clMemLocallyUncachedResourceFixture : Test<HelloWorldFixture<HelloWorldFixtureFactory>>,
                                             ::testing::WithParamInterface<bool> {};

HWTEST_P(clMemLocallyUncachedResourceFixture, GivenLocallyCachedOrUncachedBufferWhenItIsSetAndQueuedThenItIsCorrectlyCached) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    const size_t n = 512;
    size_t globalWorkSize[3] = {n, 1, 1};
    size_t localWorkSize[3] = {256, 1, 1};
    bool useUncachedFlag = GetParam();

    cl_int retVal = CL_SUCCESS;
    std::unique_ptr<Kernel> kernel(Kernel::create(pProgram, *pProgram->getKernelInfo("CopyBuffer"), &retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);

    cl_mem_properties_intel propertiesUncached[] = {CL_MEM_FLAGS_INTEL, CL_MEM_LOCALLY_UNCACHED_RESOURCE, 0};
    cl_mem_properties_intel *properties = (useUncachedFlag ? propertiesUncached : nullptr);
    auto buffer1 = clCreateBufferWithPropertiesINTEL(context, properties, n * sizeof(float), nullptr, nullptr);
    auto buffer2 = clCreateBufferWithPropertiesINTEL(context, properties, n * sizeof(float), nullptr, nullptr);

    retVal = clSetKernelArg(kernel.get(), 0, sizeof(cl_mem), &buffer1);
    EXPECT_EQ(CL_SUCCESS, retVal);
    auto surfaceStateHeapAddress = kernel.get()->getSurfaceStateHeap();
    auto surfaceStateHeapAddressOffset = kernel.get()->getKernelInfo().kernelArgInfo[0].offsetHeap;
    auto surfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(ptrOffset(surfaceStateHeapAddress, surfaceStateHeapAddressOffset));
    auto expectedMocs = pDevice->getGmmHelper()->getMOCS(useUncachedFlag ? GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED
                                                                         : GMM_RESOURCE_USAGE_OCL_BUFFER);
    EXPECT_EQ(expectedMocs, surfaceState->getMemoryObjectControlState());

    retVal = clSetKernelArg(kernel.get(), 1, sizeof(cl_mem), &buffer2);
    EXPECT_EQ(CL_SUCCESS, retVal);
    surfaceStateHeapAddressOffset = kernel.get()->getKernelInfo().kernelArgInfo[1].offsetHeap;
    surfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(ptrOffset(surfaceStateHeapAddress, surfaceStateHeapAddressOffset));
    EXPECT_EQ(expectedMocs, surfaceState->getMemoryObjectControlState());

    EXPECT_TRUE(kernel->isPatched());
    retVal = clEnqueueNDRangeKernel(pCmdQ, kernel.get(), 1, nullptr, globalWorkSize, localWorkSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto pCmdQHw = reinterpret_cast<CommandQueueHw<FamilyType> *>(pCmdQ);
    ASSERT_NE(nullptr, pCmdQHw);
    auto &csr = pCmdQHw->getCommandStreamReceiver();
    HardwareParse hwParse;
    hwParse.parseCommands<FamilyType>(csr.getCS(0), 0);
    auto itorCmd = find<STATE_BASE_ADDRESS *>(hwParse.cmdList.begin(), hwParse.cmdList.end());
    EXPECT_NE(hwParse.cmdList.end(), itorCmd);
    auto sba = genCmdCast<STATE_BASE_ADDRESS *>(*itorCmd);
    ASSERT_NE(nullptr, sba);

    EXPECT_EQ(expectedMocs, sba->getStatelessDataPortAccessMemoryObjectControlState());

    retVal = clReleaseMemObject(buffer1);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clReleaseMemObject(buffer2);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

INSTANTIATE_TEST_CASE_P(clMemLocallyUncachedResourceTest,
                        clMemLocallyUncachedResourceFixture,
                        ::testing::Bool());

} // namespace clMemLocallyUncachedResourceTests
