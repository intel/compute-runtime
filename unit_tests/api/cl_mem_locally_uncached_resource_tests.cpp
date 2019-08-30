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
#include "unit_tests/utilities/base_object_utils.h"

using namespace NEO;

namespace clMemLocallyUncachedResourceTests {

template <typename FamilyType>
uint32_t argMocs(Kernel &kernel, size_t argIndex) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    auto surfaceStateHeapAddress = kernel.getSurfaceStateHeap();
    auto surfaceStateHeapAddressOffset = kernel.getKernelInfo().kernelArgInfo[argIndex].offsetHeap;
    auto surfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(ptrOffset(surfaceStateHeapAddress, surfaceStateHeapAddressOffset));
    return surfaceState->getMemoryObjectControlState();
}

template <typename FamilyType>
uint32_t cmdQueueMocs(CommandQueue *pCmdQ) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    auto pCmdQHw = reinterpret_cast<CommandQueueHw<FamilyType> *>(pCmdQ);
    auto &csr = pCmdQHw->getGpgpuCommandStreamReceiver();
    HardwareParse hwParse;
    hwParse.parseCommands<FamilyType>(csr.getCS(0), 0);
    auto itorCmd = reverse_find<STATE_BASE_ADDRESS *>(hwParse.cmdList.rbegin(), hwParse.cmdList.rend());
    EXPECT_NE(hwParse.cmdList.rend(), itorCmd);
    auto sba = genCmdCast<STATE_BASE_ADDRESS *>(*itorCmd);
    EXPECT_NE(nullptr, sba);

    return sba->getStatelessDataPortAccessMemoryObjectControlState();
}

const size_t n = 512;
const size_t globalWorkSize[3] = {n, 1, 1};
const size_t localWorkSize[3] = {256, 1, 1};

const cl_mem_properties_intel *propertiesCacheable = nullptr;
const cl_mem_properties_intel propertiesUncacheable[] = {CL_MEM_FLAGS_INTEL, CL_MEM_LOCALLY_UNCACHED_RESOURCE, 0};
const cl_mem_properties_intel propertiesUncacheableInSurfaceState[] = {CL_MEM_FLAGS_INTEL, CL_MEM_LOCALLY_UNCACHED_SURFACE_STATE_RESOURCE, 0};

using clMemLocallyUncachedResourceFixture = Test<HelloWorldFixture<HelloWorldFixtureFactory>>;

HWTEST_F(clMemLocallyUncachedResourceFixture, GivenAtLeastOneLocallyUncacheableResourceWhenSettingKernelArgumentsThenKernelIsUncacheable) {
    cl_int retVal = CL_SUCCESS;
    MockKernelWithInternals mockKernel(*this->pDevice, context, true);
    mockKernel.kernelInfo.usesSsh = true;
    mockKernel.kernelInfo.requiresSshForBuffers = true;

    auto kernel = mockKernel.mockKernel;

    auto bufferCacheable1 = clCreateBufferWithPropertiesINTEL(context, propertiesCacheable, n * sizeof(float), nullptr, nullptr);
    auto pBufferCacheable1 = clUniquePtr(castToObject<Buffer>(bufferCacheable1));
    auto bufferCacheable2 = clCreateBufferWithPropertiesINTEL(context, propertiesCacheable, n * sizeof(float), nullptr, nullptr);
    auto pBufferCacheable2 = clUniquePtr(castToObject<Buffer>(bufferCacheable2));

    auto bufferUncacheable1 = clCreateBufferWithPropertiesINTEL(context, propertiesUncacheable, n * sizeof(float), nullptr, nullptr);
    auto pBufferUncacheable1 = clUniquePtr(castToObject<Buffer>(bufferUncacheable1));
    auto bufferUncacheable2 = clCreateBufferWithPropertiesINTEL(context, propertiesUncacheable, n * sizeof(float), nullptr, nullptr);
    auto pBufferUncacheable2 = clUniquePtr(castToObject<Buffer>(bufferUncacheable2));

    auto mocsCacheable = kernel->getDevice().getGmmHelper()->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER);
    auto mocsUncacheable = kernel->getDevice().getGmmHelper()->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED);

    retVal = clSetKernelArg(kernel, 0, sizeof(cl_mem), &bufferCacheable1);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(mocsCacheable, argMocs<FamilyType>(*kernel, 0));

    retVal = clSetKernelArg(kernel, 1, sizeof(cl_mem), &bufferCacheable2);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(mocsCacheable, argMocs<FamilyType>(*kernel, 1));

    EXPECT_TRUE(kernel->isPatched());
    retVal = clEnqueueNDRangeKernel(pCmdQ, kernel, 1, nullptr, globalWorkSize, localWorkSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(mocsCacheable, cmdQueueMocs<FamilyType>(pCmdQ));
    EXPECT_FALSE(kernel->hasUncacheableStatelessArgs());

    retVal = clSetKernelArg(kernel, 0, sizeof(cl_mem), &bufferUncacheable1);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(mocsUncacheable, argMocs<FamilyType>(*kernel, 0));

    EXPECT_TRUE(kernel->isPatched());
    retVal = clEnqueueNDRangeKernel(pCmdQ, kernel, 1, nullptr, globalWorkSize, localWorkSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(mocsUncacheable, cmdQueueMocs<FamilyType>(pCmdQ));
    EXPECT_TRUE(kernel->hasUncacheableStatelessArgs());

    retVal = clSetKernelArg(kernel, 1, sizeof(cl_mem), &bufferUncacheable2);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(mocsUncacheable, argMocs<FamilyType>(*kernel, 0));

    EXPECT_TRUE(kernel->isPatched());
    retVal = clEnqueueNDRangeKernel(pCmdQ, kernel, 1, nullptr, globalWorkSize, localWorkSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(mocsUncacheable, cmdQueueMocs<FamilyType>(pCmdQ));
    EXPECT_TRUE(kernel->hasUncacheableStatelessArgs());

    retVal = clSetKernelArg(kernel, 0, sizeof(cl_mem), &bufferCacheable1);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(mocsCacheable, argMocs<FamilyType>(*kernel, 0));

    EXPECT_TRUE(kernel->isPatched());
    retVal = clEnqueueNDRangeKernel(pCmdQ, kernel, 1, nullptr, globalWorkSize, localWorkSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(mocsUncacheable, cmdQueueMocs<FamilyType>(pCmdQ));
    EXPECT_TRUE(kernel->hasUncacheableStatelessArgs());

    retVal = clSetKernelArg(kernel, 1, sizeof(cl_mem), &bufferCacheable2);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(mocsCacheable, argMocs<FamilyType>(*kernel, 1));

    EXPECT_TRUE(kernel->isPatched());
    retVal = clEnqueueNDRangeKernel(pCmdQ, kernel, 1, nullptr, globalWorkSize, localWorkSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(mocsCacheable, cmdQueueMocs<FamilyType>(pCmdQ));
    EXPECT_FALSE(kernel->hasUncacheableStatelessArgs());
}

HWTEST_F(clMemLocallyUncachedResourceFixture, givenBuffersThatAreUncachedInSurfaceStateWhenStatelessIsProgrammedItIsCached) {
    cl_int retVal = CL_SUCCESS;

    MockKernelWithInternals mockKernel(*this->pDevice, context, true);
    auto kernel = mockKernel.mockKernel;
    mockKernel.kernelInfo.usesSsh = true;
    mockKernel.kernelInfo.requiresSshForBuffers = true;

    EXPECT_EQ(CL_SUCCESS, retVal);

    auto bufferCacheable1 = clCreateBufferWithPropertiesINTEL(context, propertiesCacheable, n * sizeof(float), nullptr, nullptr);
    auto pBufferCacheable1 = clUniquePtr(castToObject<Buffer>(bufferCacheable1));
    auto bufferCacheable2 = clCreateBufferWithPropertiesINTEL(context, propertiesCacheable, n * sizeof(float), nullptr, nullptr);
    auto pBufferCacheable2 = clUniquePtr(castToObject<Buffer>(bufferCacheable2));

    auto bufferUncacheable1 = clCreateBufferWithPropertiesINTEL(context, propertiesUncacheableInSurfaceState, n * sizeof(float), nullptr, nullptr);
    auto pBufferUncacheable1 = clUniquePtr(castToObject<Buffer>(bufferUncacheable1));
    auto bufferUncacheable2 = clCreateBufferWithPropertiesINTEL(context, propertiesUncacheableInSurfaceState, n * sizeof(float), nullptr, nullptr);
    auto pBufferUncacheable2 = clUniquePtr(castToObject<Buffer>(bufferUncacheable2));

    auto mocsCacheable = kernel->getDevice().getGmmHelper()->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER);
    auto mocsUncacheable = kernel->getDevice().getGmmHelper()->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED);

    retVal = clSetKernelArg(kernel, 0, sizeof(cl_mem), &bufferCacheable1);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(mocsCacheable, argMocs<FamilyType>(*kernel, 0));

    retVal = clSetKernelArg(kernel, 1, sizeof(cl_mem), &bufferCacheable2);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(mocsCacheable, argMocs<FamilyType>(*kernel, 1));

    EXPECT_TRUE(kernel->isPatched());
    retVal = clEnqueueNDRangeKernel(pCmdQ, kernel, 1, nullptr, globalWorkSize, localWorkSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(mocsCacheable, cmdQueueMocs<FamilyType>(pCmdQ));

    retVal = clSetKernelArg(kernel, 0, sizeof(cl_mem), &bufferUncacheable1);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(mocsUncacheable, argMocs<FamilyType>(*kernel, 0));
    EXPECT_FALSE(kernel->hasUncacheableStatelessArgs());

    EXPECT_TRUE(kernel->isPatched());
    retVal = clEnqueueNDRangeKernel(pCmdQ, kernel, 1, nullptr, globalWorkSize, localWorkSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(mocsCacheable, cmdQueueMocs<FamilyType>(pCmdQ));

    retVal = clSetKernelArg(kernel, 1, sizeof(cl_mem), &bufferUncacheable2);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(mocsUncacheable, argMocs<FamilyType>(*kernel, 0));
    EXPECT_FALSE(kernel->hasUncacheableStatelessArgs());

    EXPECT_TRUE(kernel->isPatched());
    retVal = clEnqueueNDRangeKernel(pCmdQ, kernel, 1, nullptr, globalWorkSize, localWorkSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(mocsCacheable, cmdQueueMocs<FamilyType>(pCmdQ));

    retVal = clSetKernelArg(kernel, 0, sizeof(cl_mem), &bufferCacheable1);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(mocsCacheable, argMocs<FamilyType>(*kernel, 0));

    EXPECT_TRUE(kernel->isPatched());
    retVal = clEnqueueNDRangeKernel(pCmdQ, kernel, 1, nullptr, globalWorkSize, localWorkSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(mocsCacheable, cmdQueueMocs<FamilyType>(pCmdQ));

    retVal = clSetKernelArg(kernel, 1, sizeof(cl_mem), &bufferCacheable2);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(mocsCacheable, argMocs<FamilyType>(*kernel, 1));

    EXPECT_TRUE(kernel->isPatched());
    retVal = clEnqueueNDRangeKernel(pCmdQ, kernel, 1, nullptr, globalWorkSize, localWorkSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(mocsCacheable, cmdQueueMocs<FamilyType>(pCmdQ));
}

HWTEST_F(clMemLocallyUncachedResourceFixture, givenBuffersThatAreUncachedButKernelDoesntHaveAnyStatelessAccessessThenSurfacesAreNotRecordedAsUncacheable) {
    cl_int retVal = CL_SUCCESS;

    MockKernelWithInternals mockKernel(*this->pDevice, context, true);
    auto kernel = mockKernel.mockKernel;
    mockKernel.kernelInfo.usesSsh = true;
    mockKernel.kernelInfo.requiresSshForBuffers = true;
    mockKernel.kernelInfo.kernelArgInfo[0].pureStatefulBufferAccess = true;
    mockKernel.kernelInfo.kernelArgInfo[1].pureStatefulBufferAccess = true;

    EXPECT_EQ(CL_SUCCESS, retVal);

    auto bufferCacheable1 = clCreateBufferWithPropertiesINTEL(context, propertiesCacheable, n * sizeof(float), nullptr, nullptr);
    auto pBufferCacheable1 = clUniquePtr(castToObject<Buffer>(bufferCacheable1));
    auto bufferCacheable2 = clCreateBufferWithPropertiesINTEL(context, propertiesCacheable, n * sizeof(float), nullptr, nullptr);
    auto pBufferCacheable2 = clUniquePtr(castToObject<Buffer>(bufferCacheable2));

    auto bufferUncacheable1 = clCreateBufferWithPropertiesINTEL(context, propertiesUncacheable, n * sizeof(float), nullptr, nullptr);
    auto pBufferUncacheable1 = clUniquePtr(castToObject<Buffer>(bufferUncacheable1));
    auto bufferUncacheable2 = clCreateBufferWithPropertiesINTEL(context, propertiesUncacheable, n * sizeof(float), nullptr, nullptr);
    auto pBufferUncacheable2 = clUniquePtr(castToObject<Buffer>(bufferUncacheable2));

    auto mocsCacheable = kernel->getDevice().getGmmHelper()->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER);
    auto mocsUncacheable = kernel->getDevice().getGmmHelper()->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED);

    retVal = clSetKernelArg(kernel, 0, sizeof(cl_mem), &bufferCacheable1);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(mocsCacheable, argMocs<FamilyType>(*kernel, 0));

    retVal = clSetKernelArg(kernel, 1, sizeof(cl_mem), &bufferCacheable2);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(mocsCacheable, argMocs<FamilyType>(*kernel, 1));

    EXPECT_TRUE(kernel->isPatched());
    retVal = clEnqueueNDRangeKernel(pCmdQ, kernel, 1, nullptr, globalWorkSize, localWorkSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(mocsCacheable, cmdQueueMocs<FamilyType>(pCmdQ));

    retVal = clSetKernelArg(kernel, 0, sizeof(cl_mem), &bufferUncacheable1);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(mocsUncacheable, argMocs<FamilyType>(*kernel, 0));
    EXPECT_FALSE(kernel->hasUncacheableStatelessArgs());

    EXPECT_TRUE(kernel->isPatched());
    retVal = clEnqueueNDRangeKernel(pCmdQ, kernel, 1, nullptr, globalWorkSize, localWorkSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(mocsCacheable, cmdQueueMocs<FamilyType>(pCmdQ));

    retVal = clSetKernelArg(kernel, 1, sizeof(cl_mem), &bufferUncacheable2);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(mocsUncacheable, argMocs<FamilyType>(*kernel, 0));
    EXPECT_FALSE(kernel->hasUncacheableStatelessArgs());

    EXPECT_TRUE(kernel->isPatched());
    retVal = clEnqueueNDRangeKernel(pCmdQ, kernel, 1, nullptr, globalWorkSize, localWorkSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(mocsCacheable, cmdQueueMocs<FamilyType>(pCmdQ));

    retVal = clSetKernelArg(kernel, 0, sizeof(cl_mem), &bufferCacheable1);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(mocsCacheable, argMocs<FamilyType>(*kernel, 0));
    EXPECT_FALSE(kernel->hasUncacheableStatelessArgs());

    EXPECT_TRUE(kernel->isPatched());
    retVal = clEnqueueNDRangeKernel(pCmdQ, kernel, 1, nullptr, globalWorkSize, localWorkSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(mocsCacheable, cmdQueueMocs<FamilyType>(pCmdQ));

    retVal = clSetKernelArg(kernel, 1, sizeof(cl_mem), &bufferCacheable2);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(mocsCacheable, argMocs<FamilyType>(*kernel, 1));
    EXPECT_FALSE(kernel->hasUncacheableStatelessArgs());

    EXPECT_TRUE(kernel->isPatched());
    retVal = clEnqueueNDRangeKernel(pCmdQ, kernel, 1, nullptr, globalWorkSize, localWorkSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(mocsCacheable, cmdQueueMocs<FamilyType>(pCmdQ));
    EXPECT_FALSE(kernel->hasUncacheableStatelessArgs());
}

HWTEST_F(clMemLocallyUncachedResourceFixture, WhenUnsettingUncacheableResourceFromKernelThanKernelContinuesToCorrectlySetMocs) {
    cl_int retVal = CL_SUCCESS;
    MockKernelWithInternals mockKernel(*this->pDevice, context, true);
    auto kernel = mockKernel.mockKernel;
    mockKernel.kernelInfo.usesSsh = true;
    mockKernel.kernelInfo.requiresSshForBuffers = true;

    EXPECT_EQ(CL_SUCCESS, retVal);

    auto bufferCacheable1 = clCreateBufferWithPropertiesINTEL(context, propertiesCacheable, n * sizeof(float), nullptr, nullptr);
    auto pBufferCacheable1 = clUniquePtr(castToObject<Buffer>(bufferCacheable1));
    auto bufferCacheable2 = clCreateBufferWithPropertiesINTEL(context, propertiesCacheable, n * sizeof(float), nullptr, nullptr);
    auto pBufferCacheable2 = clUniquePtr(castToObject<Buffer>(bufferCacheable2));

    auto bufferUncacheable = clCreateBufferWithPropertiesINTEL(context, propertiesUncacheable, n * sizeof(float), nullptr, nullptr);
    auto pBufferUncacheable = clUniquePtr(castToObject<Buffer>(bufferUncacheable));

    auto mocsCacheable = kernel->getDevice().getGmmHelper()->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER);
    auto mocsUncacheable = kernel->getDevice().getGmmHelper()->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED);

    retVal = clSetKernelArg(kernel, 0, sizeof(cl_mem), &bufferCacheable1);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(mocsCacheable, argMocs<FamilyType>(*kernel, 0));

    retVal = clSetKernelArg(kernel, 1, sizeof(cl_mem), &bufferCacheable2);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(mocsCacheable, argMocs<FamilyType>(*kernel, 1));

    EXPECT_TRUE(kernel->isPatched());
    retVal = clEnqueueNDRangeKernel(pCmdQ, kernel, 1, nullptr, globalWorkSize, localWorkSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(mocsCacheable, cmdQueueMocs<FamilyType>(pCmdQ));

    retVal = clSetKernelArg(kernel, 0, sizeof(cl_mem), &bufferUncacheable);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(mocsUncacheable, argMocs<FamilyType>(*kernel, 0));

    EXPECT_TRUE(kernel->isPatched());
    retVal = clEnqueueNDRangeKernel(pCmdQ, kernel, 1, nullptr, globalWorkSize, localWorkSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(mocsUncacheable, cmdQueueMocs<FamilyType>(pCmdQ));

    kernel->unsetArg(0);

    retVal = clSetKernelArg(kernel, 0, sizeof(cl_mem), &bufferCacheable1);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(mocsCacheable, argMocs<FamilyType>(*kernel, 0));

    EXPECT_TRUE(kernel->isPatched());
    retVal = clEnqueueNDRangeKernel(pCmdQ, kernel, 1, nullptr, globalWorkSize, localWorkSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(mocsCacheable, cmdQueueMocs<FamilyType>(pCmdQ));

    kernel->unsetArg(0);

    retVal = clSetKernelArg(kernel, 0, sizeof(cl_mem), &bufferUncacheable);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(mocsUncacheable, argMocs<FamilyType>(*kernel, 0));

    EXPECT_TRUE(kernel->isPatched());
    retVal = clEnqueueNDRangeKernel(pCmdQ, kernel, 1, nullptr, globalWorkSize, localWorkSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(mocsUncacheable, cmdQueueMocs<FamilyType>(pCmdQ));
}

HWTEST_F(clMemLocallyUncachedResourceFixture, givenBuffersThatAreUncachedInSurfaceStateAndAreNotUsedInStatelessFashionThenThoseResourcesAreNotRegistredAsResourcesForCacheFlush) {
    cl_int retVal = CL_SUCCESS;

    MockKernelWithInternals mockKernel(*this->pDevice, context, true);
    auto kernel = mockKernel.mockKernel;
    mockKernel.kernelInfo.usesSsh = true;
    mockKernel.kernelInfo.requiresSshForBuffers = true;
    mockKernel.kernelInfo.kernelArgInfo[0].pureStatefulBufferAccess = true;
    mockKernel.kernelInfo.kernelArgInfo[1].pureStatefulBufferAccess = true;

    EXPECT_EQ(CL_SUCCESS, retVal);

    auto bufferCacheable = clCreateBufferWithPropertiesINTEL(context, propertiesCacheable, n * sizeof(float), nullptr, nullptr);

    auto bufferUncacheableInSurfaceState = clCreateBufferWithPropertiesINTEL(context, propertiesUncacheableInSurfaceState, n * sizeof(float), nullptr, nullptr);
    auto bufferUncacheable = clCreateBufferWithPropertiesINTEL(context, propertiesUncacheable, n * sizeof(float), nullptr, nullptr);

    retVal = clSetKernelArg(kernel, 0, sizeof(cl_mem), &bufferUncacheableInSurfaceState);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(nullptr, kernel->kernelArgRequiresCacheFlush[0]);

    retVal = clSetKernelArg(kernel, 0, sizeof(cl_mem), &bufferCacheable);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_NE(nullptr, kernel->kernelArgRequiresCacheFlush[0]);

    retVal = clSetKernelArg(kernel, 0, sizeof(cl_mem), &bufferUncacheable);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(nullptr, kernel->kernelArgRequiresCacheFlush[0]);

    clReleaseMemObject(bufferUncacheableInSurfaceState);
    clReleaseMemObject(bufferUncacheable);
    clReleaseMemObject(bufferCacheable);
}

} // namespace clMemLocallyUncachedResourceTests
