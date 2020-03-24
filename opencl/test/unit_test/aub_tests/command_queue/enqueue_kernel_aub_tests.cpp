/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/ptr_math.h"
#include "shared/test/unit_test/helpers/debug_manager_state_restore.h"

#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/event/event.h"
#include "opencl/source/helpers/hardware_commands_helper.h"
#include "opencl/test/unit_test/aub_tests/fixtures/aub_fixture.h"
#include "opencl/test/unit_test/aub_tests/fixtures/hello_world_fixture.h"
#include "opencl/test/unit_test/fixtures/hello_world_fixture.h"
#include "opencl/test/unit_test/fixtures/simple_arg_fixture.h"
#include "opencl/test/unit_test/fixtures/two_walker_fixture.h"
#include "opencl/test/unit_test/gen_common/gen_cmd_parse.h"
#include "opencl/test/unit_test/helpers/unit_test_helper.h"
#include "opencl/test/unit_test/mocks/mock_buffer.h"
#include "test.h"

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
      public HardwareParse,
      public ::testing::Test {

    void SetUp() override {
        HelloWorldFixture<AUBHelloWorldFixtureFactory>::SetUp();
        HardwareParse::SetUp();
    }

    void TearDown() override {
        HardwareParse::TearDown();
        HelloWorldFixture<AUBHelloWorldFixtureFactory>::TearDown();
    }
};

HWCMDTEST_F(IGFX_GEN8_CORE, AUBHelloWorld, simple) {
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

        aubCsr->writeMemory(*allocation);
    }
    TestParam param;
};

HWTEST_P(AUBHelloWorldIntegrateTest, simple) {
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

    writeMemory<FamilyType>(destBuffer->getGraphicsAllocation());
    writeMemory<FamilyType>(srcBuffer->getGraphicsAllocation());

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

    auto pDestGpuAddress = reinterpret_cast<void *>((destBuffer->getGraphicsAllocation()->getGpuAddress()));

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
      public HardwareParse,
      public ::testing::Test {

    using SimpleArgKernelFixture::SetUp;

    void SetUp() override {
        SimpleArgFixture<AUBSimpleArgFixtureFactory>::SetUp();
        HardwareParse::SetUp();
    }

    void TearDown() override {
        HardwareParse::TearDown();
        SimpleArgFixture<AUBSimpleArgFixtureFactory>::TearDown();
    }
};

HWCMDTEST_F(IGFX_GEN8_CORE, AUBSimpleArg, simple) {
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

HWTEST_P(AUBSimpleArgIntegrateTest, simple) {
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
        if (NEO::defaultHwInfo->capabilityTable.clVersionSupport < 20) {
            GTEST_SKIP();
        }
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
        bufferGpuAddress = reinterpret_cast<void *>(outBuffer->getGraphicsAllocation()->getGpuAddress());
        kernel->setArg(1, outBuffer.get());

        sizeWrittenMemory = maxId * typeSize;
        //add single int size for atomic sum of all work-items
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
        if (NEO::defaultHwInfo->capabilityTable.clVersionSupport < 20) {
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

    HardwareParse hwParser;
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
    EXPECT_THAT(this->pProgram->getInternalOptions(),
                testing::HasSubstr(std::string(NEO::CompilerOptions::greaterThan4gbBuffersRequired)));

    if (this->device->getSharedDeviceInfo().force32BitAddressess) {
        EXPECT_THAT(this->pProgram->getInternalOptions(),
                    testing::HasSubstr(std::string(NEO::CompilerOptions::arch32bit)));
    }

    EXPECT_FALSE(this->kernel->getKernelInfo().kernelArgInfo[0].pureStatefulBufferAccess);
    EXPECT_TRUE(this->kernel->getKernelInfo().patchInfo.executionEnvironment->CompiledForGreaterThan4GBBuffers);

    this->pCmdQ->flush();
    expectMemory<FamilyType>(reinterpret_cast<void *>(pBuffer->getGraphicsAllocation()->getGpuAddress()),
                             bufferExpected, bufferSize);
}

using AUBSimpleArgNonUniformTest = Test<AUBSimpleArgNonUniformFixture>;
HWTEST_F(AUBSimpleArgNonUniformTest, givenOpenCL20SupportWhenProvidingWork1DimNonUniformGroupThenExpectTwoWalkers) {
    using WALKER_TYPE = WALKER_TYPE<FamilyType>;
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
    using WALKER_TYPE = WALKER_TYPE<FamilyType>;
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
    using WALKER_TYPE = WALKER_TYPE<FamilyType>;
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
    using WALKER_TYPE = WALKER_TYPE<FamilyType>;
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
    using WALKER_TYPE = WALKER_TYPE<FamilyType>;
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
    using WALKER_TYPE = WALKER_TYPE<FamilyType>;
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
    using WALKER_TYPE = WALKER_TYPE<FamilyType>;
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
    using WALKER_TYPE = WALKER_TYPE<FamilyType>;
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
    using WALKER_TYPE = WALKER_TYPE<FamilyType>;
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
    using WALKER_TYPE = WALKER_TYPE<FamilyType>;
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
    using WALKER_TYPE = WALKER_TYPE<FamilyType>;
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
