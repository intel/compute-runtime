/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/pause_on_gpu_properties.h"
#include "shared/source/helpers/preamble.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/kernel_binary_helper.h"
#include "shared/test/common/helpers/raii_gfx_core_helper.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/mocks/mock_csr.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
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

template <template <typename> class CsrType>
class EnqueueKernelTestT
    : public EnqueueKernelTest {
  public:
    void SetUp() override {}
    void TearDown() override {}

    template <typename FamilyType>
    void setUpT() {
        EnvironmentWithCsrWrapper environment;
        environment.setCsrType<CsrType<FamilyType>>();
        EnqueueKernelTest::SetUp();
    }

    template <typename FamilyType>
    void tearDownT() {
        EnqueueKernelTest::TearDown();
    }
};

typedef EnqueueKernelTestT<MockCsrHw2> EnqueueKernelTestWithMockCsrHw2;

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

    std::unique_ptr<MultiDeviceKernel> pMultiDeviceKernel(MultiDeviceKernel::create(pProgram, pProgram->getKernelInfosForKernel("CopyBuffer"), retVal));
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
    USE_REAL_FILE_SYSTEM();
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

    std::unique_ptr<MultiDeviceKernel> pMultiDeviceKernel(MultiDeviceKernel::create(pProgram, pProgram->getKernelInfosForKernel("CopyBuffer"), retVal));
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

    std::unique_ptr<MultiDeviceKernel> pMultiDeviceKernel(MultiDeviceKernel::create(pProgram, pProgram->getKernelInfosForKernel("CopyBuffer"), retVal));
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

using clEnqueueNDCountKernelTests = ApiTests;

TEST_F(clEnqueueNDCountKernelTests, GivenQueueIncapableWhenEnqueuingNDCountKernelINTELThenInvalidOperationIsReturned) {

    auto &gfxCoreHelper = pDevice->getGfxCoreHelper();
    auto engineGroupType = gfxCoreHelper.getEngineGroupType(pCommandQueue->getGpgpuEngine().getEngineType(),
                                                            pCommandQueue->getGpgpuEngine().getEngineUsage(), *::defaultHwInfo);
    if (!gfxCoreHelper.isCooperativeDispatchSupported(engineGroupType, pDevice->getRootDeviceEnvironment())) {
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

    auto &gfxCoreHelper = pDevice->getGfxCoreHelper();
    auto engineGroupType = gfxCoreHelper.getEngineGroupType(pCmdQ2->getGpgpuEngine().getEngineType(),
                                                            pCmdQ2->getGpgpuEngine().getEngineUsage(), hardwareInfo);
    if (!gfxCoreHelper.isCooperativeDispatchSupported(engineGroupType, pDevice->getRootDeviceEnvironment())) {
        pCmdQ2->getGpgpuEngine().osContext = pCmdQ2->getDevice().getEngine(aub_stream::ENGINE_CCS, EngineUsage::lowPriority).osContext;
    }

    std::unique_ptr<MultiDeviceKernel> pMultiDeviceKernel(MultiDeviceKernel::create(pProgram, pProgram->getKernelInfosForKernel("CopyBuffer"), retVal));
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

TEST_F(EnqueueKernelTest, givenLocalWorkSizeEqualZeroThenClEnqueueNDCountKernelINTELReturnsError) {
    size_t workgroupCount[3] = {1, 1, 1};
    size_t localWorkSize[3] = {0, 1, 1};
    cl_int retVal = CL_SUCCESS;
    std::unique_ptr<MultiDeviceKernel> pMultiDeviceKernel(MultiDeviceKernel::create(pProgram, pProgram->getKernelInfosForKernel("CopyBuffer"), retVal));

    retVal = clEnqueueNDCountKernelINTEL(pCmdQ, pMultiDeviceKernel.get(), 1, nullptr, workgroupCount, localWorkSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_WORK_GROUP_SIZE, retVal);

    pMultiDeviceKernel.get()->setKernelExecutionType(CL_KERNEL_EXEC_INFO_CONCURRENT_TYPE_INTEL);
    retVal = clEnqueueNDCountKernelINTEL(pCmdQ, pMultiDeviceKernel.get(), 1, nullptr, workgroupCount, localWorkSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_WORK_GROUP_SIZE, retVal);
}

TEST_F(EnqueueKernelTest, givenKernelWhenNotAllArgsAreSetButSetKernelArgIsCalledTwiceThenClEnqueueNDCountKernelINTELReturnsError) {
    const size_t n = 512;
    size_t workgroupCount[3] = {2, 1, 1};
    size_t localWorkSize[3] = {256, 1, 1};
    cl_int retVal = CL_SUCCESS;
    CommandQueue *pCmdQ2 = createCommandQueue(pClDevice);

    auto &gfxCoreHelper = pClDevice->getGfxCoreHelper();
    auto engineGroupType = gfxCoreHelper.getEngineGroupType(pCmdQ2->getGpgpuEngine().getEngineType(),
                                                            pCmdQ2->getGpgpuEngine().getEngineUsage(), hardwareInfo);
    if (!gfxCoreHelper.isCooperativeDispatchSupported(engineGroupType, pDevice->getRootDeviceEnvironment())) {
        pCmdQ2->getGpgpuEngine().osContext = pCmdQ2->getDevice().getEngine(aub_stream::ENGINE_CCS, EngineUsage::lowPriority).osContext;
    }

    std::unique_ptr<MultiDeviceKernel> pMultiDeviceKernel(MultiDeviceKernel::create(pProgram, pProgram->getKernelInfosForKernel("CopyBuffer"), retVal));
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

    auto &gfxCoreHelper = pClDevice->getGfxCoreHelper();
    auto engineGroupType = gfxCoreHelper.getEngineGroupType(pCmdQ2->getGpgpuEngine().getEngineType(),
                                                            pCmdQ2->getGpgpuEngine().getEngineUsage(), hardwareInfo);
    if (!gfxCoreHelper.isCooperativeDispatchSupported(engineGroupType, pDevice->getRootDeviceEnvironment())) {
        pCmdQ2->getGpgpuEngine().osContext = pCmdQ2->getDevice().getEngine(aub_stream::ENGINE_CCS, EngineUsage::lowPriority).osContext;
    }

    std::unique_ptr<MultiDeviceKernel> pMultiDeviceKernel(MultiDeviceKernel::create(pProgram, pProgram->getKernelInfosForKernel("CopyBuffer"), retVal));
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

HWTEST_F(EnqueueKernelTest, WhenEnqueingKernelThenLatestSentEnqueueTypeIsUpdated) {
    EXPECT_NE(pCmdQ->peekLatestSentEnqueueOperation(), EnqueueProperties::Operation::gpuKernel);
    callOneWorkItemNDRKernel();
    EXPECT_EQ(pCmdQ->peekLatestSentEnqueueOperation(), EnqueueProperties::Operation::gpuKernel);
}

HWTEST_F(EnqueueKernelTest, WhenEnqueingKernelThenCsrTaskLevelIsIncremented) {
    // this test case assumes IOQ
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.taskCount = pCmdQ->taskCount + 100;
    csr.taskLevel = pCmdQ->taskLevel + 50;

    callOneWorkItemNDRKernel();
    EXPECT_EQ(pCmdQ->taskCount, csr.peekTaskCount());

    auto expectedTaskLevel = pCmdQ->taskLevel;
    if (!csr.isUpdateTagFromWaitEnabled()) {
        expectedTaskLevel++;
    }
    EXPECT_EQ(expectedTaskLevel, csr.peekTaskLevel());
}

HWTEST_F(EnqueueKernelTest, WhenEnqueingKernelThenCommandsAreAdded) {
    auto usedCmdBufferBefore = pCS->getUsed();

    callOneWorkItemNDRKernel();
    EXPECT_NE(usedCmdBufferBefore, pCS->getUsed());
}

HWTEST_F(EnqueueKernelTest, GivenGpuHangAndBlockingCallWhenEnqueingKernelThenOutOfResourcesIsReported) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.MakeEachEnqueueBlocking.set(true);

    std::unique_ptr<ClDevice> device(new MockClDevice{MockClDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr)});
    cl_queue_properties props = {};

    MockCommandQueueHw<FamilyType> mockCommandQueueHw(context, device.get(), &props);
    mockCommandQueueHw.waitForAllEnginesReturnValue = WaitStatus::gpuHang;

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
    const auto &compilerProductHelper = pDevice->getRootDeviceEnvironment().getHelper<CompilerProductHelper>();

    auto dshBefore = pDSH->getUsed();
    auto iohBefore = pIOH->getUsed();
    auto sshBefore = pSSH->getUsed();

    callOneWorkItemNDRKernel();
    EXPECT_TRUE(UnitTestHelper<FamilyType>::evaluateDshUsage(dshBefore, pDSH->getUsed(), &pKernel->getKernelInfo().kernelDescriptor, rootDeviceIndex));

    auto crossThreadDatSize = this->pKernel->getCrossThreadDataSize();
    auto heaplessEnabled = pCmdQ->getHeaplessModeEnabled();
    auto inlineDataSize = UnitTestHelper<FamilyType>::getInlineDataSize(heaplessEnabled);
    bool crossThreadDataFitsInInlineData = (crossThreadDatSize <= inlineDataSize);

    if (crossThreadDataFitsInInlineData) {
        EXPECT_EQ(iohBefore, pIOH->getUsed());
    } else {
        EXPECT_NE(iohBefore, pIOH->getUsed());
    }

    if (((pKernel->getKernelInfo().kernelDescriptor.kernelAttributes.bufferAddressingMode == KernelDescriptor::BindfulAndStateless) || pKernel->getKernelInfo().kernelDescriptor.kernelAttributes.flags.usesImages) && compilerProductHelper.isStatelessToStatefulBufferOffsetSupported()) {
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

HWCMDTEST_F(IGFX_GEN12LP_CORE, EnqueueKernelTest, givenSecondEnqueueWithTheSameScratchRequirementWhenPreemptionIsEnabledThenDontProgramMVSAgain) {
    typedef typename FamilyType::MEDIA_VFE_STATE MEDIA_VFE_STATE;
    pDevice->setPreemptionMode(PreemptionMode::ThreadGroup);
    auto &csr = pDevice->getGpgpuCommandStreamReceiver();
    csr.getMemoryManager()->setForce32BitAllocations(false);
    ClHardwareParse hwParser;
    size_t off[3] = {0, 0, 0};
    size_t gws[3] = {1, 1, 1};
    uint32_t scratchSize = 4096u;

    MockKernelWithInternals mockKernel(*pClDevice);
    mockKernel.kernelInfo.kernelDescriptor.kernelAttributes.perThreadScratchSize[0] = scratchSize;

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
    uint32_t scratchSizeSlot1 = 4096u;

    MockKernelWithInternals mockKernel(*pClDevice);
    mockKernel.kernelInfo.kernelDescriptor.kernelAttributes.perThreadScratchSize[1] = scratchSizeSlot1;

    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, off, gws, nullptr, 0, nullptr, nullptr);

    EXPECT_EQ(scratchSizeSlot1, csr.requiredScratchSlot1Size);
}

HWTEST_F(EnqueueKernelTest, whenEnqueueKernelWithNoStatelessWriteWhenSbaIsBeingProgrammedThenConstPolicyIsChoosen) {

    if (pCmdQ->getHeaplessStateInitEnabled()) {
        GTEST_SKIP();
    }

    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    size_t off[3] = {0, 0, 0};
    size_t gws[3] = {1, 1, 1};

    MockKernelWithInternals mockKernel(*pClDevice);
    mockKernel.mockKernel->containsStatelessWrites = false;

    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, off, gws, nullptr, 0, nullptr, nullptr);

    EXPECT_EQ(csr.recordedDispatchFlags.l3CacheSettings, L3CachingSettings::l3AndL1On);

    auto &gfxCoreHelper = pDevice->getGfxCoreHelper();
    auto gmmHelper = this->pDevice->getGmmHelper();
    auto expectedMocsIndex = gfxCoreHelper.getMocsIndex(*gmmHelper, true, true);
    EXPECT_EQ(expectedMocsIndex, csr.latestSentStatelessMocsConfig);
}

HWTEST_F(EnqueueKernelTest, whenEnqueueKernelWithNoStatelessWriteOnBlockedCodePathWhenSbaIsBeingProgrammedThenConstPolicyIsChoosen) {

    if (pCmdQ->getHeaplessStateInitEnabled()) {
        GTEST_SKIP();
    }

    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    size_t off[3] = {0, 0, 0};
    size_t gws[3] = {1, 1, 1};

    auto userEvent = clCreateUserEvent(this->context, nullptr);

    MockKernelWithInternals mockKernel(*pClDevice);
    mockKernel.mockKernel->containsStatelessWrites = false;

    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, off, gws, nullptr, 1, &userEvent, nullptr);

    clSetUserEventStatus(userEvent, 0u);

    EXPECT_EQ(csr.recordedDispatchFlags.l3CacheSettings, L3CachingSettings::l3AndL1On);

    auto &gfxCoreHelper = pDevice->getGfxCoreHelper();

    auto gmmHelper = this->pDevice->getGmmHelper();
    auto expectedMocsIndex = gfxCoreHelper.getMocsIndex(*gmmHelper, true, true);
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
    debugManager.flags.MakeEachEnqueueBlocking.set(true);

    std::unique_ptr<ClDevice> device(new MockClDevice{MockClDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr)});
    cl_queue_properties props = {};

    MockCommandQueueHw<FamilyType> mockCommandQueueHw(context, device.get(), &props);
    mockCommandQueueHw.waitForAllEnginesReturnValue = WaitStatus::gpuHang;

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
    mockCsr->overrideDispatchPolicy(DispatchMode::batchedDispatch);
    pDevice->resetCommandStreamReceiver(mockCsr);

    auto mockedSubmissionsAggregator = new MockSubmissionsAggregator();
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
    size_t timestampPacketSurfacesCount = (mockCsr->peekTimestampPacketWriteEnabled() && !mockCsr->heaplessStateInitialized) ? 1 : 0;
    size_t fenceSurfaceCount = mockCsr->globalFenceAllocation ? 1 : 0;
    size_t clearColorSize = mockCsr->clearColorAllocation ? 1 : 0;
    size_t commandBufferCount = pDevice->getProductHelper().getCommandBuffersPreallocatedPerCommandQueue() > 0 ? 0 : 1;
    size_t rtSurface = pDevice->getRTMemoryBackedBuffer() ? 1u : 0u;

    EXPECT_EQ(mockCsr->heaplessStateInitialized ? 1u : 0u, mockCsr->flushCalledCount);
    EXPECT_EQ(4u + csrSurfaceCount + timestampPacketSurfacesCount + fenceSurfaceCount + clearColorSize + commandBufferCount + rtSurface, cmdBuffer->surfaces.size());
}

HWTEST_F(EnqueueKernelTest, givenReducedAddressSpaceGraphicsAllocationForHostPtrWithL3FlushRequiredWhenEnqueueKernelIsCalledThenFlushIsCalledForReducedAddressSpacePlatforms) {
    EnvironmentWithCsrWrapper environment;
    environment.setCsrType<MockCsrHw2<FamilyType>>();
    std::unique_ptr<MockClDevice> device;
    std::unique_ptr<CommandQueue> cmdQ;
    auto hwInfoToModify = *defaultHwInfo;
    hwInfoToModify.capabilityTable.gpuAddressSpace = MemoryConstants::max36BitAddress;
    device.reset(new MockClDevice{MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfoToModify)});
    auto mockCsr = static_cast<MockCsrHw2<FamilyType> *>(&device->getGpgpuCommandStreamReceiver());
    auto memoryManager = mockCsr->getMemoryManager();
    uint32_t hostPtr[10]{};

    AllocationProperties properties{device->getRootDeviceIndex(), false, 1, AllocationType::externalHostPtr, false, device->getDeviceBitfield()};
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
    EnvironmentWithCsrWrapper environment;
    environment.setCsrType<MockCsrHw2<FamilyType>>();
    std::unique_ptr<MockClDevice> device;
    std::unique_ptr<CommandQueue> cmdQ;
    auto hwInfoToModify = *defaultHwInfo;
    hwInfoToModify.capabilityTable.gpuAddressSpace = MemoryConstants::max36BitAddress;
    device.reset(new MockClDevice{MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfoToModify)});
    auto mockCsr = static_cast<MockCsrHw2<FamilyType> *>(&device->getGpgpuCommandStreamReceiver());
    auto memoryManager = mockCsr->getMemoryManager();
    uint32_t hostPtr[10]{};

    AllocationProperties properties{device->getRootDeviceIndex(), false, 1, AllocationType::externalHostPtr, false, device->getDeviceBitfield()};
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
    EnvironmentWithCsrWrapper environment;
    environment.setCsrType<MockCsrHw2<FamilyType>>();
    HardwareInfo hwInfoToModify;
    std::unique_ptr<MockClDevice> device;
    std::unique_ptr<CommandQueue> cmdQ;
    hwInfoToModify = *defaultHwInfo;
    hwInfoToModify.capabilityTable.gpuAddressSpace = MemoryConstants::max48BitAddress;
    device.reset(new MockClDevice{MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfoToModify)});
    auto mockCsr = static_cast<MockCsrHw2<FamilyType> *>(&device->getGpgpuCommandStreamReceiver());
    auto memoryManager = mockCsr->getMemoryManager();
    uint32_t hostPtr[10]{};

    AllocationProperties properties{device->getRootDeviceIndex(), false, 1, AllocationType::externalHostPtr, false, device->getDeviceBitfield()};
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

HWTEST_TEMPLATED_F(EnqueueKernelTestWithMockCsrHw2, givenCommandStreamReceiverInBatchingModeAndBatchedKernelWhenFlushIsCalledThenKernelIsSubmitted) {
    auto *mockCsrmockCsr = static_cast<MockCsrHw2<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());
    mockCsrmockCsr->overrideDispatchPolicy(DispatchMode::batchedDispatch);
    mockCsrmockCsr->useNewResourceImplicitFlush = false;
    mockCsrmockCsr->useGpuIdleImplicitFlush = false;

    auto mockedSubmissionsAggregator = new MockSubmissionsAggregator();
    mockCsrmockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    MockKernelWithInternals mockKernel(*pClDevice);
    size_t gws[3] = {1, 0, 0};
    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    EXPECT_FALSE(mockedSubmissionsAggregator->peekCmdBufferList().peekIsEmpty());

    auto ret = clFlush(pCmdQ);
    EXPECT_EQ(CL_SUCCESS, ret);

    EXPECT_TRUE(mockedSubmissionsAggregator->peekCmdBufferList().peekIsEmpty());
    EXPECT_EQ(mockCsrmockCsr->heaplessStateInitialized ? 2u : 1u, mockCsrmockCsr->flushCalledCount);
}

HWTEST_TEMPLATED_F(EnqueueKernelTestWithMockCsrHw2, givenCommandStreamReceiverInBatchingModeAndBatchedKernelWhenFlushIsCalledTwiceThenNothingChanges) {
    auto *mockCsrmockCsr = static_cast<MockCsrHw2<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());
    mockCsrmockCsr->overrideDispatchPolicy(DispatchMode::batchedDispatch);

    auto mockedSubmissionsAggregator = new MockSubmissionsAggregator();
    mockCsrmockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    MockKernelWithInternals mockKernel(*pClDevice);
    size_t gws[3] = {1, 0, 0};
    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    auto ret = clFlush(pCmdQ);
    EXPECT_EQ(CL_SUCCESS, ret);

    ret = clFlush(pCmdQ);
    EXPECT_EQ(CL_SUCCESS, ret);

    EXPECT_TRUE(mockedSubmissionsAggregator->peekCmdBufferList().peekIsEmpty());
    EXPECT_EQ(mockCsrmockCsr->heaplessStateInitialized ? 2u : 1u, mockCsrmockCsr->flushCalledCount);
}

HWTEST_F(EnqueueKernelTest, givenCommandStreamReceiverInBatchingModeWhenKernelIsEnqueuedTwiceThenTwoSubmissionsAreRecorded) {
    auto &mockCsrmockCsr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    mockCsrmockCsr.overrideDispatchPolicy(DispatchMode::batchedDispatch);
    mockCsrmockCsr.useNewResourceImplicitFlush = false;
    mockCsrmockCsr.useGpuIdleImplicitFlush = false;
    auto mockedSubmissionsAggregator = new MockSubmissionsAggregator();
    mockCsrmockCsr.submissionAggregator.reset(mockedSubmissionsAggregator);

    pDevice->getGpgpuCommandStreamReceiver().flushTagUpdate(); // to clear residency allocations after preallocations

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

HWTEST_TEMPLATED_F(EnqueueKernelTestWithMockCsrHw2, givenCommandStreamReceiverInBatchingModeWhenFlushIsCalledOnTwoBatchedKernelsThenTheyAreExecutedInOrder) {
    auto *mockCsr = static_cast<MockCsrHw2<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());
    mockCsr->overrideDispatchPolicy(DispatchMode::batchedDispatch);
    mockCsr->useNewResourceImplicitFlush = false;
    mockCsr->useGpuIdleImplicitFlush = false;

    auto mockedSubmissionsAggregator = new MockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    MockKernelWithInternals mockKernel(*pClDevice);
    size_t gws[3] = {1, 0, 0};
    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    pCmdQ->flush();

    EXPECT_TRUE(mockedSubmissionsAggregator->peekCmdBufferList().peekIsEmpty());
    EXPECT_EQ(mockCsr->heaplessStateInitialized ? 2u : 1u, mockCsr->flushCalledCount);
}

HWCMDTEST_TEMPLATED_F(IGFX_XE_HP_CORE, EnqueueKernelTestWithMockCsrHw2, givenTwoEnqueueProgrammedWithinSameCommandBufferWhenBatchedThenNoBBSBetweenThem) {
    auto mockCsr = static_cast<MockCsrHw2<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());
    mockCsr->overrideDispatchPolicy(DispatchMode::batchedDispatch);
    mockCsr->useNewResourceImplicitFlush = false;
    mockCsr->useGpuIdleImplicitFlush = false;

    auto mockedSubmissionsAggregator = new MockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    ClHardwareParse hwParse;

    MockKernelWithInternals mockKernel(*pClDevice);
    size_t gws[3] = {1, 0, 0};
    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    pCmdQ->flush();

    hwParse.parseCommands<FamilyType>(*pCmdQ);
    auto bbsCommands = findAll<typename FamilyType::MI_BATCH_BUFFER_START *>(hwParse.cmdList.begin(), hwParse.cmdList.end());

    EXPECT_EQ(pCmdQ->getHeaplessStateInitEnabled() ? 0u : 1u, bbsCommands.size());
}

HWTEST_TEMPLATED_F(EnqueueKernelTestWithMockCsrHw2, givenCsrInBatchingModeWhenFinishIsCalledThenBatchesSubmissionsAreFlushed) {
    DebugManagerStateRestore restorer{};
    debugManager.flags.EnableL3FlushAfterPostSync.set(0);

    auto *mockCsr = static_cast<MockCsrHw2<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());
    mockCsr->overrideDispatchPolicy(DispatchMode::batchedDispatch);
    mockCsr->useNewResourceImplicitFlush = false;
    mockCsr->useGpuIdleImplicitFlush = false;

    auto mockedSubmissionsAggregator = new MockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    MockKernelWithInternals mockKernel(*pClDevice);
    size_t gws[3] = {1, 0, 0};
    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    pCmdQ->finish();

    EXPECT_TRUE(mockedSubmissionsAggregator->peekCmdBufferList().peekIsEmpty());
    EXPECT_EQ(mockCsr->heaplessStateInitialized ? 2u : 1u, mockCsr->flushCalledCount);
}

HWTEST_TEMPLATED_F(EnqueueKernelTestWithMockCsrHw2, givenCsrInBatchingModeWhenThressEnqueueKernelsAreCalledThenBatchesSubmissionsAreFlushed) {

    DebugManagerStateRestore restorer{};
    debugManager.flags.EnableL3FlushAfterPostSync.set(0);

    auto *mockCsr = static_cast<MockCsrHw2<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());
    mockCsr->overrideDispatchPolicy(DispatchMode::batchedDispatch);
    mockCsr->useNewResourceImplicitFlush = false;
    mockCsr->useGpuIdleImplicitFlush = false;

    auto mockedSubmissionsAggregator = new MockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    MockKernelWithInternals mockKernel(*pClDevice);
    size_t gws[3] = {1, 0, 0};
    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    pCmdQ->finish();

    EXPECT_TRUE(mockedSubmissionsAggregator->peekCmdBufferList().peekIsEmpty());
    EXPECT_EQ(mockCsr->heaplessStateInitialized ? 2u : 1u, mockCsr->flushCalledCount);
}

HWTEST_TEMPLATED_F(EnqueueKernelTestWithMockCsrHw2, givenCsrInBatchingModeWhenWaitForEventsIsCalledThenBatchedSubmissionsAreFlushed) {
    DebugManagerStateRestore restorer{};
    debugManager.flags.EnableL3FlushAfterPostSync.set(0);
    auto *mockCsr = static_cast<MockCsrHw2<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());
    mockCsr->overrideDispatchPolicy(DispatchMode::batchedDispatch);
    mockCsr->useNewResourceImplicitFlush = false;
    mockCsr->useGpuIdleImplicitFlush = false;

    auto mockedSubmissionsAggregator = new MockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    MockKernelWithInternals mockKernel(*pClDevice);
    size_t gws[3] = {1, 0, 0};
    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    cl_event event;
    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, &event);

    auto status = clWaitForEvents(1, &event);

    EXPECT_EQ(CL_SUCCESS, status);
    EXPECT_TRUE(mockedSubmissionsAggregator->peekCmdBufferList().peekIsEmpty());
    EXPECT_EQ(mockCsr->heaplessStateInitialized ? 2u : 1u, mockCsr->flushCalledCount);

    status = clReleaseEvent(event);
    EXPECT_EQ(CL_SUCCESS, status);
}

HWTEST_TEMPLATED_F(EnqueueKernelTestWithMockCsrHw2, givenCsrInBatchingModeWhenCommandIsFlushedThenFlushStampIsUpdatedInCommandQueueCsrAndEvent) {

    DebugManagerStateRestore restorer{};
    debugManager.flags.EnableL3FlushAfterPostSync.set(0);

    auto *mockCsr = static_cast<MockCsrHw2<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());
    mockCsr->overrideDispatchPolicy(DispatchMode::batchedDispatch);
    mockCsr->useNewResourceImplicitFlush = false;
    mockCsr->useGpuIdleImplicitFlush = false;

    auto mockedSubmissionsAggregator = new MockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    MockKernelWithInternals mockKernel(*pClDevice);
    size_t gws[3] = {1, 0, 0};
    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    cl_event event;
    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, &event);
    auto neoEvent = castToObject<Event>(event);

    auto expectedStamp = mockCsr->heaplessStateInitialized ? 1u : 0u;
    EXPECT_EQ(expectedStamp, mockCsr->flushStamp->peekStamp());
    EXPECT_EQ(expectedStamp, neoEvent->flushStamp->peekStamp());
    EXPECT_EQ(expectedStamp, pCmdQ->flushStamp->peekStamp());

    auto status = clWaitForEvents(1, &event);

    expectedStamp++;
    EXPECT_EQ(CL_SUCCESS, status);
    EXPECT_EQ(1, neoEvent->getRefInternalCount());
    EXPECT_EQ(expectedStamp, mockCsr->flushStamp->peekStamp());
    EXPECT_EQ(expectedStamp, neoEvent->flushStamp->peekStamp());
    EXPECT_EQ(expectedStamp, pCmdQ->flushStamp->peekStamp());

    status = clFinish(pCmdQ);
    EXPECT_EQ(CL_SUCCESS, status);
    EXPECT_EQ(expectedStamp, pCmdQ->flushStamp->peekStamp());

    status = clReleaseEvent(event);
    EXPECT_EQ(CL_SUCCESS, status);
}

HWTEST_TEMPLATED_F(EnqueueKernelTestWithMockCsrHw2, givenCsrInBatchingModeWhenNonBlockingMapFollowsNdrCallThenFlushStampIsUpdatedProperly) {
    auto *mockCsr = static_cast<MockCsrHw2<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());
    mockCsr->overrideDispatchPolicy(DispatchMode::batchedDispatch);

    EXPECT_TRUE(this->destBuffer->isMemObjZeroCopy());
    MockKernelWithInternals mockKernel(*pClDevice);
    size_t gws[3] = {1, 0, 0};
    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    cl_event event;
    pCmdQ->enqueueMapBuffer(this->destBuffer, false, CL_MAP_READ, 0u, 1u, 0, nullptr, &event, this->retVal);

    pCmdQ->flush();
    auto neoEvent = castToObject<Event>(event);
    EXPECT_EQ(mockCsr->heaplessStateInitialized ? 2u : 1u, neoEvent->flushStamp->peekStamp());
    clReleaseEvent(event);
}

HWTEST_TEMPLATED_F(EnqueueKernelTestWithMockCsrHw2, givenCsrInBatchingModeWhenCommandWithEventIsFollowedByCommandWithoutEventThenFlushStampIsUpdatedInCommandQueueCsrAndEvent) {
    DebugManagerStateRestore restorer{};
    debugManager.flags.EnableL3FlushAfterPostSync.set(0);

    auto *mockCsr = static_cast<MockCsrHw2<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());
    mockCsr->overrideDispatchPolicy(DispatchMode::batchedDispatch);
    mockCsr->useNewResourceImplicitFlush = false;
    mockCsr->useGpuIdleImplicitFlush = false;

    auto mockedSubmissionsAggregator = new MockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    MockKernelWithInternals mockKernel(*pClDevice);
    size_t gws[3] = {1, 0, 0};
    cl_event event;
    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, &event);
    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    auto neoEvent = castToObject<Event>(event);

    auto expectedStamp = mockCsr->heaplessStateInitialized ? 1u : 0u;
    EXPECT_EQ(expectedStamp, mockCsr->flushStamp->peekStamp());
    EXPECT_EQ(expectedStamp, neoEvent->flushStamp->peekStamp());
    EXPECT_EQ(expectedStamp, pCmdQ->flushStamp->peekStamp());

    auto status = clWaitForEvents(1, &event);
    expectedStamp++;

    EXPECT_EQ(CL_SUCCESS, status);
    EXPECT_EQ(1, neoEvent->getRefInternalCount());
    EXPECT_EQ(expectedStamp, mockCsr->flushStamp->peekStamp());
    EXPECT_EQ(expectedStamp, neoEvent->flushStamp->peekStamp());
    EXPECT_EQ(expectedStamp, pCmdQ->flushStamp->peekStamp());

    status = clFinish(pCmdQ);
    EXPECT_EQ(CL_SUCCESS, status);
    EXPECT_EQ(expectedStamp, pCmdQ->flushStamp->peekStamp());

    status = clReleaseEvent(event);
    EXPECT_EQ(CL_SUCCESS, status);
}

HWTEST_TEMPLATED_F(EnqueueKernelTestWithMockCsrHw2, givenCsrInBatchingModeWhenClFlushIsCalledThenQueueFlushStampIsUpdated) {
    auto *mockCsr = static_cast<MockCsrHw2<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());
    mockCsr->overrideDispatchPolicy(DispatchMode::batchedDispatch);
    mockCsr->useNewResourceImplicitFlush = false;
    mockCsr->useGpuIdleImplicitFlush = false;

    MockKernelWithInternals mockKernel(*pClDevice);
    size_t gws[3] = {1, 0, 0};
    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    auto expectedStamp = mockCsr->heaplessStateInitialized ? 1u : 0u;
    EXPECT_EQ(expectedStamp, mockCsr->flushStamp->peekStamp());
    EXPECT_EQ(expectedStamp, pCmdQ->flushStamp->peekStamp());

    clFlush(pCmdQ);
    expectedStamp++;
    EXPECT_EQ(expectedStamp, mockCsr->flushStamp->peekStamp());
    EXPECT_EQ(expectedStamp, pCmdQ->flushStamp->peekStamp());
}

HWTEST_TEMPLATED_F(EnqueueKernelTestWithMockCsrHw2, givenCsrInBatchingModeWhenWaitForEventsIsCalledWithUnflushedTaskCountThenBatchedSubmissionsAreFlushed) {
    auto *mockCsr = static_cast<MockCsrHw2<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());
    mockCsr->overrideDispatchPolicy(DispatchMode::batchedDispatch);

    auto mockedSubmissionsAggregator = new MockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    MockKernelWithInternals mockKernel(*pClDevice);
    size_t gws[3] = {1, 0, 0};
    cl_event event;
    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    pCmdQ->enqueueMarkerWithWaitList(0, nullptr, &event);

    auto status = clWaitForEvents(1, &event);

    EXPECT_EQ(CL_SUCCESS, status);
    EXPECT_TRUE(mockedSubmissionsAggregator->peekCmdBufferList().peekIsEmpty());

    auto expectedFlushCalledCount = mockCsr->heaplessStateInitialized ? 2u : 1u;
    if (mockCsr->isUpdateTagFromWaitEnabled()) {
        expectedFlushCalledCount++;
    }

    EXPECT_EQ(expectedFlushCalledCount, mockCsr->flushCalledCount);

    status = clReleaseEvent(event);
    EXPECT_EQ(CL_SUCCESS, status);
}

HWTEST_TEMPLATED_F(EnqueueKernelTestWithMockCsrHw2, givenCsrInBatchingModeWhenFinishIsCalledWithUnflushedTaskCountThenBatchedSubmissionsAreFlushed) {

    DebugManagerStateRestore restorer{};
    debugManager.flags.EnableL3FlushAfterPostSync.set(0);

    auto *mockCsr = static_cast<MockCsrHw2<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());
    mockCsr->overrideDispatchPolicy(DispatchMode::batchedDispatch);

    auto mockedSubmissionsAggregator = new MockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    MockKernelWithInternals mockKernel(*pClDevice);
    size_t gws[3] = {1, 0, 0};
    cl_event event;
    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    pCmdQ->enqueueMarkerWithWaitList(0, nullptr, &event);

    auto status = clFinish(pCmdQ);

    EXPECT_EQ(CL_SUCCESS, status);
    EXPECT_TRUE(mockedSubmissionsAggregator->peekCmdBufferList().peekIsEmpty());
    EXPECT_EQ(mockCsr->heaplessStateInitialized ? 2u : 1u, mockCsr->flushCalledCount);

    status = clReleaseEvent(event);
    EXPECT_EQ(CL_SUCCESS, status);
}

HWTEST_TEMPLATED_F(EnqueueKernelTestWithMockCsrHw2, givenOutOfOrderCommandQueueWhenEnqueueKernelIsMadeThenPipeControlPositionIsRecorded) {
    DebugManagerStateRestore restorer{};
    debugManager.flags.EnableL3FlushAfterPostSync.set(0);

    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, 0};
    auto ooq = clCreateCommandQueueWithProperties(context, pClDevice, props, nullptr);

    auto *mockCsr = static_cast<MockCsrHw2<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());
    mockCsr->overrideDispatchPolicy(DispatchMode::batchedDispatch);
    mockCsr->useNewResourceImplicitFlush = false;
    mockCsr->useGpuIdleImplicitFlush = false;

    auto mockedSubmissionsAggregator = new MockSubmissionsAggregator();
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
    DebugManagerStateRestore restorer{};
    debugManager.flags.EnableL3FlushAfterPostSync.set(0);

    const cl_queue_properties props[] = {0};
    auto inOrderQueue = clCreateCommandQueueWithProperties(context, pClDevice, props, nullptr);

    auto &mockCsr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    mockCsr.overrideDispatchPolicy(DispatchMode::batchedDispatch);
    mockCsr.useNewResourceImplicitFlush = false;
    mockCsr.useGpuIdleImplicitFlush = false;
    auto mockedSubmissionsAggregator = new MockSubmissionsAggregator();
    mockCsr.submissionAggregator.reset(mockedSubmissionsAggregator);

    MockKernelWithInternals mockKernel(*pClDevice, context);
    size_t gws[3] = {1, 0, 0};
    clEnqueueNDRangeKernel(inOrderQueue, mockKernel.mockMultiDeviceKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    EXPECT_FALSE(mockedSubmissionsAggregator->peekCmdBufferList().peekIsEmpty());
    auto cmdBuffer = mockedSubmissionsAggregator->peekCmdBufferList().peekHead();
    EXPECT_NE(nullptr, cmdBuffer->pipeControlThatMayBeErasedLocation);

    clReleaseCommandQueue(inOrderQueue);
}

HWTEST_TEMPLATED_F(EnqueueKernelTestWithMockCsrHw2, givenInOrderCommandQueueWhenEnqueueKernelThatHasSharedObjectsAsArgIsMadeThenPipeControlPositionIsRecorded) {
    DebugManagerStateRestore restorer{};
    debugManager.flags.EnableL3FlushAfterPostSync.set(0);
    const cl_queue_properties props[] = {0};
    auto inOrderQueue = clCreateCommandQueueWithProperties(context, pClDevice, props, nullptr);

    auto *mockCsr = static_cast<MockCsrHw2<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());
    mockCsr->overrideDispatchPolicy(DispatchMode::batchedDispatch);
    mockCsr->useNewResourceImplicitFlush = false;
    mockCsr->useGpuIdleImplicitFlush = false;

    auto mockedSubmissionsAggregator = new MockSubmissionsAggregator();
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

HWTEST_TEMPLATED_F(EnqueueKernelTestWithMockCsrHw2, givenInOrderCommandQueueWhenEnqueueKernelThatHasSharedObjectsAsArgIsMadeThenPipeControlDoesntHaveDcFlush) {
    auto *mockCsr = static_cast<MockCsrHw2<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());
    mockCsr->overrideDispatchPolicy(DispatchMode::batchedDispatch);

    MockKernelWithInternals mockKernel(*pClDevice, context);
    size_t gws[3] = {1, 0, 0};
    mockKernel.mockKernel->setUsingSharedArgs(true);
    clEnqueueNDRangeKernel(this->pCmdQ, mockKernel.mockMultiDeviceKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    EXPECT_FALSE(mockCsr->passedDispatchFlags.dcFlush);
}

HWTEST_TEMPLATED_F(EnqueueKernelTestWithMockCsrHw2, givenInOrderCommandQueueWhenEnqueueKernelReturningEventIsMadeThenPipeControlPositionIsNotRecorded) {
    const cl_queue_properties props[] = {0};
    auto inOrderQueue = clCreateCommandQueueWithProperties(context, pClDevice, props, nullptr);

    auto *mockCsr = static_cast<MockCsrHw2<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());
    mockCsr->overrideDispatchPolicy(DispatchMode::batchedDispatch);
    mockCsr->useNewResourceImplicitFlush = false;
    mockCsr->useGpuIdleImplicitFlush = false;
    mockCsr->timestampPacketWriteEnabled = false;

    auto mockedSubmissionsAggregator = new MockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    MockKernelWithInternals mockKernel(*pClDevice, context);
    size_t gws[3] = {1, 0, 0};
    cl_event event;

    clEnqueueNDRangeKernel(inOrderQueue, mockKernel.mockMultiDeviceKernel, 1, nullptr, gws, nullptr, 0, nullptr, &event);

    EXPECT_FALSE(mockedSubmissionsAggregator->peekCmdBufferList().peekIsEmpty());
    auto cmdBuffer = mockedSubmissionsAggregator->peekCmdBufferList().peekHead();
    EXPECT_EQ(nullptr, cmdBuffer->pipeControlThatMayBeErasedLocation);

    if (mockCsr->isUpdateTagFromWaitEnabled()) {
        EXPECT_EQ(nullptr, cmdBuffer->epiloguePipeControlLocation);
    } else {
        EXPECT_NE(nullptr, cmdBuffer->epiloguePipeControlLocation);
    }

    clReleaseCommandQueue(inOrderQueue);
    clReleaseEvent(event);
}

HWTEST_TEMPLATED_F(EnqueueKernelTestWithMockCsrHw2, givenInOrderCommandQueueWhenEnqueueKernelReturningEventIsMadeAndCommandStreamReceiverIsInNTo1ModeThenPipeControlPositionIsRecorded) {
    DebugManagerStateRestore restorer{};
    debugManager.flags.EnableL3FlushAfterPostSync.set(0);

    const cl_queue_properties props[] = {0};

    auto inOrderQueue = clCreateCommandQueueWithProperties(context, pClDevice, props, nullptr);

    auto *mockCsr = static_cast<MockCsrHw2<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());
    mockCsr->overrideDispatchPolicy(DispatchMode::batchedDispatch);
    mockCsr->useNewResourceImplicitFlush = false;
    mockCsr->useGpuIdleImplicitFlush = false;
    mockCsr->enableNTo1SubmissionModel();

    auto mockedSubmissionsAggregator = new MockSubmissionsAggregator();
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

HWTEST_TEMPLATED_F(EnqueueKernelTestWithMockCsrHw2, givenOutOfOrderCommandQueueWhenEnqueueKernelReturningEventIsMadeThenPipeControlPositionIsRecorded) {
    DebugManagerStateRestore restorer{};
    debugManager.flags.EnableL3FlushAfterPostSync.set(0);

    auto *mockCsr = static_cast<MockCsrHw2<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());
    mockCsr->overrideDispatchPolicy(DispatchMode::batchedDispatch);
    mockCsr->useNewResourceImplicitFlush = false;
    mockCsr->useGpuIdleImplicitFlush = false;

    auto mockedSubmissionsAggregator = new MockSubmissionsAggregator();
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

HWTEST_TEMPLATED_F(EnqueueKernelTestWithMockCsrHw2, givenCsrInBatchingModeWhenBlockingCallIsMadeThenEventAssociatedWithCommandHasProperFlushStamp) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.MakeEachEnqueueBlocking.set(true);

    auto *mockCsr = static_cast<MockCsrHw2<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());
    mockCsr->overrideDispatchPolicy(DispatchMode::batchedDispatch);

    MockKernelWithInternals mockKernel(*pClDevice);
    size_t gws[3] = {1, 0, 0};
    cl_event event;
    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, &event);
    auto neoEvent = castToObject<Event>(event);
    auto expectedCount = mockCsr->heaplessStateInitialized ? 2u : 1u;

    EXPECT_EQ(expectedCount, neoEvent->flushStamp->peekStamp());

    if (mockCsr->isUpdateTagFromWaitEnabled()) {
        expectedCount++;
    }
    EXPECT_EQ(expectedCount, mockCsr->flushCalledCount);

    auto status = clReleaseEvent(event);
    EXPECT_EQ(CL_SUCCESS, status);
}

HWTEST_TEMPLATED_F(EnqueueKernelTestWithMockCsrHw2, givenKernelWhenItIsEnqueuedThenAllResourceGraphicsAllocationsAreUpdatedWithCsrTaskCount) {
    auto *mockCsr = static_cast<MockCsrHw2<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());

    MockKernelWithInternals mockKernel(*pClDevice);
    size_t gws[3] = {1, 0, 0};
    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    EXPECT_EQ(mockCsr->heaplessStateInitialized ? 2u : 1u, mockCsr->flushCalledCount);

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

    std::unique_ptr<MultiDeviceKernel> pMultiDeviceKernel(MultiDeviceKernel::create(pProgram, pProgram->getKernelInfosForKernel("CopyBuffer"), retVal));
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

HWTEST_TEMPLATED_F(EnqueueKernelTestWithMockCsrHw2, givenVMEKernelWhenEnqueueKernelThenDispatchFlagsHaveMediaSamplerRequired) {
    auto *mockCsr = static_cast<MockCsrHw2<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());
    mockCsr->overrideDispatchPolicy(DispatchMode::batchedDispatch);

    MockKernelWithInternals mockKernel(*pClDevice, context);
    size_t gws[3] = {1, 0, 0};
    mockKernel.kernelInfo.kernelDescriptor.kernelAttributes.flags.usesVme = true;
    clEnqueueNDRangeKernel(this->pCmdQ, mockKernel.mockMultiDeviceKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    EXPECT_TRUE(mockCsr->passedDispatchFlags.pipelineSelectArgs.mediaSamplerRequired);
}

HWTEST_TEMPLATED_F(EnqueueKernelTestWithMockCsrHw2, givenContextWithSeveralDevicesWhenEnqueueKernelThenDispatchFlagsHaveCorrectInfoAboutMultipleSubDevicesInContext) {
    auto *mockCsr = static_cast<MockCsrHw2<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());
    mockCsr->overrideDispatchPolicy(DispatchMode::batchedDispatch);

    MockKernelWithInternals mockKernel(*pClDevice, context);
    size_t gws[3] = {1, 0, 0};
    clEnqueueNDRangeKernel(this->pCmdQ, mockKernel.mockMultiDeviceKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    EXPECT_FALSE(mockCsr->passedDispatchFlags.areMultipleSubDevicesInContext);

    context->deviceBitfields[rootDeviceIndex].set(3, true);
    clEnqueueNDRangeKernel(this->pCmdQ, mockKernel.mockMultiDeviceKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    EXPECT_TRUE(mockCsr->passedDispatchFlags.areMultipleSubDevicesInContext);
    context->deviceBitfields[rootDeviceIndex].set(3, false);
}

HWTEST_TEMPLATED_F(EnqueueKernelTestWithMockCsrHw2, givenNonVMEKernelWhenEnqueueKernelThenDispatchFlagsDoesntHaveMediaSamplerRequired) {
    auto *mockCsr = static_cast<MockCsrHw2<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());
    mockCsr->overrideDispatchPolicy(DispatchMode::batchedDispatch);

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

HWTEST_F(EnqueueKernelTest, GivenForceMemoryPrefetchForKmdMigratedSharedAllocationsWhenEnqueingKernelWithoutSharedAllocationsThenMemoryPrefetchIsNotCalled) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.UseKmdMigration.set(true);
    debugManager.flags.ForceMemoryPrefetchForKmdMigratedSharedAllocations.set(true);

    MockKernelWithInternals mockKernel(*pClDevice);
    size_t gws[3] = {1, 1, 1};

    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    auto memoryManager = static_cast<MockMemoryManager *>(context->getMemoryManager());
    EXPECT_FALSE(memoryManager->setMemPrefetchCalled);
}

HWTEST_F(EnqueueKernelTest, GivenForceMemoryPrefetchForKmdMigratedSharedAllocationsWhenEnqueingKernelWithSharedAllocationsThenMemoryPrefetchIsCalled) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.UseKmdMigration.set(true);
    debugManager.flags.ForceMemoryPrefetchForKmdMigratedSharedAllocations.set(true);

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::sharedUnifiedMemory, 1, context->getRootDeviceIndices(), context->getDeviceBitfields());
    auto ptr = context->getSVMAllocsManager()->createSharedUnifiedMemoryAllocation(4096u, unifiedMemoryProperties, pCmdQ);
    EXPECT_NE(nullptr, ptr);

    MockKernelWithInternals mockKernel(*pClDevice);
    size_t gws[3] = {1, 1, 1};

    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    auto memoryManager = static_cast<MockMemoryManager *>(context->getMemoryManager());
    EXPECT_TRUE(memoryManager->setMemPrefetchCalled);

    context->getSVMAllocsManager()->freeSVMAlloc(ptr);
}

struct PauseOnGpuTests : public EnqueueKernelTest {
    void SetUp() override {
        EnqueueKernelTest::SetUp();

        auto &csr = pDevice->getGpgpuCommandStreamReceiver();
        debugPauseStateAddress = csr.getDebugPauseStateGPUAddress();

        auto &compilerProductHelper = pDevice->getCompilerProductHelper();
        auto heapless = compilerProductHelper.isHeaplessModeEnabled(*defaultHwInfo);
        heaplessStateInit = compilerProductHelper.isHeaplessStateInitEnabled(heapless);
    }

    template <typename FamilyType>
    bool verifySemaphore(const GenCmdList::iterator &iterator, uint64_t debugPauseStateAddress, DebugPauseState requiredDebugPauseState) {
        using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
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

            EXPECT_EQ(MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, this->pDevice->getRootDeviceEnvironment()), pipeControlCmd->getDcFlushEnable());

            EXPECT_EQ(PIPE_CONTROL::POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA, pipeControlCmd->getPostSyncOperation());

            return true;
        }

        return false;
    }

    template <typename FamilyType>
    bool verifyPipeControlNoPostSync(const GenCmdList::iterator &iterator) {
        using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

        auto pipeControlCmd = genCmdCast<PIPE_CONTROL *>(*iterator);
        const auto dcFlushEnable = MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, this->pDevice->getRootDeviceEnvironment());

        if (0u == pipeControlCmd->getImmediateData() && 0u == NEO::UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*pipeControlCmd) && dcFlushEnable == pipeControlCmd->getDcFlushEnable()) {

            EXPECT_TRUE(pipeControlCmd->getCommandStreamerStallEnable());

            EXPECT_EQ(PIPE_CONTROL::POST_SYNC_OPERATION::POST_SYNC_OPERATION_NO_WRITE, pipeControlCmd->getPostSyncOperation());

            return true;
        }

        return false;
    }

    template <typename FamilyType>
    bool verifyLoadRegImm(const GenCmdList::iterator &iterator) {
        using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
        uint32_t expectedRegisterOffset = debugManager.flags.GpuScratchRegWriteRegisterOffset.get();
        uint32_t expectedRegisterData = debugManager.flags.GpuScratchRegWriteRegisterData.get();
        auto loadRegImm = genCmdCast<MI_LOAD_REGISTER_IMM *>(*iterator);

        if ((expectedRegisterOffset == loadRegImm->getRegisterOffset()) &&
            (expectedRegisterData == loadRegImm->getDataDword())) {
            return true;
        }

        return false;
    }

    template <typename FamilyType>
    void findSemaphores(GenCmdList &cmdList) {
        using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
        auto semaphore = find<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());

        while (semaphore != cmdList.end()) {
            if (verifySemaphore<FamilyType>(semaphore, debugPauseStateAddress, DebugPauseState::hasUserStartConfirmation)) {
                semaphoreBeforeWalkerFound++;
            }

            if (verifySemaphore<FamilyType>(semaphore, debugPauseStateAddress, DebugPauseState::hasUserEndConfirmation)) {
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

    template <typename FamilyType>
    void findPipeControlsBeforeLoadRegImm(GenCmdList &cmdList) {
        using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
        using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

        auto itWalkers = NEO::UnitTestHelper<FamilyType>::findAllWalkerTypeCmds(cmdList.begin(), cmdList.end());
        for (auto walkerId = 0u; walkerId < itWalkers.size(); walkerId++) {

            auto threshold = walkerId + 1 < itWalkers.size() ? itWalkers[walkerId + 1] : cmdList.end();
            auto walker = itWalkers[walkerId];
            auto loadRegImm = find<MI_LOAD_REGISTER_IMM *>(walker, threshold);
            if (loadRegImm == threshold) {
                continue;
            }

            if (verifyLoadRegImm<FamilyType>(loadRegImm)) {
                auto pipeControl = find<PIPE_CONTROL *>(walker, loadRegImm);
                while (pipeControl != loadRegImm) {
                    if (verifyPipeControlNoPostSync<FamilyType>(pipeControl)) {
                        pipeControlsBeforeLoadRegImm++;
                    }
                    pipeControl = find<PIPE_CONTROL *>(++pipeControl, loadRegImm);
                }
            }
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
    uint32_t pipeControlsBeforeLoadRegImm = 0;
    bool heaplessStateInit = false;
};

HWTEST_F(PauseOnGpuTests, givenPauseOnEnqueueFlagSetWhenDispatchWalkersThenInsertPauseCommandsAroundSpecifiedEnqueue) {

    debugManager.flags.PauseOnEnqueue.set(1);

    MockKernelWithInternals mockKernel(*pClDevice);

    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, off, gws, nullptr, 0, nullptr, nullptr);
    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, off, gws, nullptr, 0, nullptr, nullptr);

    ClHardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(*pCmdQ);

    findSemaphores<FamilyType>(hwParser.cmdList);

    EXPECT_EQ(1u, semaphoreBeforeWalkerFound);
    EXPECT_EQ(1u, semaphoreAfterWalkerFound);

    findPipeControls<FamilyType>(hwParser.cmdList);

    EXPECT_EQ(1u, pipeControlBeforeWalkerFound);
    EXPECT_EQ(1u, pipeControlAfterWalkerFound);
}

HWTEST_F(PauseOnGpuTests, givenPauseOnEnqueueFlagSetToMinusTwoWhenDispatchWalkersThenInsertPauseCommandsAroundEachEnqueue) {

    debugManager.flags.PauseOnEnqueue.set(-2);

    MockKernelWithInternals mockKernel(*pClDevice);

    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, off, gws, nullptr, 0, nullptr, nullptr);
    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, off, gws, nullptr, 0, nullptr, nullptr);

    ClHardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(*pCmdQ);

    findSemaphores<FamilyType>(hwParser.cmdList);

    findPipeControls<FamilyType>(hwParser.cmdList);

    EXPECT_EQ(2u, semaphoreBeforeWalkerFound);
    EXPECT_EQ(2u, semaphoreAfterWalkerFound);
    EXPECT_EQ(2u, pipeControlBeforeWalkerFound);
    EXPECT_EQ(2u, pipeControlAfterWalkerFound);
}

HWTEST_F(PauseOnGpuTests, givenPauseModeSetToBeforeOnlyWhenDispatchingThenInsertPauseOnlyBeforeEnqueue) {
    if (heaplessStateInit) {
        GTEST_SKIP();
    }
    debugManager.flags.PauseOnEnqueue.set(0);
    debugManager.flags.PauseOnGpuMode.set(PauseOnGpuProperties::PauseMode::BeforeWorkload);

    MockKernelWithInternals mockKernel(*pClDevice);

    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, off, gws, nullptr, 0, nullptr, nullptr);

    ClHardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(*pCmdQ);

    findSemaphores<FamilyType>(hwParser.cmdList);

    findPipeControls<FamilyType>(hwParser.cmdList);

    EXPECT_EQ(1u, semaphoreBeforeWalkerFound);
    EXPECT_EQ(0u, semaphoreAfterWalkerFound);
    EXPECT_EQ(1u, pipeControlBeforeWalkerFound);
    EXPECT_EQ(0u, pipeControlAfterWalkerFound);
}

HWTEST_F(PauseOnGpuTests, givenPauseModeSetToAfterOnlyWhenDispatchingThenInsertPauseOnlyAfterEnqueue) {

    if (heaplessStateInit) {
        GTEST_SKIP();
    }

    debugManager.flags.PauseOnEnqueue.set(0);
    debugManager.flags.PauseOnGpuMode.set(PauseOnGpuProperties::PauseMode::AfterWorkload);

    MockKernelWithInternals mockKernel(*pClDevice);

    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, off, gws, nullptr, 0, nullptr, nullptr);

    ClHardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(*pCmdQ);

    findSemaphores<FamilyType>(hwParser.cmdList);

    findPipeControls<FamilyType>(hwParser.cmdList);

    EXPECT_EQ(0u, semaphoreBeforeWalkerFound);
    EXPECT_EQ(1u, semaphoreAfterWalkerFound);
    EXPECT_EQ(0u, pipeControlBeforeWalkerFound);
    EXPECT_EQ(1u, pipeControlAfterWalkerFound);
}

HWTEST_F(PauseOnGpuTests, givenPauseModeSetToBeforeAndAfterWhenDispatchingThenInsertPauseAroundEnqueue) {
    if (heaplessStateInit) {
        GTEST_SKIP();
    }
    debugManager.flags.PauseOnEnqueue.set(0);
    debugManager.flags.PauseOnGpuMode.set(PauseOnGpuProperties::PauseMode::BeforeAndAfterWorkload);

    MockKernelWithInternals mockKernel(*pClDevice);

    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, off, gws, nullptr, 0, nullptr, nullptr);

    ClHardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(*pCmdQ);

    findSemaphores<FamilyType>(hwParser.cmdList);

    findPipeControls<FamilyType>(hwParser.cmdList);

    EXPECT_EQ(1u, semaphoreBeforeWalkerFound);
    EXPECT_EQ(1u, semaphoreAfterWalkerFound);
    EXPECT_EQ(1u, pipeControlBeforeWalkerFound);
    EXPECT_EQ(1u, pipeControlAfterWalkerFound);
}

HWTEST_F(PauseOnGpuTests, givenPauseOnEnqueueFlagSetWhenDispatchWalkersThenDontInsertPauseCommandsWhenUsingSpecialQueue) {
    debugManager.flags.PauseOnEnqueue.set(0);

    pCmdQ->setIsSpecialCommandQueue(true);

    MockKernelWithInternals mockKernel(*pClDevice);

    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, off, gws, nullptr, 0, nullptr, nullptr);

    ClHardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(*pCmdQ);

    findSemaphores<FamilyType>(hwParser.cmdList);

    findPipeControls<FamilyType>(hwParser.cmdList);

    EXPECT_EQ(0u, semaphoreBeforeWalkerFound);
    EXPECT_EQ(0u, semaphoreAfterWalkerFound);
    EXPECT_EQ(0u, pipeControlBeforeWalkerFound);
    EXPECT_EQ(0u, pipeControlAfterWalkerFound);

    pCmdQ->setIsSpecialCommandQueue(false);
}

HWTEST_F(PauseOnGpuTests, givenGpuScratchWriteEnabledWhenDispatchWalkersThenInsertLoadRegisterImmCommandAroundSpecifiedEnqueue) {
    debugManager.flags.GpuScratchRegWriteAfterWalker.set(1);
    debugManager.flags.GpuScratchRegWriteRegisterData.set(0x1234);
    debugManager.flags.GpuScratchRegWriteRegisterOffset.set(0x5678);

    MockKernelWithInternals mockKernel(*pClDevice);

    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, off, gws, nullptr, 0, nullptr, nullptr);

    ClHardwareParse hwParser;

    hwParser.parseCommands<FamilyType>(*pCmdQ);

    findLoadRegImms<FamilyType>(hwParser.cmdList);

    EXPECT_EQ(pCmdQ->getHeaplessStateInitEnabled() ? 1u : 0u, loadRegImmsFound);

    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, off, gws, nullptr, 0, nullptr, nullptr);
    hwParser.parseCommands<FamilyType>(*pCmdQ);

    findLoadRegImms<FamilyType>(hwParser.cmdList);

    EXPECT_EQ(pCmdQ->getHeaplessStateInitEnabled() ? 2u : 1u, loadRegImmsFound);
}

HWTEST_F(PauseOnGpuTests, givenGpuScratchWriteEnabledWhenDispatchMultiplehWalkersThenInsertPipeControlAndLoadRegisterImmCommandsOnlyOnce) {
    debugManager.flags.GpuScratchRegWriteAfterWalker.set(2);
    debugManager.flags.GpuScratchRegWriteRegisterData.set(0x1234);
    debugManager.flags.GpuScratchRegWriteRegisterOffset.set(0x5678);

    MockKernelWithInternals mockKernel(*pClDevice);

    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, off, gws, nullptr, 0, nullptr, nullptr);
    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, off, gws, nullptr, 0, nullptr, nullptr);
    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, off, gws, nullptr, 0, nullptr, nullptr);
    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, off, gws, nullptr, 0, nullptr, nullptr);

    ClHardwareParse hwParser;

    hwParser.parseCommands<FamilyType>(*pCmdQ);

    findLoadRegImms<FamilyType>(hwParser.cmdList);
    findPipeControlsBeforeLoadRegImm<FamilyType>(hwParser.cmdList);

    EXPECT_EQ(1u, loadRegImmsFound);
    EXPECT_EQ(1u, pipeControlsBeforeLoadRegImm);
}

HWTEST_F(PauseOnGpuTests, givenGpuScratchWriteEnabledWhenEstimatingCommandStreamSizeThenMiLoadRegisterImmCommandSizeIsIncluded) {
    MockKernelWithInternals mockKernel(*pClDevice);
    DispatchInfo dispatchInfo;
    MultiDispatchInfo multiDispatchInfo(mockKernel.mockKernel);
    dispatchInfo.setKernel(mockKernel.mockKernel);
    multiDispatchInfo.push(dispatchInfo);

    auto baseCommandStreamSize = EnqueueOperation<FamilyType>::getTotalSizeRequiredCS(CL_COMMAND_NDRANGE_KERNEL, {}, false, false, false, *pCmdQ, multiDispatchInfo, false, false, false, nullptr);
    debugManager.flags.GpuScratchRegWriteAfterWalker.set(1);

    auto extendedCommandStreamSize = EnqueueOperation<FamilyType>::getTotalSizeRequiredCS(CL_COMMAND_NDRANGE_KERNEL, {}, false, false, false, *pCmdQ, multiDispatchInfo, false, false, false, nullptr);

    EXPECT_EQ(baseCommandStreamSize + sizeof(typename FamilyType::MI_LOAD_REGISTER_IMM), extendedCommandStreamSize);
}

HWTEST_F(PauseOnGpuTests, givenResolveDependenciesByPipecontrolWhenEstimatingCommandStreamSizeThenStallingBarrierSizeIsIncluded) {
    MockKernelWithInternals mockKernel(*pClDevice);
    DispatchInfo dispatchInfo;
    MultiDispatchInfo multiDispatchInfo(mockKernel.mockKernel);
    dispatchInfo.setKernel(mockKernel.mockKernel);
    multiDispatchInfo.push(dispatchInfo);
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.timestampPacketWriteEnabled = true;

    auto baseCommandStreamSize = EnqueueOperation<FamilyType>::getTotalSizeRequiredCS(CL_COMMAND_NDRANGE_KERNEL, {}, false, false, false, *pCmdQ, multiDispatchInfo, false, false, false, nullptr);

    auto extendedCommandStreamSize = EnqueueOperation<FamilyType>::getTotalSizeRequiredCS(CL_COMMAND_NDRANGE_KERNEL, {}, false, false, false, *pCmdQ, multiDispatchInfo, false, false, true, nullptr);

    EXPECT_EQ(baseCommandStreamSize + MemorySynchronizationCommands<FamilyType>::getSizeForStallingBarrier(), extendedCommandStreamSize);
}

HWTEST_F(PauseOnGpuTests, givenTimestampPacketWriteDisabledAndMarkerWithProfilingWhenEstimatingCommandStreamSizeThenStoreMMIOSizeIsIncluded) {
    MockKernelWithInternals mockKernel(*pClDevice);
    DispatchInfo dispatchInfo;
    MultiDispatchInfo multiDispatchInfo(mockKernel.mockKernel);
    dispatchInfo.setKernel(mockKernel.mockKernel);
    multiDispatchInfo.push(dispatchInfo);
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.timestampPacketWriteEnabled = false;

    struct FakeGfxCoreHelper : GfxCoreHelperHw<FamilyType> {
        bool useOnlyGlobalTimestampsValue{false};
        bool useOnlyGlobalTimestamps() const override {
            return useOnlyGlobalTimestampsValue;
        }
    };
    RAIIGfxCoreHelperFactory<FakeGfxCoreHelper> overrideGfxCoreHelper{*pDevice->executionEnvironment->rootDeviceEnvironments[0]};

    overrideGfxCoreHelper.mockGfxCoreHelper->useOnlyGlobalTimestampsValue = true;
    auto baseCommandStreamSize = EnqueueOperation<FamilyType>::getTotalSizeRequiredCS(CL_COMMAND_NDRANGE_KERNEL, {}, false, false, false, *pCmdQ, multiDispatchInfo, true, false, false, nullptr);
    overrideGfxCoreHelper.mockGfxCoreHelper->useOnlyGlobalTimestampsValue = false;
    auto extendedCommandStreamSize = EnqueueOperation<FamilyType>::getTotalSizeRequiredCS(CL_COMMAND_NDRANGE_KERNEL, {}, false, false, false, *pCmdQ, multiDispatchInfo, true, false, true, nullptr);

    EXPECT_EQ(baseCommandStreamSize + 2 * EncodeStoreMMIO<FamilyType>::size, extendedCommandStreamSize);
}
