/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/flat_batch_buffer_helper_hw.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/event/event.h"
#include "opencl/source/helpers/hardware_commands_helper.h"
#include "opencl/test/unit_test/aub_tests/fixtures/aub_fixture.h"
#include "opencl/test/unit_test/aub_tests/fixtures/hello_world_fixture.h"
#include "opencl/test/unit_test/fixtures/hello_world_fixture.h"
#include "opencl/test/unit_test/fixtures/two_walker_fixture.h"
#include "opencl/test/unit_test/test_macros/test_checks_ocl.h"

using namespace NEO;

extern const HardwareInfo *defaultHwInfo;

struct TestParam {
    cl_uint globalWorkSizeX;
    cl_uint globalWorkSizeY;
    cl_uint globalWorkSizeZ;
    cl_uint localWorkSizeX;
    cl_uint localWorkSizeY;
    cl_uint localWorkSizeZ;
};

static TestParam TestParamTable[] = {
    {1, 1, 1, 1, 1, 1},
    {16, 1, 1, 16, 1, 1},
    {32, 1, 1, 16, 1, 1},
    {64, 1, 1, 1, 1, 1},
    {64, 1, 1, 16, 1, 1},
    {64, 1, 1, 64, 1, 1}};

cl_uint TestSimdTable[] = {
    8, 16, 32};

namespace ULT {
struct AUBHelloWorld
    : public HelloWorldFixture<AUBHelloWorldFixtureFactory>,
      public ClHardwareParse,
      public ::testing::Test {

    void SetUp() override {
        HelloWorldFixture<AUBHelloWorldFixtureFactory>::SetUp();
        ClHardwareParse::SetUp();
    }

    void TearDown() override {
        ClHardwareParse::TearDown();
        HelloWorldFixture<AUBHelloWorldFixtureFactory>::TearDown();
    }
};

HWCMDTEST_F(IGFX_GEN8_CORE, AUBHelloWorld, WhenEnqueuingKernelThenAdressesAreAligned) {
    typedef typename FamilyType::GPGPU_WALKER GPGPU_WALKER;
    typedef typename FamilyType::STATE_BASE_ADDRESS STATE_BASE_ADDRESS;
    typedef typename FamilyType::MEDIA_INTERFACE_DESCRIPTOR_LOAD MEDIA_INTERFACE_DESCRIPTOR_LOAD;
    typedef typename FamilyType::INTERFACE_DESCRIPTOR_DATA INTERFACE_DESCRIPTOR_DATA;

    cl_uint workDim = 1;
    size_t globalWorkOffset[3] = {0, 0, 0};
    size_t globalWorkSize[3] = {1, 1, 1};
    size_t localWorkSize[3] = {1, 1, 1};
    cl_uint numEventsInWaitList = 0;
    cl_event *eventWaitList = nullptr;
    cl_event *event = nullptr;

    // Intentionally mis-align data as we're going to test driver properly aligns commands
    pDSH->getSpace(sizeof(uint32_t));

    auto retVal = pCmdQ->enqueueKernel(
        pKernel,
        workDim,
        globalWorkOffset,
        globalWorkSize,
        localWorkSize,
        numEventsInWaitList,
        eventWaitList,
        event);

    ASSERT_EQ(CL_SUCCESS, retVal);

    pCmdQ->flush();

    parseCommands<FamilyType>(*pCmdQ);

    auto *pWalker = reinterpret_cast<GPGPU_WALKER *>(cmdWalker);
    ASSERT_NE(nullptr, pWalker);
    auto alignmentIDSA = 32 * sizeof(uint8_t);
    EXPECT_EQ(0u, pWalker->getIndirectDataStartAddress() % alignmentIDSA);

    // Check interface descriptor alignment
    auto pMIDL = reinterpret_cast<MEDIA_INTERFACE_DESCRIPTOR_LOAD *>(cmdMediaInterfaceDescriptorLoad);
    ASSERT_NE(nullptr, pMIDL);
    uintptr_t addrIDD = pMIDL->getInterfaceDescriptorDataStartAddress();
    auto alignmentIDD = 64 * sizeof(uint8_t);
    EXPECT_EQ(0u, addrIDD % alignmentIDD);

    // Check kernel start pointer matches hard-coded kernel.
    auto pExpectedISA = pKernel->getKernelHeap();
    auto expectedSize = pKernel->getKernelHeapSize();

    auto pSBA = reinterpret_cast<STATE_BASE_ADDRESS *>(cmdStateBaseAddress);
    ASSERT_NE(nullptr, pSBA);
    auto pISA = pKernel->getKernelInfo().getGraphicsAllocation()->getUnderlyingBuffer();
    EXPECT_EQ(0, memcmp(pISA, pExpectedISA, expectedSize));
}

struct AUBHelloWorldIntegrateTest : public HelloWorldFixture<AUBHelloWorldFixtureFactory>,
                                    public ::testing::TestWithParam<std::tuple<uint32_t /*cl_uint*/, TestParam>> {
    typedef HelloWorldFixture<AUBHelloWorldFixtureFactory> ParentClass;

    void SetUp() override {
        std::tie(KernelFixture::simd, param) = GetParam();
        if (KernelFixture::simd < HwHelper::get(NEO::defaultHwInfo->platform.eRenderCoreFamily).getMinimalSIMDSize()) {
            GTEST_SKIP();
        }
        ParentClass::SetUp();
    }

    void TearDown() override {
        if (!IsSkipped()) {
            ParentClass::TearDown();
        }
    }

    template <typename FamilyType>
    void writeMemory(GraphicsAllocation *allocation) {
        AUBCommandStreamReceiverHw<FamilyType> *aubCsr = nullptr;
        if (testMode == TestMode::AubTests) {
            aubCsr = static_cast<AUBCommandStreamReceiverHw<FamilyType> *>(pCommandStreamReceiver);
        } else if (testMode == TestMode::AubTestsWithTbx) {
            auto tbxWithAubCsr = static_cast<CommandStreamReceiverWithAUBDump<TbxCommandStreamReceiverHw<FamilyType>> *>(pCommandStreamReceiver);
            aubCsr = static_cast<AUBCommandStreamReceiverHw<FamilyType> *>(tbxWithAubCsr->aubCSR.get());
            tbxWithAubCsr->writeMemory(*allocation);
        }

        aubCsr->writeMemory(*allocation); // NOLINT(clang-analyzer-core.CallAndMessage)
    }
    TestParam param;
};

HWTEST_P(AUBHelloWorldIntegrateTest, WhenEnqueingKernelThenExpectationsAreMet) {
    if (this->simd < UnitTestHelper<FamilyType>::smallestTestableSimdSize) {
        GTEST_SKIP();
    }
    cl_uint workDim = 1;
    size_t globalWorkOffset[3] = {0, 0, 0};
    size_t globalWorkSize[3] = {param.globalWorkSizeX, param.globalWorkSizeY, param.globalWorkSizeZ};
    size_t localWorkSize[3] = {param.localWorkSizeX, param.localWorkSizeY, param.localWorkSizeZ};
    cl_uint numEventsInWaitList = 0;
    cl_event *eventWaitList = nullptr;
    cl_event *event = nullptr;

    getSimulatedCsr<FamilyType>()->initializeEngine();

    writeMemory<FamilyType>(destBuffer->getGraphicsAllocation(pClDevice->getRootDeviceIndex()));
    writeMemory<FamilyType>(srcBuffer->getGraphicsAllocation(pClDevice->getRootDeviceIndex()));

    auto retVal = this->pCmdQ->enqueueKernel(
        this->pKernel,
        workDim,
        globalWorkOffset,
        globalWorkSize,
        localWorkSize,
        numEventsInWaitList,
        eventWaitList,
        event);
    ASSERT_EQ(CL_SUCCESS, retVal);

    pCmdQ->flush();

    // Compute our memory expecations based on kernel execution
    auto globalWorkItems = globalWorkSize[0] * globalWorkSize[1] * globalWorkSize[2];
    auto sizeWritten = globalWorkItems * sizeof(float);

    auto pDestGpuAddress = reinterpret_cast<void *>((destBuffer->getGraphicsAllocation(pClDevice->getRootDeviceIndex())->getGpuAddress()));

    AUBCommandStreamFixture::expectMemory<FamilyType>(pDestGpuAddress, this->pSrcMemory, sizeWritten);

    // If the copykernel wasn't max sized, ensure we didn't overwrite existing memory
    if (sizeWritten < this->sizeUserMemory) {
        auto sizeRemaining = this->sizeUserMemory - sizeWritten;
        auto pDestUnwrittenMemory = ptrOffset(pDestGpuAddress, sizeWritten);
        auto pUnwrittenMemory = ptrOffset(this->pDestMemory, sizeWritten);
        AUBCommandStreamFixture::expectMemory<FamilyType>(pDestUnwrittenMemory, pUnwrittenMemory, sizeRemaining);
    }
}

INSTANTIATE_TEST_CASE_P(
    AUB,
    AUBHelloWorldIntegrateTest,
    ::testing::Combine(
        ::testing::ValuesIn(TestSimdTable),
        ::testing::ValuesIn(TestParamTable)));

struct AUBSimpleArg
    : public SimpleArgFixture<AUBSimpleArgFixtureFactory>,
      public ClHardwareParse,
      public ::testing::Test {

    using SimpleArgKernelFixture::SetUp;

    void SetUp() override {
        SimpleArgFixture<AUBSimpleArgFixtureFactory>::SetUp();
        ClHardwareParse::SetUp();
    }

    void TearDown() override {
        ClHardwareParse::TearDown();
        SimpleArgFixture<AUBSimpleArgFixtureFactory>::TearDown();
    }
};

HWCMDTEST_F(IGFX_GEN8_CORE, AUBSimpleArg, WhenEnqueingKernelThenAdressesAreAligned) {
    typedef typename FamilyType::GPGPU_WALKER GPGPU_WALKER;
    typedef typename FamilyType::STATE_BASE_ADDRESS STATE_BASE_ADDRESS;
    typedef typename FamilyType::MEDIA_INTERFACE_DESCRIPTOR_LOAD MEDIA_INTERFACE_DESCRIPTOR_LOAD;
    typedef typename FamilyType::INTERFACE_DESCRIPTOR_DATA INTERFACE_DESCRIPTOR_DATA;

    cl_uint workDim = 1;
    size_t globalWorkOffset[3] = {0, 0, 0};
    size_t globalWorkSize[3] = {1, 1, 1};
    size_t localWorkSize[3] = {1, 1, 1};
    cl_uint numEventsInWaitList = 0;
    cl_event *eventWaitList = nullptr;
    cl_event *event = nullptr;

    // Intentionally mis-align data as we're going to test driver properly aligns commands
    pDSH->getSpace(sizeof(uint32_t));

    auto retVal = pCmdQ->enqueueKernel(
        pKernel,
        workDim,
        globalWorkOffset,
        globalWorkSize,
        localWorkSize,
        numEventsInWaitList,
        eventWaitList,
        event);

    ASSERT_EQ(CL_SUCCESS, retVal);

    parseCommands<FamilyType>(*pCmdQ);

    pCmdQ->flush();

    auto *pWalker = reinterpret_cast<GPGPU_WALKER *>(cmdWalker);
    ASSERT_NE(nullptr, pWalker);
    auto alignmentIDSA = 32 * sizeof(uint8_t);
    EXPECT_EQ(0u, pWalker->getIndirectDataStartAddress() % alignmentIDSA);

    // Check interface descriptor alignment
    auto pMIDL = reinterpret_cast<MEDIA_INTERFACE_DESCRIPTOR_LOAD *>(cmdMediaInterfaceDescriptorLoad);
    ASSERT_NE(nullptr, pMIDL);
    uintptr_t addrIDD = pMIDL->getInterfaceDescriptorDataStartAddress();
    auto alignmentIDD = 64 * sizeof(uint8_t);
    EXPECT_EQ(0u, addrIDD % alignmentIDD);

    // Check kernel start pointer matches hard-coded kernel.
    auto pExpectedISA = pKernel->getKernelHeap();
    auto expectedSize = pKernel->getKernelHeapSize();

    auto pSBA = reinterpret_cast<STATE_BASE_ADDRESS *>(cmdStateBaseAddress);
    ASSERT_NE(nullptr, pSBA);
    auto pISA = pKernel->getKernelInfo().getGraphicsAllocation()->getUnderlyingBuffer();
    EXPECT_EQ(0, memcmp(pISA, pExpectedISA, expectedSize));
}

HWTEST_F(AUBSimpleArg, givenAubCommandStreamerReceiverWhenBatchBufferFlateningIsForcedThenDumpedAubIsStillValid) {
    cl_uint workDim = 1;
    size_t globalWorkOffset[3] = {0, 0, 0};
    size_t globalWorkSize[3] = {1, 1, 1};
    size_t localWorkSize[3] = {1, 1, 1};
    cl_uint numEventsInWaitList = 0;
    cl_event *eventWaitList = nullptr;
    cl_event *event = nullptr;

    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.FlattenBatchBufferForAUBDump.set(true);

    pCmdQ->getGpgpuCommandStreamReceiver().overwriteFlatBatchBufferHelper(new FlatBatchBufferHelperHw<FamilyType>(*pCmdQ->getDevice().getExecutionEnvironment()));

    pCmdQ->getGpgpuCommandStreamReceiver().overrideDispatchPolicy(DispatchMode::ImmediateDispatch);

    auto retVal = pCmdQ->enqueueKernel(
        pKernel,
        workDim,
        globalWorkOffset,
        globalWorkSize,
        localWorkSize,
        numEventsInWaitList,
        eventWaitList,
        event);

    ASSERT_EQ(CL_SUCCESS, retVal);
    pCmdQ->flush();
}

struct AUBSimpleArgIntegrateTest : public SimpleArgFixture<AUBSimpleArgFixtureFactory>,
                                   public ::testing::TestWithParam<std::tuple<uint32_t /*cl_uint*/, TestParam>> {
    typedef SimpleArgFixture<AUBSimpleArgFixtureFactory> ParentClass;

    void SetUp() override {
        std::tie(simd, param) = GetParam();
        if (simd < HwHelper::get(NEO::defaultHwInfo->platform.eRenderCoreFamily).getMinimalSIMDSize()) {
            GTEST_SKIP();
        }
        ParentClass::SetUp();
    }

    void TearDown() override {
        if (!IsSkipped()) {
            ParentClass::TearDown();
        }
    }
    cl_uint simd;
    TestParam param;
};

HWTEST_P(AUBSimpleArgIntegrateTest, WhenEnqueingKernelThenExpectationsAreMet) {
    cl_uint workDim = 1;
    size_t globalWorkOffset[3] = {0, 0, 0};
    size_t globalWorkSize[3] = {param.globalWorkSizeX, param.globalWorkSizeY, param.globalWorkSizeZ};
    size_t localWorkSize[3] = {param.localWorkSizeX, param.localWorkSizeY, param.localWorkSizeZ};
    cl_uint numEventsInWaitList = 0;
    cl_event *eventWaitList = nullptr;
    cl_event *event = nullptr;

    auto retVal = this->pCmdQ->enqueueKernel(
        this->pKernel,
        workDim,
        globalWorkOffset,
        globalWorkSize,
        localWorkSize,
        numEventsInWaitList,
        eventWaitList,
        event);
    ASSERT_EQ(CL_SUCCESS, retVal);

    pCmdQ->flush();

    // Compute our memory expecations based on kernel execution
    size_t globalWorkItems = globalWorkSize[0] * globalWorkSize[1] * globalWorkSize[2];
    size_t sizeWritten = globalWorkItems * sizeof(int);
    AUBCommandStreamFixture::expectMemory<FamilyType>(this->pDestMemory, this->pExpectedMemory, sizeWritten);

    // If the copykernel wasn't max sized, ensure we didn't overwrite existing memory
    if (sizeWritten < this->sizeUserMemory) {
        auto sizeRemaining = this->sizeUserMemory - sizeWritten;
        auto pUnwrittenMemory = ptrOffset(this->pDestMemory, sizeWritten);
        AUBCommandStreamFixture::expectMemory<FamilyType>(pUnwrittenMemory, pUnwrittenMemory, sizeRemaining);
    }
}

INSTANTIATE_TEST_CASE_P(
    AUB,
    AUBSimpleArgIntegrateTest,
    ::testing::Combine(
        ::testing::ValuesIn(TestSimdTable),
        ::testing::ValuesIn(TestParamTable)));
} // namespace ULT

struct AUBSimpleArgNonUniformFixture : public KernelAUBFixture<SimpleArgNonUniformKernelFixture> {
    void SetUp() override {
        REQUIRE_OCL_21_OR_SKIP(NEO::defaultHwInfo);
        KernelAUBFixture<SimpleArgNonUniformKernelFixture>::SetUp();

        sizeUserMemory = alignUp(typeItems * typeSize, 64);

        destMemory = alignedMalloc(sizeUserMemory, 4096);
        ASSERT_NE(nullptr, destMemory);
        for (uint32_t i = 0; i < typeItems; i++) {
            *(static_cast<int *>(destMemory) + i) = 0xdeadbeef;
        }

        expectedMemory = alignedMalloc(sizeUserMemory, 4096);
        ASSERT_NE(nullptr, expectedMemory);
        memset(expectedMemory, 0x0, sizeUserMemory);
    }

    void initializeExpectedMemory(size_t globalX, size_t globalY, size_t globalZ) {
        uint32_t id = 0;
        size_t testGlobalMax = globalX * globalY * globalZ;
        ASSERT_GT(typeItems, testGlobalMax);
        int maxId = static_cast<int>(testGlobalMax);

        argVal = maxId;
        kernel->setArg(0, sizeof(int), &argVal);

        int *expectedData = static_cast<int *>(expectedMemory);
        for (size_t z = 0; z < globalZ; z++) {
            for (size_t y = 0; y < globalY; y++) {
                for (size_t x = 0; x < globalX; x++) {
                    *(expectedData + id) = id;
                    ++id;
                }
            }
        }

        *(static_cast<int *>(destMemory) + maxId) = 0;
        *(expectedData + maxId) = maxId;

        outBuffer.reset(Buffer::create(context, CL_MEM_COPY_HOST_PTR, alignUp(sizeUserMemory, 4096), destMemory, retVal));
        bufferGpuAddress = reinterpret_cast<void *>(outBuffer->getGraphicsAllocation(device->getRootDeviceIndex())->getGpuAddress());
        kernel->setArg(1, outBuffer.get());

        sizeWrittenMemory = maxId * typeSize;
        // add single int size for atomic sum of all work-items
        sizeWrittenMemory += typeSize;

        sizeRemainderMemory = sizeUserMemory - sizeWrittenMemory;
        expectedRemainderMemory = alignedMalloc(sizeRemainderMemory, 4096);
        ASSERT_NE(nullptr, expectedRemainderMemory);
        int *expectedReminderData = static_cast<int *>(expectedRemainderMemory);
        size_t reminderElements = sizeRemainderMemory / typeSize;
        for (size_t i = 0; i < reminderElements; i++) {
            *(expectedReminderData + i) = 0xdeadbeef;
        }
        remainderBufferGpuAddress = ptrOffset(bufferGpuAddress, sizeWrittenMemory);
    }

    void TearDown() override {
        if (NEO::defaultHwInfo->capabilityTable.supportsOcl21Features == false) {
            return;
        }
        if (destMemory) {
            alignedFree(destMemory);
            destMemory = nullptr;
        }
        if (expectedMemory) {
            alignedFree(expectedMemory);
            expectedMemory = nullptr;
        }
        if (expectedRemainderMemory) {
            alignedFree(expectedRemainderMemory);
            expectedRemainderMemory = nullptr;
        }
        KernelAUBFixture<SimpleArgNonUniformKernelFixture>::TearDown();
    }
    unsigned int deviceClVersionSupport;

    const size_t typeSize = sizeof(int);
    const size_t typeItems = 40 * 40 * 40;
    size_t sizeWrittenMemory = 0;
    size_t sizeUserMemory;
    size_t sizeRemainderMemory;
    int argVal = 0x22222222;
    void *destMemory = nullptr;
    void *expectedMemory = nullptr;
    void *expectedRemainderMemory = nullptr;
    void *remainderBufferGpuAddress = nullptr;
    void *bufferGpuAddress = nullptr;
    std::unique_ptr<Buffer> outBuffer;

    ClHardwareParse hwParser;
};

using AUBSimpleKernelStatelessTest = Test<KernelAUBFixture<SimpleKernelStatelessFixture>>;

HWTEST_F(AUBSimpleKernelStatelessTest, givenSimpleKernelWhenStatelessPathIsUsedThenExpectCorrectBuffer) {

    constexpr size_t bufferSize = MemoryConstants::pageSize;
    cl_uint workDim = 1;
    size_t globalWorkOffset[3] = {0, 0, 0};
    size_t globalWorkSize[3] = {bufferSize, 1, 1};
    size_t localWorkSize[3] = {1, 1, 1};
    cl_uint numEventsInWaitList = 0;
    cl_event *eventWaitList = nullptr;
    cl_event *event = nullptr;

    uint8_t bufferData[bufferSize] = {};
    uint8_t bufferExpected[bufferSize];
    memset(bufferExpected, 0xCD, bufferSize);

    auto pBuffer = std::unique_ptr<Buffer>(Buffer::create(context,
                                                          CL_MEM_USE_HOST_PTR | CL_MEM_ALLOW_UNRESTRICTED_SIZE_INTEL,
                                                          bufferSize,
                                                          bufferData,
                                                          retVal));
    ASSERT_NE(nullptr, pBuffer);

    kernel->setArg(0, pBuffer.get());

    retVal = this->pCmdQ->enqueueKernel(
        kernel.get(),
        workDim,
        globalWorkOffset,
        globalWorkSize,
        localWorkSize,
        numEventsInWaitList,
        eventWaitList,
        event);

    ASSERT_EQ(CL_SUCCESS, retVal);

    EXPECT_FALSE(this->kernel->getKernelInfo().kernelDescriptor.payloadMappings.explicitArgs[0].as<ArgDescPointer>().isPureStateful());
    EXPECT_TRUE(this->kernel->getKernelInfo().kernelDescriptor.kernelAttributes.supportsBuffersBiggerThan4Gb());

    this->pCmdQ->flush();
    expectMemory<FamilyType>(reinterpret_cast<void *>(pBuffer->getGraphicsAllocation(device->getRootDeviceIndex())->getGpuAddress()),
                             bufferExpected, bufferSize);
}

using AUBSimpleArgNonUniformTest = Test<AUBSimpleArgNonUniformFixture>;
HWTEST_F(AUBSimpleArgNonUniformTest, givenOpenCL20SupportWhenProvidingWork1DimNonUniformGroupThenExpectTwoWalkers) {
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;
    cl_uint workDim = 1;
    size_t globalWorkOffset[3] = {0, 0, 0};
    size_t globalWorkSize[3] = {39, 1, 1};
    size_t localWorkSize[3] = {32, 1, 1};
    cl_uint numEventsInWaitList = 0;
    cl_event *eventWaitList = nullptr;
    cl_event *event = nullptr;

    initializeExpectedMemory(globalWorkSize[0], globalWorkSize[1], globalWorkSize[2]);

    auto retVal = this->pCmdQ->enqueueKernel(
        this->kernel,
        workDim,
        globalWorkOffset,
        globalWorkSize,
        localWorkSize,
        numEventsInWaitList,
        eventWaitList,
        event);
    ASSERT_EQ(CL_SUCCESS, retVal);

    hwParser.parseCommands<FamilyType>(*pCmdQ);
    uint32_t walkerCount = hwParser.getCommandCount<WALKER_TYPE>();
    EXPECT_EQ(2u, walkerCount);

    pCmdQ->flush();
    expectMemory<FamilyType>(bufferGpuAddress, this->expectedMemory, sizeWrittenMemory);
    expectMemory<FamilyType>(remainderBufferGpuAddress, this->expectedRemainderMemory, sizeRemainderMemory);
}

HWTEST_F(AUBSimpleArgNonUniformTest, givenOpenCL20SupportWhenProvidingWork2DimNonUniformGroupInXDimensionThenExpectTwoWalkers) {
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;
    cl_uint workDim = 2;
    size_t globalWorkOffset[3] = {0, 0, 0};
    size_t globalWorkSize[3] = {39, 32, 1};
    size_t localWorkSize[3] = {16, 16, 1};
    cl_uint numEventsInWaitList = 0;
    cl_event *eventWaitList = nullptr;
    cl_event *event = nullptr;

    initializeExpectedMemory(globalWorkSize[0], globalWorkSize[1], globalWorkSize[2]);

    auto retVal = this->pCmdQ->enqueueKernel(
        this->kernel,
        workDim,
        globalWorkOffset,
        globalWorkSize,
        localWorkSize,
        numEventsInWaitList,
        eventWaitList,
        event);
    ASSERT_EQ(CL_SUCCESS, retVal);

    hwParser.parseCommands<FamilyType>(*pCmdQ);
    uint32_t walkerCount = hwParser.getCommandCount<WALKER_TYPE>();
    EXPECT_EQ(2u, walkerCount);

    pCmdQ->flush();
    expectMemory<FamilyType>(bufferGpuAddress, this->expectedMemory, sizeWrittenMemory);
    expectMemory<FamilyType>(remainderBufferGpuAddress, this->expectedRemainderMemory, sizeRemainderMemory);
}

HWTEST_F(AUBSimpleArgNonUniformTest, givenOpenCL20SupportWhenProvidingWork2DimNonUniformGroupInYDimensionThenExpectTwoWalkers) {
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;
    cl_uint workDim = 2;
    size_t globalWorkOffset[3] = {0, 0, 0};
    size_t globalWorkSize[3] = {32, 39, 1};
    size_t localWorkSize[3] = {16, 16, 1};
    cl_uint numEventsInWaitList = 0;
    cl_event *eventWaitList = nullptr;
    cl_event *event = nullptr;

    initializeExpectedMemory(globalWorkSize[0], globalWorkSize[1], globalWorkSize[2]);

    auto retVal = this->pCmdQ->enqueueKernel(
        this->kernel,
        workDim,
        globalWorkOffset,
        globalWorkSize,
        localWorkSize,
        numEventsInWaitList,
        eventWaitList,
        event);
    ASSERT_EQ(CL_SUCCESS, retVal);

    hwParser.parseCommands<FamilyType>(*pCmdQ);
    uint32_t walkerCount = hwParser.getCommandCount<WALKER_TYPE>();
    EXPECT_EQ(2u, walkerCount);

    pCmdQ->flush();
    expectMemory<FamilyType>(bufferGpuAddress, this->expectedMemory, sizeWrittenMemory);
    expectMemory<FamilyType>(remainderBufferGpuAddress, this->expectedRemainderMemory, sizeRemainderMemory);
}

HWTEST_F(AUBSimpleArgNonUniformTest, givenOpenCL20SupportWhenProvidingWork2DimNonUniformGroupInXandYDimensionThenExpectFourWalkers) {
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;
    cl_uint workDim = 2;
    size_t globalWorkOffset[3] = {0, 0, 0};
    size_t globalWorkSize[3] = {39, 39, 1};
    size_t localWorkSize[3] = {16, 16, 1};
    cl_uint numEventsInWaitList = 0;
    cl_event *eventWaitList = nullptr;
    cl_event *event = nullptr;

    initializeExpectedMemory(globalWorkSize[0], globalWorkSize[1], globalWorkSize[2]);

    auto retVal = this->pCmdQ->enqueueKernel(
        this->kernel,
        workDim,
        globalWorkOffset,
        globalWorkSize,
        localWorkSize,
        numEventsInWaitList,
        eventWaitList,
        event);
    ASSERT_EQ(CL_SUCCESS, retVal);

    hwParser.parseCommands<FamilyType>(*pCmdQ);
    uint32_t walkerCount = hwParser.getCommandCount<WALKER_TYPE>();
    EXPECT_EQ(4u, walkerCount);

    pCmdQ->flush();
    expectMemory<FamilyType>(bufferGpuAddress, this->expectedMemory, sizeWrittenMemory);
    expectMemory<FamilyType>(remainderBufferGpuAddress, this->expectedRemainderMemory, sizeRemainderMemory);
}

HWTEST_F(AUBSimpleArgNonUniformTest, givenOpenCL20SupportWhenProvidingWork3DimNonUniformGroupInXDimensionThenExpectTwoWalkers) {
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;
    cl_uint workDim = 3;
    size_t globalWorkOffset[3] = {0, 0, 0};
    size_t globalWorkSize[3] = {39, 32, 32};
    size_t localWorkSize[3] = {8, 8, 2};
    cl_uint numEventsInWaitList = 0;
    cl_event *eventWaitList = nullptr;
    cl_event *event = nullptr;

    initializeExpectedMemory(globalWorkSize[0], globalWorkSize[1], globalWorkSize[2]);

    auto retVal = this->pCmdQ->enqueueKernel(
        this->kernel,
        workDim,
        globalWorkOffset,
        globalWorkSize,
        localWorkSize,
        numEventsInWaitList,
        eventWaitList,
        event);
    ASSERT_EQ(CL_SUCCESS, retVal);

    hwParser.parseCommands<FamilyType>(*pCmdQ);
    uint32_t walkerCount = hwParser.getCommandCount<WALKER_TYPE>();
    EXPECT_EQ(2u, walkerCount);

    pCmdQ->flush();
    expectMemory<FamilyType>(bufferGpuAddress, this->expectedMemory, sizeWrittenMemory);
    expectMemory<FamilyType>(remainderBufferGpuAddress, this->expectedRemainderMemory, sizeRemainderMemory);
}

HWTEST_F(AUBSimpleArgNonUniformTest, givenOpenCL20SupportWhenProvidingWork3DimNonUniformGroupInYDimensionThenExpectTwoWalkers) {
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;
    cl_uint workDim = 3;
    size_t globalWorkOffset[3] = {0, 0, 0};
    size_t globalWorkSize[3] = {32, 39, 32};
    size_t localWorkSize[3] = {8, 8, 2};
    cl_uint numEventsInWaitList = 0;
    cl_event *eventWaitList = nullptr;
    cl_event *event = nullptr;

    initializeExpectedMemory(globalWorkSize[0], globalWorkSize[1], globalWorkSize[2]);

    auto retVal = this->pCmdQ->enqueueKernel(
        this->kernel,
        workDim,
        globalWorkOffset,
        globalWorkSize,
        localWorkSize,
        numEventsInWaitList,
        eventWaitList,
        event);
    ASSERT_EQ(CL_SUCCESS, retVal);

    hwParser.parseCommands<FamilyType>(*pCmdQ);
    uint32_t walkerCount = hwParser.getCommandCount<WALKER_TYPE>();
    EXPECT_EQ(2u, walkerCount);

    pCmdQ->flush();
    expectMemory<FamilyType>(bufferGpuAddress, this->expectedMemory, sizeWrittenMemory);
    expectMemory<FamilyType>(remainderBufferGpuAddress, this->expectedRemainderMemory, sizeRemainderMemory);
}

HWTEST_F(AUBSimpleArgNonUniformTest, givenOpenCL20SupportWhenProvidingWork3DimNonUniformGroupInZDimensionThenExpectTwoWalkers) {
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;
    cl_uint workDim = 3;
    size_t globalWorkOffset[3] = {0, 0, 0};
    size_t globalWorkSize[3] = {32, 32, 39};
    size_t localWorkSize[3] = {8, 2, 8};
    cl_uint numEventsInWaitList = 0;
    cl_event *eventWaitList = nullptr;
    cl_event *event = nullptr;

    initializeExpectedMemory(globalWorkSize[0], globalWorkSize[1], globalWorkSize[2]);

    auto retVal = this->pCmdQ->enqueueKernel(
        this->kernel,
        workDim,
        globalWorkOffset,
        globalWorkSize,
        localWorkSize,
        numEventsInWaitList,
        eventWaitList,
        event);
    ASSERT_EQ(CL_SUCCESS, retVal);

    hwParser.parseCommands<FamilyType>(*pCmdQ);
    uint32_t walkerCount = hwParser.getCommandCount<WALKER_TYPE>();
    EXPECT_EQ(2u, walkerCount);

    pCmdQ->flush();
    expectMemory<FamilyType>(bufferGpuAddress, this->expectedMemory, sizeWrittenMemory);
    expectMemory<FamilyType>(remainderBufferGpuAddress, this->expectedRemainderMemory, sizeRemainderMemory);
}

HWTEST_F(AUBSimpleArgNonUniformTest, givenOpenCL20SupportWhenProvidingWork3DimNonUniformGroupInXandYDimensionThenExpectFourWalkers) {
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;
    cl_uint workDim = 3;
    size_t globalWorkOffset[3] = {0, 0, 0};
    size_t globalWorkSize[3] = {39, 39, 32};
    size_t localWorkSize[3] = {8, 8, 2};
    cl_uint numEventsInWaitList = 0;
    cl_event *eventWaitList = nullptr;
    cl_event *event = nullptr;

    initializeExpectedMemory(globalWorkSize[0], globalWorkSize[1], globalWorkSize[2]);

    auto retVal = this->pCmdQ->enqueueKernel(
        this->kernel,
        workDim,
        globalWorkOffset,
        globalWorkSize,
        localWorkSize,
        numEventsInWaitList,
        eventWaitList,
        event);
    ASSERT_EQ(CL_SUCCESS, retVal);

    hwParser.parseCommands<FamilyType>(*pCmdQ);
    uint32_t walkerCount = hwParser.getCommandCount<WALKER_TYPE>();
    EXPECT_EQ(4u, walkerCount);

    pCmdQ->flush();
    expectMemory<FamilyType>(bufferGpuAddress, this->expectedMemory, sizeWrittenMemory);
    expectMemory<FamilyType>(remainderBufferGpuAddress, this->expectedRemainderMemory, sizeRemainderMemory);
}

HWTEST_F(AUBSimpleArgNonUniformTest, givenOpenCL20SupportWhenProvidingWork3DimNonUniformGroupInXandZDimensionThenExpectFourWalkers) {
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;
    cl_uint workDim = 3;
    size_t globalWorkOffset[3] = {0, 0, 0};
    size_t globalWorkSize[3] = {39, 32, 39};
    size_t localWorkSize[3] = {8, 2, 8};
    cl_uint numEventsInWaitList = 0;
    cl_event *eventWaitList = nullptr;
    cl_event *event = nullptr;

    initializeExpectedMemory(globalWorkSize[0], globalWorkSize[1], globalWorkSize[2]);

    auto retVal = this->pCmdQ->enqueueKernel(
        this->kernel,
        workDim,
        globalWorkOffset,
        globalWorkSize,
        localWorkSize,
        numEventsInWaitList,
        eventWaitList,
        event);
    ASSERT_EQ(CL_SUCCESS, retVal);

    hwParser.parseCommands<FamilyType>(*pCmdQ);
    uint32_t walkerCount = hwParser.getCommandCount<WALKER_TYPE>();
    EXPECT_EQ(4u, walkerCount);

    pCmdQ->flush();
    expectMemory<FamilyType>(bufferGpuAddress, this->expectedMemory, sizeWrittenMemory);
    expectMemory<FamilyType>(remainderBufferGpuAddress, this->expectedRemainderMemory, sizeRemainderMemory);
}

HWTEST_F(AUBSimpleArgNonUniformTest, givenOpenCL20SupportWhenProvidingWork3DimNonUniformGroupInYandZDimensionThenExpectFourWalkers) {
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;
    cl_uint workDim = 3;
    size_t globalWorkOffset[3] = {0, 0, 0};
    size_t globalWorkSize[3] = {32, 39, 39};
    size_t localWorkSize[3] = {2, 8, 8};
    cl_uint numEventsInWaitList = 0;
    cl_event *eventWaitList = nullptr;
    cl_event *event = nullptr;

    initializeExpectedMemory(globalWorkSize[0], globalWorkSize[1], globalWorkSize[2]);

    auto retVal = this->pCmdQ->enqueueKernel(
        this->kernel,
        workDim,
        globalWorkOffset,
        globalWorkSize,
        localWorkSize,
        numEventsInWaitList,
        eventWaitList,
        event);
    ASSERT_EQ(CL_SUCCESS, retVal);

    hwParser.parseCommands<FamilyType>(*pCmdQ);
    uint32_t walkerCount = hwParser.getCommandCount<WALKER_TYPE>();
    EXPECT_EQ(4u, walkerCount);

    pCmdQ->flush();
    expectMemory<FamilyType>(bufferGpuAddress, this->expectedMemory, sizeWrittenMemory);
    expectMemory<FamilyType>(remainderBufferGpuAddress, this->expectedRemainderMemory, sizeRemainderMemory);
}

HWTEST_F(AUBSimpleArgNonUniformTest, givenOpenCL20SupportWhenProvidingWork3DimNonUniformGroupInXandYandZDimensionThenExpectEightWalkers) {
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;
    cl_uint workDim = 3;
    size_t globalWorkOffset[3] = {0, 0, 0};
    size_t globalWorkSize[3] = {39, 39, 39};
    size_t localWorkSize[3] = {8, 8, 2};
    cl_uint numEventsInWaitList = 0;
    cl_event *eventWaitList = nullptr;
    cl_event *event = nullptr;

    initializeExpectedMemory(globalWorkSize[0], globalWorkSize[1], globalWorkSize[2]);

    auto retVal = this->pCmdQ->enqueueKernel(
        this->kernel,
        workDim,
        globalWorkOffset,
        globalWorkSize,
        localWorkSize,
        numEventsInWaitList,
        eventWaitList,
        event);
    ASSERT_EQ(CL_SUCCESS, retVal);

    hwParser.parseCommands<FamilyType>(*pCmdQ);
    uint32_t walkerCount = hwParser.getCommandCount<WALKER_TYPE>();
    EXPECT_EQ(8u, walkerCount);

    pCmdQ->flush();
    expectMemory<FamilyType>(bufferGpuAddress, this->expectedMemory, sizeWrittenMemory);
    expectMemory<FamilyType>(remainderBufferGpuAddress, this->expectedRemainderMemory, sizeRemainderMemory);
}

struct AUBBindlessKernel : public KernelAUBFixture<BindlessKernelFixture>,
                           public ::testing::Test {

    void SetUp() override {
        DebugManager.flags.UseBindlessMode.set(1);
        DebugManager.flags.UseExternalAllocatorForSshAndDsh.set(1);
        KernelAUBFixture<BindlessKernelFixture>::SetUp();
    }

    void TearDown() override {
        KernelAUBFixture<BindlessKernelFixture>::TearDown();
    }
    DebugManagerStateRestore restorer;
};

HWTEST2_F(AUBBindlessKernel, DISABLED_givenBindlessCopyKernelWhenEnqueuedThenResultsValidate, IsAtLeastSkl) {
    constexpr size_t bufferSize = MemoryConstants::pageSize;
    auto simulatedCsr = AUBFixture::getSimulatedCsr<FamilyType>();
    simulatedCsr->initializeEngine();

    createKernel(std::string("bindless_stateful_copy_buffer"), std::string("StatefulCopyBuffer"));

    cl_uint workDim = 1;
    size_t globalWorkOffset[3] = {0, 0, 0};
    size_t globalWorkSize[3] = {bufferSize / 2, 1, 1};
    size_t localWorkSize[3] = {1, 1, 1};
    cl_uint numEventsInWaitList = 0;
    cl_event *eventWaitList = nullptr;
    cl_event *event = nullptr;

    uint8_t bufferDataSrc[bufferSize];
    uint8_t bufferDataDst[bufferSize];

    memset(bufferDataSrc, 1, bufferSize);
    memset(bufferDataDst, 0, bufferSize);

    auto pBufferSrc = std::unique_ptr<Buffer>(Buffer::create(context,
                                                             CL_MEM_READ_WRITE,
                                                             bufferSize,
                                                             nullptr,
                                                             retVal));
    ASSERT_NE(nullptr, pBufferSrc);

    auto pBufferDst = std::unique_ptr<Buffer>(Buffer::create(context,
                                                             CL_MEM_READ_WRITE,
                                                             bufferSize,
                                                             nullptr,
                                                             retVal));
    ASSERT_NE(nullptr, pBufferDst);

    memcpy(pBufferSrc->getGraphicsAllocation(device->getRootDeviceIndex())->getUnderlyingBuffer(), bufferDataSrc, bufferSize);
    memcpy(pBufferDst->getGraphicsAllocation(device->getRootDeviceIndex())->getUnderlyingBuffer(), bufferDataDst, bufferSize);

    simulatedCsr->writeMemory(*pBufferSrc->getGraphicsAllocation(device->getRootDeviceIndex()));
    simulatedCsr->writeMemory(*pBufferDst->getGraphicsAllocation(device->getRootDeviceIndex()));

    // Src
    kernel->setArg(0, pBufferSrc.get());
    // Dst
    kernel->setArg(1, pBufferDst.get());

    retVal = this->pCmdQ->enqueueKernel(
        kernel.get(),
        workDim,
        globalWorkOffset,
        globalWorkSize,
        localWorkSize,
        numEventsInWaitList,
        eventWaitList,
        event);

    ASSERT_EQ(CL_SUCCESS, retVal);

    globalWorkOffset[0] = bufferSize / 2;
    retVal = this->pCmdQ->enqueueKernel(
        kernel.get(),
        workDim,
        globalWorkOffset,
        globalWorkSize,
        localWorkSize,
        numEventsInWaitList,
        eventWaitList,
        event);

    ASSERT_EQ(CL_SUCCESS, retVal);

    EXPECT_TRUE(this->kernel->getKernelInfo().kernelDescriptor.payloadMappings.explicitArgs[0].as<ArgDescPointer>().isPureStateful());

    this->pCmdQ->finish();
    expectMemory<FamilyType>(reinterpret_cast<void *>(pBufferDst->getGraphicsAllocation(device->getRootDeviceIndex())->getGpuAddress()),
                             bufferDataSrc, bufferSize);
}

HWTEST2_F(AUBBindlessKernel, DISABLED_givenBindlessCopyImageKernelWhenEnqueuedThenResultsValidate, IsAtLeastSkl) {
    constexpr unsigned int testWidth = 5;
    constexpr unsigned int testHeight = 1;
    constexpr unsigned int testDepth = 1;
    auto simulatedCsr = AUBFixture::getSimulatedCsr<FamilyType>();
    simulatedCsr->initializeEngine();

    createKernel(std::string("bindless_copy_buffer_to_image"), std::string("CopyBufferToImage3d"));

    constexpr size_t imageSize = testWidth * testHeight * testDepth;
    cl_uint workDim = 1;
    size_t globalWorkOffset[3] = {0, 0, 0};
    size_t globalWorkSize[3] = {imageSize, 1, 1};
    size_t localWorkSize[3] = {1, 1, 1};
    cl_uint numEventsInWaitList = 0;
    cl_event *eventWaitList = nullptr;
    cl_event *event = nullptr;

    uint8_t imageDataSrc[imageSize];
    uint8_t imageDataDst[imageSize + 1];

    memset(imageDataSrc, 1, imageSize);
    memset(imageDataDst, 0, imageSize + 1);

    cl_image_format imageFormat = {0};
    cl_image_desc imageDesc = {0};

    imageFormat.image_channel_data_type = CL_UNSIGNED_INT8;
    imageFormat.image_channel_order = CL_R;
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE1D;
    imageDesc.image_width = testWidth;
    imageDesc.image_height = testHeight;
    imageDesc.image_depth = testDepth;
    imageDesc.image_array_size = 1;
    imageDesc.image_row_pitch = 0;
    imageDesc.image_slice_pitch = 0;
    imageDesc.num_mip_levels = 0;
    imageDesc.num_samples = 0;

    auto retVal = CL_INVALID_VALUE;
    cl_mem_flags flags = CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR;

    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat, device->getHardwareInfo().capabilityTable.supportsOcl21Features);
    auto image = std::unique_ptr<Image>(Image::create(
        contextCl,
        ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &contextCl->getDevice(0)->getDevice()),
        flags,
        0,
        surfaceFormat,
        &imageDesc,
        imageDataDst,
        retVal));
    ASSERT_NE(nullptr, image.get());
    EXPECT_FALSE(image->isMemObjZeroCopy());

    auto bufferSrc = std::unique_ptr<Buffer>(Buffer::create(context,
                                                            CL_MEM_READ_WRITE,
                                                            imageSize,
                                                            nullptr,
                                                            retVal));
    ASSERT_NE(nullptr, bufferSrc);

    memcpy(image->getGraphicsAllocation(device->getRootDeviceIndex())->getUnderlyingBuffer(), imageDataDst, imageSize);
    memcpy(bufferSrc->getGraphicsAllocation(device->getRootDeviceIndex())->getUnderlyingBuffer(), imageDataSrc, imageSize);

    simulatedCsr->writeMemory(*bufferSrc->getGraphicsAllocation(device->getRootDeviceIndex()));
    simulatedCsr->writeMemory(*image->getGraphicsAllocation(device->getRootDeviceIndex()));

    kernel->setArg(0, bufferSrc.get());
    kernel->setArg(1, image.get());

    int srcOffset = 0;
    int dstOffset[4] = {0, 0, 0, 0};
    int pitch[2] = {0, 0};

    kernel->setArg(2, sizeof(srcOffset), &srcOffset);
    kernel->setArg(3, sizeof(dstOffset), &dstOffset);
    kernel->setArg(4, sizeof(pitch), &pitch);

    retVal = this->pCmdQ->enqueueKernel(
        kernel.get(),
        workDim,
        globalWorkOffset,
        globalWorkSize,
        localWorkSize,
        numEventsInWaitList,
        eventWaitList,
        event);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = this->pCmdQ->finish();
    EXPECT_EQ(CL_SUCCESS, retVal);

    expectMemory<FamilyType>(reinterpret_cast<void *>(image->getGraphicsAllocation(device->getRootDeviceIndex())->getGpuAddress()),
                             imageDataSrc, imageSize);
}
