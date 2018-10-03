/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_queue/command_queue.h"
#include "runtime/helpers/ptr_math.h"
#include "gen_cmd_parse.h"
#include "unit_tests/aub_tests/fixtures/hello_world_fixture.h"
#include "unit_tests/fixtures/hello_world_fixture.h"
#include "unit_tests/fixtures/simple_arg_fixture.h"
#include "unit_tests/fixtures/two_walker_fixture.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "test.h"

using namespace OCLRT;

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
    {16, 1, 1, 1, 1, 1},
    {16, 1, 1, 16, 1, 1},
    {32, 1, 1, 1, 1, 1},
    {32, 1, 1, 16, 1, 1},
    {32, 1, 1, 32, 1, 1},
    {64, 1, 1, 1, 1, 1},
    {64, 1, 1, 16, 1, 1},
    {64, 1, 1, 32, 1, 1},
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
        ParentClass::SetUp();
        auto &commandStreamReceiver = pDevice->getCommandStreamReceiver();
        commandStreamReceiver.createAllocationAndHandleResidency(pDestMemory, sizeUserMemory);
        commandStreamReceiver.createAllocationAndHandleResidency(pSrcMemory, sizeUserMemory);
    }

    void TearDown() override {
        ParentClass::TearDown();
    }
    TestParam param;
};

HWTEST_P(AUBHelloWorldIntegrateTest, simple) {
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
    pCmdQ->getDevice().getCommandStreamReceiver().overrideDispatchPolicy(DispatchMode::ImmediateDispatch);

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
        ParentClass::SetUp();
        std::tie(simd, param) = GetParam();
    }

    void TearDown() override {
        ParentClass::TearDown();
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
