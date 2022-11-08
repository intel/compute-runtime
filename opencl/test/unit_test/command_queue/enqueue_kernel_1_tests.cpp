/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/compiler_hw_info_config.h"
#include "shared/source/helpers/pause_on_gpu_properties.h"
#include "shared/source/helpers/preamble.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/kernel_binary_helper.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/mocks/mock_csr.h"
#include "shared/test/common/mocks/mock_submissions_aggregator.h"

#include "opencl/source/api/api.h"
#include "opencl/source/built_ins/builtins_dispatch_builder.h"
#include "opencl/test/unit_test/api/cl_api_tests.h"
#include "opencl/test/unit_test/command_queue/enqueue_fixture.h"
#include "opencl/test/unit_test/fixtures/hello_world_fixture.h"
#include "opencl/test/unit_test/helpers/cl_hw_parse.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/test_macros/test_checks_ocl.h"

using namespace NEO;

typedef HelloWorldFixture<HelloWorldFixtureFactory> EnqueueKernelFixture;
typedef Test<EnqueueKernelFixture> EnqueueKernelTest;

TEST_F(EnqueueKernelTest, GivenNullKernelWhenEnqueuingKernelThenInvalidKernelErrorIsReturned) {
    size_t globalWorkSize[3] = {1, 1, 1};
    auto retVal = clEnqueueNDRangeKernel(
        pCmdQ,
        nullptr,
        1,
        nullptr,
        globalWorkSize,
        nullptr,
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_INVALID_KERNEL, retVal);
}

TEST_F(EnqueueKernelTest, givenKernelWhenAllArgsAreSetThenClEnqueueNDRangeKernelReturnsSuccess) {
    const size_t n = 512;
    size_t globalWorkSize[3] = {n, 1, 1};
    size_t localWorkSize[3] = {64, 1, 1};
    cl_int retVal = CL_INVALID_KERNEL;
    CommandQueue *pCmdQ2 = createCommandQueue(pClDevice);

    std::unique_ptr<MultiDeviceKernel> pMultiDeviceKernel(MultiDeviceKernel::create(pProgram, pProgram->getKernelInfosForKernel("CopyBuffer"), &retVal));
    auto kernel = pMultiDeviceKernel->getKernel(rootDeviceIndex);

    EXPECT_EQ(CL_SUCCESS, retVal);

    auto b0 = clCreateBuffer(context, 0, n * sizeof(float), nullptr, nullptr);
    auto b1 = clCreateBuffer(context, 0, n * sizeof(float), nullptr, nullptr);

    EXPECT_FALSE(kernel->isPatched());
    retVal = clEnqueueNDRangeKernel(pCmdQ2, pMultiDeviceKernel.get(), 1, nullptr, globalWorkSize, localWorkSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_KERNEL_ARGS, retVal);

    retVal = clSetKernelArg(pMultiDeviceKernel.get(), 0, sizeof(cl_mem), &b0);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_FALSE(kernel->isPatched());
    retVal = clEnqueueNDRangeKernel(pCmdQ2, pMultiDeviceKernel.get(), 1, nullptr, globalWorkSize, localWorkSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_KERNEL_ARGS, retVal);

    retVal = clSetKernelArg(pMultiDeviceKernel.get(), 1, sizeof(cl_mem), &b1);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_TRUE(kernel->isPatched());
    retVal = clEnqueueNDRangeKernel(pCmdQ2, pMultiDeviceKernel.get(), 1, nullptr, globalWorkSize, localWorkSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseMemObject(b0);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clReleaseMemObject(b1);
    EXPECT_EQ(CL_SUCCESS, retVal);
    clReleaseCommandQueue(pCmdQ2);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST(EnqueueMultiDeviceKernelTest, givenMultiDeviceKernelWhenSetArgDeviceUSMThenOnlyOneKernelIsPatched) {
    REQUIRE_SVM_OR_SKIP(defaultHwInfo);
    auto deviceFactory = std::make_unique<UltClDeviceFactory>(3, 0);
    auto device0 = deviceFactory->rootDevices[0];
    auto device1 = deviceFactory->rootDevices[1];
    auto device2 = deviceFactory->rootDevices[2];

    cl_device_id devices[] = {device0, device1, device2};

    auto context = std::make_unique<MockContext>(ClDeviceVector(devices, 3), false);

    auto pCmdQ1 = context->getSpecialQueue(1u);
    auto pCmdQ2 = context->getSpecialQueue(2u);

    std::unique_ptr<char[]> pSource = nullptr;
    size_t sourceSize = 0;
    std::string testFile;

    KernelBinaryHelper kbHelper("CopyBuffer_simd16");

    testFile.append(clFiles);
    testFile.append("CopyBuffer_simd16.cl");

    pSource = loadDataFromFile(
        testFile.c_str(),
        sourceSize);

    ASSERT_NE(0u, sourceSize);
    ASSERT_NE(nullptr, pSource);

    const char *sources[1] = {pSource.get()};

    cl_int retVal = CL_INVALID_PROGRAM;

    auto clProgram = clCreateProgramWithSource(
        context.get(),
        1,
        sources,
        &sourceSize,
        &retVal);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, clProgram);

    clBuildProgram(clProgram, 0, nullptr, nullptr, nullptr, nullptr);

    auto clKernel = clCreateKernel(clProgram, "CopyBuffer", &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto pMultiDeviceKernel = castToObject<MultiDeviceKernel>(clKernel);

    auto buffer0 = clCreateBuffer(context.get(), 0, MemoryConstants::pageSize, nullptr, nullptr);
    size_t globalWorkSize[3] = {1, 1, 1};
    size_t localWorkSize[3] = {1, 1, 1};

    retVal = clSetKernelArg(clKernel, 0, sizeof(cl_mem), &buffer0);

    EXPECT_EQ(CL_SUCCESS, retVal);

    auto deviceMem = clDeviceMemAllocINTEL(context.get(), device1, {}, MemoryConstants::pageSize, MemoryConstants::pageSize, &retVal);

    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clSetKernelArgSVMPointer(clKernel, 1, deviceMem);

    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_FALSE(pMultiDeviceKernel->getKernel(0u)->isPatched());
    EXPECT_TRUE(pMultiDeviceKernel->getKernel(1u)->isPatched());
    EXPECT_FALSE(pMultiDeviceKernel->getKernel(2u)->isPatched());

    retVal = clEnqueueNDRangeKernel(pCmdQ1, pMultiDeviceKernel, 1, nullptr, globalWorkSize, localWorkSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clEnqueueNDRangeKernel(pCmdQ2, pMultiDeviceKernel, 1, nullptr, globalWorkSize, localWorkSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_KERNEL_ARGS, retVal);

    retVal = clReleaseMemObject(buffer0);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clMemFreeINTEL(context.get(), deviceMem);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clReleaseKernel(clKernel);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseProgram(clProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(EnqueueKernelTest, givenKernelWhenNotAllArgsAreSetButSetKernelArgIsCalledTwiceThenClEnqueueNDRangeKernelReturnsError) {
    const size_t n = 512;
    size_t globalWorkSize[3] = {n, 1, 1};
    size_t localWorkSize[3] = {256, 1, 1};
    cl_int retVal = CL_SUCCESS;
    CommandQueue *pCmdQ2 = createCommandQueue(pClDevice);

    std::unique_ptr<MultiDeviceKernel> pMultiDeviceKernel(MultiDeviceKernel::create(pProgram, pProgram->getKernelInfosForKernel("CopyBuffer"), &retVal));
    auto kernel = pMultiDeviceKernel->getKernel(rootDeviceIndex);

    EXPECT_EQ(CL_SUCCESS, retVal);

    auto b0 = clCreateBuffer(context, 0, n * sizeof(float), nullptr, nullptr);
    auto b1 = clCreateBuffer(context, 0, n * sizeof(float), nullptr, nullptr);

    EXPECT_FALSE(kernel->isPatched());
    retVal = clEnqueueNDRangeKernel(pCmdQ2, pMultiDeviceKernel.get(), 1, nullptr, globalWorkSize, localWorkSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_KERNEL_ARGS, retVal);

    retVal = clSetKernelArg(pMultiDeviceKernel.get(), 0, sizeof(cl_mem), &b0);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_FALSE(kernel->isPatched());
    retVal = clEnqueueNDRangeKernel(pCmdQ2, pMultiDeviceKernel.get(), 1, nullptr, globalWorkSize, localWorkSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_KERNEL_ARGS, retVal);

    retVal = clSetKernelArg(pMultiDeviceKernel.get(), 0, sizeof(cl_mem), &b1);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_FALSE(kernel->isPatched());
    retVal = clEnqueueNDRangeKernel(pCmdQ2, pMultiDeviceKernel.get(), 1, nullptr, globalWorkSize, localWorkSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_KERNEL_ARGS, retVal);

    retVal = clReleaseMemObject(b0);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clReleaseMemObject(b1);
    EXPECT_EQ(CL_SUCCESS, retVal);
    clReleaseCommandQueue(pCmdQ2);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(EnqueueKernelTest, givenKernelWhenSetKernelArgIsCalledForEachArgButAtLeastFailsThenClEnqueueNDRangeKernelReturnsError) {
    const size_t n = 512;
    size_t globalWorkSize[3] = {n, 1, 1};
    size_t localWorkSize[3] = {256, 1, 1};
    cl_int retVal = CL_SUCCESS;
    CommandQueue *pCmdQ2 = createCommandQueue(pClDevice);

    std::unique_ptr<MultiDeviceKernel> pMultiDeviceKernel(MultiDeviceKernel::create(pProgram, pProgram->getKernelInfosForKernel("CopyBuffer"), &retVal));
    auto kernel = pMultiDeviceKernel->getKernel(rootDeviceIndex);

    EXPECT_EQ(CL_SUCCESS, retVal);

    auto b0 = clCreateBuffer(context, 0, n * sizeof(float), nullptr, nullptr);
    auto b1 = clCreateBuffer(context, 0, n * sizeof(float), nullptr, nullptr);

    EXPECT_FALSE(kernel->isPatched());
    retVal = clEnqueueNDRangeKernel(pCmdQ2, pMultiDeviceKernel.get(), 1, nullptr, globalWorkSize, localWorkSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_KERNEL_ARGS, retVal);

    retVal = clSetKernelArg(pMultiDeviceKernel.get(), 0, sizeof(cl_mem), &b0);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_FALSE(kernel->isPatched());
    retVal = clEnqueueNDRangeKernel(pCmdQ2, pMultiDeviceKernel.get(), 1, nullptr, globalWorkSize, localWorkSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_KERNEL_ARGS, retVal);

    retVal = clSetKernelArg(pMultiDeviceKernel.get(), 1, 2 * sizeof(cl_mem), &b1);
    EXPECT_NE(CL_SUCCESS, retVal);

    EXPECT_FALSE(kernel->isPatched());
    retVal = clEnqueueNDRangeKernel(pCmdQ2, pMultiDeviceKernel.get(), 1, nullptr, globalWorkSize, localWorkSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_KERNEL_ARGS, retVal);

    retVal = clReleaseMemObject(b0);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clReleaseMemObject(b1);
    EXPECT_EQ(CL_SUCCESS, retVal);
    clReleaseCommandQueue(pCmdQ2);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(EnqueueKernelTest, GivenInvalidEventListCountWhenEnqueuingKernelThenInvalidEventWaitListErrorIsReturned) {
    size_t globalWorkSize[3] = {1, 1, 1};

    auto retVal = clEnqueueNDRangeKernel(
        pCmdQ,
        pMultiDeviceKernel,
        1,
        nullptr,
        globalWorkSize,
        nullptr,
        1,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_INVALID_EVENT_WAIT_LIST, retVal);
}

TEST_F(EnqueueKernelTest, GivenInvalidWorkGroupSizeWhenEnqueuingKernelThenInvalidWorkGroupSizeErrorIsReturned) {
    size_t globalWorkSize[3] = {12, 12, 12};
    size_t localWorkSize[3] = {11, 12, 12};

    auto retVal = clEnqueueNDRangeKernel(
        pCmdQ,
        pMultiDeviceKernel,
        3,
        nullptr,
        globalWorkSize,
        localWorkSize,
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_INVALID_WORK_GROUP_SIZE, retVal);
}

TEST_F(EnqueueKernelTest, GivenNullKernelWhenEnqueuingNDCountKernelINTELThenInvalidKernelErrorIsReturned) {
    size_t workgroupCount[3] = {1, 1, 1};
    auto retVal = clEnqueueNDCountKernelINTEL(
        pCmdQ,
        nullptr,
        1,
        nullptr,
        workgroupCount,
        nullptr,
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_INVALID_KERNEL, retVal);
}

using clEnqueueNDCountKernelTests = api_tests;

TEST_F(clEnqueueNDCountKernelTests, GivenQueueIncapableWhenEnqueuingNDCountKernelINTELThenInvalidOperationIsReturned) {
    auto &hwHelper = HwHelper::get(::defaultHwInfo->platform.eRenderCoreFamily);
    auto engineGroupType = hwHelper.getEngineGroupType(pCommandQueue->getGpgpuEngine().getEngineType(),
                                                       pCommandQueue->getGpgpuEngine().getEngineUsage(), *::defaultHwInfo);
    if (!hwHelper.isCooperativeDispatchSupported(engineGroupType, *::defaultHwInfo)) {
        GTEST_SKIP();
    }

    cl_uint workDim = 1;
    size_t globalWorkOffset[3] = {0, 0, 0};
    size_t workgroupCount[3] = {1, 1, 1};
    size_t localWorkSize[3] = {1, 1, 1};

    this->disableQueueCapabilities(CL_QUEUE_CAPABILITY_KERNEL_INTEL);
    retVal = clEnqueueNDCountKernelINTEL(
        pCommandQueue,
        pMultiDeviceKernel,
        workDim,
        globalWorkOffset,
        workgroupCount,
        localWorkSize,
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_INVALID_OPERATION, retVal);
}

TEST_F(EnqueueKernelTest, givenKernelWhenAllArgsAreSetThenClEnqueueNDCountKernelINTELReturnsSuccess) {
    const size_t n = 512;
    size_t workgroupCount[3] = {2, 1, 1};
    size_t localWorkSize[3] = {64, 1, 1};
    cl_int retVal = CL_SUCCESS;
    CommandQueue *pCmdQ2 = createCommandQueue(pClDevice);

    HwHelper &hwHelper = HwHelper::get(hardwareInfo.platform.eRenderCoreFamily);
    auto engineGroupType = hwHelper.getEngineGroupType(pCmdQ2->getGpgpuEngine().getEngineType(),
                                                       pCmdQ2->getGpgpuEngine().getEngineUsage(), hardwareInfo);
    if (!hwHelper.isCooperativeDispatchSupported(engineGroupType, hardwareInfo)) {
        pCmdQ2->getGpgpuEngine().osContext = pCmdQ2->getDevice().getEngine(aub_stream::ENGINE_CCS, EngineUsage::LowPriority).osContext;
    }

    std::unique_ptr<MultiDeviceKernel> pMultiDeviceKernel(MultiDeviceKernel::create(pProgram, pProgram->getKernelInfosForKernel("CopyBuffer"), &retVal));
    auto kernel = pMultiDeviceKernel->getKernel(rootDeviceIndex);

    EXPECT_EQ(CL_SUCCESS, retVal);

    auto b0 = clCreateBuffer(context, 0, n * sizeof(float), nullptr, nullptr);
    auto b1 = clCreateBuffer(context, 0, n * sizeof(float), nullptr, nullptr);

    EXPECT_FALSE(kernel->isPatched());
    retVal = clEnqueueNDCountKernelINTEL(pCmdQ2, pMultiDeviceKernel.get(), 1, nullptr, workgroupCount, localWorkSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_KERNEL_ARGS, retVal);

    retVal = clSetKernelArg(pMultiDeviceKernel.get(), 0, sizeof(cl_mem), &b0);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_FALSE(kernel->isPatched());
    retVal = clEnqueueNDCountKernelINTEL(pCmdQ2, pMultiDeviceKernel.get(), 1, nullptr, workgroupCount, localWorkSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_KERNEL_ARGS, retVal);

    retVal = clSetKernelArg(pMultiDeviceKernel.get(), 1, sizeof(cl_mem), &b1);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_TRUE(kernel->isPatched());
    retVal = clEnqueueNDCountKernelINTEL(pCmdQ2, pMultiDeviceKernel.get(), 1, nullptr, workgroupCount, localWorkSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseMemObject(b0);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clReleaseMemObject(b1);
    EXPECT_EQ(CL_SUCCESS, retVal);
    clReleaseCommandQueue(pCmdQ2);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(EnqueueKernelTest, givenKernelWhenNotAllArgsAreSetButSetKernelArgIsCalledTwiceThenClEnqueueNDCountKernelINTELReturnsError) {
    const size_t n = 512;
    size_t workgroupCount[3] = {2, 1, 1};
    size_t localWorkSize[3] = {256, 1, 1};
    cl_int retVal = CL_SUCCESS;
    CommandQueue *pCmdQ2 = createCommandQueue(pClDevice);

    HwHelper &hwHelper = HwHelper::get(hardwareInfo.platform.eRenderCoreFamily);
    auto engineGroupType = hwHelper.getEngineGroupType(pCmdQ2->getGpgpuEngine().getEngineType(),
                                                       pCmdQ2->getGpgpuEngine().getEngineUsage(), hardwareInfo);
    if (!hwHelper.isCooperativeDispatchSupported(engineGroupType, hardwareInfo)) {
        pCmdQ2->getGpgpuEngine().osContext = pCmdQ2->getDevice().getEngine(aub_stream::ENGINE_CCS, EngineUsage::LowPriority).osContext;
    }

    std::unique_ptr<MultiDeviceKernel> pMultiDeviceKernel(MultiDeviceKernel::create(pProgram, pProgram->getKernelInfosForKernel("CopyBuffer"), &retVal));
    auto kernel = pMultiDeviceKernel->getKernel(rootDeviceIndex);

    EXPECT_EQ(CL_SUCCESS, retVal);

    auto b0 = clCreateBuffer(context, 0, n * sizeof(float), nullptr, nullptr);
    auto b1 = clCreateBuffer(context, 0, n * sizeof(float), nullptr, nullptr);

    EXPECT_FALSE(kernel->isPatched());
    retVal = clEnqueueNDCountKernelINTEL(pCmdQ2, pMultiDeviceKernel.get(), 1, nullptr, workgroupCount, localWorkSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_KERNEL_ARGS, retVal);

    retVal = clSetKernelArg(pMultiDeviceKernel.get(), 0, sizeof(cl_mem), &b0);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_FALSE(kernel->isPatched());
    retVal = clEnqueueNDCountKernelINTEL(pCmdQ2, pMultiDeviceKernel.get(), 1, nullptr, workgroupCount, localWorkSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_KERNEL_ARGS, retVal);

    retVal = clSetKernelArg(pMultiDeviceKernel.get(), 0, sizeof(cl_mem), &b1);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_FALSE(kernel->isPatched());
    retVal = clEnqueueNDCountKernelINTEL(pCmdQ2, pMultiDeviceKernel.get(), 1, nullptr, workgroupCount, localWorkSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_KERNEL_ARGS, retVal);

    retVal = clReleaseMemObject(b0);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clReleaseMemObject(b1);
    EXPECT_EQ(CL_SUCCESS, retVal);
    clReleaseCommandQueue(pCmdQ2);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(EnqueueKernelTest, givenKernelWhenSetKernelArgIsCalledForEachArgButAtLeastFailsThenClEnqueueNDCountKernelINTELReturnsError) {
    const size_t n = 512;
    size_t workgroupCount[3] = {2, 1, 1};
    size_t localWorkSize[3] = {256, 1, 1};
    cl_int retVal = CL_SUCCESS;
    CommandQueue *pCmdQ2 = createCommandQueue(pClDevice);

    HwHelper &hwHelper = HwHelper::get(hardwareInfo.platform.eRenderCoreFamily);
    auto engineGroupType = hwHelper.getEngineGroupType(pCmdQ2->getGpgpuEngine().getEngineType(),
                                                       pCmdQ2->getGpgpuEngine().getEngineUsage(), hardwareInfo);
    if (!hwHelper.isCooperativeDispatchSupported(engineGroupType, hardwareInfo)) {
        pCmdQ2->getGpgpuEngine().osContext = pCmdQ2->getDevice().getEngine(aub_stream::ENGINE_CCS, EngineUsage::LowPriority).osContext;
    }

    std::unique_ptr<MultiDeviceKernel> pMultiDeviceKernel(MultiDeviceKernel::create(pProgram, pProgram->getKernelInfosForKernel("CopyBuffer"), &retVal));
    auto kernel = pMultiDeviceKernel->getKernel(rootDeviceIndex);

    EXPECT_EQ(CL_SUCCESS, retVal);

    auto b0 = clCreateBuffer(context, 0, n * sizeof(float), nullptr, nullptr);
    auto b1 = clCreateBuffer(context, 0, n * sizeof(float), nullptr, nullptr);

    EXPECT_FALSE(kernel->isPatched());
    retVal = clEnqueueNDCountKernelINTEL(pCmdQ2, pMultiDeviceKernel.get(), 1, nullptr, workgroupCount, localWorkSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_KERNEL_ARGS, retVal);

    retVal = clSetKernelArg(pMultiDeviceKernel.get(), 0, sizeof(cl_mem), &b0);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_FALSE(kernel->isPatched());
    retVal = clEnqueueNDCountKernelINTEL(pCmdQ2, pMultiDeviceKernel.get(), 1, nullptr, workgroupCount, localWorkSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_KERNEL_ARGS, retVal);

    retVal = clSetKernelArg(pMultiDeviceKernel.get(), 1, 2 * sizeof(cl_mem), &b1);
    EXPECT_NE(CL_SUCCESS, retVal);

    EXPECT_FALSE(kernel->isPatched());
    retVal = clEnqueueNDCountKernelINTEL(pCmdQ2, pMultiDeviceKernel.get(), 1, nullptr, workgroupCount, localWorkSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_KERNEL_ARGS, retVal);

    retVal = clReleaseMemObject(b0);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clReleaseMemObject(b1);
    EXPECT_EQ(CL_SUCCESS, retVal);
    clReleaseCommandQueue(pCmdQ2);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(EnqueueKernelTest, GivenInvalidEventListCountWhenEnqueuingNDCountKernelINTELThenInvalidEventWaitListErrorIsReturned) {
    size_t workgroupCount[3] = {1, 1, 1};

    auto retVal = clEnqueueNDCountKernelINTEL(
        pCmdQ,
        pMultiDeviceKernel,
        1,
        nullptr,
        workgroupCount,
        nullptr,
        1,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_INVALID_EVENT_WAIT_LIST, retVal);
}

HWTEST_F(EnqueueKernelTest, WhenEnqueingKernelThenTaskLevelIsIncremented) {
    auto taskLevelBefore = pCmdQ->taskLevel;
    callOneWorkItemNDRKernel();
    EXPECT_GT(pCmdQ->taskLevel, taskLevelBefore);
}

HWTEST_F(EnqueueKernelTest, WhenEnqueingKernelThenCsrTaskLevelIsIncremented) {
    // this test case assumes IOQ
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.taskCount = pCmdQ->taskCount + 100;
    csr.taskLevel = pCmdQ->taskLevel + 50;

    callOneWorkItemNDRKernel();
    EXPECT_EQ(pCmdQ->taskCount, csr.peekTaskCount());
    EXPECT_EQ(pCmdQ->taskLevel + 1, csr.peekTaskLevel());
}

HWTEST_F(EnqueueKernelTest, WhenEnqueingKernelThenCommandsAreAdded) {
    auto usedCmdBufferBefore = pCS->getUsed();

    callOneWorkItemNDRKernel();
    EXPECT_NE(usedCmdBufferBefore, pCS->getUsed());
}

HWTEST_F(EnqueueKernelTest, GivenGpuHangAndBlockingCallWhenEnqueingKernelThenOutOfResourcesIsReported) {
    DebugManagerStateRestore stateRestore;
    DebugManager.flags.MakeEachEnqueueBlocking.set(true);

    std::unique_ptr<ClDevice> device(new MockClDevice{MockClDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr)});
    cl_queue_properties props = {};

    MockCommandQueueHw<FamilyType> mockCommandQueueHw(context, device.get(), &props);
    mockCommandQueueHw.waitForAllEnginesReturnValue = WaitStatus::GpuHang;

    cl_uint workDim = 1;
    size_t globalWorkOffset[3] = {0, 0, 0};
    size_t globalWorkSize[3] = {1, 1, 1};
    size_t localWorkSize[3] = {1, 1, 1};

    cl_event *eventWaitList = nullptr;
    cl_int waitListSize = 0;

    const auto enqueueResult = mockCommandQueueHw.enqueueKernel(
        pKernel,
        workDim,
        globalWorkOffset,
        globalWorkSize,
        localWorkSize,
        waitListSize,
        eventWaitList,
        nullptr);

    EXPECT_EQ(CL_OUT_OF_RESOURCES, enqueueResult);
    EXPECT_EQ(1, mockCommandQueueHw.waitForAllEnginesCalledCount);
}

HWTEST_F(EnqueueKernelTest, WhenEnqueingKernelThenIndirectDataIsAdded) {
    const auto &compilerHwInfoConfig = *CompilerHwInfoConfig::get(defaultHwInfo->platform.eProductFamily);

    auto dshBefore = pDSH->getUsed();
    auto iohBefore = pIOH->getUsed();
    auto sshBefore = pSSH->getUsed();

    callOneWorkItemNDRKernel();
    EXPECT_TRUE(UnitTestHelper<FamilyType>::evaluateDshUsage(dshBefore, pDSH->getUsed(), &pKernel->getKernelInfo().kernelDescriptor, rootDeviceIndex));
    EXPECT_NE(iohBefore, pIOH->getUsed());
    if ((pKernel->usesBindfulAddressingForBuffers() || pKernel->getKernelInfo().kernelDescriptor.kernelAttributes.flags.usesImages) && compilerHwInfoConfig.isStatelessToStatefulBufferOffsetSupported()) {
        EXPECT_NE(sshBefore, pSSH->getUsed());
    }
}

TEST_F(EnqueueKernelTest, GivenKernelWithBuiltinDispatchInfoBuilderWhenBeingDispatchedThenBuiltinDispatcherIsUsedForDispatchValidation) {
    struct MockBuiltinDispatchBuilder : BuiltinDispatchInfoBuilder {
        using BuiltinDispatchInfoBuilder::BuiltinDispatchInfoBuilder;

        cl_int validateDispatch(Kernel *kernel, uint32_t inworkDim, const Vec3<size_t> &gws,
                                const Vec3<size_t> &elws, const Vec3<size_t> &offset) const override {
            receivedKernel = kernel;
            receivedWorkDim = inworkDim;
            receivedGws = gws;
            receivedElws = elws;
            receivedOffset = offset;

            wasValidateDispatchCalled = true;

            return valueToReturn;
        }

        cl_int valueToReturn = CL_SUCCESS;
        mutable Kernel *receivedKernel = nullptr;
        mutable uint32_t receivedWorkDim = 0;
        mutable Vec3<size_t> receivedGws = {0, 0, 0};
        mutable Vec3<size_t> receivedElws = {0, 0, 0};
        mutable Vec3<size_t> receivedOffset = {0, 0, 0};

        mutable bool wasValidateDispatchCalled = false;
    };

    MockBuiltinDispatchBuilder mockNuiltinDispatchBuilder(*pCmdQ->getDevice().getBuiltIns(), pCmdQ->getClDevice());

    MockKernelWithInternals mockKernel(*pClDevice);
    mockKernel.kernelInfo.builtinDispatchBuilder = &mockNuiltinDispatchBuilder;

    EXPECT_FALSE(mockNuiltinDispatchBuilder.wasValidateDispatchCalled);

    mockNuiltinDispatchBuilder.valueToReturn = CL_SUCCESS;
    size_t gws[2] = {10, 1};
    size_t lws[2] = {5, 1};
    size_t off[2] = {7, 0};
    uint32_t dim = 1;
    auto ret = pCmdQ->enqueueKernel(mockKernel.mockKernel, dim, off, gws, lws, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, ret);
    EXPECT_TRUE(mockNuiltinDispatchBuilder.wasValidateDispatchCalled);
    EXPECT_EQ(mockKernel.mockKernel, mockNuiltinDispatchBuilder.receivedKernel);
    EXPECT_EQ(gws[0], mockNuiltinDispatchBuilder.receivedGws.x);
    EXPECT_EQ(lws[0], mockNuiltinDispatchBuilder.receivedElws.x);
    EXPECT_EQ(off[0], mockNuiltinDispatchBuilder.receivedOffset.x);
    EXPECT_EQ(dim, mockNuiltinDispatchBuilder.receivedWorkDim);

    mockNuiltinDispatchBuilder.wasValidateDispatchCalled = false;
    gws[0] = 26;
    lws[0] = 13;
    off[0] = 17;
    dim = 2;
    cl_int forcedErr = 37;
    mockNuiltinDispatchBuilder.valueToReturn = forcedErr;
    ret = pCmdQ->enqueueKernel(mockKernel.mockKernel, dim, off, gws, lws, 0, nullptr, nullptr);
    EXPECT_EQ(forcedErr, ret);
    EXPECT_TRUE(mockNuiltinDispatchBuilder.wasValidateDispatchCalled);
    EXPECT_EQ(mockKernel.mockKernel, mockNuiltinDispatchBuilder.receivedKernel);
    EXPECT_EQ(gws[0], mockNuiltinDispatchBuilder.receivedGws.x);
    EXPECT_EQ(lws[0], mockNuiltinDispatchBuilder.receivedElws.x);
    EXPECT_EQ(off[0], mockNuiltinDispatchBuilder.receivedOffset.x);
    EXPECT_EQ(dim, mockNuiltinDispatchBuilder.receivedWorkDim);
}

HWCMDTEST_F(IGFX_GEN8_CORE, EnqueueKernelTest, givenSecondEnqueueWithTheSameScratchRequirementWhenPreemptionIsEnabledThenDontProgramMVSAgain) {
    typedef typename FamilyType::MEDIA_VFE_STATE MEDIA_VFE_STATE;
    pDevice->setPreemptionMode(PreemptionMode::ThreadGroup);
    auto &csr = pDevice->getGpgpuCommandStreamReceiver();
    csr.getMemoryManager()->setForce32BitAllocations(false);
    ClHardwareParse hwParser;
    size_t off[3] = {0, 0, 0};
    size_t gws[3] = {1, 1, 1};
    uint32_t scratchSize = 4096u;

    MockKernelWithInternals mockKernel(*pClDevice);
    mockKernel.kernelInfo.setPerThreadScratchSize(scratchSize, 0);

    auto sizeToProgram = PreambleHelper<FamilyType>::getScratchSizeValueToProgramMediaVfeState(scratchSize);

    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, off, gws, nullptr, 0, nullptr, nullptr);
    hwParser.parseCommands<FamilyType>(*pCmdQ);

    // All state should be programmed before walker
    auto itorCmd = find<MEDIA_VFE_STATE *>(hwParser.itorPipelineSelect, hwParser.itorWalker);
    ASSERT_NE(hwParser.itorWalker, itorCmd);

    auto *cmd = (MEDIA_VFE_STATE *)*itorCmd;

    EXPECT_EQ(sizeToProgram, cmd->getPerThreadScratchSpace());
    EXPECT_EQ(sizeToProgram, cmd->getStackSize());
    auto scratchAlloc = csr.getScratchAllocation();

    auto itorfirstBBEnd = find<typename FamilyType::MI_BATCH_BUFFER_END *>(hwParser.itorWalker, hwParser.cmdList.end());
    ASSERT_NE(hwParser.cmdList.end(), itorfirstBBEnd);

    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, off, gws, nullptr, 0, nullptr, nullptr);
    hwParser.parseCommands<FamilyType>(*pCmdQ);

    itorCmd = find<MEDIA_VFE_STATE *>(itorfirstBBEnd, hwParser.cmdList.end());
    ASSERT_EQ(hwParser.cmdList.end(), itorCmd);
    EXPECT_EQ(csr.getScratchAllocation(), scratchAlloc);
}

HWTEST_F(EnqueueKernelTest, whenEnqueueingKernelThatRequirePrivateScratchThenPrivateScratchIsSetInCommandStreamReceviver) {
    pDevice->setPreemptionMode(PreemptionMode::ThreadGroup);
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.getMemoryManager()->setForce32BitAllocations(false);
    size_t off[3] = {0, 0, 0};
    size_t gws[3] = {1, 1, 1};
    uint32_t privateScratchSize = 4096u;

    MockKernelWithInternals mockKernel(*pClDevice);
    mockKernel.kernelInfo.setPerThreadScratchSize(privateScratchSize, 1);

    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, off, gws, nullptr, 0, nullptr, nullptr);

    EXPECT_EQ(privateScratchSize, csr.requiredPrivateScratchSize);
}

HWTEST_F(EnqueueKernelTest, whenEnqueueKernelWithNoStatelessWriteWhenSbaIsBeingProgrammedThenConstPolicyIsChoosen) {
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    size_t off[3] = {0, 0, 0};
    size_t gws[3] = {1, 1, 1};

    MockKernelWithInternals mockKernel(*pClDevice);
    mockKernel.mockKernel->containsStatelessWrites = false;

    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, off, gws, nullptr, 0, nullptr, nullptr);

    EXPECT_EQ(csr.recordedDispatchFlags.l3CacheSettings, L3CachingSettings::l3AndL1On);

    auto &helper = HwHelper::get(renderCoreFamily);
    auto gmmHelper = this->pDevice->getGmmHelper();
    auto expectedMocsIndex = helper.getMocsIndex(*gmmHelper, true, true);
    EXPECT_EQ(expectedMocsIndex, csr.latestSentStatelessMocsConfig);
}

HWTEST_F(EnqueueKernelTest, whenEnqueueKernelWithNoStatelessWriteOnBlockedCodePathWhenSbaIsBeingProgrammedThenConstPolicyIsChoosen) {
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    size_t off[3] = {0, 0, 0};
    size_t gws[3] = {1, 1, 1};

    auto userEvent = clCreateUserEvent(this->context, nullptr);

    MockKernelWithInternals mockKernel(*pClDevice);
    mockKernel.mockKernel->containsStatelessWrites = false;

    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, off, gws, nullptr, 1, &userEvent, nullptr);

    clSetUserEventStatus(userEvent, 0u);

    EXPECT_EQ(csr.recordedDispatchFlags.l3CacheSettings, L3CachingSettings::l3AndL1On);

    auto &helper = HwHelper::get(renderCoreFamily);
    auto gmmHelper = this->pDevice->getGmmHelper();
    auto expectedMocsIndex = helper.getMocsIndex(*gmmHelper, true, true);
    EXPECT_EQ(expectedMocsIndex, csr.latestSentStatelessMocsConfig);

    clReleaseEvent(userEvent);
}

HWTEST_F(EnqueueKernelTest, givenEnqueueWithGlobalWorkSizeWhenZeroValueIsPassedInDimensionThenTheKernelCommandWillTriviallySucceed) {
    size_t gws[3] = {0, 0, 0};
    MockKernelWithInternals mockKernel(*pClDevice);
    auto ret = pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, ret);
}

HWTEST_F(EnqueueKernelTest, givenGpuHangAndBlockingCallAndEnqueueWithGlobalWorkSizeWhenZeroValueIsPassedInDimensionThenOutOfResourcesIsReturned) {
    DebugManagerStateRestore stateRestore;
    DebugManager.flags.MakeEachEnqueueBlocking.set(true);

    std::unique_ptr<ClDevice> device(new MockClDevice{MockClDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr)});
    cl_queue_properties props = {};

    MockCommandQueueHw<FamilyType> mockCommandQueueHw(context, device.get(), &props);
    mockCommandQueueHw.waitForAllEnginesReturnValue = WaitStatus::GpuHang;

    size_t gws[3] = {0, 0, 0};
    MockKernelWithInternals mockKernel(*pClDevice);

    const auto enqueueResult = mockCommandQueueHw.enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(CL_OUT_OF_RESOURCES, enqueueResult);
    EXPECT_EQ(1, mockCommandQueueHw.waitForAllEnginesCalledCount);
}

HWTEST_F(EnqueueKernelTest, givenCommandStreamReceiverInBatchingModeWhenEnqueueKernelIsCalledThenKernelIsRecorded) {
    auto mockCsr = new MockCsrHw2<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    mockCsr->useNewResourceImplicitFlush = false;
    mockCsr->useGpuIdleImplicitFlush = false;
    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);
    pDevice->resetCommandStreamReceiver(mockCsr);

    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    MockKernelWithInternals mockKernel(*pClDevice, context);
    size_t gws[3] = {1, 0, 0};
    auto ret = pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, ret);
    EXPECT_FALSE(mockedSubmissionsAggregator->peekCmdBufferList().peekIsEmpty());

    auto cmdBuffer = mockedSubmissionsAggregator->peekCmdBufferList().peekHead();

    // Three more surfaces from preemptionAllocation, SipKernel and clearColorAllocation
    size_t csrSurfaceCount = (pDevice->getPreemptionMode() == PreemptionMode::MidThread) ? 2 : 0;
    csrSurfaceCount -= pDevice->getHardwareInfo().capabilityTable.supportsImages ? 0 : 1;
    csrSurfaceCount += mockCsr->getKernelArgsBufferAllocation() ? 1 : 0;
    size_t timestampPacketSurfacesCount = mockCsr->peekTimestampPacketWriteEnabled() ? 1 : 0;
    size_t fenceSurfaceCount = mockCsr->globalFenceAllocation ? 1 : 0;
    size_t clearColorSize = mockCsr->clearColorAllocation ? 1 : 0;

    EXPECT_EQ(0, mockCsr->flushCalledCount);
    EXPECT_EQ(5u + csrSurfaceCount + timestampPacketSurfacesCount + fenceSurfaceCount + clearColorSize, cmdBuffer->surfaces.size());
}

HWTEST_F(EnqueueKernelTest, givenReducedAddressSpaceGraphicsAllocationForHostPtrWithL3FlushRequiredWhenEnqueueKernelIsCalledThenFlushIsCalledForReducedAddressSpacePlatforms) {
    std::unique_ptr<MockClDevice> device;
    std::unique_ptr<CommandQueue> cmdQ;
    auto hwInfoToModify = *defaultHwInfo;
    hwInfoToModify.capabilityTable.gpuAddressSpace = MemoryConstants::max36BitAddress;
    device.reset(new MockClDevice{MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfoToModify)});
    auto mockCsr = new MockCsrHw2<FamilyType>(*device->executionEnvironment, device->getRootDeviceIndex(), device->getDeviceBitfield());
    device->resetCommandStreamReceiver(mockCsr);
    auto memoryManager = mockCsr->getMemoryManager();
    uint32_t hostPtr[10]{};

    AllocationProperties properties{device->getRootDeviceIndex(), false, 1, AllocationType::EXTERNAL_HOST_PTR, false, device->getDeviceBitfield()};
    properties.flags.flushL3RequiredForRead = properties.flags.flushL3RequiredForWrite = true;
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(properties, hostPtr);
    MockKernelWithInternals mockKernel(*device, context);
    size_t gws[3] = {1, 0, 0};
    mockCsr->makeResident(*allocation);
    cmdQ.reset(createCommandQueue(device.get()));
    auto ret = cmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, ret);
    EXPECT_TRUE(mockCsr->passedDispatchFlags.dcFlush);
    memoryManager->freeGraphicsMemory(allocation);
}

HWTEST_F(EnqueueKernelTest, givenReducedAddressSpaceGraphicsAllocationForHostPtrWithL3FlushUnrequiredWhenEnqueueKernelIsCalledThenFlushIsNotForcedByGraphicsAllocation) {
    std::unique_ptr<MockClDevice> device;
    std::unique_ptr<CommandQueue> cmdQ;
    auto hwInfoToModify = *defaultHwInfo;
    hwInfoToModify.capabilityTable.gpuAddressSpace = MemoryConstants::max36BitAddress;
    device.reset(new MockClDevice{MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfoToModify)});
    auto mockCsr = new MockCsrHw2<FamilyType>(*device->executionEnvironment, device->getRootDeviceIndex(), device->getDeviceBitfield());
    device->resetCommandStreamReceiver(mockCsr);
    auto memoryManager = mockCsr->getMemoryManager();
    uint32_t hostPtr[10]{};

    AllocationProperties properties{device->getRootDeviceIndex(), false, 1, AllocationType::EXTERNAL_HOST_PTR, false, device->getDeviceBitfield()};
    properties.flags.flushL3RequiredForRead = properties.flags.flushL3RequiredForWrite = false;
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(properties, hostPtr);
    MockKernelWithInternals mockKernel(*device, context);
    size_t gws[3] = {1, 0, 0};
    mockCsr->makeResident(*allocation);
    cmdQ.reset(createCommandQueue(device.get()));
    auto ret = cmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, ret);
    EXPECT_FALSE(mockCsr->passedDispatchFlags.dcFlush);
    memoryManager->freeGraphicsMemory(allocation);
}

HWTEST_F(EnqueueKernelTest, givenFullAddressSpaceGraphicsAllocationWhenEnqueueKernelIsCalledThenFlushIsNotForcedByGraphicsAllocation) {
    HardwareInfo hwInfoToModify;
    std::unique_ptr<MockClDevice> device;
    std::unique_ptr<CommandQueue> cmdQ;
    hwInfoToModify = *defaultHwInfo;
    hwInfoToModify.capabilityTable.gpuAddressSpace = MemoryConstants::max48BitAddress;
    device.reset(new MockClDevice{MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfoToModify)});
    auto mockCsr = new MockCsrHw2<FamilyType>(*device->executionEnvironment, device->getRootDeviceIndex(), device->getDeviceBitfield());
    device->resetCommandStreamReceiver(mockCsr);
    auto memoryManager = mockCsr->getMemoryManager();
    uint32_t hostPtr[10]{};

    AllocationProperties properties{device->getRootDeviceIndex(), false, 1, AllocationType::EXTERNAL_HOST_PTR, false, device->getDeviceBitfield()};
    properties.flags.flushL3RequiredForRead = properties.flags.flushL3RequiredForWrite = false;
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(properties, hostPtr);
    MockKernelWithInternals mockKernel(*device, context);
    size_t gws[3] = {1, 0, 0};
    mockCsr->makeResident(*allocation);
    cmdQ.reset(createCommandQueue(device.get()));
    auto ret = cmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, ret);
    EXPECT_FALSE(mockCsr->passedDispatchFlags.dcFlush);
    memoryManager->freeGraphicsMemory(allocation);

    properties.flags.flushL3RequiredForRead = properties.flags.flushL3RequiredForWrite = false;
    allocation = memoryManager->allocateGraphicsMemoryWithProperties(properties, hostPtr);
    mockCsr->makeResident(*allocation);
    cmdQ.reset(createCommandQueue(device.get()));
    ret = cmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, ret);
    EXPECT_FALSE(mockCsr->passedDispatchFlags.dcFlush);
    memoryManager->freeGraphicsMemory(allocation);
}

HWTEST_F(EnqueueKernelTest, givenDefaultCommandStreamReceiverWhenClFlushIsCalledThenSuccessIsReturned) {
    MockKernelWithInternals mockKernel(*pClDevice);
    size_t gws[3] = {1, 0, 0};
    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    auto ret = clFlush(pCmdQ);
    EXPECT_EQ(CL_SUCCESS, ret);
}

HWTEST_F(EnqueueKernelTest, givenCommandStreamReceiverInBatchingModeAndBatchedKernelWhenFlushIsCalledThenKernelIsSubmitted) {
    auto mockCsrmockCsr = new MockCsrHw2<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    mockCsrmockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);
    mockCsrmockCsr->useNewResourceImplicitFlush = false;
    mockCsrmockCsr->useGpuIdleImplicitFlush = false;
    pDevice->resetCommandStreamReceiver(mockCsrmockCsr);

    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    mockCsrmockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    MockKernelWithInternals mockKernel(*pClDevice);
    size_t gws[3] = {1, 0, 0};
    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    EXPECT_FALSE(mockedSubmissionsAggregator->peekCmdBufferList().peekIsEmpty());

    auto ret = clFlush(pCmdQ);
    EXPECT_EQ(CL_SUCCESS, ret);

    EXPECT_TRUE(mockedSubmissionsAggregator->peekCmdBufferList().peekIsEmpty());
    EXPECT_EQ(1, mockCsrmockCsr->flushCalledCount);
}

HWTEST_F(EnqueueKernelTest, givenCommandStreamReceiverInBatchingModeAndBatchedKernelWhenFlushIsCalledTwiceThenNothingChanges) {
    auto mockCsrmockCsr = new MockCsrHw2<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    mockCsrmockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);
    pDevice->resetCommandStreamReceiver(mockCsrmockCsr);

    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    mockCsrmockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    MockKernelWithInternals mockKernel(*pClDevice);
    size_t gws[3] = {1, 0, 0};
    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    auto ret = clFlush(pCmdQ);
    EXPECT_EQ(CL_SUCCESS, ret);

    ret = clFlush(pCmdQ);
    EXPECT_EQ(CL_SUCCESS, ret);

    EXPECT_TRUE(mockedSubmissionsAggregator->peekCmdBufferList().peekIsEmpty());
    EXPECT_EQ(1, mockCsrmockCsr->flushCalledCount);
}

HWTEST_F(EnqueueKernelTest, givenCommandStreamReceiverInBatchingModeWhenKernelIsEnqueuedTwiceThenTwoSubmissionsAreRecorded) {
    auto &mockCsrmockCsr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    mockCsrmockCsr.overrideDispatchPolicy(DispatchMode::BatchedDispatch);
    mockCsrmockCsr.useNewResourceImplicitFlush = false;
    mockCsrmockCsr.useGpuIdleImplicitFlush = false;
    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    mockCsrmockCsr.submissionAggregator.reset(mockedSubmissionsAggregator);

    MockKernelWithInternals mockKernel(*pClDevice, context);
    size_t gws[3] = {1, 0, 0};
    // make sure csr emits something
    mockCsrmockCsr.mediaVfeStateDirty = true;
    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    mockCsrmockCsr.mediaVfeStateDirty = true;
    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    EXPECT_FALSE(mockedSubmissionsAggregator->peekCmdBufferList().peekIsEmpty());
    auto &cmdBufferList = mockedSubmissionsAggregator->peekCmdBufferList();
    EXPECT_NE(nullptr, cmdBufferList.peekHead());
    EXPECT_NE(cmdBufferList.peekTail(), cmdBufferList.peekHead());

    auto cmdBuffer1 = cmdBufferList.peekHead();
    auto cmdBuffer2 = cmdBufferList.peekTail();

    EXPECT_EQ(cmdBuffer1->surfaces.size(), cmdBuffer2->surfaces.size());
    EXPECT_EQ(cmdBuffer1->batchBuffer.commandBufferAllocation, cmdBuffer2->batchBuffer.commandBufferAllocation);
    EXPECT_GT(cmdBuffer2->batchBuffer.startOffset, cmdBuffer1->batchBuffer.startOffset);
}

HWTEST_F(EnqueueKernelTest, givenCommandStreamReceiverInBatchingModeWhenFlushIsCalledOnTwoBatchedKernelsThenTheyAreExecutedInOrder) {
    auto mockCsr = new MockCsrHw2<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);
    mockCsr->useNewResourceImplicitFlush = false;
    mockCsr->useGpuIdleImplicitFlush = false;
    pDevice->resetCommandStreamReceiver(mockCsr);

    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    MockKernelWithInternals mockKernel(*pClDevice);
    size_t gws[3] = {1, 0, 0};
    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    pCmdQ->flush();

    EXPECT_TRUE(mockedSubmissionsAggregator->peekCmdBufferList().peekIsEmpty());
    EXPECT_EQ(1, mockCsr->flushCalledCount);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, EnqueueKernelTest, givenTwoEnqueueProgrammedWithinSameCommandBufferWhenBatchedThenNoBBSBetweenThem) {
    auto mockCsr = new MockCsrHw2<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);
    mockCsr->useNewResourceImplicitFlush = false;
    mockCsr->useGpuIdleImplicitFlush = false;
    pDevice->resetCommandStreamReceiver(mockCsr);

    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    ClHardwareParse hwParse;

    MockKernelWithInternals mockKernel(*pClDevice);
    size_t gws[3] = {1, 0, 0};
    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    pCmdQ->flush();

    hwParse.parseCommands<FamilyType>(*pCmdQ);
    auto bbsCommands = findAll<typename FamilyType::MI_BATCH_BUFFER_START *>(hwParse.cmdList.begin(), hwParse.cmdList.end());

    EXPECT_EQ(bbsCommands.size(), 1u);
}

HWTEST_F(EnqueueKernelTest, givenCsrInBatchingModeWhenFinishIsCalledThenBatchesSubmissionsAreFlushed) {
    auto mockCsr = new MockCsrHw2<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);
    mockCsr->useNewResourceImplicitFlush = false;
    mockCsr->useGpuIdleImplicitFlush = false;
    pDevice->resetCommandStreamReceiver(mockCsr);

    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    MockKernelWithInternals mockKernel(*pClDevice);
    size_t gws[3] = {1, 0, 0};
    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    pCmdQ->finish();

    EXPECT_TRUE(mockedSubmissionsAggregator->peekCmdBufferList().peekIsEmpty());
    EXPECT_EQ(1, mockCsr->flushCalledCount);
}

HWTEST_F(EnqueueKernelTest, givenCsrInBatchingModeWhenThressEnqueueKernelsAreCalledThenBatchesSubmissionsAreFlushed) {
    auto mockCsr = new MockCsrHw2<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);
    mockCsr->useNewResourceImplicitFlush = false;
    mockCsr->useGpuIdleImplicitFlush = false;
    pDevice->resetCommandStreamReceiver(mockCsr);

    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    MockKernelWithInternals mockKernel(*pClDevice);
    size_t gws[3] = {1, 0, 0};
    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    pCmdQ->finish();

    EXPECT_TRUE(mockedSubmissionsAggregator->peekCmdBufferList().peekIsEmpty());
    EXPECT_EQ(1, mockCsr->flushCalledCount);
}

HWTEST_F(EnqueueKernelTest, givenCsrInBatchingModeWhenWaitForEventsIsCalledThenBatchedSubmissionsAreFlushed) {
    auto mockCsr = new MockCsrHw2<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);
    mockCsr->useNewResourceImplicitFlush = false;
    mockCsr->useGpuIdleImplicitFlush = false;
    pDevice->resetCommandStreamReceiver(mockCsr);

    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    MockKernelWithInternals mockKernel(*pClDevice);
    size_t gws[3] = {1, 0, 0};
    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    cl_event event;
    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, &event);

    auto status = clWaitForEvents(1, &event);

    EXPECT_EQ(CL_SUCCESS, status);
    EXPECT_TRUE(mockedSubmissionsAggregator->peekCmdBufferList().peekIsEmpty());
    EXPECT_EQ(1, mockCsr->flushCalledCount);

    status = clReleaseEvent(event);
    EXPECT_EQ(CL_SUCCESS, status);
}

HWTEST_F(EnqueueKernelTest, givenCsrInBatchingModeWhenCommandIsFlushedThenFlushStampIsUpdatedInCommandQueueCsrAndEvent) {
    auto mockCsr = new MockCsrHw2<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);
    mockCsr->useNewResourceImplicitFlush = false;
    mockCsr->useGpuIdleImplicitFlush = false;
    pDevice->resetCommandStreamReceiver(mockCsr);

    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    MockKernelWithInternals mockKernel(*pClDevice);
    size_t gws[3] = {1, 0, 0};
    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    cl_event event;
    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, &event);
    auto neoEvent = castToObject<Event>(event);

    EXPECT_EQ(0u, mockCsr->flushStamp->peekStamp());
    EXPECT_EQ(0u, neoEvent->flushStamp->peekStamp());
    EXPECT_EQ(0u, pCmdQ->flushStamp->peekStamp());

    auto status = clWaitForEvents(1, &event);

    EXPECT_EQ(CL_SUCCESS, status);
    EXPECT_EQ(1, neoEvent->getRefInternalCount());
    EXPECT_EQ(1u, mockCsr->flushStamp->peekStamp());
    EXPECT_EQ(1u, neoEvent->flushStamp->peekStamp());
    EXPECT_EQ(1u, pCmdQ->flushStamp->peekStamp());

    status = clFinish(pCmdQ);
    EXPECT_EQ(CL_SUCCESS, status);
    EXPECT_EQ(1u, pCmdQ->flushStamp->peekStamp());

    status = clReleaseEvent(event);
    EXPECT_EQ(CL_SUCCESS, status);
}

HWTEST_F(EnqueueKernelTest, givenCsrInBatchingModeWhenNonBlockingMapFollowsNdrCallThenFlushStampIsUpdatedProperly) {
    auto mockCsr = new MockCsrHw2<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);
    pDevice->resetCommandStreamReceiver(mockCsr);

    EXPECT_TRUE(this->destBuffer->isMemObjZeroCopy());
    MockKernelWithInternals mockKernel(*pClDevice);
    size_t gws[3] = {1, 0, 0};
    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    cl_event event;
    pCmdQ->enqueueMapBuffer(this->destBuffer, false, CL_MAP_READ, 0u, 1u, 0, nullptr, &event, this->retVal);

    pCmdQ->flush();
    auto neoEvent = castToObject<Event>(event);
    EXPECT_EQ(1u, neoEvent->flushStamp->peekStamp());
    clReleaseEvent(event);
}

HWTEST_F(EnqueueKernelTest, givenCsrInBatchingModeWhenCommandWithEventIsFollowedByCommandWithoutEventThenFlushStampIsUpdatedInCommandQueueCsrAndEvent) {
    auto mockCsr = new MockCsrHw2<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);
    mockCsr->useNewResourceImplicitFlush = false;
    mockCsr->useGpuIdleImplicitFlush = false;
    pDevice->resetCommandStreamReceiver(mockCsr);

    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    MockKernelWithInternals mockKernel(*pClDevice);
    size_t gws[3] = {1, 0, 0};
    cl_event event;
    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, &event);
    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    auto neoEvent = castToObject<Event>(event);

    EXPECT_EQ(0u, mockCsr->flushStamp->peekStamp());
    EXPECT_EQ(0u, neoEvent->flushStamp->peekStamp());
    EXPECT_EQ(0u, pCmdQ->flushStamp->peekStamp());

    auto status = clWaitForEvents(1, &event);

    EXPECT_EQ(CL_SUCCESS, status);
    EXPECT_EQ(1, neoEvent->getRefInternalCount());
    EXPECT_EQ(1u, mockCsr->flushStamp->peekStamp());
    EXPECT_EQ(1u, neoEvent->flushStamp->peekStamp());
    EXPECT_EQ(1u, pCmdQ->flushStamp->peekStamp());

    status = clFinish(pCmdQ);
    EXPECT_EQ(CL_SUCCESS, status);
    EXPECT_EQ(1u, pCmdQ->flushStamp->peekStamp());

    status = clReleaseEvent(event);
    EXPECT_EQ(CL_SUCCESS, status);
}

HWTEST_F(EnqueueKernelTest, givenCsrInBatchingModeWhenClFlushIsCalledThenQueueFlushStampIsUpdated) {
    auto mockCsr = new MockCsrHw2<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);
    mockCsr->useNewResourceImplicitFlush = false;
    mockCsr->useGpuIdleImplicitFlush = false;
    pDevice->resetCommandStreamReceiver(mockCsr);

    MockKernelWithInternals mockKernel(*pClDevice);
    size_t gws[3] = {1, 0, 0};
    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    EXPECT_EQ(0u, mockCsr->flushStamp->peekStamp());
    EXPECT_EQ(0u, pCmdQ->flushStamp->peekStamp());

    clFlush(pCmdQ);
    EXPECT_EQ(1u, mockCsr->flushStamp->peekStamp());
    EXPECT_EQ(1u, pCmdQ->flushStamp->peekStamp());
}

HWTEST_F(EnqueueKernelTest, givenCsrInBatchingModeWhenWaitForEventsIsCalledWithUnflushedTaskCountThenBatchedSubmissionsAreFlushed) {
    auto mockCsr = new MockCsrHw2<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);
    pDevice->resetCommandStreamReceiver(mockCsr);

    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    MockKernelWithInternals mockKernel(*pClDevice);
    size_t gws[3] = {1, 0, 0};
    cl_event event;
    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    pCmdQ->enqueueMarkerWithWaitList(0, nullptr, &event);

    auto status = clWaitForEvents(1, &event);

    EXPECT_EQ(CL_SUCCESS, status);
    EXPECT_TRUE(mockedSubmissionsAggregator->peekCmdBufferList().peekIsEmpty());
    EXPECT_EQ(1, mockCsr->flushCalledCount);

    status = clReleaseEvent(event);
    EXPECT_EQ(CL_SUCCESS, status);
}

HWTEST_F(EnqueueKernelTest, givenCsrInBatchingModeWhenFinishIsCalledWithUnflushedTaskCountThenBatchedSubmissionsAreFlushed) {
    auto mockCsr = new MockCsrHw2<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);
    pDevice->resetCommandStreamReceiver(mockCsr);

    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    MockKernelWithInternals mockKernel(*pClDevice);
    size_t gws[3] = {1, 0, 0};
    cl_event event;
    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    pCmdQ->enqueueMarkerWithWaitList(0, nullptr, &event);

    auto status = clFinish(pCmdQ);

    EXPECT_EQ(CL_SUCCESS, status);
    EXPECT_TRUE(mockedSubmissionsAggregator->peekCmdBufferList().peekIsEmpty());
    EXPECT_EQ(1, mockCsr->flushCalledCount);

    status = clReleaseEvent(event);
    EXPECT_EQ(CL_SUCCESS, status);
}

HWTEST_F(EnqueueKernelTest, givenOutOfOrderCommandQueueWhenEnqueueKernelIsMadeThenPipeControlPositionIsRecorded) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, 0};
    auto ooq = clCreateCommandQueueWithProperties(context, pClDevice, props, nullptr);

    auto mockCsr = new MockCsrHw2<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);
    mockCsr->useNewResourceImplicitFlush = false;
    mockCsr->useGpuIdleImplicitFlush = false;
    pDevice->resetCommandStreamReceiver(mockCsr);

    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    MockKernelWithInternals mockKernel(*pClDevice, context);
    size_t gws[3] = {1, 0, 0};
    clEnqueueNDRangeKernel(ooq, mockKernel.mockMultiDeviceKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    EXPECT_FALSE(mockedSubmissionsAggregator->peekCmdBufferList().peekIsEmpty());
    auto cmdBuffer = mockedSubmissionsAggregator->peekCmdBufferList().peekHead();
    EXPECT_NE(nullptr, cmdBuffer->pipeControlThatMayBeErasedLocation);

    clReleaseCommandQueue(ooq);
}

HWTEST_F(EnqueueKernelTest, givenInOrderCommandQueueWhenEnqueueKernelIsMadeThenPipeControlPositionIsRecorded) {
    const cl_queue_properties props[] = {0};
    auto inOrderQueue = clCreateCommandQueueWithProperties(context, pClDevice, props, nullptr);

    auto &mockCsr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    mockCsr.overrideDispatchPolicy(DispatchMode::BatchedDispatch);
    mockCsr.useNewResourceImplicitFlush = false;
    mockCsr.useGpuIdleImplicitFlush = false;
    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    mockCsr.submissionAggregator.reset(mockedSubmissionsAggregator);

    MockKernelWithInternals mockKernel(*pClDevice, context);
    size_t gws[3] = {1, 0, 0};
    clEnqueueNDRangeKernel(inOrderQueue, mockKernel.mockMultiDeviceKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    EXPECT_FALSE(mockedSubmissionsAggregator->peekCmdBufferList().peekIsEmpty());
    auto cmdBuffer = mockedSubmissionsAggregator->peekCmdBufferList().peekHead();
    EXPECT_NE(nullptr, cmdBuffer->pipeControlThatMayBeErasedLocation);

    clReleaseCommandQueue(inOrderQueue);
}

HWTEST_F(EnqueueKernelTest, givenInOrderCommandQueueWhenEnqueueKernelThatHasSharedObjectsAsArgIsMadeThenPipeControlPositionIsRecorded) {
    const cl_queue_properties props[] = {0};
    auto inOrderQueue = clCreateCommandQueueWithProperties(context, pClDevice, props, nullptr);

    auto mockCsr = new MockCsrHw2<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);
    mockCsr->useNewResourceImplicitFlush = false;
    mockCsr->useGpuIdleImplicitFlush = false;
    pDevice->resetCommandStreamReceiver(mockCsr);

    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    MockKernelWithInternals mockKernel(*pClDevice, context);
    size_t gws[3] = {1, 0, 0};
    mockKernel.mockKernel->setUsingSharedArgs(true);
    clEnqueueNDRangeKernel(inOrderQueue, mockKernel.mockMultiDeviceKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    EXPECT_FALSE(mockedSubmissionsAggregator->peekCmdBufferList().peekIsEmpty());
    auto cmdBuffer = mockedSubmissionsAggregator->peekCmdBufferList().peekHead();
    EXPECT_NE(nullptr, cmdBuffer->pipeControlThatMayBeErasedLocation);
    EXPECT_NE(nullptr, cmdBuffer->epiloguePipeControlLocation);

    clReleaseCommandQueue(inOrderQueue);
}

HWTEST_F(EnqueueKernelTest, givenInOrderCommandQueueWhenEnqueueKernelThatHasSharedObjectsAsArgIsMadeThenPipeControlDoesntHaveDcFlush) {
    auto mockCsr = new MockCsrHw2<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);
    pDevice->resetCommandStreamReceiver(mockCsr);

    MockKernelWithInternals mockKernel(*pClDevice, context);
    size_t gws[3] = {1, 0, 0};
    mockKernel.mockKernel->setUsingSharedArgs(true);
    clEnqueueNDRangeKernel(this->pCmdQ, mockKernel.mockMultiDeviceKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    EXPECT_FALSE(mockCsr->passedDispatchFlags.dcFlush);
}

HWTEST_F(EnqueueKernelTest, givenInOrderCommandQueueWhenEnqueueKernelReturningEventIsMadeThenPipeControlPositionIsNotRecorded) {
    const cl_queue_properties props[] = {0};
    auto inOrderQueue = clCreateCommandQueueWithProperties(context, pClDevice, props, nullptr);

    auto mockCsr = new MockCsrHw2<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);
    mockCsr->useNewResourceImplicitFlush = false;
    mockCsr->useGpuIdleImplicitFlush = false;
    mockCsr->timestampPacketWriteEnabled = false;
    pDevice->resetCommandStreamReceiver(mockCsr);

    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    MockKernelWithInternals mockKernel(*pClDevice, context);
    size_t gws[3] = {1, 0, 0};
    cl_event event;

    clEnqueueNDRangeKernel(inOrderQueue, mockKernel.mockMultiDeviceKernel, 1, nullptr, gws, nullptr, 0, nullptr, &event);

    EXPECT_FALSE(mockedSubmissionsAggregator->peekCmdBufferList().peekIsEmpty());
    auto cmdBuffer = mockedSubmissionsAggregator->peekCmdBufferList().peekHead();
    EXPECT_EQ(nullptr, cmdBuffer->pipeControlThatMayBeErasedLocation);
    EXPECT_NE(nullptr, cmdBuffer->epiloguePipeControlLocation);

    clReleaseCommandQueue(inOrderQueue);
    clReleaseEvent(event);
}

HWTEST_F(EnqueueKernelTest, givenInOrderCommandQueueWhenEnqueueKernelReturningEventIsMadeAndCommandStreamReceiverIsInNTo1ModeThenPipeControlPositionIsRecorded) {
    const cl_queue_properties props[] = {0};
    auto inOrderQueue = clCreateCommandQueueWithProperties(context, pClDevice, props, nullptr);

    auto mockCsr = new MockCsrHw2<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);
    mockCsr->useNewResourceImplicitFlush = false;
    mockCsr->useGpuIdleImplicitFlush = false;
    pDevice->resetCommandStreamReceiver(mockCsr);
    mockCsr->enableNTo1SubmissionModel();

    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    MockKernelWithInternals mockKernel(*pClDevice, context);
    size_t gws[3] = {1, 0, 0};
    cl_event event;

    clEnqueueNDRangeKernel(inOrderQueue, mockKernel.mockMultiDeviceKernel, 1, nullptr, gws, nullptr, 0, nullptr, &event);

    EXPECT_FALSE(mockedSubmissionsAggregator->peekCmdBufferList().peekIsEmpty());
    auto cmdBuffer = mockedSubmissionsAggregator->peekCmdBufferList().peekHead();
    EXPECT_NE(nullptr, cmdBuffer->pipeControlThatMayBeErasedLocation);
    EXPECT_NE(nullptr, cmdBuffer->epiloguePipeControlLocation);

    clReleaseCommandQueue(inOrderQueue);
    clReleaseEvent(event);
}

HWTEST_F(EnqueueKernelTest, givenOutOfOrderCommandQueueWhenEnqueueKernelReturningEventIsMadeThenPipeControlPositionIsRecorded) {
    auto mockCsr = new MockCsrHw2<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);
    mockCsr->useNewResourceImplicitFlush = false;
    mockCsr->useGpuIdleImplicitFlush = false;
    pDevice->resetCommandStreamReceiver(mockCsr);

    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, 0};
    auto inOrderQueue = clCreateCommandQueueWithProperties(context, pClDevice, props, nullptr);

    MockKernelWithInternals mockKernel(*pClDevice, context);
    size_t gws[3] = {1, 0, 0};
    cl_event event;

    clEnqueueNDRangeKernel(inOrderQueue, mockKernel.mockMultiDeviceKernel, 1, nullptr, gws, nullptr, 0, nullptr, &event);

    EXPECT_FALSE(mockedSubmissionsAggregator->peekCmdBufferList().peekIsEmpty());
    auto cmdBuffer = mockedSubmissionsAggregator->peekCmdBufferList().peekHead();
    EXPECT_NE(nullptr, cmdBuffer->pipeControlThatMayBeErasedLocation);
    EXPECT_EQ(cmdBuffer->epiloguePipeControlLocation, cmdBuffer->pipeControlThatMayBeErasedLocation);

    clReleaseCommandQueue(inOrderQueue);
    clReleaseEvent(event);
}

HWTEST_F(EnqueueKernelTest, givenCsrInBatchingModeWhenBlockingCallIsMadeThenEventAssociatedWithCommandHasProperFlushStamp) {
    DebugManagerStateRestore stateRestore;
    DebugManager.flags.MakeEachEnqueueBlocking.set(true);
    auto mockCsr = new MockCsrHw2<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);
    pDevice->resetCommandStreamReceiver(mockCsr);

    MockKernelWithInternals mockKernel(*pClDevice);
    size_t gws[3] = {1, 0, 0};
    cl_event event;
    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, &event);
    auto neoEvent = castToObject<Event>(event);
    EXPECT_EQ(1u, neoEvent->flushStamp->peekStamp());
    EXPECT_EQ(1, mockCsr->flushCalledCount);

    auto status = clReleaseEvent(event);
    EXPECT_EQ(CL_SUCCESS, status);
}

HWTEST_F(EnqueueKernelTest, givenKernelWhenItIsEnqueuedThenAllResourceGraphicsAllocationsAreUpdatedWithCsrTaskCount) {
    auto mockCsr = new MockCsrHw2<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    pDevice->resetCommandStreamReceiver(mockCsr);

    MockKernelWithInternals mockKernel(*pClDevice);
    size_t gws[3] = {1, 0, 0};
    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    EXPECT_EQ(1, mockCsr->flushCalledCount);

    auto csrTaskCount = mockCsr->peekTaskCount();
    auto &passedAllocationPack = mockCsr->copyOfAllocations;
    for (auto &allocation : passedAllocationPack) {
        EXPECT_EQ(csrTaskCount, allocation->getTaskCount(mockCsr->getOsContext().getContextId()));
    }
}

HWTEST_F(EnqueueKernelTest, givenKernelWhenItIsSubmittedFromTwoDifferentCommandQueuesThenCsrDoesntReloadAnyCommands) {
    auto &csr = this->pDevice->getUltCommandStreamReceiver<FamilyType>();
    MockKernelWithInternals mockKernel(*pClDevice);
    size_t gws[3] = {1, 0, 0};
    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    auto currentUsed = csr.commandStream.getUsed();

    const cl_queue_properties props[] = {0};
    auto inOrderQueue = clCreateCommandQueueWithProperties(context, pClDevice, props, nullptr);
    clEnqueueNDRangeKernel(inOrderQueue, mockKernel.mockMultiDeviceKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    auto usedAfterSubmission = csr.commandStream.getUsed();

    EXPECT_EQ(usedAfterSubmission, currentUsed);
    clReleaseCommandQueue(inOrderQueue);
}

TEST_F(EnqueueKernelTest, givenKernelWhenAllArgsAreNotAndEventExistSetThenClEnqueueNDRangeKernelReturnsInvalidKernelArgsAndSetEventToNull) {
    const size_t n = 512;
    size_t globalWorkSize[3] = {n, 1, 1};
    size_t localWorkSize[3] = {256, 1, 1};
    cl_int retVal = CL_SUCCESS;
    CommandQueue *pCmdQ2 = createCommandQueue(pClDevice);

    std::unique_ptr<MultiDeviceKernel> pMultiDeviceKernel(MultiDeviceKernel::create(pProgram, pProgram->getKernelInfosForKernel("CopyBuffer"), &retVal));
    auto kernel = pMultiDeviceKernel->getKernel(rootDeviceIndex);

    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_FALSE(kernel->isPatched());
    cl_event event;
    retVal = clEnqueueNDRangeKernel(pCmdQ2, pMultiDeviceKernel.get(), 1, nullptr, globalWorkSize, localWorkSize, 0, nullptr, &event);
    EXPECT_EQ(CL_INVALID_KERNEL_ARGS, retVal);

    clFlush(pCmdQ2);
    clReleaseCommandQueue(pCmdQ2);
}

TEST_F(EnqueueKernelTest, givenEnqueueCommandThatLwsExceedsDeviceCapabilitiesWhenEnqueueNDRangeKernelIsCalledThenErrorIsReturned) {
    MockKernelWithInternals mockKernel(*pClDevice);

    mockKernel.mockKernel->maxKernelWorkGroupSize = static_cast<uint32_t>(pDevice->getDeviceInfo().maxWorkGroupSize / 2);

    auto maxKernelWorkgroupSize = mockKernel.mockKernel->maxKernelWorkGroupSize;
    size_t globalWorkSize[3] = {maxKernelWorkgroupSize + 1, 1, 1};
    size_t localWorkSize[3] = {maxKernelWorkgroupSize + 1, 1, 1};

    auto status = pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, globalWorkSize, localWorkSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_WORK_GROUP_SIZE, status);
}

TEST_F(EnqueueKernelTest, givenEnqueueCommandThatLocalWorkgroupSizeContainsZeroWhenEnqueueNDRangeKernelIsCalledThenClInvalidWorkGroupSizeIsReturned) {
    size_t globalWorkSize[3] = {1, 1, 1};
    size_t localWorkSize[3] = {1, 0, 1};
    MockKernelWithInternals mockKernel(*pClDevice);

    auto status = pCmdQ->enqueueKernel(mockKernel.mockKernel, 3, nullptr, globalWorkSize, localWorkSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_WORK_GROUP_SIZE, status);
}

TEST_F(EnqueueKernelTest, givenEnqueueCommandWithWorkDimLargerThanAllowedWhenEnqueueNDRangeKernelIsCalledThenClInvalidWorkDimensionIsReturned) {
    size_t globalWorkSize[3] = {1, 1, 1};
    size_t localWorkSize[3] = {1, 1, 1};
    MockKernelWithInternals mockKernel(*pClDevice);
    auto testedWorkDim = pClDevice->deviceInfo.maxWorkItemDimensions;
    auto status = clEnqueueNDRangeKernel(pCmdQ, mockKernel.mockMultiDeviceKernel, testedWorkDim, nullptr, globalWorkSize, localWorkSize, 0, nullptr, nullptr);
    ASSERT_EQ(CL_SUCCESS, status);

    testedWorkDim += 1;
    status = clEnqueueNDRangeKernel(pCmdQ, mockKernel.mockMultiDeviceKernel, testedWorkDim, nullptr, globalWorkSize, localWorkSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_WORK_DIMENSION, status);
}

TEST_F(EnqueueKernelTest, givenEnqueueCommandWithWorkDimsResultingInMoreThan32BitMaxGroupsWhenEnqueueNDRangeKernelIsCalledThenInvalidGlobalSizeIsReturned) {

    if (sizeof(size_t) < 8) {
        GTEST_SKIP();
    }

    size_t max32Bit = std::numeric_limits<uint32_t>::max();
    size_t globalWorkSize[3] = {max32Bit * 4, 4, 4};
    size_t localWorkSize[3] = {4, 4, 4};
    MockKernelWithInternals mockKernel(*pClDevice);
    auto testedWorkDim = 3;

    auto status = clEnqueueNDRangeKernel(pCmdQ, mockKernel.mockMultiDeviceKernel, testedWorkDim, nullptr, globalWorkSize, localWorkSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, status);

    globalWorkSize[0] = max32Bit * 4 + 4;
    status = clEnqueueNDRangeKernel(pCmdQ, mockKernel.mockMultiDeviceKernel, testedWorkDim, nullptr, globalWorkSize, localWorkSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_GLOBAL_WORK_SIZE, status);

    globalWorkSize[0] = 4;
    globalWorkSize[1] = max32Bit * 4 + 4;

    status = clEnqueueNDRangeKernel(pCmdQ, mockKernel.mockMultiDeviceKernel, testedWorkDim, nullptr, globalWorkSize, localWorkSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_GLOBAL_WORK_SIZE, status);

    globalWorkSize[1] = 4;
    globalWorkSize[2] = max32Bit * 4 + 4;

    status = clEnqueueNDRangeKernel(pCmdQ, mockKernel.mockMultiDeviceKernel, testedWorkDim, nullptr, globalWorkSize, localWorkSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_GLOBAL_WORK_SIZE, status);
}

TEST_F(EnqueueKernelTest, givenEnqueueCommandWithNullLwsAndWorkDimsResultingInMoreThan32BitMaxGroupsWhenEnqueueNDRangeKernelIsCalledThenInvalidGlobalSizeIsReturned) {

    if (sizeof(size_t) < 8) {
        GTEST_SKIP();
    }

    auto maxWgSize = static_cast<uint32_t>(pClDevice->getDevice().getDeviceInfo().maxWorkGroupSize);

    size_t max32Bit = std::numeric_limits<uint32_t>::max();
    size_t globalWorkSize[3] = {(max32Bit + 1) * maxWgSize, 3, 4};
    MockKernelWithInternals mockKernel(*pClDevice);
    auto testedWorkDim = 3;

    auto status = clEnqueueNDRangeKernel(pCmdQ, mockKernel.mockMultiDeviceKernel, testedWorkDim, nullptr, globalWorkSize, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_GLOBAL_WORK_SIZE, status);

    globalWorkSize[0] = (max32Bit + 1) * maxWgSize + 3;
    status = clEnqueueNDRangeKernel(pCmdQ, mockKernel.mockMultiDeviceKernel, testedWorkDim, nullptr, globalWorkSize, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_GLOBAL_WORK_SIZE, status);

    globalWorkSize[0] = 4;
    globalWorkSize[1] = (max32Bit + 1) * maxWgSize;

    status = clEnqueueNDRangeKernel(pCmdQ, mockKernel.mockMultiDeviceKernel, testedWorkDim, nullptr, globalWorkSize, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_GLOBAL_WORK_SIZE, status);

    globalWorkSize[1] = 4;
    globalWorkSize[2] = (max32Bit + 1) * maxWgSize * 2 + 3;

    status = clEnqueueNDRangeKernel(pCmdQ, mockKernel.mockMultiDeviceKernel, testedWorkDim, nullptr, globalWorkSize, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_GLOBAL_WORK_SIZE, status);
}

TEST_F(EnqueueKernelTest, givenEnqueueCommandWithNullLwsAndWorkDimsResultingInLessThan32BitMaxGroupsWhenEnqueueNDRangeKernelIsCalledThenSuccessIsReturned) {

    if (sizeof(size_t) < 8) {
        GTEST_SKIP();
    }

    size_t max32Bit = std::numeric_limits<uint32_t>::max();
    size_t globalWorkSize[3] = {(max32Bit + 1) * 4, 1, 1};
    MockKernelWithInternals mockKernel(*pClDevice);
    auto testedWorkDim = 3;

    auto status = clEnqueueNDRangeKernel(pCmdQ, mockKernel.mockMultiDeviceKernel, testedWorkDim, nullptr, globalWorkSize, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, status);

    globalWorkSize[0] = 1;
    globalWorkSize[1] = (max32Bit + 1) * 4;
    status = clEnqueueNDRangeKernel(pCmdQ, mockKernel.mockMultiDeviceKernel, testedWorkDim, nullptr, globalWorkSize, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, status);

    globalWorkSize[1] = 1;
    globalWorkSize[2] = (max32Bit + 1) * 4;

    status = clEnqueueNDRangeKernel(pCmdQ, mockKernel.mockMultiDeviceKernel, testedWorkDim, nullptr, globalWorkSize, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, status);
}

HWTEST_F(EnqueueKernelTest, givenVMEKernelWhenEnqueueKernelThenDispatchFlagsHaveMediaSamplerRequired) {
    auto mockCsr = new MockCsrHw2<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);
    pDevice->resetCommandStreamReceiver(mockCsr);

    MockKernelWithInternals mockKernel(*pClDevice, context);
    size_t gws[3] = {1, 0, 0};
    mockKernel.kernelInfo.kernelDescriptor.kernelAttributes.flags.usesVme = true;
    clEnqueueNDRangeKernel(this->pCmdQ, mockKernel.mockMultiDeviceKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    EXPECT_TRUE(mockCsr->passedDispatchFlags.pipelineSelectArgs.mediaSamplerRequired);
}

HWTEST_F(EnqueueKernelTest, givenUseGlobalAtomicsSetWhenEnqueueKernelThenDispatchFlagsUseGlobalAtomicsIsSet) {
    auto mockCsr = new MockCsrHw2<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);
    pDevice->resetCommandStreamReceiver(mockCsr);

    MockKernelWithInternals mockKernel(*pClDevice, context);
    size_t gws[3] = {1, 0, 0};
    mockKernel.kernelInfo.kernelDescriptor.kernelAttributes.flags.useGlobalAtomics = true;
    clEnqueueNDRangeKernel(this->pCmdQ, mockKernel.mockMultiDeviceKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    EXPECT_TRUE(mockCsr->passedDispatchFlags.useGlobalAtomics);
}

HWTEST_F(EnqueueKernelTest, givenUseGlobalAtomicsIsNotSetWhenEnqueueKernelThenDispatchFlagsUseGlobalAtomicsIsNotSet) {
    auto mockCsr = new MockCsrHw2<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);
    pDevice->resetCommandStreamReceiver(mockCsr);

    MockKernelWithInternals mockKernel(*pClDevice, context);
    size_t gws[3] = {1, 0, 0};
    mockKernel.kernelInfo.kernelDescriptor.kernelAttributes.flags.useGlobalAtomics = false;
    clEnqueueNDRangeKernel(this->pCmdQ, mockKernel.mockMultiDeviceKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    EXPECT_FALSE(mockCsr->passedDispatchFlags.useGlobalAtomics);
}

HWTEST_F(EnqueueKernelTest, givenContextWithSeveralDevicesWhenEnqueueKernelThenDispatchFlagsHaveCorrectInfoAboutMultipleSubDevicesInContext) {
    auto mockCsr = new MockCsrHw2<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);
    pDevice->resetCommandStreamReceiver(mockCsr);

    MockKernelWithInternals mockKernel(*pClDevice, context);
    size_t gws[3] = {1, 0, 0};
    clEnqueueNDRangeKernel(this->pCmdQ, mockKernel.mockMultiDeviceKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    EXPECT_FALSE(mockCsr->passedDispatchFlags.areMultipleSubDevicesInContext);

    context->deviceBitfields[rootDeviceIndex].set(3, true);
    clEnqueueNDRangeKernel(this->pCmdQ, mockKernel.mockMultiDeviceKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    EXPECT_TRUE(mockCsr->passedDispatchFlags.areMultipleSubDevicesInContext);
    context->deviceBitfields[rootDeviceIndex].set(3, false);
}

HWTEST_F(EnqueueKernelTest, givenNonVMEKernelWhenEnqueueKernelThenDispatchFlagsDoesntHaveMediaSamplerRequired) {
    auto mockCsr = new MockCsrHw2<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);
    pDevice->resetCommandStreamReceiver(mockCsr);

    MockKernelWithInternals mockKernel(*pClDevice, context);
    size_t gws[3] = {1, 0, 0};
    mockKernel.kernelInfo.kernelDescriptor.kernelAttributes.flags.usesVme = false;
    clEnqueueNDRangeKernel(this->pCmdQ, mockKernel.mockMultiDeviceKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    EXPECT_FALSE(mockCsr->passedDispatchFlags.pipelineSelectArgs.mediaSamplerRequired);
}

HWTEST_F(EnqueueKernelTest, whenEnqueueKernelWithEngineHintsThenEpilogRequiredIsSet) {
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    size_t off[3] = {0, 0, 0};
    size_t gws[3] = {1, 1, 1};

    MockKernelWithInternals mockKernel(*pClDevice);
    pCmdQ->dispatchHints = 1;

    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, off, gws, nullptr, 0, nullptr, nullptr);

    EXPECT_EQ(csr.recordedDispatchFlags.epilogueRequired, true);
    EXPECT_EQ(csr.recordedDispatchFlags.engineHints, 1u);
}

struct PauseOnGpuTests : public EnqueueKernelTest {
    void SetUp() override {
        EnqueueKernelTest::SetUp();

        auto &csr = pDevice->getGpgpuCommandStreamReceiver();
        debugPauseStateAddress = csr.getDebugPauseStateGPUAddress();
    }

    template <typename MI_SEMAPHORE_WAIT>
    bool verifySemaphore(const GenCmdList::iterator &iterator, uint64_t debugPauseStateAddress, DebugPauseState requiredDebugPauseState) {
        auto semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*iterator);

        if ((static_cast<uint32_t>(requiredDebugPauseState) == semaphoreCmd->getSemaphoreDataDword()) &&
            (debugPauseStateAddress == semaphoreCmd->getSemaphoreGraphicsAddress())) {

            EXPECT_EQ(MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_EQUAL_SDD, semaphoreCmd->getCompareOperation());
            EXPECT_EQ(MI_SEMAPHORE_WAIT::WAIT_MODE::WAIT_MODE_POLLING_MODE, semaphoreCmd->getWaitMode());

            return true;
        }

        return false;
    }

    template <typename FamilyType>
    bool verifyPipeControl(const GenCmdList::iterator &iterator, uint64_t debugPauseStateAddress, DebugPauseState requiredDebugPauseState) {
        using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

        auto pipeControlCmd = genCmdCast<PIPE_CONTROL *>(*iterator);

        if ((static_cast<uint32_t>(requiredDebugPauseState) == pipeControlCmd->getImmediateData()) &&
            (debugPauseStateAddress == NEO::UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*pipeControlCmd))) {

            EXPECT_TRUE(pipeControlCmd->getCommandStreamerStallEnable());

            EXPECT_EQ(MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, *defaultHwInfo), pipeControlCmd->getDcFlushEnable());

            EXPECT_EQ(PIPE_CONTROL::POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA, pipeControlCmd->getPostSyncOperation());

            return true;
        }

        return false;
    }

    template <typename FamilyType>
    bool verifyLoadRegImm(const GenCmdList::iterator &iterator) {
        using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
        uint32_t expectedRegisterOffset = DebugManager.flags.GpuScratchRegWriteRegisterOffset.get();
        uint32_t expectedRegisterData = DebugManager.flags.GpuScratchRegWriteRegisterData.get();
        auto loadRegImm = genCmdCast<MI_LOAD_REGISTER_IMM *>(*iterator);

        if ((expectedRegisterOffset == loadRegImm->getRegisterOffset()) &&
            (expectedRegisterData == loadRegImm->getDataDword())) {
            return true;
        }

        return false;
    }

    template <typename MI_SEMAPHORE_WAIT>
    void findSemaphores(GenCmdList &cmdList) {
        auto semaphore = find<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());

        while (semaphore != cmdList.end()) {
            if (verifySemaphore<MI_SEMAPHORE_WAIT>(semaphore, debugPauseStateAddress, DebugPauseState::hasUserStartConfirmation)) {
                semaphoreBeforeWalkerFound++;
            }

            if (verifySemaphore<MI_SEMAPHORE_WAIT>(semaphore, debugPauseStateAddress, DebugPauseState::hasUserEndConfirmation)) {
                semaphoreAfterWalkerFound++;
            }

            semaphore = find<MI_SEMAPHORE_WAIT *>(++semaphore, cmdList.end());
        }
    }

    template <typename FamilyType>
    void findPipeControls(GenCmdList &cmdList) {
        using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
        auto pipeControl = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());

        while (pipeControl != cmdList.end()) {
            if (verifyPipeControl<FamilyType>(pipeControl, debugPauseStateAddress, DebugPauseState::waitingForUserStartConfirmation)) {
                pipeControlBeforeWalkerFound++;
            }

            if (verifyPipeControl<FamilyType>(pipeControl, debugPauseStateAddress, DebugPauseState::waitingForUserEndConfirmation)) {
                pipeControlAfterWalkerFound++;
            }

            pipeControl = find<PIPE_CONTROL *>(++pipeControl, cmdList.end());
        }
    }

    template <typename FamilyType>
    void findLoadRegImms(GenCmdList &cmdList) {
        using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
        auto loadRegImm = find<MI_LOAD_REGISTER_IMM *>(cmdList.begin(), cmdList.end());

        while (loadRegImm != cmdList.end()) {
            if (verifyLoadRegImm<FamilyType>(loadRegImm)) {
                loadRegImmsFound++;
            }

            loadRegImm = find<MI_LOAD_REGISTER_IMM *>(++loadRegImm, cmdList.end());
        }
    }

    DebugManagerStateRestore restore;

    const size_t off[3] = {0, 0, 0};
    const size_t gws[3] = {1, 1, 1};

    uint64_t debugPauseStateAddress = 0;

    uint32_t semaphoreBeforeWalkerFound = 0;
    uint32_t semaphoreAfterWalkerFound = 0;
    uint32_t pipeControlBeforeWalkerFound = 0;
    uint32_t pipeControlAfterWalkerFound = 0;
    uint32_t loadRegImmsFound = 0;
};

HWTEST_F(PauseOnGpuTests, givenPauseOnEnqueueFlagSetWhenDispatchWalkersThenInsertPauseCommandsAroundSpecifiedEnqueue) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    DebugManager.flags.PauseOnEnqueue.set(1);

    MockKernelWithInternals mockKernel(*pClDevice);

    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, off, gws, nullptr, 0, nullptr, nullptr);
    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, off, gws, nullptr, 0, nullptr, nullptr);

    ClHardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(*pCmdQ);

    findSemaphores<MI_SEMAPHORE_WAIT>(hwParser.cmdList);

    EXPECT_EQ(1u, semaphoreBeforeWalkerFound);
    EXPECT_EQ(1u, semaphoreAfterWalkerFound);

    findPipeControls<FamilyType>(hwParser.cmdList);

    EXPECT_EQ(1u, pipeControlBeforeWalkerFound);
    EXPECT_EQ(1u, pipeControlAfterWalkerFound);
}

HWTEST_F(PauseOnGpuTests, givenPauseOnEnqueueFlagSetToMinusTwoWhenDispatchWalkersThenInsertPauseCommandsAroundEachEnqueue) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    DebugManager.flags.PauseOnEnqueue.set(-2);

    MockKernelWithInternals mockKernel(*pClDevice);

    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, off, gws, nullptr, 0, nullptr, nullptr);
    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, off, gws, nullptr, 0, nullptr, nullptr);

    ClHardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(*pCmdQ);

    findSemaphores<MI_SEMAPHORE_WAIT>(hwParser.cmdList);

    findPipeControls<FamilyType>(hwParser.cmdList);

    EXPECT_EQ(2u, semaphoreBeforeWalkerFound);
    EXPECT_EQ(2u, semaphoreAfterWalkerFound);
    EXPECT_EQ(2u, pipeControlBeforeWalkerFound);
    EXPECT_EQ(2u, pipeControlAfterWalkerFound);
}

HWTEST_F(PauseOnGpuTests, givenPauseModeSetToBeforeOnlyWhenDispatchingThenInsertPauseOnlyBeforeEnqueue) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    DebugManager.flags.PauseOnEnqueue.set(0);
    DebugManager.flags.PauseOnGpuMode.set(PauseOnGpuProperties::PauseMode::BeforeWorkload);

    MockKernelWithInternals mockKernel(*pClDevice);

    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, off, gws, nullptr, 0, nullptr, nullptr);

    ClHardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(*pCmdQ);

    findSemaphores<MI_SEMAPHORE_WAIT>(hwParser.cmdList);

    findPipeControls<FamilyType>(hwParser.cmdList);

    EXPECT_EQ(1u, semaphoreBeforeWalkerFound);
    EXPECT_EQ(0u, semaphoreAfterWalkerFound);
    EXPECT_EQ(1u, pipeControlBeforeWalkerFound);
    EXPECT_EQ(0u, pipeControlAfterWalkerFound);
}

HWTEST_F(PauseOnGpuTests, givenPauseModeSetToAfterOnlyWhenDispatchingThenInsertPauseOnlyAfterEnqueue) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    DebugManager.flags.PauseOnEnqueue.set(0);
    DebugManager.flags.PauseOnGpuMode.set(PauseOnGpuProperties::PauseMode::AfterWorkload);

    MockKernelWithInternals mockKernel(*pClDevice);

    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, off, gws, nullptr, 0, nullptr, nullptr);

    ClHardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(*pCmdQ);

    findSemaphores<MI_SEMAPHORE_WAIT>(hwParser.cmdList);

    findPipeControls<FamilyType>(hwParser.cmdList);

    EXPECT_EQ(0u, semaphoreBeforeWalkerFound);
    EXPECT_EQ(1u, semaphoreAfterWalkerFound);
    EXPECT_EQ(0u, pipeControlBeforeWalkerFound);
    EXPECT_EQ(1u, pipeControlAfterWalkerFound);
}

HWTEST_F(PauseOnGpuTests, givenPauseModeSetToBeforeAndAfterWhenDispatchingThenInsertPauseAroundEnqueue) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    DebugManager.flags.PauseOnEnqueue.set(0);
    DebugManager.flags.PauseOnGpuMode.set(PauseOnGpuProperties::PauseMode::BeforeAndAfterWorkload);

    MockKernelWithInternals mockKernel(*pClDevice);

    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, off, gws, nullptr, 0, nullptr, nullptr);

    ClHardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(*pCmdQ);

    findSemaphores<MI_SEMAPHORE_WAIT>(hwParser.cmdList);

    findPipeControls<FamilyType>(hwParser.cmdList);

    EXPECT_EQ(1u, semaphoreBeforeWalkerFound);
    EXPECT_EQ(1u, semaphoreAfterWalkerFound);
    EXPECT_EQ(1u, pipeControlBeforeWalkerFound);
    EXPECT_EQ(1u, pipeControlAfterWalkerFound);
}

HWTEST_F(PauseOnGpuTests, givenPauseOnEnqueueFlagSetWhenDispatchWalkersThenDontInsertPauseCommandsWhenUsingSpecialQueue) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    DebugManager.flags.PauseOnEnqueue.set(0);

    pCmdQ->setIsSpecialCommandQueue(true);

    MockKernelWithInternals mockKernel(*pClDevice);

    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, off, gws, nullptr, 0, nullptr, nullptr);

    ClHardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(*pCmdQ);

    findSemaphores<MI_SEMAPHORE_WAIT>(hwParser.cmdList);

    findPipeControls<FamilyType>(hwParser.cmdList);

    EXPECT_EQ(0u, semaphoreBeforeWalkerFound);
    EXPECT_EQ(0u, semaphoreAfterWalkerFound);
    EXPECT_EQ(0u, pipeControlBeforeWalkerFound);
    EXPECT_EQ(0u, pipeControlAfterWalkerFound);

    pCmdQ->setIsSpecialCommandQueue(false);
}

HWTEST_F(PauseOnGpuTests, givenGpuScratchWriteEnabledWhenDispatchWalkersThenInsertLoadRegisterImmCommandAroundSpecifiedEnqueue) {
    DebugManager.flags.GpuScratchRegWriteAfterWalker.set(1);
    DebugManager.flags.GpuScratchRegWriteRegisterData.set(0x1234);
    DebugManager.flags.GpuScratchRegWriteRegisterOffset.set(0x5678);

    MockKernelWithInternals mockKernel(*pClDevice);

    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, off, gws, nullptr, 0, nullptr, nullptr);

    ClHardwareParse hwParser;

    hwParser.parseCommands<FamilyType>(*pCmdQ);

    findLoadRegImms<FamilyType>(hwParser.cmdList);

    EXPECT_EQ(0u, loadRegImmsFound);

    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, off, gws, nullptr, 0, nullptr, nullptr);
    hwParser.parseCommands<FamilyType>(*pCmdQ);

    findLoadRegImms<FamilyType>(hwParser.cmdList);

    EXPECT_EQ(1u, loadRegImmsFound);
}

HWTEST_F(PauseOnGpuTests, givenGpuScratchWriteEnabledWhenDispatcMultiplehWalkersThenInsertLoadRegisterImmCommandOnlyOnce) {
    DebugManager.flags.GpuScratchRegWriteAfterWalker.set(1);
    DebugManager.flags.GpuScratchRegWriteRegisterData.set(0x1234);
    DebugManager.flags.GpuScratchRegWriteRegisterOffset.set(0x5678);

    MockKernelWithInternals mockKernel(*pClDevice);

    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, off, gws, nullptr, 0, nullptr, nullptr);
    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, off, gws, nullptr, 0, nullptr, nullptr);
    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, off, gws, nullptr, 0, nullptr, nullptr);
    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, off, gws, nullptr, 0, nullptr, nullptr);

    ClHardwareParse hwParser;

    hwParser.parseCommands<FamilyType>(*pCmdQ);

    findLoadRegImms<FamilyType>(hwParser.cmdList);

    EXPECT_EQ(1u, loadRegImmsFound);
}

HWTEST_F(PauseOnGpuTests, givenGpuScratchWriteEnabledWhenEstimatingCommandStreamSizeThenMiLoadRegisterImmCommandSizeIsIncluded) {
    MockKernelWithInternals mockKernel(*pClDevice);
    DispatchInfo dispatchInfo;
    MultiDispatchInfo multiDispatchInfo(mockKernel.mockKernel);
    dispatchInfo.setKernel(mockKernel.mockKernel);
    multiDispatchInfo.push(dispatchInfo);

    auto baseCommandStreamSize = EnqueueOperation<FamilyType>::getTotalSizeRequiredCS(CL_COMMAND_NDRANGE_KERNEL, {}, false, false, false, *pCmdQ, multiDispatchInfo, false, false);
    DebugManager.flags.GpuScratchRegWriteAfterWalker.set(1);

    auto extendedCommandStreamSize = EnqueueOperation<FamilyType>::getTotalSizeRequiredCS(CL_COMMAND_NDRANGE_KERNEL, {}, false, false, false, *pCmdQ, multiDispatchInfo, false, false);

    EXPECT_EQ(baseCommandStreamSize + sizeof(typename FamilyType::MI_LOAD_REGISTER_IMM), extendedCommandStreamSize);
}
