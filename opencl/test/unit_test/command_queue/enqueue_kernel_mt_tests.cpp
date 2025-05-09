/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/kernel_binary_helper.h"
#include "shared/test/common/mocks/mock_csr.h"
#include "shared/test/common/mocks/mock_submissions_aggregator.h"

#include "opencl/source/command_queue/command_queue_hw.h"
#include "opencl/test/unit_test/command_queue/enqueue_fixture.h"
#include "opencl/test/unit_test/fixtures/hello_world_fixture.h"

typedef HelloWorldFixture<HelloWorldFixtureFactory> EnqueueKernelFixture;
typedef Test<EnqueueKernelFixture> EnqueueKernelTest;

class EnqueueKernelTestWithMockCsrHw2
    : public EnqueueKernelTest {
  public:
    void SetUp() override {}
    void TearDown() override {}

    template <typename FamilyType>
    void setUpT() {
        EnvironmentWithCsrWrapper environment;
        environment.setCsrType<MockCsrHw2<FamilyType>>();
        EnqueueKernelTest::SetUp();
    }

    template <typename FamilyType>
    void tearDownT() {
        EnqueueKernelTest::TearDown();
    }
};

HWTEST_TEMPLATED_F(EnqueueKernelTestWithMockCsrHw2, givenCsrInBatchingModeWhenFinishIsCalledThenBatchesSubmissionsAreFlushed) {
    auto mockCsr = static_cast<MockCsrHw2<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());

    mockCsr->overrideDispatchPolicy(DispatchMode::batchedDispatch);

    auto mockedSubmissionsAggregator = new MockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    std::atomic<bool> startEnqueueProcess(false);

    MockKernelWithInternals mockKernel(*pClDevice);
    size_t gws[3] = {1, 0, 0};

    auto enqueueCount = 10;
    auto threadCount = 4;

    auto function = [&]() {
        // wait until we are signalled
        while (!startEnqueueProcess)
            ;
        for (int enqueue = 0; enqueue < enqueueCount; enqueue++) {
            pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
        }
    };

    std::vector<std::thread> threads;
    for (auto thread = 0; thread < threadCount; thread++) {
        threads.push_back(std::thread(function));
    }

    int64_t currentTaskCount = 0;

    startEnqueueProcess = true;

    // call a flush while other threads enqueue, we can't drop anything
    while (currentTaskCount < enqueueCount * threadCount) {
        clFlush(pCmdQ);
        auto locker = mockCsr->obtainUniqueOwnership();
        currentTaskCount = mockCsr->peekTaskCount();
    }

    for (auto &thread : threads) {
        thread.join();
    }

    pCmdQ->finish();

    EXPECT_GE(mockCsr->flushCalledCount, 1u);

    EXPECT_LE(static_cast<int>(mockCsr->flushCalledCount), enqueueCount * threadCount);
    EXPECT_EQ(mockedSubmissionsAggregator->peekInspectionId() - 1u, mockCsr->flushCalledCount);
}

template <typename GfxFamily>
struct MockCommandQueueHw : public CommandQueueHw<GfxFamily> {
    using CommandQueue::bcsInitialized;
};

HWTEST_F(EnqueueKernelTest, givenTwoThreadsAndBcsEnabledWhenEnqueueWriteBufferAndEnqueueNDRangeKernelInLoopThenIsNoRace) {
    DebugManagerStateRestore debugRestorer;
    debugManager.flags.ForceCsrLockInBcsEnqueueOnlyForGpgpuSubmission.set(1);
    HardwareInfo hwInfo = *pDevice->executionEnvironment->rootDeviceEnvironments[0]->getMutableHardwareInfo();
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    REQUIRE_FULL_BLITTER_OR_SKIP(*pDevice->executionEnvironment->rootDeviceEnvironments[0]);

    std::atomic<bool> startEnqueueProcess(false);

    auto iterationCount = 40;
    auto threadCount = 2;

    constexpr size_t n = 256;
    unsigned int data[n] = {};
    constexpr size_t bufferSize = n * sizeof(unsigned int);

    size_t gws[3] = {1, 0, 0};
    size_t gwsSize[3] = {n, 1, 1};
    size_t lws[3] = {1, 1, 1};
    cl_uint workDim = 1;

    KernelBinaryHelper kbHelper("CopyBuffer_simd16", false);
    std::string testFile;
    testFile.append(clFiles);
    testFile.append("CopyBuffer_simd16.cl");
    size_t sourceSize = 0;
    auto pSource = loadDataFromFile(testFile.c_str(), sourceSize);
    EXPECT_NE(0u, sourceSize);
    EXPECT_NE(nullptr, pSource);

    MockClDevice mockClDevice{MockDevice::createWithExecutionEnvironment<MockDevice>(&hwInfo, pDevice->executionEnvironment, 0)};

    const cl_device_id deviceId = &mockClDevice;
    auto context = clCreateContext(nullptr, 1, &deviceId, nullptr, nullptr, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, context);

    auto queue = clCreateCommandQueue(context, deviceId, 0, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, queue);

    const char *sources[1] = {pSource.get()};
    auto program = clCreateProgramWithSource(
        context,
        1,
        sources,
        &sourceSize,
        &retVal);
    ASSERT_NE(nullptr, program);

    retVal = clBuildProgram(
        program,
        1,
        &deviceId,
        nullptr,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto kernel = clCreateKernel(program, "CopyBuffer", &retVal);
    ASSERT_NE(nullptr, kernel);
    EXPECT_EQ(CL_SUCCESS, retVal);

    cl_mem_flags flags = CL_MEM_READ_WRITE;
    auto buffer0 = clCreateBuffer(context, flags, bufferSize, nullptr, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    auto buffer1 = clCreateBuffer(context, flags, bufferSize, nullptr, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clSetKernelArg(kernel, 0, sizeof(cl_mem), &buffer0);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clSetKernelArg(kernel, 1, sizeof(cl_mem), &buffer1);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto function = [&]() {
        while (!startEnqueueProcess)
            ;
        cl_int fRetVal;
        for (int i = 0; i < iterationCount; i++) {
            fRetVal = clEnqueueWriteBuffer(queue, buffer0, false, 0, bufferSize, data, 0, nullptr, nullptr);
            EXPECT_EQ(CL_SUCCESS, fRetVal);
            fRetVal = clEnqueueNDRangeKernel(queue, kernel, workDim, gws, gwsSize, lws, 0, nullptr, nullptr);
            EXPECT_EQ(CL_SUCCESS, fRetVal);
        }
    };

    std::vector<std::thread> threads;
    for (auto thread = 0; thread < threadCount; thread++) {
        threads.push_back(std::thread(function));
    }

    startEnqueueProcess = true;

    for (auto &thread : threads) {
        thread.join();
    }

    EXPECT_TRUE(NEO::castToObject<MockCommandQueueHw<FamilyType>>(queue)->bcsInitialized);

    retVal = clFinish(queue);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseMemObject(buffer0);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseMemObject(buffer1);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseKernel(kernel);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseProgram(program);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseCommandQueue(queue);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseContext(context);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

HWTEST_F(EnqueueKernelTest, givenBcsEnabledWhenThread1EnqueueWriteBufferAndThread2EnqueueNDRangeKernelInLoopThenIsNoRace) {
    DebugManagerStateRestore debugRestorer;
    debugManager.flags.ForceCsrLockInBcsEnqueueOnlyForGpgpuSubmission.set(1);
    HardwareInfo hwInfo = *pDevice->executionEnvironment->rootDeviceEnvironments[0]->getMutableHardwareInfo();
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    REQUIRE_FULL_BLITTER_OR_SKIP(*pDevice->executionEnvironment->rootDeviceEnvironments[0]);

    std::atomic<bool> startEnqueueProcess(false);

    auto iterationCount = 40;

    constexpr size_t n = 256;
    unsigned int data[n] = {};
    constexpr size_t bufferSize = n * sizeof(unsigned int);

    size_t gws[3] = {1, 0, 0};
    size_t gwsSize[3] = {n, 1, 1};
    size_t lws[3] = {1, 1, 1};
    cl_uint workDim = 1;

    KernelBinaryHelper kbHelper("CopyBuffer_simd16", false);
    std::string testFile;
    testFile.append(clFiles);
    testFile.append("CopyBuffer_simd16.cl");
    size_t sourceSize = 0;
    auto pSource = loadDataFromFile(testFile.c_str(), sourceSize);
    EXPECT_NE(0u, sourceSize);
    EXPECT_NE(nullptr, pSource);

    MockClDevice mockClDevice{MockDevice::createWithExecutionEnvironment<MockDevice>(&hwInfo, pDevice->executionEnvironment, 0)};

    const cl_device_id deviceId = &mockClDevice;
    auto context = clCreateContext(nullptr, 1, &deviceId, nullptr, nullptr, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, context);

    auto queue = clCreateCommandQueue(context, deviceId, 0, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, queue);

    const char *sources[1] = {pSource.get()};
    auto program = clCreateProgramWithSource(
        context,
        1,
        sources,
        &sourceSize,
        &retVal);
    ASSERT_NE(nullptr, program);

    retVal = clBuildProgram(
        program,
        1,
        &deviceId,
        nullptr,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto kernel = clCreateKernel(program, "CopyBuffer", &retVal);
    ASSERT_NE(nullptr, kernel);
    EXPECT_EQ(CL_SUCCESS, retVal);

    cl_mem_flags flags = CL_MEM_READ_WRITE;
    auto buffer0 = clCreateBuffer(context, flags, bufferSize, nullptr, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    auto buffer1 = clCreateBuffer(context, flags, bufferSize, nullptr, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clSetKernelArg(kernel, 0, sizeof(cl_mem), &buffer0);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clSetKernelArg(kernel, 1, sizeof(cl_mem), &buffer1);
    EXPECT_EQ(CL_SUCCESS, retVal);

    std::vector<std::thread::id> threadsIds;
    auto functionEnqueueWriteBuffer = [&]() {
        while (!startEnqueueProcess)
            ;
        cl_int fRetVal;
        for (int i = 0; i < iterationCount; i++) {
            fRetVal = clEnqueueWriteBuffer(queue, buffer0, false, 0, bufferSize, data, 0, nullptr, nullptr);
            EXPECT_EQ(CL_SUCCESS, fRetVal);
        }
    };
    auto functionEnqueueNDRangeKernel = [&]() {
        while (!startEnqueueProcess)
            ;
        cl_int fRetVal;
        for (int i = 0; i < iterationCount; i++) {
            fRetVal = clEnqueueNDRangeKernel(queue, kernel, workDim, gws, gwsSize, lws, 0, nullptr, nullptr);
            EXPECT_EQ(CL_SUCCESS, fRetVal);
        }
    };

    std::vector<std::thread> threads;
    threads.push_back(std::thread(functionEnqueueWriteBuffer));
    threads.push_back(std::thread(functionEnqueueNDRangeKernel));

    startEnqueueProcess = true;

    for (auto &thread : threads) {
        thread.join();
    }

    EXPECT_TRUE(NEO::castToObject<MockCommandQueueHw<FamilyType>>(queue)->bcsInitialized);

    retVal = clFinish(queue);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseMemObject(buffer0);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseMemObject(buffer1);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseKernel(kernel);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseProgram(program);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseCommandQueue(queue);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseContext(context);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

HWTEST_F(EnqueueKernelTest, givenBcsEnabledAndQueuePerThreadWhenEnqueueWriteBufferAndEnqueueNDRangeKernelInLoopThenIsNoRace) {
    DebugManagerStateRestore debugRestorer;
    debugManager.flags.ForceCsrLockInBcsEnqueueOnlyForGpgpuSubmission.set(1);
    HardwareInfo hwInfo = *pDevice->executionEnvironment->rootDeviceEnvironments[0]->getMutableHardwareInfo();
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    REQUIRE_FULL_BLITTER_OR_SKIP(*pDevice->executionEnvironment->rootDeviceEnvironments[0]);

    std::atomic<bool> startEnqueueProcess(false);

    auto iterationCount = 40;
    const auto threadCount = 10;

    constexpr size_t n = 256;
    unsigned int data[n] = {};
    constexpr size_t bufferSize = n * sizeof(unsigned int);

    size_t gws[3] = {1, 0, 0};
    size_t gwsSize[3] = {n, 1, 1};
    size_t lws[3] = {1, 1, 1};
    cl_uint workDim = 1;

    KernelBinaryHelper kbHelper("CopyBuffer_simd16", false);
    std::string testFile;
    testFile.append(clFiles);
    testFile.append("CopyBuffer_simd16.cl");
    size_t sourceSize = 0;
    auto pSource = loadDataFromFile(testFile.c_str(), sourceSize);
    EXPECT_NE(0u, sourceSize);
    EXPECT_NE(nullptr, pSource);

    MockClDevice mockClDevice{MockDevice::createWithExecutionEnvironment<MockDevice>(&hwInfo, pDevice->executionEnvironment, 0)};

    const cl_device_id deviceId = &mockClDevice;
    auto context = clCreateContext(nullptr, 1, &deviceId, nullptr, nullptr, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, context);

    const char *sources[1] = {pSource.get()};
    auto program = clCreateProgramWithSource(
        context,
        1,
        sources,
        &sourceSize,
        &retVal);
    ASSERT_NE(nullptr, program);

    retVal = clBuildProgram(
        program,
        1,
        &deviceId,
        nullptr,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto kernel = clCreateKernel(program, "CopyBuffer", &retVal);
    ASSERT_NE(nullptr, kernel);
    EXPECT_EQ(CL_SUCCESS, retVal);

    cl_mem_flags flags = CL_MEM_READ_WRITE;
    auto buffer0 = clCreateBuffer(context, flags, bufferSize, nullptr, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    auto buffer1 = clCreateBuffer(context, flags, bufferSize, nullptr, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clSetKernelArg(kernel, 0, sizeof(cl_mem), &buffer0);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clSetKernelArg(kernel, 1, sizeof(cl_mem), &buffer1);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto function = [&]() {
        while (!startEnqueueProcess)
            ;
        cl_int fRetVal;
        auto queue = clCreateCommandQueue(context, deviceId, 0, &fRetVal);
        EXPECT_EQ(CL_SUCCESS, fRetVal);
        EXPECT_NE(nullptr, queue);
        for (int i = 0; i < iterationCount; i++) {

            fRetVal = clEnqueueWriteBuffer(queue, buffer0, false, 0, bufferSize, data, 0, nullptr, nullptr);
            EXPECT_EQ(CL_SUCCESS, fRetVal);
            fRetVal = clEnqueueNDRangeKernel(queue, kernel, workDim, gws, gwsSize, lws, 0, nullptr, nullptr);
            EXPECT_EQ(CL_SUCCESS, fRetVal);
        }
        fRetVal = clFinish(queue);
        EXPECT_EQ(CL_SUCCESS, fRetVal);
        fRetVal = clReleaseCommandQueue(queue);
        EXPECT_EQ(CL_SUCCESS, fRetVal);
    };

    std::vector<std::thread> threads;
    for (auto thread = 0; thread < threadCount; thread++) {
        threads.push_back(std::thread(function));
    }

    startEnqueueProcess = true;

    for (auto &thread : threads) {
        thread.join();
    }

    retVal = clReleaseMemObject(buffer0);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseMemObject(buffer1);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseKernel(kernel);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseProgram(program);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseContext(context);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

HWTEST_F(EnqueueKernelTest, givenBcsEnabledAndQueuePerThreadWhenHalfQueuesEnqueueWriteBufferAndSecondHalfEnqueueNDRangeKernelInLoopThenIsNoRace) {
    DebugManagerStateRestore debugRestorer;
    debugManager.flags.ForceCsrLockInBcsEnqueueOnlyForGpgpuSubmission.set(1);
    HardwareInfo hwInfo = *pDevice->executionEnvironment->rootDeviceEnvironments[0]->getMutableHardwareInfo();
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    REQUIRE_FULL_BLITTER_OR_SKIP(*pDevice->executionEnvironment->rootDeviceEnvironments[0]);

    std::atomic<bool> startEnqueueProcess(false);

    auto iterationCount = 40;
    const auto threadCount = 10;

    constexpr size_t n = 256;
    unsigned int data[n] = {};
    constexpr size_t bufferSize = n * sizeof(unsigned int);

    size_t gws[3] = {1, 0, 0};
    size_t gwsSize[3] = {n, 1, 1};
    size_t lws[3] = {1, 1, 1};
    cl_uint workDim = 1;

    KernelBinaryHelper kbHelper("CopyBuffer_simd16", false);
    std::string testFile;
    testFile.append(clFiles);
    testFile.append("CopyBuffer_simd16.cl");
    size_t sourceSize = 0;
    auto pSource = loadDataFromFile(testFile.c_str(), sourceSize);
    EXPECT_NE(0u, sourceSize);
    EXPECT_NE(nullptr, pSource);

    MockClDevice mockClDevice{MockDevice::createWithExecutionEnvironment<MockDevice>(&hwInfo, pDevice->executionEnvironment, 0)};

    const cl_device_id deviceId = &mockClDevice;
    auto context = clCreateContext(nullptr, 1, &deviceId, nullptr, nullptr, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, context);

    const char *sources[1] = {pSource.get()};
    auto program = clCreateProgramWithSource(
        context,
        1,
        sources,
        &sourceSize,
        &retVal);
    ASSERT_NE(nullptr, program);

    retVal = clBuildProgram(
        program,
        1,
        &deviceId,
        nullptr,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto kernel = clCreateKernel(program, "CopyBuffer", &retVal);
    ASSERT_NE(nullptr, kernel);
    EXPECT_EQ(CL_SUCCESS, retVal);

    cl_mem_flags flags = CL_MEM_READ_WRITE;
    auto buffer0 = clCreateBuffer(context, flags, bufferSize, nullptr, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    auto buffer1 = clCreateBuffer(context, flags, bufferSize, nullptr, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clSetKernelArg(kernel, 0, sizeof(cl_mem), &buffer0);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clSetKernelArg(kernel, 1, sizeof(cl_mem), &buffer1);
    EXPECT_EQ(CL_SUCCESS, retVal);

    std::vector<std::thread::id> threadsIds;
    auto functionEnqueueWriteBuffer = [&]() {
        while (!startEnqueueProcess)
            ;
        cl_int fRetVal;
        auto queue = clCreateCommandQueue(context, deviceId, 0, &fRetVal);
        EXPECT_EQ(CL_SUCCESS, fRetVal);
        EXPECT_NE(nullptr, queue);
        for (int i = 0; i < iterationCount; i++) {
            fRetVal = clEnqueueWriteBuffer(queue, buffer0, false, 0, bufferSize, data, 0, nullptr, nullptr);
            EXPECT_EQ(CL_SUCCESS, fRetVal);
        }
        fRetVal = clFinish(queue);
        EXPECT_EQ(CL_SUCCESS, fRetVal);
        EXPECT_TRUE(NEO::castToObject<MockCommandQueueHw<FamilyType>>(queue)->bcsInitialized);
        fRetVal = clReleaseCommandQueue(queue);
        EXPECT_EQ(CL_SUCCESS, fRetVal);
    };
    auto functionEnqueueNDRangeKernel = [&]() {
        while (!startEnqueueProcess)
            ;
        cl_int fRetVal;
        auto queue = clCreateCommandQueue(context, deviceId, 0, &fRetVal);
        EXPECT_EQ(CL_SUCCESS, fRetVal);
        EXPECT_NE(nullptr, queue);
        for (int i = 0; i < iterationCount; i++) {
            fRetVal = clEnqueueNDRangeKernel(queue, kernel, workDim, gws, gwsSize, lws, 0, nullptr, nullptr);
            EXPECT_EQ(CL_SUCCESS, fRetVal);
        }
        fRetVal = clFinish(queue);
        EXPECT_EQ(CL_SUCCESS, fRetVal);
        EXPECT_TRUE(NEO::castToObject<MockCommandQueueHw<FamilyType>>(queue)->bcsInitialized);
        fRetVal = clReleaseCommandQueue(queue);
        EXPECT_EQ(CL_SUCCESS, fRetVal);
    };

    std::vector<std::thread> threads;
    for (auto thread = 0; thread < threadCount; thread++) {
        if (thread < threadCount / 2) {
            threads.push_back(std::thread(functionEnqueueNDRangeKernel));
        } else {
            threads.push_back(std::thread(functionEnqueueWriteBuffer));
        }
    }

    startEnqueueProcess = true;

    for (auto &thread : threads) {
        thread.join();
    }

    retVal = clReleaseMemObject(buffer0);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseMemObject(buffer1);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseKernel(kernel);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseProgram(program);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseContext(context);
    EXPECT_EQ(CL_SUCCESS, retVal);
}