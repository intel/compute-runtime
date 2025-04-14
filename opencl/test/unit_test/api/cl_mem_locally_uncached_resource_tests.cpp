/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/device/device.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/gmm_helper/gmm_lib.h"
#include "shared/source/helpers/state_base_address.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/common/utilities/base_object_utils.h"

#include "opencl/extensions/public/cl_ext_private.h"
#include "opencl/source/api/api.h"
#include "opencl/source/command_queue/command_queue_hw.h"
#include "opencl/source/kernel/kernel.h"
#include "opencl/test/unit_test/fixtures/hello_world_fixture.h"

using namespace NEO;

namespace clMemLocallyUncachedResourceTests {

template <typename FamilyType>
uint32_t argMocs(Kernel &kernel, size_t argIndex) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    auto surfaceStateHeapAddress = kernel.getSurfaceStateHeap();
    auto surfaceStateHeapAddressOffset = static_cast<size_t>(kernel.getKernelInfo().getArgDescriptorAt(static_cast<uint32_t>(argIndex)).as<ArgDescPointer>().bindful);
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
    auto itorCmd = reverseFind<STATE_BASE_ADDRESS *>(hwParse.cmdList.rbegin(), hwParse.cmdList.rend());
    EXPECT_NE(hwParse.cmdList.rend(), itorCmd);
    auto sba = genCmdCast<STATE_BASE_ADDRESS *>(*itorCmd);
    EXPECT_NE(nullptr, sba);

    return sba->getStatelessDataPortAccessMemoryObjectControlState();
}

const size_t n = 512;
[[maybe_unused]] const size_t globalWorkSize[3] = {n, 1, 1};
[[maybe_unused]] const size_t localWorkSize[3] = {256, 1, 1};

[[maybe_unused]] const cl_mem_properties_intel *propertiesCacheable = nullptr;
[[maybe_unused]] const cl_mem_properties_intel propertiesUncacheable[] = {CL_MEM_FLAGS_INTEL, CL_MEM_LOCALLY_UNCACHED_RESOURCE, 0};
[[maybe_unused]] const cl_mem_properties_intel propertiesUncacheableInSurfaceState[] = {CL_MEM_FLAGS_INTEL, CL_MEM_LOCALLY_UNCACHED_SURFACE_STATE_RESOURCE, 0};

using clMemLocallyUncachedResourceFixture = Test<HelloWorldFixture<HelloWorldFixtureFactory>>;

HWCMDTEST_F(IGFX_GEN12LP_CORE, clMemLocallyUncachedResourceFixture, GivenAtLeastOneLocallyUncacheableResourceWhenSettingKernelArgumentsThenKernelIsUncacheable) {
    cl_int retVal = CL_SUCCESS;
    MockKernelWithInternals mockKernel(*this->pClDevice, context, true);

    auto kernel = mockKernel.mockKernel;
    auto pMultiDeviceKernel = mockKernel.mockMultiDeviceKernel;

    auto bufferCacheable1 = clCreateBufferWithPropertiesINTEL(context, propertiesCacheable, 0, n * sizeof(float), nullptr, nullptr);
    auto pBufferCacheable1 = clUniquePtr(castToObject<Buffer>(bufferCacheable1));
    auto bufferCacheable2 = clCreateBufferWithPropertiesINTEL(context, propertiesCacheable, 0, n * sizeof(float), nullptr, nullptr);
    auto pBufferCacheable2 = clUniquePtr(castToObject<Buffer>(bufferCacheable2));

    auto bufferUncacheable1 = clCreateBufferWithPropertiesINTEL(context, propertiesUncacheable, 0, n * sizeof(float), nullptr, nullptr);
    auto pBufferUncacheable1 = clUniquePtr(castToObject<Buffer>(bufferUncacheable1));
    auto bufferUncacheable2 = clCreateBufferWithPropertiesINTEL(context, propertiesUncacheable, 0, n * sizeof(float), nullptr, nullptr);
    auto pBufferUncacheable2 = clUniquePtr(castToObject<Buffer>(bufferUncacheable2));

    auto mocsCacheable = pClDevice->getGmmHelper()->getL3EnabledMOCS();
    auto mocsUncacheable = pClDevice->getGmmHelper()->getUncachedMOCS();

    retVal = clSetKernelArg(pMultiDeviceKernel, 0, sizeof(cl_mem), &bufferCacheable1);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(mocsCacheable, argMocs<FamilyType>(*kernel, 0));

    retVal = clSetKernelArg(pMultiDeviceKernel, 1, sizeof(cl_mem), &bufferCacheable2);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(mocsCacheable, argMocs<FamilyType>(*kernel, 1));

    EXPECT_TRUE(kernel->isPatched());
    retVal = clEnqueueNDRangeKernel(pCmdQ, pMultiDeviceKernel, 1, nullptr, globalWorkSize, localWorkSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(mocsCacheable, cmdQueueMocs<FamilyType>(pCmdQ));
    EXPECT_FALSE(kernel->hasUncacheableStatelessArgs());

    retVal = clSetKernelArg(pMultiDeviceKernel, 0, sizeof(cl_mem), &bufferUncacheable1);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(mocsUncacheable, argMocs<FamilyType>(*kernel, 0));

    EXPECT_TRUE(kernel->isPatched());
    retVal = clEnqueueNDRangeKernel(pCmdQ, pMultiDeviceKernel, 1, nullptr, globalWorkSize, localWorkSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(mocsUncacheable, cmdQueueMocs<FamilyType>(pCmdQ));
    EXPECT_TRUE(kernel->hasUncacheableStatelessArgs());

    retVal = clSetKernelArg(pMultiDeviceKernel, 1, sizeof(cl_mem), &bufferUncacheable2);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(mocsUncacheable, argMocs<FamilyType>(*kernel, 0));

    EXPECT_TRUE(kernel->isPatched());
    retVal = clEnqueueNDRangeKernel(pCmdQ, pMultiDeviceKernel, 1, nullptr, globalWorkSize, localWorkSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(mocsUncacheable, cmdQueueMocs<FamilyType>(pCmdQ));
    EXPECT_TRUE(kernel->hasUncacheableStatelessArgs());

    retVal = clSetKernelArg(pMultiDeviceKernel, 0, sizeof(cl_mem), &bufferCacheable1);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(mocsCacheable, argMocs<FamilyType>(*kernel, 0));

    EXPECT_TRUE(kernel->isPatched());
    retVal = clEnqueueNDRangeKernel(pCmdQ, pMultiDeviceKernel, 1, nullptr, globalWorkSize, localWorkSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(mocsUncacheable, cmdQueueMocs<FamilyType>(pCmdQ));
    EXPECT_TRUE(kernel->hasUncacheableStatelessArgs());

    retVal = clSetKernelArg(pMultiDeviceKernel, 1, sizeof(cl_mem), &bufferCacheable2);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(mocsCacheable, argMocs<FamilyType>(*kernel, 1));

    EXPECT_TRUE(kernel->isPatched());
    retVal = clEnqueueNDRangeKernel(pCmdQ, pMultiDeviceKernel, 1, nullptr, globalWorkSize, localWorkSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(mocsCacheable, cmdQueueMocs<FamilyType>(pCmdQ));
    EXPECT_FALSE(kernel->hasUncacheableStatelessArgs());
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, clMemLocallyUncachedResourceFixture, givenBuffersThatAreUncachedInSurfaceStateWhenStatelessIsProgrammedThenItIsCached) {
    cl_int retVal = CL_SUCCESS;

    MockKernelWithInternals mockKernel(*this->pClDevice, context, true);
    auto kernel = mockKernel.mockKernel;
    auto pMultiDeviceKernel = mockKernel.mockMultiDeviceKernel;

    EXPECT_EQ(CL_SUCCESS, retVal);

    auto bufferCacheable1 = clCreateBufferWithPropertiesINTEL(context, propertiesCacheable, 0, n * sizeof(float), nullptr, nullptr);
    auto pBufferCacheable1 = clUniquePtr(castToObject<Buffer>(bufferCacheable1));
    auto bufferCacheable2 = clCreateBufferWithPropertiesINTEL(context, propertiesCacheable, 0, n * sizeof(float), nullptr, nullptr);
    auto pBufferCacheable2 = clUniquePtr(castToObject<Buffer>(bufferCacheable2));

    auto bufferUncacheable1 = clCreateBufferWithPropertiesINTEL(context, propertiesUncacheableInSurfaceState, 0, n * sizeof(float), nullptr, nullptr);
    auto pBufferUncacheable1 = clUniquePtr(castToObject<Buffer>(bufferUncacheable1));
    auto bufferUncacheable2 = clCreateBufferWithPropertiesINTEL(context, propertiesUncacheableInSurfaceState, 0, n * sizeof(float), nullptr, nullptr);
    auto pBufferUncacheable2 = clUniquePtr(castToObject<Buffer>(bufferUncacheable2));

    auto mocsCacheable = pClDevice->getGmmHelper()->getL3EnabledMOCS();
    auto mocsUncacheable = pClDevice->getGmmHelper()->getUncachedMOCS();

    retVal = clSetKernelArg(pMultiDeviceKernel, 0, sizeof(cl_mem), &bufferCacheable1);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(mocsCacheable, argMocs<FamilyType>(*kernel, 0));

    retVal = clSetKernelArg(pMultiDeviceKernel, 1, sizeof(cl_mem), &bufferCacheable2);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(mocsCacheable, argMocs<FamilyType>(*kernel, 1));

    EXPECT_TRUE(kernel->isPatched());
    retVal = clEnqueueNDRangeKernel(pCmdQ, pMultiDeviceKernel, 1, nullptr, globalWorkSize, localWorkSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(mocsCacheable, cmdQueueMocs<FamilyType>(pCmdQ));

    retVal = clSetKernelArg(pMultiDeviceKernel, 0, sizeof(cl_mem), &bufferUncacheable1);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(mocsUncacheable, argMocs<FamilyType>(*kernel, 0));
    EXPECT_FALSE(kernel->hasUncacheableStatelessArgs());

    EXPECT_TRUE(kernel->isPatched());
    retVal = clEnqueueNDRangeKernel(pCmdQ, pMultiDeviceKernel, 1, nullptr, globalWorkSize, localWorkSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(mocsCacheable, cmdQueueMocs<FamilyType>(pCmdQ));

    retVal = clSetKernelArg(pMultiDeviceKernel, 1, sizeof(cl_mem), &bufferUncacheable2);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(mocsUncacheable, argMocs<FamilyType>(*kernel, 0));
    EXPECT_FALSE(kernel->hasUncacheableStatelessArgs());

    EXPECT_TRUE(kernel->isPatched());
    retVal = clEnqueueNDRangeKernel(pCmdQ, pMultiDeviceKernel, 1, nullptr, globalWorkSize, localWorkSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(mocsCacheable, cmdQueueMocs<FamilyType>(pCmdQ));

    retVal = clSetKernelArg(pMultiDeviceKernel, 0, sizeof(cl_mem), &bufferCacheable1);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(mocsCacheable, argMocs<FamilyType>(*kernel, 0));

    EXPECT_TRUE(kernel->isPatched());
    retVal = clEnqueueNDRangeKernel(pCmdQ, pMultiDeviceKernel, 1, nullptr, globalWorkSize, localWorkSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(mocsCacheable, cmdQueueMocs<FamilyType>(pCmdQ));

    retVal = clSetKernelArg(pMultiDeviceKernel, 1, sizeof(cl_mem), &bufferCacheable2);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(mocsCacheable, argMocs<FamilyType>(*kernel, 1));

    EXPECT_TRUE(kernel->isPatched());
    retVal = clEnqueueNDRangeKernel(pCmdQ, pMultiDeviceKernel, 1, nullptr, globalWorkSize, localWorkSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(mocsCacheable, cmdQueueMocs<FamilyType>(pCmdQ));
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, clMemLocallyUncachedResourceFixture, givenBuffersThatAreUncachedButKernelDoesntHaveAnyStatelessAccessesThenSurfacesAreNotRecordedAsUncacheable) {
    cl_int retVal = CL_SUCCESS;

    MockKernelWithInternals mockKernel(*this->pClDevice, context, true);
    auto kernel = mockKernel.mockKernel;
    auto pMultiDeviceKernel = mockKernel.mockMultiDeviceKernel;
    mockKernel.kernelInfo.setBufferStateful(0);
    mockKernel.kernelInfo.setBufferStateful(1);

    auto bufferCacheable1 = clCreateBufferWithPropertiesINTEL(context, propertiesCacheable, 0, n * sizeof(float), nullptr, nullptr);
    auto pBufferCacheable1 = clUniquePtr(castToObject<Buffer>(bufferCacheable1));
    auto bufferCacheable2 = clCreateBufferWithPropertiesINTEL(context, propertiesCacheable, 0, n * sizeof(float), nullptr, nullptr);
    auto pBufferCacheable2 = clUniquePtr(castToObject<Buffer>(bufferCacheable2));

    auto bufferUncacheable1 = clCreateBufferWithPropertiesINTEL(context, propertiesUncacheable, 0, n * sizeof(float), nullptr, nullptr);
    auto pBufferUncacheable1 = clUniquePtr(castToObject<Buffer>(bufferUncacheable1));
    auto bufferUncacheable2 = clCreateBufferWithPropertiesINTEL(context, propertiesUncacheable, 0, n * sizeof(float), nullptr, nullptr);
    auto pBufferUncacheable2 = clUniquePtr(castToObject<Buffer>(bufferUncacheable2));

    auto mocsCacheable = pClDevice->getGmmHelper()->getL3EnabledMOCS();
    auto mocsUncacheable = pClDevice->getGmmHelper()->getUncachedMOCS();

    retVal = clSetKernelArg(pMultiDeviceKernel, 0, sizeof(cl_mem), &bufferCacheable1);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(mocsCacheable, argMocs<FamilyType>(*kernel, 0));

    retVal = clSetKernelArg(pMultiDeviceKernel, 1, sizeof(cl_mem), &bufferCacheable2);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(mocsCacheable, argMocs<FamilyType>(*kernel, 1));

    EXPECT_TRUE(kernel->isPatched());
    retVal = clEnqueueNDRangeKernel(pCmdQ, pMultiDeviceKernel, 1, nullptr, globalWorkSize, localWorkSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(mocsCacheable, cmdQueueMocs<FamilyType>(pCmdQ));

    retVal = clSetKernelArg(pMultiDeviceKernel, 0, sizeof(cl_mem), &bufferUncacheable1);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(mocsUncacheable, argMocs<FamilyType>(*kernel, 0));
    EXPECT_FALSE(kernel->hasUncacheableStatelessArgs());

    EXPECT_TRUE(kernel->isPatched());
    retVal = clEnqueueNDRangeKernel(pCmdQ, pMultiDeviceKernel, 1, nullptr, globalWorkSize, localWorkSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(mocsCacheable, cmdQueueMocs<FamilyType>(pCmdQ));

    retVal = clSetKernelArg(pMultiDeviceKernel, 1, sizeof(cl_mem), &bufferUncacheable2);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(mocsUncacheable, argMocs<FamilyType>(*kernel, 0));
    EXPECT_FALSE(kernel->hasUncacheableStatelessArgs());

    EXPECT_TRUE(kernel->isPatched());
    retVal = clEnqueueNDRangeKernel(pCmdQ, pMultiDeviceKernel, 1, nullptr, globalWorkSize, localWorkSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(mocsCacheable, cmdQueueMocs<FamilyType>(pCmdQ));

    retVal = clSetKernelArg(pMultiDeviceKernel, 0, sizeof(cl_mem), &bufferCacheable1);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(mocsCacheable, argMocs<FamilyType>(*kernel, 0));
    EXPECT_FALSE(kernel->hasUncacheableStatelessArgs());

    EXPECT_TRUE(kernel->isPatched());
    retVal = clEnqueueNDRangeKernel(pCmdQ, pMultiDeviceKernel, 1, nullptr, globalWorkSize, localWorkSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(mocsCacheable, cmdQueueMocs<FamilyType>(pCmdQ));

    retVal = clSetKernelArg(pMultiDeviceKernel, 1, sizeof(cl_mem), &bufferCacheable2);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(mocsCacheable, argMocs<FamilyType>(*kernel, 1));
    EXPECT_FALSE(kernel->hasUncacheableStatelessArgs());

    EXPECT_TRUE(kernel->isPatched());
    retVal = clEnqueueNDRangeKernel(pCmdQ, pMultiDeviceKernel, 1, nullptr, globalWorkSize, localWorkSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(mocsCacheable, cmdQueueMocs<FamilyType>(pCmdQ));
    EXPECT_FALSE(kernel->hasUncacheableStatelessArgs());
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, clMemLocallyUncachedResourceFixture, WhenUnsettingUncacheableResourceFromKernelThenKernelContinuesToCorrectlySetMocs) {
    cl_int retVal = CL_SUCCESS;
    MockKernelWithInternals mockKernel(*this->pClDevice, context, true);
    auto pMultiDeviceKernel = mockKernel.mockMultiDeviceKernel;
    auto kernel = mockKernel.mockKernel;

    EXPECT_EQ(CL_SUCCESS, retVal);

    auto bufferCacheable1 = clCreateBufferWithPropertiesINTEL(context, propertiesCacheable, 0, n * sizeof(float), nullptr, nullptr);
    auto pBufferCacheable1 = clUniquePtr(castToObject<Buffer>(bufferCacheable1));
    auto bufferCacheable2 = clCreateBufferWithPropertiesINTEL(context, propertiesCacheable, 0, n * sizeof(float), nullptr, nullptr);
    auto pBufferCacheable2 = clUniquePtr(castToObject<Buffer>(bufferCacheable2));

    auto bufferUncacheable = clCreateBufferWithPropertiesINTEL(context, propertiesUncacheable, 0, n * sizeof(float), nullptr, nullptr);
    auto pBufferUncacheable = clUniquePtr(castToObject<Buffer>(bufferUncacheable));

    auto mocsCacheable = pClDevice->getGmmHelper()->getL3EnabledMOCS();
    auto mocsUncacheable = pClDevice->getGmmHelper()->getUncachedMOCS();

    retVal = clSetKernelArg(pMultiDeviceKernel, 0, sizeof(cl_mem), &bufferCacheable1);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(mocsCacheable, argMocs<FamilyType>(*kernel, 0));

    retVal = clSetKernelArg(pMultiDeviceKernel, 1, sizeof(cl_mem), &bufferCacheable2);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(mocsCacheable, argMocs<FamilyType>(*kernel, 1));

    EXPECT_TRUE(kernel->isPatched());
    retVal = clEnqueueNDRangeKernel(pCmdQ, pMultiDeviceKernel, 1, nullptr, globalWorkSize, localWorkSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(mocsCacheable, cmdQueueMocs<FamilyType>(pCmdQ));

    retVal = clSetKernelArg(pMultiDeviceKernel, 0, sizeof(cl_mem), &bufferUncacheable);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(mocsUncacheable, argMocs<FamilyType>(*kernel, 0));

    EXPECT_TRUE(kernel->isPatched());
    retVal = clEnqueueNDRangeKernel(pCmdQ, pMultiDeviceKernel, 1, nullptr, globalWorkSize, localWorkSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(mocsUncacheable, cmdQueueMocs<FamilyType>(pCmdQ));

    kernel->unsetArg(0);

    retVal = clSetKernelArg(pMultiDeviceKernel, 0, sizeof(cl_mem), &bufferCacheable1);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(mocsCacheable, argMocs<FamilyType>(*kernel, 0));

    EXPECT_TRUE(kernel->isPatched());
    retVal = clEnqueueNDRangeKernel(pCmdQ, pMultiDeviceKernel, 1, nullptr, globalWorkSize, localWorkSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(mocsCacheable, cmdQueueMocs<FamilyType>(pCmdQ));

    kernel->unsetArg(0);

    retVal = clSetKernelArg(pMultiDeviceKernel, 0, sizeof(cl_mem), &bufferUncacheable);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(mocsUncacheable, argMocs<FamilyType>(*kernel, 0));

    EXPECT_TRUE(kernel->isPatched());
    retVal = clEnqueueNDRangeKernel(pCmdQ, pMultiDeviceKernel, 1, nullptr, globalWorkSize, localWorkSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(mocsUncacheable, cmdQueueMocs<FamilyType>(pCmdQ));
}

} // namespace clMemLocallyUncachedResourceTests
