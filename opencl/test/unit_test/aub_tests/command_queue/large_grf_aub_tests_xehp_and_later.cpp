/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/common/utilities/base_object_utils.h"

#include "opencl/source/mem_obj/buffer.h"
#include "opencl/test/unit_test/aub_tests/fixtures/aub_fixture.h"
#include "opencl/test/unit_test/aub_tests/fixtures/multicontext_ocl_aub_fixture.h"
#include "opencl/test/unit_test/fixtures/program_fixture.h"
#include "opencl/test/unit_test/helpers/cl_hw_parse.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"

#include "test_traits_common.h"

using namespace NEO;

class AUBRunLargeGrfKernelFixture : public ProgramFixture {
  public:
    AUBRunLargeGrfKernelFixture()
        : indexMemory(nullptr, alignedFree), sourceMemory(nullptr, alignedFree),
          destinationMemory(nullptr, alignedFree),
          smallGrfSourceMemory(nullptr, alignedFree),
          smallGrfDestinationMemory(nullptr, alignedFree) {}

  protected:
    void setUp() {
        debugManagerStateRestore.reset(new DebugManagerStateRestore());
        bool staticLargeGrfMode = getLargeGrfConfig();
        if (staticLargeGrfMode) {
            setStaticLargeGrfDebugFlags();
        }
        ProgramFixture::setUp();
    }

    void tearDown() {
        ProgramFixture::tearDown();
    }

    virtual bool getLargeGrfConfig() const = 0;
    virtual Context *getContext() const = 0;
    virtual ClDevice *getDevice() const = 0;

    void setStaticLargeGrfDebugFlags() {
        debugManager.flags.ForceRunAloneContext.set(1);
        debugManager.flags.AubDumpAddMmioRegistersList.set(
            "0xE48C;0x2020202;0xE4F0;0x20002");
    }

    void createKernels(bool largeOnly) {
        createProgramFromBinary(getContext(), getContext()->getDevices(),
                                "spill_fill_kernel_large_grf");
        ASSERT_NE(nullptr, pProgram);
        auto rootDeviceIndex = pProgram->getDevices()[0]->getRootDeviceIndex();
        largeGrfProgram.reset(pProgram);
        pProgram = nullptr;

        auto retVal =
            largeGrfProgram->build(largeGrfProgram->getDevices(), nullptr);
        ASSERT_EQ(CL_SUCCESS, retVal);

        const KernelInfo *kernelInfoLargeGrf =
            largeGrfProgram->getKernelInfo("spillFillKernelLargeGRF", rootDeviceIndex);
        ASSERT_NE(nullptr, kernelInfoLargeGrf);

        EXPECT_EQ(
            256u,
            kernelInfoLargeGrf->kernelDescriptor.kernelAttributes.numGrfRequired);

        largeGrfMultiDeviceKernel.reset(MultiDeviceKernel::create<MockKernel>(
            largeGrfProgram.get(), MockKernel::toKernelInfoContainer(*kernelInfoLargeGrf, rootDeviceIndex), retVal));
        EXPECT_EQ(CL_SUCCESS, retVal);
        ASSERT_NE(nullptr, largeGrfMultiDeviceKernel.get());
        largeGrfKernel = largeGrfMultiDeviceKernel->getKernel(rootDeviceIndex);
        ASSERT_NE(nullptr, largeGrfKernel);

        if (!largeOnly) {
            createProgramFromBinary(getContext(), getContext()->getDevices(),
                                    "simple_kernels");
            ASSERT_NE(nullptr, pProgram);

            retVal = pProgram->build(pProgram->getDevices(), nullptr);
            ASSERT_EQ(CL_SUCCESS, retVal);
            const KernelInfo *kernelInfoSmallGrf =
                pProgram->getKernelInfo("simple_kernel_1", rootDeviceIndex);
            ASSERT_NE(nullptr, kernelInfoSmallGrf);
            EXPECT_LE(
                kernelInfoSmallGrf->kernelDescriptor.kernelAttributes.numGrfRequired,
                128u);

            smallGrfMultiDeviceKernel.reset(
                MultiDeviceKernel::create<MockKernel>(pProgram, MockKernel::toKernelInfoContainer(*kernelInfoSmallGrf, rootDeviceIndex), retVal));
            EXPECT_EQ(CL_SUCCESS, retVal);
            ASSERT_NE(nullptr, smallGrfMultiDeviceKernel.get());
            smallGrfKernel = smallGrfMultiDeviceKernel->getKernel(rootDeviceIndex);
            ASSERT_NE(nullptr, smallGrfKernel);
        }
    }

    void fillLargeGrfBuffers(cl_long *source, cl_long *destination, cl_int *index,
                             cl_long *expected, cl_int numElems) {
        for (cl_long i = 0; i < numElems; i++) {
            index[i] = static_cast<cl_int>(i % 16);
            for (cl_long j = 0; j < 16; j++) {
                cl_long index = 16 * i + j;
                source[index] = index;
                destination[index] = 0xdeadbeef;
                expected[index] = (index + 1) * (index + 1);
            }
        }
    }

    void fillSmallGrfBuffers(cl_uint *expectedMem, cl_uint addition) {
        for (cl_uint i = 0; i < static_cast<cl_uint>(numElems); i++) {
            reinterpret_cast<cl_uint *>(smallGrfSourceMemory.get())[i] = i;
            reinterpret_cast<cl_uint *>(smallGrfDestinationMemory.get())[i] =
                0xDEADBEAF;
            expectedMem[i] = i + addition;
        }
    }

    void allocateLargeGrfMemory() {
        indexMemory.reset(alignedMalloc(indexSize, MemoryConstants::pageSize));
        sourceMemory.reset(alignedMalloc(bufferSize, MemoryConstants::pageSize));
        destinationMemory.reset(
            alignedMalloc(bufferSize, MemoryConstants::pageSize));
        ASSERT_NE(nullptr, indexMemory.get());
        ASSERT_NE(nullptr, sourceMemory.get());
        ASSERT_NE(nullptr, destinationMemory.get());
    }

    void allocateSmallGrfMemory() {
        smallGrfSourceMemory.reset(
            alignedMalloc(numElems * sizeof(cl_uint), MemoryConstants::pageSize));
        smallGrfDestinationMemory.reset(
            alignedMalloc(numElems * sizeof(cl_uint), MemoryConstants::pageSize));
        ASSERT_NE(nullptr, smallGrfSourceMemory.get());
        ASSERT_NE(nullptr, smallGrfDestinationMemory.get());
    }

    void createLargeGrfBuffers() {
        debugManager.flags.DoCpuCopyOnWriteBuffer.set(true);
        indexBuffer.reset(Buffer::create(
            getContext(),
            CL_MEM_USE_HOST_PTR | CL_MEM_FORCE_HOST_MEMORY_INTEL,
            indexSize, indexMemory.get(), retVal));

        sourceBuffer.reset(Buffer::create(
            getContext(),
            CL_MEM_USE_HOST_PTR | CL_MEM_FORCE_HOST_MEMORY_INTEL,
            bufferSize, sourceMemory.get(), retVal));

        destinationBuffer.reset(Buffer::create(
            getContext(),
            CL_MEM_USE_HOST_PTR | CL_MEM_FORCE_HOST_MEMORY_INTEL,
            bufferSize, destinationMemory.get(), retVal));
        debugManager.flags.DoCpuCopyOnWriteBuffer.set(false);
        ASSERT_NE(nullptr, indexBuffer.get());
        ASSERT_NE(nullptr, sourceBuffer.get());
        ASSERT_NE(nullptr, destinationBuffer.get());
    }

    void createSmallGrfBuffers() {
        smallGrfSourceBuffer.reset(Buffer::create(
            getContext(), CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
            numElems * sizeof(cl_uint), smallGrfSourceMemory.get(), retVal));
        EXPECT_EQ(CL_SUCCESS, retVal);

        smallGrfDestinationBuffer.reset(Buffer::create(
            getContext(), CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
            numElems * sizeof(cl_uint), smallGrfDestinationMemory.get(), retVal));
        EXPECT_EQ(CL_SUCCESS, retVal);
        ASSERT_NE(nullptr, smallGrfSourceBuffer.get());
        ASSERT_NE(nullptr, smallGrfDestinationBuffer.get());
    }

    void setLargeGrfKernelArgs(cl_kernel kernel) {
        cl_mem arg0 = static_cast<cl_mem>(indexBuffer.get());
        cl_mem arg1 = static_cast<cl_mem>(sourceBuffer.get());
        cl_mem arg2 = static_cast<cl_mem>(destinationBuffer.get());

        retVal = clSetKernelArg(kernel, 0, sizeof(cl_mem), &arg0);
        ASSERT_EQ(CL_SUCCESS, retVal);

        retVal = clSetKernelArg(kernel, 1, sizeof(cl_mem), &arg1);
        ASSERT_EQ(CL_SUCCESS, retVal);

        retVal = clSetKernelArg(kernel, 2, sizeof(cl_mem), &arg2);
        ASSERT_EQ(CL_SUCCESS, retVal);
    }

    void setSmallGrfKernelArgs(cl_kernel kernel, cl_uint addition) {
        cl_mem smallGrfArg0 = static_cast<cl_mem>(smallGrfSourceBuffer.get());
        cl_uint smallGrfArg1 = addition;
        cl_mem smallGrfArg2 = static_cast<cl_mem>(smallGrfDestinationBuffer.get());

        retVal = clSetKernelArg(kernel, 0, sizeof(cl_mem), &smallGrfArg0);
        ASSERT_EQ(CL_SUCCESS, retVal);
        retVal = clSetKernelArg(kernel, 1, sizeof(cl_mem), &smallGrfArg1);
        ASSERT_EQ(CL_SUCCESS, retVal);
        retVal = clSetKernelArg(kernel, 2, sizeof(cl_mem), &smallGrfArg2);
        ASSERT_EQ(CL_SUCCESS, retVal);
    }

    cl_int retVal = CL_FALSE;
    std::unique_ptr<DebugManagerStateRestore> debugManagerStateRestore;

    static const cl_int numElems = 16;
    static const size_t bufferSize = numElems * sizeof(cl_long) * 16;
    static const size_t indexSize = numElems * sizeof(cl_int);

    HardwareInfo testHwInfo = {};

    std::unique_ptr<void, std::function<void(void *)>> indexMemory;
    std::unique_ptr<void, std::function<void(void *)>> sourceMemory;
    std::unique_ptr<void, std::function<void(void *)>> destinationMemory;

    std::unique_ptr<void, std::function<void(void *)>> smallGrfSourceMemory;
    std::unique_ptr<void, std::function<void(void *)>> smallGrfDestinationMemory;

    ReleaseableObjectPtr<Buffer> indexBuffer;
    ReleaseableObjectPtr<Buffer> sourceBuffer;
    ReleaseableObjectPtr<Buffer> destinationBuffer;
    ReleaseableObjectPtr<Buffer> smallGrfSourceBuffer;
    ReleaseableObjectPtr<Buffer> smallGrfDestinationBuffer;

    ReleaseableObjectPtr<Program> largeGrfProgram;
    ReleaseableObjectPtr<MultiDeviceKernel> largeGrfMultiDeviceKernel;
    ReleaseableObjectPtr<MultiDeviceKernel> smallGrfMultiDeviceKernel;
    Kernel *largeGrfKernel = nullptr;
    Kernel *smallGrfKernel = nullptr;

    size_t globalWorkOffset[3] = {0, 0, 0};
    size_t globalWorkSize[3] = {numElems, 1, 1};
};

class LargeGrfTest : public AUBFixture,
                     public AUBRunLargeGrfKernelFixture,
                     public ::testing::TestWithParam<bool> {
  protected:
    bool getLargeGrfConfig() const override { return GetParam(); }

    Context *getContext() const override { return context; }

    ClDevice *getDevice() const override { return device.get(); }

    void SetUp() override {
        testHwInfo = *defaultHwInfo;
        AUBFixture::setUp(&testHwInfo);
        AUBRunLargeGrfKernelFixture::setUp();
    }

    void TearDown() override {
        AUBRunLargeGrfKernelFixture::tearDown();
        AUBFixture::tearDown();
    }
};

static bool largeGrfModes[] = {true, false};

HWTEST2_P(LargeGrfTest, givenLargeGrfKernelWhenExecutedThenResultsAreCorrect, IsAtLeastXeCore) {
    createProgramFromBinary(getContext(), getContext()->getDevices(),
                            "simple_kernel_large_grf");

    retVal = pProgram->build(
        pProgram->getDevices(),
        nullptr);
    ASSERT_EQ(CL_SUCCESS, retVal);

    const KernelInfo *kernelInfo = pProgram->getKernelInfo("SimpleKernelLargeGRF", rootDeviceIndex);
    ASSERT_NE(nullptr, kernelInfo);

    EXPECT_EQ(256u, kernelInfo->kernelDescriptor.kernelAttributes.numGrfRequired);

    std::unique_ptr<MultiDeviceKernel> multiDeviceKernel(MultiDeviceKernel::create<MockKernel>(
        pProgram,
        MockKernel::toKernelInfoContainer(*kernelInfo, rootDeviceIndex),
        retVal));
    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, multiDeviceKernel.get());

    size_t bufferSize = numElems * sizeof(cl_uint);

    std::unique_ptr<void, std::function<void(void *)>> sourceMemory(alignedMalloc(bufferSize, 4096), alignedFree);
    std::unique_ptr<void, std::function<void(void *)>> destinationMemory(alignedMalloc(bufferSize, 4096), alignedFree);
    ASSERT_NE(nullptr, sourceMemory.get());
    ASSERT_NE(nullptr, destinationMemory.get());
    cl_uint expectedMemory[numElems];

    for (cl_uint i = 0; i < numElems; i++) {
        reinterpret_cast<cl_uint *>(sourceMemory.get())[i] = i;
        reinterpret_cast<cl_uint *>(destinationMemory.get())[i] = 0xDEADBEEF;
        expectedMemory[i] = i + 1;
    }

    std::unique_ptr<Buffer> sourceBuffer(Buffer::create(
        getContext(),
        CL_MEM_USE_HOST_PTR | CL_MEM_FORCE_HOST_MEMORY_INTEL,
        bufferSize, sourceMemory.get(), retVal));

    ASSERT_NE(nullptr, sourceBuffer.get());

    std::unique_ptr<Buffer> destinationBuffer(Buffer::create(
        getContext(),
        CL_MEM_USE_HOST_PTR | CL_MEM_FORCE_HOST_MEMORY_INTEL,
        bufferSize, destinationMemory.get(), retVal));

    ASSERT_NE(nullptr, destinationBuffer.get());

    cl_mem arg0 = static_cast<cl_mem>(sourceBuffer.get());
    cl_mem arg1 = static_cast<cl_mem>(destinationBuffer.get());

    retVal = clSetKernelArg(multiDeviceKernel.get(), 0, sizeof(cl_mem), &arg0);
    ASSERT_EQ(CL_SUCCESS, retVal);

    retVal = clSetKernelArg(multiDeviceKernel.get(), 1, sizeof(cl_mem), &arg1);
    ASSERT_EQ(CL_SUCCESS, retVal);

    size_t globalWorkOffset[3] = {0, 0, 0};
    size_t globalWorkSize[3] = {numElems, 1, 1};
    size_t localWorkSize[3] = {numElems, 1, 1};

    retVal = pCmdQ->enqueueKernel(
        multiDeviceKernel->getKernel(rootDeviceIndex),
        1,
        globalWorkOffset,
        globalWorkSize,
        localWorkSize,
        0,
        nullptr,
        nullptr);

    ASSERT_EQ(CL_SUCCESS, retVal);

    pCmdQ->finish();

    auto largeGrfValues = NEO::UnitTestHelper<FamilyType>::getProgrammedLargeGrfValues(pCmdQ->getGpgpuCommandStreamReceiver(),
                                                                                       pCmdQ->getCS(0));
    EXPECT_EQ(largeGrfValues.size(), 1u);
    if constexpr (TestTraits<FamilyType::gfxCoreFamily>::largeGrfModeInStateComputeModeSupported) {
        EXPECT_TRUE(largeGrfValues[0]);
    } else {
        EXPECT_FALSE(largeGrfValues[0]);
        const auto grfValue = NEO::UnitTestHelper<FamilyType>::getProgrammedGrfValue(pCmdQ->getGpgpuCommandStreamReceiver(),
                                                                                     pCmdQ->getCS(0));
        EXPECT_EQ(256u, grfValue);
    }

    expectMemory<FamilyType>(AUBFixture::getGpuPointer(destinationBuffer->getGraphicsAllocation(rootDeviceIndex), destinationBuffer->getOffset()), expectedMemory, bufferSize);
}

HWTEST2_P(LargeGrfTest, givenKernelWithSpillWhenExecutedInLargeGrfThenDontSpillAndResultsAreCorrect, IsAtLeastXeCore) {
    createKernels(true);

    allocateLargeGrfMemory();

    cl_long expectedMemory[numElems * 16];

    fillLargeGrfBuffers(reinterpret_cast<cl_long *>(sourceMemory.get()),
                        reinterpret_cast<cl_long *>(destinationMemory.get()),
                        reinterpret_cast<cl_int *>(indexMemory.get()),
                        expectedMemory, numElems);
    createLargeGrfBuffers();
    setLargeGrfKernelArgs(largeGrfMultiDeviceKernel.get());

    size_t globalWorkOffset[3] = {0, 0, 0};
    size_t globalWorkSize[3] = {numElems, 1, 1};

    retVal = pCmdQ->enqueueKernel(largeGrfKernel, 1, globalWorkOffset,
                                  globalWorkSize, nullptr, 0, nullptr, nullptr);

    ASSERT_EQ(CL_SUCCESS, retVal);

    pCmdQ->finish();

    auto largeGrfValues = NEO::UnitTestHelper<FamilyType>::getProgrammedLargeGrfValues(pCmdQ->getGpgpuCommandStreamReceiver(),
                                                                                       pCmdQ->getCS(0));
    EXPECT_EQ(largeGrfValues.size(), 1u);
    if constexpr (TestTraits<FamilyType::gfxCoreFamily>::largeGrfModeInStateComputeModeSupported) {
        EXPECT_TRUE(largeGrfValues[0]);
    } else {
        EXPECT_FALSE(largeGrfValues[0]);
        const auto grfValue = NEO::UnitTestHelper<FamilyType>::getProgrammedGrfValue(pCmdQ->getGpgpuCommandStreamReceiver(),
                                                                                     pCmdQ->getCS(0));
        EXPECT_EQ(256u, grfValue);
    }

    expectMemory<FamilyType>(
        AUBFixture::getGpuPointer(destinationBuffer->getGraphicsAllocation(rootDeviceIndex), destinationBuffer->getOffset()),
        expectedMemory, bufferSize);
}

HWTEST2_P(LargeGrfTest, givenMixedLargeGrfAndSmallGrfKernelsWhenExecutedThenResultsAreCorrect, IsAtLeastXeCore) {
    DebugManagerStateRestore restorer;
    debugManager.flags.CopyHostPtrOnCpu.set(0);
    createKernels(false);
    allocateLargeGrfMemory();

    cl_long largeGrfExpectedMemory[numElems * 16];

    fillLargeGrfBuffers(reinterpret_cast<cl_long *>(sourceMemory.get()),
                        reinterpret_cast<cl_long *>(destinationMemory.get()),
                        reinterpret_cast<cl_int *>(indexMemory.get()),
                        largeGrfExpectedMemory, numElems);
    createLargeGrfBuffers();
    setLargeGrfKernelArgs(largeGrfMultiDeviceKernel.get());

    allocateSmallGrfMemory();
    cl_uint smallGrfExpectedMemory[numElems];
    const cl_uint addition = 11;
    fillSmallGrfBuffers(smallGrfExpectedMemory, addition);
    createSmallGrfBuffers();
    setSmallGrfKernelArgs(smallGrfMultiDeviceKernel.get(), addition);

    // Execute largeGrf - smallGrf - LargeGrf - smallGrf
    retVal = pCmdQ->enqueueKernel(largeGrfKernel, 1, globalWorkOffset,
                                  globalWorkSize, nullptr, 0, nullptr, nullptr);
    ASSERT_EQ(CL_SUCCESS, retVal);

    retVal = pCmdQ->enqueueKernel(smallGrfKernel, 1, globalWorkOffset,
                                  globalWorkSize, nullptr, 0, nullptr, nullptr);
    ASSERT_EQ(CL_SUCCESS, retVal);

    retVal = pCmdQ->enqueueKernel(largeGrfKernel, 1, globalWorkOffset,
                                  globalWorkSize, nullptr, 0, nullptr, nullptr);
    ASSERT_EQ(CL_SUCCESS, retVal);

    retVal = pCmdQ->enqueueKernel(smallGrfKernel, 1, globalWorkOffset,
                                  globalWorkSize, nullptr, 0, nullptr, nullptr);
    ASSERT_EQ(CL_SUCCESS, retVal);

    pCmdQ->finish();

    auto largeGrfValues = NEO::UnitTestHelper<FamilyType>::getProgrammedLargeGrfValues(pCmdQ->getGpgpuCommandStreamReceiver(),
                                                                                       pCmdQ->getCS(0));

    if constexpr (TestTraits<FamilyType::gfxCoreFamily>::largeGrfModeInStateComputeModeSupported) {
        auto allocation = smallGrfSourceBuffer->getGraphicsAllocation(rootDeviceIndex);
        auto &productHelper = pCmdQ->getDevice().getProductHelper();
        if (allocation->isAllocatedInLocalMemoryPool() && productHelper.isGrfNumReportedWithScm()) {
            ASSERT_EQ(5u, largeGrfValues.size());
            EXPECT_FALSE(largeGrfValues[0]); // copy buffer
            EXPECT_TRUE(largeGrfValues[1]);
            EXPECT_FALSE(largeGrfValues[2]);
            EXPECT_TRUE(largeGrfValues[3]);
            EXPECT_FALSE(largeGrfValues[4]);
        } else {
            ASSERT_EQ(4u, largeGrfValues.size());
            EXPECT_TRUE(largeGrfValues[0]);
            EXPECT_FALSE(largeGrfValues[1]);
            EXPECT_TRUE(largeGrfValues[2]);
            EXPECT_FALSE(largeGrfValues[3]);
        }
    } else {
        auto &gfxCoreHelper = executionEnvironment->rootDeviceEnvironments[0]->getHelper<GfxCoreHelper>();
        if (gfxCoreHelper.areSecondaryContextsSupported() == false || device->device.disableSecondaryEngines) {
            ASSERT_EQ(1u, largeGrfValues.size());
            EXPECT_FALSE(largeGrfValues[0]);
        } else {
            EXPECT_EQ(0u, largeGrfValues.size());
            largeGrfValues = NEO::UnitTestHelper<FamilyType>::getProgrammedLargeGrfValues(this->csr->getCS(0));
            ASSERT_EQ(1u, largeGrfValues.size());
            EXPECT_FALSE(largeGrfValues[0]);
        }
    }

    expectMemory<FamilyType>(
        AUBFixture::getGpuPointer(destinationBuffer->getGraphicsAllocation(rootDeviceIndex), destinationBuffer->getOffset()),
        largeGrfExpectedMemory, bufferSize);
    expectMemory<FamilyType>(AUBFixture::getGpuPointer(smallGrfDestinationBuffer->getGraphicsAllocation(rootDeviceIndex), smallGrfDestinationBuffer->getOffset()), smallGrfExpectedMemory, numElems * sizeof(cl_uint));
}

INSTANTIATE_TEST_SUITE_P(LargeGrfTests, LargeGrfTest, testing::ValuesIn(largeGrfModes));

class MultiContextLargeGrfKernelAubTest : public MulticontextOclAubFixture, public AUBRunLargeGrfKernelFixture, public ::testing::TestWithParam<std::tuple<bool, uint32_t, uint32_t>> {
  protected:
    bool getLargeGrfConfig() const override { return std::get<0>(GetParam()); }

    Context *getContext() const override {
        return MulticontextOclAubFixture::context.get();
    }

    ClDevice *getDevice() const override { return tileDevices[0]; }

    void SetUp() override {
        AUBRunLargeGrfKernelFixture::setUp();
        MulticontextOclAubFixture::setUp(
            1, MulticontextOclAubFixture::EnabledCommandStreamers::dual, false);
    }

    void TearDown() override {
        AUBRunLargeGrfKernelFixture::tearDown();
    }
};

static uint32_t enginesIndices[] = {0, 1};

HWTEST2_P(MultiContextLargeGrfKernelAubTest, givenLargeAndSmallGrfWhenParallelRunOnCcsAndRcsThenResultsAreOk, IsXeCore) {
    createKernels(false);
    allocateLargeGrfMemory();

    cl_long largeGrfExpectedMemory[numElems * 16];
    fillLargeGrfBuffers(reinterpret_cast<cl_long *>(sourceMemory.get()),
                        reinterpret_cast<cl_long *>(destinationMemory.get()),
                        reinterpret_cast<cl_int *>(indexMemory.get()),
                        largeGrfExpectedMemory, numElems);
    createLargeGrfBuffers();
    setLargeGrfKernelArgs(largeGrfMultiDeviceKernel.get());

    allocateSmallGrfMemory();
    cl_uint smallGrfExpectedMemory[numElems];
    const cl_uint addition = 11;
    fillSmallGrfBuffers(smallGrfExpectedMemory, addition);
    createSmallGrfBuffers();
    setSmallGrfKernelArgs(smallGrfMultiDeviceKernel.get(), addition);

    auto largeGrfEngine = std::get<1>(GetParam());
    auto smallGrfEngine = std::get<2>(GetParam());

    auto &largeGrfQueue = commandQueues[0][largeGrfEngine];
    auto &smallGrfQueue = commandQueues[0][smallGrfEngine];

    retVal = largeGrfQueue->enqueueKernel(largeGrfKernel, 1,
                                          globalWorkOffset, globalWorkSize,
                                          nullptr, 0, nullptr, nullptr);
    ASSERT_EQ(CL_SUCCESS, retVal);

    retVal = smallGrfQueue->enqueueKernel(smallGrfKernel, 1,
                                          globalWorkOffset, globalWorkSize,
                                          nullptr, 0, nullptr, nullptr);
    ASSERT_EQ(CL_SUCCESS, retVal);

    largeGrfQueue->finish();
    smallGrfQueue->finish();

    MulticontextOclAubFixture::expectMemory<FamilyType>(
        AUBFixture::getGpuPointer(destinationBuffer->getGraphicsAllocation(rootDeviceIndex), destinationBuffer->getOffset()),
        largeGrfExpectedMemory, bufferSize, 0, largeGrfEngine);
    MulticontextOclAubFixture::expectMemory<FamilyType>(
        AUBFixture::getGpuPointer(
            smallGrfDestinationBuffer->getGraphicsAllocation(rootDeviceIndex), smallGrfDestinationBuffer->getOffset()),
        smallGrfExpectedMemory, numElems * sizeof(cl_uint), 0, smallGrfEngine);
}

INSTANTIATE_TEST_SUITE_P(
    MultiContextLargeGrfTests, MultiContextLargeGrfKernelAubTest,
    ::testing::Combine(::testing::ValuesIn(largeGrfModes),
                       ::testing::ValuesIn(enginesIndices),
                       ::testing::ValuesIn(enginesIndices)));
