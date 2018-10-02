/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/built_ins/built_ins.h"
#include "runtime/built_ins/builtins_dispatch_builder.h"
#include "runtime/command_queue/command_queue_hw.h"
#include "reg_configs_common.h"
#include "runtime/helpers/preamble.h"
#include "runtime/memory_manager/graphics_allocation.h"
#include "runtime/memory_manager/memory_constants.h"
#include "runtime/utilities/tag_allocator.h"
#include "unit_tests/command_queue/enqueue_fixture.h"
#include "unit_tests/fixtures/hello_world_fixture.h"
#include "unit_tests/fixtures/memory_management_fixture.h"
#include "unit_tests/gen_common/gen_commands_common_validation.h"
#include "unit_tests/helpers/hw_parse.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/helpers/unit_test_helper.h"
#include "unit_tests/mocks/mock_csr.h"
#include "unit_tests/mocks/mock_command_queue.h"
#include "unit_tests/mocks/mock_device_queue.h"
#include "unit_tests/mocks/mock_buffer.h"
#include "unit_tests/mocks/mock_submissions_aggregator.h"
#include "runtime/helpers/hw_info.h"

using namespace OCLRT;

typedef HelloWorldFixture<HelloWorldFixtureFactory> EnqueueKernelFixture;
typedef Test<EnqueueKernelFixture> EnqueueKernelTest;

TEST_F(EnqueueKernelTest, clEnqueueNDRangeKernel_null_kernel) {
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
    size_t localWorkSize[3] = {256, 1, 1};
    cl_int retVal = CL_SUCCESS;
    CommandQueue *pCmdQ2 = createCommandQueue(pDevice, 0);

    std::unique_ptr<Kernel> kernel(Kernel::create(pProgram, *pProgram->getKernelInfo("CopyBuffer"), &retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto b0 = clCreateBuffer(context, 0, n * sizeof(float), nullptr, nullptr);
    auto b1 = clCreateBuffer(context, 0, n * sizeof(float), nullptr, nullptr);

    EXPECT_FALSE(kernel->isPatched());
    retVal = clEnqueueNDRangeKernel(pCmdQ2, kernel.get(), 1, nullptr, globalWorkSize, localWorkSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_KERNEL_ARGS, retVal);

    retVal = clSetKernelArg(kernel.get(), 0, sizeof(cl_mem), &b0);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_FALSE(kernel->isPatched());
    retVal = clEnqueueNDRangeKernel(pCmdQ2, kernel.get(), 1, nullptr, globalWorkSize, localWorkSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_KERNEL_ARGS, retVal);

    retVal = clSetKernelArg(kernel.get(), 1, sizeof(cl_mem), &b1);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_TRUE(kernel->isPatched());
    retVal = clEnqueueNDRangeKernel(pCmdQ2, kernel.get(), 1, nullptr, globalWorkSize, localWorkSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseMemObject(b0);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clReleaseMemObject(b1);
    EXPECT_EQ(CL_SUCCESS, retVal);
    clReleaseCommandQueue(pCmdQ2);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(EnqueueKernelTest, givenKernelWhenNotAllArgsAreSetButSetKernelArgIsCalledTwiceThenClEnqueueNDRangeKernelReturnsError) {
    const size_t n = 512;
    size_t globalWorkSize[3] = {n, 1, 1};
    size_t localWorkSize[3] = {256, 1, 1};
    cl_int retVal = CL_SUCCESS;
    CommandQueue *pCmdQ2 = createCommandQueue(pDevice, 0);

    std::unique_ptr<Kernel> kernel(Kernel::create(pProgram, *pProgram->getKernelInfo("CopyBuffer"), &retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto b0 = clCreateBuffer(context, 0, n * sizeof(float), nullptr, nullptr);
    auto b1 = clCreateBuffer(context, 0, n * sizeof(float), nullptr, nullptr);

    EXPECT_FALSE(kernel->isPatched());
    retVal = clEnqueueNDRangeKernel(pCmdQ2, kernel.get(), 1, nullptr, globalWorkSize, localWorkSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_KERNEL_ARGS, retVal);

    retVal = clSetKernelArg(kernel.get(), 0, sizeof(cl_mem), &b0);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_FALSE(kernel->isPatched());
    retVal = clEnqueueNDRangeKernel(pCmdQ2, kernel.get(), 1, nullptr, globalWorkSize, localWorkSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_KERNEL_ARGS, retVal);

    retVal = clSetKernelArg(kernel.get(), 0, sizeof(cl_mem), &b1);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_FALSE(kernel->isPatched());
    retVal = clEnqueueNDRangeKernel(pCmdQ2, kernel.get(), 1, nullptr, globalWorkSize, localWorkSize, 0, nullptr, nullptr);
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
    CommandQueue *pCmdQ2 = createCommandQueue(pDevice, 0);

    std::unique_ptr<Kernel> kernel(Kernel::create(pProgram, *pProgram->getKernelInfo("CopyBuffer"), &retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto b0 = clCreateBuffer(context, 0, n * sizeof(float), nullptr, nullptr);
    auto b1 = clCreateBuffer(context, 0, n * sizeof(float), nullptr, nullptr);

    EXPECT_FALSE(kernel->isPatched());
    retVal = clEnqueueNDRangeKernel(pCmdQ2, kernel.get(), 1, nullptr, globalWorkSize, localWorkSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_KERNEL_ARGS, retVal);

    retVal = clSetKernelArg(kernel.get(), 0, sizeof(cl_mem), &b0);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_FALSE(kernel->isPatched());
    retVal = clEnqueueNDRangeKernel(pCmdQ2, kernel.get(), 1, nullptr, globalWorkSize, localWorkSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_KERNEL_ARGS, retVal);

    retVal = clSetKernelArg(kernel.get(), 1, 2 * sizeof(cl_mem), &b1);
    EXPECT_NE(CL_SUCCESS, retVal);

    EXPECT_FALSE(kernel->isPatched());
    retVal = clEnqueueNDRangeKernel(pCmdQ2, kernel.get(), 1, nullptr, globalWorkSize, localWorkSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_KERNEL_ARGS, retVal);

    retVal = clReleaseMemObject(b0);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clReleaseMemObject(b1);
    EXPECT_EQ(CL_SUCCESS, retVal);
    clReleaseCommandQueue(pCmdQ2);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(EnqueueKernelTest, clEnqueueNDRangeKernel_invalid_event_list_count) {
    size_t globalWorkSize[3] = {1, 1, 1};

    auto retVal = clEnqueueNDRangeKernel(
        pCmdQ,
        pKernel,
        1,
        nullptr,
        globalWorkSize,
        nullptr,
        1,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_INVALID_EVENT_WAIT_LIST, retVal);
}

TEST_F(EnqueueKernelTest, clEnqueueNDRangeKernel_invalidWorkGroupSize) {
    size_t globalWorkSize[3] = {12, 12, 12};
    size_t localWorkSize[3] = {11, 12, 12};

    auto retVal = clEnqueueNDRangeKernel(
        pCmdQ,
        pKernel,
        3,
        nullptr,
        globalWorkSize,
        localWorkSize,
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_INVALID_WORK_GROUP_SIZE, retVal);
}

struct TestParam2 {
    cl_uint ScratchSize;
} TestParamTable2[] = {{1024}, {2048}, {4096}, {8192}, {16384}};

struct TestParam {
    cl_uint globalWorkSizeX;
    cl_uint globalWorkSizeY;
    cl_uint globalWorkSizeZ;
    cl_uint localWorkSizeX;
    cl_uint localWorkSizeY;
    cl_uint localWorkSizeZ;
} TestParamTable[] = {
    {1, 1, 1, 1, 1, 1},
    {16, 1, 1, 1, 1, 1},
    {16, 1, 1, 16, 1, 1},
    {32, 1, 1, 1, 1, 1},
    {32, 1, 1, 16, 1, 1},
    {32, 1, 1, 32, 1, 1},
    {64, 1, 1, 1, 1, 1},
    {64, 1, 1, 16, 1, 1},
    {64, 1, 1, 32, 1, 1},
    {64, 1, 1, 64, 1, 1},
    {190, 1, 1, 95, 1, 1},
    {510, 1, 1, 255, 1, 1},
    {512, 1, 1, 256, 1, 1}},
  OneEntryTestParamTable[] = {
      {1, 1, 1, 1, 1, 1},
};
template <typename InputType>
struct EnqueueKernelTypeTest : public HelloWorldFixture<HelloWorldFixtureFactory>,
                               public HardwareParse,
                               ::testing::TestWithParam<InputType> {
    typedef HelloWorldFixture<HelloWorldFixtureFactory> ParentClass;
    using ParentClass::pCmdBuffer;
    using ParentClass::pCS;

    EnqueueKernelTypeTest() {
    }
    void FillValues() {
        globalWorkSize[0] = 1;
        globalWorkSize[1] = 1;
        globalWorkSize[2] = 1;
        localWorkSize[0] = 1;
        localWorkSize[1] = 1;
        localWorkSize[2] = 1;
    };

    template <typename FamilyType, bool ParseCommands>
    typename std::enable_if<false == ParseCommands, void>::type enqueueKernel(Kernel *inputKernel = nullptr) {
        cl_uint workDim = 1;
        size_t globalWorkOffset[3] = {0, 0, 0};

        cl_uint numEventsInWaitList = 0;
        cl_event *eventWaitList = nullptr;
        cl_event *event = nullptr;

        FillValues();
        // Compute # of expected work items
        expectedWorkItems = 1;
        for (auto i = 0u; i < workDim; i++) {
            expectedWorkItems *= globalWorkSize[i];
        }

        auto usedKernel = inputKernel ? inputKernel : pKernel;

        auto retVal = pCmdQ->enqueueKernel(
            usedKernel,
            workDim,
            globalWorkOffset,
            globalWorkSize,
            localWorkSize,
            numEventsInWaitList,
            eventWaitList,
            event);
        ASSERT_EQ(CL_SUCCESS, retVal);
    }

    template <typename FamilyType, bool ParseCommands>
    typename std::enable_if<ParseCommands, void>::type enqueueKernel(Kernel *inputKernel = nullptr) {
        enqueueKernel<FamilyType, false>(inputKernel);

        parseCommands<FamilyType>(*pCmdQ);
    }

    template <typename FamilyType>
    void enqueueKernel(Kernel *inputKernel = nullptr) {
        enqueueKernel<FamilyType, true>(inputKernel);
    }

    void SetUp() override {
        ParentClass::SetUp();
        HardwareParse::SetUp();
    }
    void TearDown() override {
        HardwareParse::TearDown();
        ParentClass::TearDown();
    }
    size_t globalWorkSize[3];
    size_t localWorkSize[3];
    size_t expectedWorkItems = 0;
};

template <>
void EnqueueKernelTypeTest<TestParam>::FillValues() {
    const TestParam &param = GetParam();
    globalWorkSize[0] = param.globalWorkSizeX;
    globalWorkSize[1] = param.globalWorkSizeY;
    globalWorkSize[2] = param.globalWorkSizeZ;
    localWorkSize[0] = param.localWorkSizeX;
    localWorkSize[1] = param.localWorkSizeY;
    localWorkSize[2] = param.localWorkSizeZ;
}

typedef EnqueueKernelTypeTest<TestParam> EnqueueWorkItemTests;
typedef EnqueueKernelTypeTest<TestParam> EnqueueWorkItemTestsWithLimitedParamSet;

HWCMDTEST_P(IGFX_GEN8_CORE, EnqueueWorkItemTests, GPGPUWalker) {
    typedef typename FamilyType::PARSE PARSE;
    typedef typename PARSE::GPGPU_WALKER GPGPU_WALKER;

    enqueueKernel<FamilyType>();

    ASSERT_NE(cmdList.end(), itorWalker);
    auto *cmd = (GPGPU_WALKER *)*itorWalker;

    // Verify GPGPU_WALKER parameters
    EXPECT_NE(0u, cmd->getThreadGroupIdXDimension());
    EXPECT_NE(0u, cmd->getThreadGroupIdYDimension());
    EXPECT_NE(0u, cmd->getThreadGroupIdZDimension());
    EXPECT_NE(0u, cmd->getRightExecutionMask());
    EXPECT_NE(0u, cmd->getBottomExecutionMask());
    EXPECT_EQ(GPGPU_WALKER::SIMD_SIZE_SIMD32, cmd->getSimdSize());
    EXPECT_NE(0u, cmd->getIndirectDataLength());
    EXPECT_FALSE(cmd->getIndirectParameterEnable());

    // Compute the SIMD lane mask
    size_t simd =
        cmd->getSimdSize() == GPGPU_WALKER::SIMD_SIZE_SIMD32 ? 32 : cmd->getSimdSize() == GPGPU_WALKER::SIMD_SIZE_SIMD16 ? 16 : 8;
    uint64_t simdMask = (1ull << simd) - 1;

    // Mask off lanes based on the execution masks
    auto laneMaskRight = cmd->getRightExecutionMask() & simdMask;
    auto lanesPerThreadX = 0;
    while (laneMaskRight) {
        lanesPerThreadX += laneMaskRight & 1;
        laneMaskRight >>= 1;
    }

    auto numWorkItems = ((cmd->getThreadWidthCounterMaximum() - 1) * simd + lanesPerThreadX) * cmd->getThreadGroupIdXDimension();
    EXPECT_EQ(expectedWorkItems, numWorkItems);
}

HWTEST_F(EnqueueKernelTest, bumpsTaskLevel) {
    auto taskLevelBefore = pCmdQ->taskLevel;
    callOneWorkItemNDRKernel();
    EXPECT_GT(pCmdQ->taskLevel, taskLevelBefore);
}

HWTEST_F(EnqueueKernelTest, alignsToCSR) {
    //this test case assumes IOQ
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.taskCount = pCmdQ->taskCount + 100;
    csr.taskLevel = pCmdQ->taskLevel + 50;

    callOneWorkItemNDRKernel();
    EXPECT_EQ(pCmdQ->taskCount, csr.peekTaskCount());
    EXPECT_EQ(pCmdQ->taskLevel + 1, csr.peekTaskLevel());
}

HWTEST_F(EnqueueKernelTest, addsCommands) {
    auto usedCmdBufferBefore = pCS->getUsed();

    callOneWorkItemNDRKernel();
    EXPECT_NE(usedCmdBufferBefore, pCS->getUsed());
}

HWTEST_F(EnqueueKernelTest, addsIndirectData) {
    auto dshBefore = pDSH->getUsed();
    auto iohBefore = pIOH->getUsed();
    auto sshBefore = pSSH->getUsed();

    callOneWorkItemNDRKernel();
    EXPECT_TRUE(UnitTestHelper<FamilyType>::evaluateDshUsage(dshBefore, pDSH->getUsed(), pKernel));
    EXPECT_NE(iohBefore, pIOH->getUsed());
    if (pKernel->requiresSshForBuffers() || (pKernel->getKernelInfo().patchInfo.imageMemObjKernelArgs.size() > 0)) {
        EXPECT_NE(sshBefore, pSSH->getUsed());
    }
}

HWCMDTEST_P(IGFX_GEN8_CORE, EnqueueWorkItemTestsWithLimitedParamSet, LoadRegisterImmediateL3CNTLREG) {
    enqueueKernel<FamilyType>();
    validateL3Programming<FamilyType>(cmdList, itorWalker);
}

HWCMDTEST_P(IGFX_GEN8_CORE, EnqueueWorkItemTestsWithLimitedParamSet, WhenEnqueueIsDoneThenStateBaseAddressIsProperlyProgrammed) {
    enqueueKernel<FamilyType>();
    validateStateBaseAddress<FamilyType>(this->pDevice->getCommandStreamReceiver().getMemoryManager()->getInternalHeapBaseAddress(),
                                         pDSH, pIOH, pSSH, itorPipelineSelect, itorWalker, cmdList,
                                         context->getMemoryManager()->peekForce32BitAllocations() ? context->getMemoryManager()->allocator32Bit->getBase() : 0llu);
}

HWCMDTEST_P(IGFX_GEN8_CORE, EnqueueWorkItemTestsWithLimitedParamSet, MediaInterfaceDescriptorLoad) {
    typedef typename FamilyType::PARSE PARSE;
    typedef typename PARSE::MEDIA_INTERFACE_DESCRIPTOR_LOAD MEDIA_INTERFACE_DESCRIPTOR_LOAD;
    typedef typename PARSE::INTERFACE_DESCRIPTOR_DATA INTERFACE_DESCRIPTOR_DATA;
    enqueueKernel<FamilyType>();

    // All state should be programmed before walker
    auto itorCmd = find<MEDIA_INTERFACE_DESCRIPTOR_LOAD *>(itorPipelineSelect, itorWalker);
    ASSERT_NE(itorWalker, itorCmd);

    auto *cmd = genCmdCast<MEDIA_INTERFACE_DESCRIPTOR_LOAD *>(*itorCmd);

    // Verify we have a valid length -- multiple of INTERFACE_DESCRIPTOR_DATAs
    EXPECT_EQ(0u, cmd->getInterfaceDescriptorTotalLength() % sizeof(INTERFACE_DESCRIPTOR_DATA));

    // Validate the start address
    size_t alignmentStartAddress = 64 * sizeof(uint8_t);
    EXPECT_EQ(0u, cmd->getInterfaceDescriptorDataStartAddress() % alignmentStartAddress);

    // Validate the length
    EXPECT_NE(0u, cmd->getInterfaceDescriptorTotalLength());
    size_t alignmentTotalLength = 32 * sizeof(uint8_t);
    EXPECT_EQ(0u, cmd->getInterfaceDescriptorTotalLength() % alignmentTotalLength);

    // Generically validate this command
    PARSE::template validateCommand<MEDIA_INTERFACE_DESCRIPTOR_LOAD *>(cmdList.begin(), itorCmd);
}

HWCMDTEST_P(IGFX_GEN8_CORE, EnqueueWorkItemTestsWithLimitedParamSet, InterfaceDescriptorData) {
    typedef typename FamilyType::PARSE PARSE;
    typedef typename PARSE::MEDIA_INTERFACE_DESCRIPTOR_LOAD MEDIA_INTERFACE_DESCRIPTOR_LOAD;
    typedef typename PARSE::STATE_BASE_ADDRESS STATE_BASE_ADDRESS;
    typedef typename PARSE::INTERFACE_DESCRIPTOR_DATA INTERFACE_DESCRIPTOR_DATA;
    enqueueKernel<FamilyType>();

    // Extract the MIDL command
    auto itorCmd = find<MEDIA_INTERFACE_DESCRIPTOR_LOAD *>(itorPipelineSelect, itorWalker);
    ASSERT_NE(itorWalker, itorCmd);
    auto *cmdMIDL = (MEDIA_INTERFACE_DESCRIPTOR_LOAD *)*itorCmd;

    // Extract the SBA command
    itorCmd = find<STATE_BASE_ADDRESS *>(cmdList.begin(), itorWalker);
    ASSERT_NE(itorWalker, itorCmd);
    auto *cmdSBA = (STATE_BASE_ADDRESS *)*itorCmd;

    // Extrach the DSH
    auto DSH = cmdSBA->getDynamicStateBaseAddress();
    ASSERT_NE(0u, DSH);

    // IDD should be located within DSH
    auto iddStart = cmdMIDL->getInterfaceDescriptorDataStartAddress();
    auto IDDEnd = iddStart + cmdMIDL->getInterfaceDescriptorTotalLength();
    ASSERT_LE(IDDEnd, cmdSBA->getDynamicStateBufferSize() * MemoryConstants::pageSize);

    // Extract the IDD
    auto &IDD = *(INTERFACE_DESCRIPTOR_DATA *)(DSH + iddStart);

    // Validate the kernel start pointer.  Technically, a kernel can start at address 0 but let's force a value.
    auto kernelStartPointer = ((uint64_t)IDD.getKernelStartPointerHigh() << 32) + IDD.getKernelStartPointer();
    EXPECT_LE(kernelStartPointer, cmdSBA->getInstructionBufferSize() * MemoryConstants::pageSize);

    EXPECT_NE(0u, IDD.getNumberOfThreadsInGpgpuThreadGroup());
    EXPECT_NE(0u, IDD.getCrossThreadConstantDataReadLength());
    EXPECT_NE(0u, IDD.getConstantIndirectUrbEntryReadLength());
}

HWCMDTEST_P(IGFX_GEN8_CORE, EnqueueWorkItemTestsWithLimitedParamSet, PipelineSelect) {
    enqueueKernel<FamilyType>();
    int numCommands = getNumberOfPipelineSelectsThatEnablePipelineSelect<FamilyType>();
    EXPECT_EQ(1, numCommands);
}

HWCMDTEST_P(IGFX_GEN8_CORE, EnqueueWorkItemTestsWithLimitedParamSet, MediaVFEState) {
    enqueueKernel<FamilyType>();
    validateMediaVFEState<FamilyType>(&pDevice->getHardwareInfo(), cmdMediaVfeState, cmdList, itorMediaVfeState);
}

INSTANTIATE_TEST_CASE_P(EnqueueKernel,
                        EnqueueWorkItemTests,
                        ::testing::ValuesIn(TestParamTable));

INSTANTIATE_TEST_CASE_P(EnqueueKernel,
                        EnqueueWorkItemTestsWithLimitedParamSet,
                        ::testing::ValuesIn(OneEntryTestParamTable));

typedef EnqueueKernelTypeTest<TestParam2> EnqueueScratchSpaceTests;

HWCMDTEST_P(IGFX_GEN8_CORE, EnqueueScratchSpaceTests, GivenKernelRequiringScratchWhenItIsEnqueuedWithDifferentScratchSizesThenMediaVFEStateAndStateBaseAddressAreProperlyProgrammed) {
    typedef typename FamilyType::PARSE PARSE;
    typedef typename PARSE::MEDIA_VFE_STATE MEDIA_VFE_STATE;
    typedef typename PARSE::STATE_BASE_ADDRESS STATE_BASE_ADDRESS;

    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();

    csr.getMemoryManager()->setForce32BitAllocations(false);

    EXPECT_TRUE(csr.getMemoryManager()->allocationsForReuse.peekIsEmpty());

    SPatchMediaVFEState mediaVFEstate;
    auto scratchSize = GetParam().ScratchSize;

    mediaVFEstate.PerThreadScratchSpace = scratchSize;

    MockKernelWithInternals mockKernel(*pDevice);
    mockKernel.kernelInfo.patchInfo.mediavfestate = &mediaVFEstate;

    auto sizeToProgram = (scratchSize / MemoryConstants::kiloByte);
    auto bitValue = 0u;
    while (sizeToProgram >>= 1) {
        bitValue++;
    }

    auto valueToProgram = Kernel::getScratchSizeValueToProgramMediaVfeState(scratchSize);
    EXPECT_EQ(bitValue, valueToProgram);

    enqueueKernel<FamilyType>(mockKernel);

    // All state should be programmed before walker
    auto itorCmd = find<MEDIA_VFE_STATE *>(itorPipelineSelect, itorWalker);
    auto itorCmdForStateBase = find<STATE_BASE_ADDRESS *>(itorPipelineSelect, itorWalker);

    ASSERT_NE(itorWalker, itorCmd);
    ASSERT_NE(itorWalker, itorCmdForStateBase);

    auto *cmd = (MEDIA_VFE_STATE *)*itorCmd;
    auto *sba = (STATE_BASE_ADDRESS *)*itorCmdForStateBase;

    const HardwareInfo &hwInfo = **platformDevices;
    uint32_t threadPerEU = (hwInfo.pSysInfo->ThreadCount / hwInfo.pSysInfo->EUCount) + hwInfo.capabilityTable.extraQuantityThreadsPerEU;
    uint32_t maxNumberOfThreads = hwInfo.pSysInfo->EUCount * threadPerEU;

    // Verify we have a valid length
    EXPECT_EQ(maxNumberOfThreads, cmd->getMaximumNumberOfThreads());
    EXPECT_NE(0u, cmd->getNumberOfUrbEntries());
    EXPECT_NE(0u, cmd->getUrbEntryAllocationSize());
    EXPECT_EQ(bitValue, cmd->getPerThreadScratchSpace());
    EXPECT_EQ(bitValue, cmd->getStackSize());
    auto graphicsAllocation = csr.getScratchAllocation();
    auto GSHaddress = (uintptr_t)sba->getGeneralStateBaseAddress();
    if (is32bit) {
        EXPECT_NE(0u, cmd->getScratchSpaceBasePointer());
        EXPECT_EQ(0u, GSHaddress);
    } else {
        EXPECT_EQ(PreambleHelper<FamilyType>::getScratchSpaceOffsetFor64bit(), cmd->getScratchSpaceBasePointer());
        EXPECT_EQ(GSHaddress + PreambleHelper<FamilyType>::getScratchSpaceOffsetFor64bit(), (uintptr_t)graphicsAllocation->getUnderlyingBuffer());
    }

    auto allocationSize = scratchSize * pDevice->getDeviceInfo().computeUnitsUsedForScratch;
    EXPECT_EQ(graphicsAllocation->getUnderlyingBufferSize(), allocationSize);

    // Generically validate this command
    PARSE::template validateCommand<MEDIA_VFE_STATE *>(cmdList.begin(), itorCmd);

    scratchSize *= 2;
    //skip if size to big 4MB, no point in stressing memory allocator.
    if (allocationSize > 4194304) {
        return;
    }

    mediaVFEstate.PerThreadScratchSpace = scratchSize;

    auto itorfirstBBEnd = find<typename FamilyType::MI_BATCH_BUFFER_END *>(itorWalker, cmdList.end());
    ASSERT_NE(cmdList.end(), itorfirstBBEnd);

    enqueueKernel<FamilyType>(mockKernel);
    bitValue++;

    itorCmd = find<MEDIA_VFE_STATE *>(itorfirstBBEnd, cmdList.end());
    itorCmdForStateBase = find<STATE_BASE_ADDRESS *>(itorWalker, cmdList.end());
    ASSERT_NE(itorWalker, itorCmd);
    if (is64bit) {
        ASSERT_NE(itorCmdForStateBase, itorCmd);
    } else {
        //no SBA not dirty
        ASSERT_EQ(itorCmdForStateBase, cmdList.end());
    }

    auto *cmd2 = (MEDIA_VFE_STATE *)*itorCmd;

    // Verify we have a valid length
    EXPECT_EQ(maxNumberOfThreads, cmd2->getMaximumNumberOfThreads());
    EXPECT_NE(0u, cmd2->getNumberOfUrbEntries());
    EXPECT_NE(0u, cmd2->getUrbEntryAllocationSize());
    EXPECT_EQ(bitValue, cmd2->getPerThreadScratchSpace());
    EXPECT_EQ(bitValue, cmd2->getStackSize());
    auto graphicsAllocation2 = csr.getScratchAllocation();

    if (is32bit) {
        auto scratchBase = (uintptr_t)cmd2->getScratchSpaceBasePointer();
        EXPECT_NE(0u, scratchBase);
        auto graphicsAddress = (uintptr_t)graphicsAllocation2->getUnderlyingBuffer();
        EXPECT_EQ(graphicsAddress, scratchBase);
    } else {
        auto *sba2 = (STATE_BASE_ADDRESS *)*itorCmdForStateBase;
        auto GSHaddress2 = sba2->getGeneralStateBaseAddress();
        EXPECT_NE(0u, GSHaddress2);
        EXPECT_EQ(PreambleHelper<FamilyType>::getScratchSpaceOffsetFor64bit(), cmd2->getScratchSpaceBasePointer());
        EXPECT_NE(GSHaddress2, GSHaddress);
    }
    EXPECT_EQ(graphicsAllocation->getUnderlyingBufferSize(), allocationSize);
    EXPECT_NE(graphicsAllocation2, graphicsAllocation);

    // Generically validate this command
    PARSE::template validateCommand<MEDIA_VFE_STATE *>(cmdList.begin(), itorCmd);

    // Trigger SBA generation
    IndirectHeap dirtyDsh(nullptr);
    csr.dshState.updateAndCheck(&dirtyDsh);

    enqueueKernel<FamilyType>(mockKernel);
    auto finalItorToSBA = find<STATE_BASE_ADDRESS *>(itorCmd, cmdList.end());
    ASSERT_NE(finalItorToSBA, cmdList.end());
    auto *finalSba2 = (STATE_BASE_ADDRESS *)*finalItorToSBA;
    auto GSBaddress = finalSba2->getGeneralStateBaseAddress();
    if (is32bit) {
        EXPECT_EQ(0u, GSBaddress);
    } else if (is64bit) {
        EXPECT_EQ((uintptr_t)graphicsAllocation2->getUnderlyingBuffer(), GSBaddress + PreambleHelper<FamilyType>::getScratchSpaceOffsetFor64bit());
    }

    EXPECT_TRUE(csr.getMemoryManager()->allocationsForReuse.peekIsEmpty());
}

INSTANTIATE_TEST_CASE_P(EnqueueKernel,
                        EnqueueScratchSpaceTests,
                        ::testing::ValuesIn(TestParamTable2));

typedef EnqueueKernelTypeTest<int> EnqueueKernelWithScratch;

HWTEST_P(EnqueueKernelWithScratch, GivenKernelRequiringScratchWhenItIsEnqueuedWithDifferentScratchSizesThenPreviousScratchAllocationIsMadeNonResidentPriorStoringOnResueList) {
    auto mockCsr = new MockCsrHw<FamilyType>(pDevice->getHardwareInfo(), *pDevice->executionEnvironment);
    pDevice->resetCommandStreamReceiver(mockCsr);

    SPatchMediaVFEState mediaVFEstate;
    auto scratchSize = 1024;

    mediaVFEstate.PerThreadScratchSpace = scratchSize;

    MockKernelWithInternals mockKernel(*pDevice);
    mockKernel.kernelInfo.patchInfo.mediavfestate = &mediaVFEstate;

    auto sizeToProgram = (scratchSize / MemoryConstants::kiloByte);
    auto bitValue = 0u;
    while (sizeToProgram >>= 1) {
        bitValue++;
    }

    auto valueToProgram = Kernel::getScratchSizeValueToProgramMediaVfeState(scratchSize);
    EXPECT_EQ(bitValue, valueToProgram);

    enqueueKernel<FamilyType, false>(mockKernel);

    auto graphicsAllocation = mockCsr->getScratchAllocation();

    EXPECT_TRUE(mockCsr->isMadeResident(graphicsAllocation));

    // Enqueue With ScratchSize bigger then previous
    scratchSize = 8196;
    mediaVFEstate.PerThreadScratchSpace = scratchSize;

    enqueueKernel<FamilyType, false>(mockKernel);

    EXPECT_TRUE(mockCsr->isMadeNonResident(graphicsAllocation));
}

HWCMDTEST_P(IGFX_GEN8_CORE, EnqueueKernelWithScratch, givenDeviceForcing32bitAllocationsWhenKernelWithScratchIsEnqueuedThenGeneralStateHeapBaseAddressIsCorrectlyProgrammedAndMediaVFEStateContainsProgramming) {

    typedef typename FamilyType::PARSE PARSE;
    typedef typename PARSE::MEDIA_VFE_STATE MEDIA_VFE_STATE;
    typedef typename PARSE::STATE_BASE_ADDRESS STATE_BASE_ADDRESS;

    if (is64bit) {
        CommandStreamReceiver *csr = &pDevice->getCommandStreamReceiver();
        auto memoryManager = csr->getMemoryManager();
        memoryManager->setForce32BitAllocations(true);

        SPatchMediaVFEState mediaVFEstate;
        auto scratchSize = 1024;
        mediaVFEstate.PerThreadScratchSpace = scratchSize;

        MockKernelWithInternals mockKernel(*pDevice);
        mockKernel.kernelInfo.patchInfo.mediavfestate = &mediaVFEstate;

        enqueueKernel<FamilyType>(mockKernel);
        auto graphicsAllocation = csr->getScratchAllocation();
        EXPECT_TRUE(graphicsAllocation->is32BitAllocation);
        auto graphicsAddress = (uint64_t)graphicsAllocation->getGpuAddress();
        auto baseAddress = graphicsAllocation->gpuBaseAddress;

        // All state should be programmed before walker
        auto itorCmd = find<MEDIA_VFE_STATE *>(itorPipelineSelect, itorWalker);
        auto itorCmdForStateBase = find<STATE_BASE_ADDRESS *>(itorPipelineSelect, itorWalker);

        auto *mediaVfeState = (MEDIA_VFE_STATE *)*itorCmd;
        auto scratchBaseLowPart = (uint64_t)mediaVfeState->getScratchSpaceBasePointer();
        auto scratchBaseHighPart = (uint64_t)mediaVfeState->getScratchSpaceBasePointerHigh();
        uint64_t scratchBaseAddr = scratchBaseHighPart << 32 | scratchBaseLowPart;

        EXPECT_EQ(graphicsAddress - baseAddress, scratchBaseAddr);

        ASSERT_NE(itorCmdForStateBase, itorWalker);
        auto *sba = (STATE_BASE_ADDRESS *)*itorCmdForStateBase;

        auto GSHaddress = (uintptr_t)sba->getGeneralStateBaseAddress();

        EXPECT_EQ(memoryManager->allocator32Bit->getBase(), GSHaddress);

        //now re-try to see if SBA is not programmed

        scratchSize *= 2;

        mediaVFEstate.PerThreadScratchSpace = scratchSize;

        enqueueKernel<FamilyType>(mockKernel);

        itorCmdForStateBase = find<STATE_BASE_ADDRESS *>(itorWalker, cmdList.end());
        EXPECT_EQ(itorCmdForStateBase, cmdList.end());
    }
}

INSTANTIATE_TEST_CASE_P(EnqueueKernel,
                        EnqueueKernelWithScratch, testing::Values(1));

TestParam TestParamPrintf[] = {
    {1, 1, 1, 1, 1, 1}};

typedef EnqueueKernelTypeTest<TestParam> EnqueueKernelPrintfTest;

HWTEST_P(EnqueueKernelPrintfTest, GivenKernelWithPrintfThenPatchCrossTHreadData) {
    typedef typename FamilyType::PARSE PARSE;

    SPatchAllocateStatelessPrintfSurface patchData;
    patchData.SurfaceStateHeapOffset = 0;
    patchData.Size = 256;
    patchData.DataParamOffset = 64;

    MockKernelWithInternals mockKernel(*pDevice);
    mockKernel.crossThreadData[64] = 0;
    mockKernel.kernelInfo.patchInfo.pAllocateStatelessPrintfSurface = &patchData;

    enqueueKernel<FamilyType, false>(mockKernel);

    EXPECT_EQ(mockKernel.crossThreadData[64], 0);
}

HWTEST_P(EnqueueKernelPrintfTest, GivenKernelWithPrintfWhenBeingDispatchedThenL3CacheIsFlushed) {
    typedef typename FamilyType::PARSE PARSE;

    SPatchAllocateStatelessPrintfSurface patchData;
    patchData.Size = 256;
    patchData.DataParamOffset = 64;

    MockKernelWithInternals mockKernel(*pDevice);
    mockKernel.crossThreadData[64] = 0;
    mockKernel.kernelInfo.patchInfo.pAllocateStatelessPrintfSurface = &patchData;
    auto &csr = pCmdQ->getDevice().getCommandStreamReceiver();
    auto latestSentTaskCount = csr.peekTaskCount();
    enqueueKernel<FamilyType, false>(mockKernel);
    auto newLatestSentTaskCount = csr.peekTaskCount();
    EXPECT_GT(newLatestSentTaskCount, latestSentTaskCount);
    EXPECT_EQ(pCmdQ->latestTaskCountWaited, newLatestSentTaskCount);
}

HWCMDTEST_P(IGFX_GEN8_CORE, EnqueueKernelPrintfTest, GivenKernelWithPrintfBlockedByEventWhenEventUnblockedThenL3CacheIsFlushed) {
    typedef typename FamilyType::PARSE PARSE;

    UserEvent userEvent(context);

    SPatchAllocateStatelessPrintfSurface patchData;
    patchData.Size = 256;
    patchData.DataParamOffset = 64;

    MockKernelWithInternals mockKernel(*pDevice);
    mockKernel.crossThreadData[64] = 0;
    mockKernel.kernelInfo.patchInfo.pAllocateStatelessPrintfSurface = &patchData;
    auto &csr = pCmdQ->getDevice().getCommandStreamReceiver();
    auto latestSentDcFlushTaskCount = csr.peekTaskCount();

    cl_uint workDim = 1;
    size_t globalWorkOffset[3] = {0, 0, 0};

    FillValues();

    cl_event blockedEvent = &userEvent;
    auto retVal = pCmdQ->enqueueKernel(
        mockKernel,
        workDim,
        globalWorkOffset,
        globalWorkSize,
        localWorkSize,
        1,
        &blockedEvent,
        nullptr);
    ASSERT_EQ(CL_SUCCESS, retVal);

    userEvent.setStatus(CL_COMPLETE);

    parseCommands<FamilyType>(*pCmdQ);

    auto newLatestSentDCFlushTaskCount = csr.peekTaskCount();
    EXPECT_GT(newLatestSentDCFlushTaskCount, latestSentDcFlushTaskCount);
    EXPECT_EQ(pCmdQ->latestTaskCountWaited, newLatestSentDCFlushTaskCount);
}

HWTEST_P(EnqueueKernelPrintfTest, GivenKernelWithPrintfBlockedByEventWhenEventUnblockedThenOutputPrinted) {
    typedef typename FamilyType::PARSE PARSE;

    // In scenarios with 32bit allocator and 64 bit tests this code won't work
    // due to inability to retrieve original buffer pointer as it is done in this test.
    if (!pDevice->getMemoryManager()->peekForce32BitAllocations()) {
        testing::internal::CaptureStdout();

        UserEvent userEvent(context);

        SPatchAllocateStatelessPrintfSurface patchData;
        patchData.Size = 256;
        patchData.DataParamSize = 8;
        patchData.DataParamOffset = 0;

        MockKernelWithInternals mockKernel(*pDevice);
        mockKernel.kernelInfo.patchInfo.pAllocateStatelessPrintfSurface = &patchData;

        auto crossThreadData = reinterpret_cast<uint64_t *>(mockKernel.mockKernel->getCrossThreadData());

        char *testString = new char[sizeof("test")];
        strcpy_s(testString, sizeof("test"), "test");

        PrintfStringInfo printfStringInfo;
        printfStringInfo.SizeInBytes = sizeof("test");
        printfStringInfo.pStringData = testString;

        mockKernel.kernelInfo.patchInfo.stringDataMap.insert(std::make_pair(0, printfStringInfo));

        cl_uint workDim = 1;
        size_t globalWorkOffset[3] = {0, 0, 0};

        FillValues();

        cl_event blockedEvent = &userEvent;
        auto retVal = pCmdQ->enqueueKernel(
            mockKernel,
            workDim,
            globalWorkOffset,
            globalWorkSize,
            localWorkSize,
            1,
            &blockedEvent,
            nullptr);

        ASSERT_EQ(CL_SUCCESS, retVal);

        auto printfAllocation = reinterpret_cast<uint32_t *>(*crossThreadData);
        printfAllocation[0] = 8;
        printfAllocation[1] = 0;

        userEvent.setStatus(CL_COMPLETE);

        std::string output = testing::internal::GetCapturedStdout();
        EXPECT_STREQ("test", output.c_str());
    }
}

INSTANTIATE_TEST_CASE_P(EnqueueKernel,
                        EnqueueKernelPrintfTest,
                        ::testing::ValuesIn(TestParamPrintf));

TEST_F(EnqueueKernelTest, GivenKernelWithBuiltinDispatchInfoBuilderWhenBeingDispatchedThenBuiltinDispatcherIsUsedForDispatchValidation) {
    struct MockBuiltinDispatchBuilder : BuiltinDispatchInfoBuilder {
        MockBuiltinDispatchBuilder(BuiltIns &builtins)
            : BuiltinDispatchInfoBuilder(builtins) {
        }

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

    MockBuiltinDispatchBuilder mockNuiltinDispatchBuilder(*pCmdQ->getDevice().getExecutionEnvironment()->getBuiltIns());

    MockKernelWithInternals mockKernel(*pDevice);
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
    auto &csr = pDevice->getCommandStreamReceiver();
    csr.getMemoryManager()->setForce32BitAllocations(false);
    HardwareParse hwParser;
    size_t off[3] = {0, 0, 0};
    size_t gws[3] = {1, 1, 1};

    SPatchMediaVFEState mediaVFEstate;
    uint32_t scratchSize = 4096u;

    mediaVFEstate.PerThreadScratchSpace = scratchSize;

    MockKernelWithInternals mockKernel(*pDevice);
    mockKernel.kernelInfo.patchInfo.mediavfestate = &mediaVFEstate;

    auto sizeToProgram = Kernel::getScratchSizeValueToProgramMediaVfeState(scratchSize);

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

HWTEST_F(EnqueueKernelTest, givenEnqueueWithGlobalWorkSizeWhenZeroValueIsPassedInDimensionThenTheKernelCommandWillTriviallySucceed) {
    size_t gws[3] = {0, 0, 0};
    MockKernelWithInternals mockKernel(*pDevice);
    auto ret = pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, ret);
}

HWTEST_F(EnqueueKernelTest, givenCommandStreamReceiverInBatchingModeWhenEnqueueKernelIsCalledThenKernelIsRecorded) {
    auto mockCsr = new MockCsrHw2<FamilyType>(pDevice->getHardwareInfo(), *pDevice->executionEnvironment);
    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);
    pDevice->resetCommandStreamReceiver(mockCsr);

    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    MockKernelWithInternals mockKernel(*pDevice, context);
    size_t gws[3] = {1, 0, 0};
    auto ret = pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, ret);
    EXPECT_FALSE(mockedSubmissionsAggregator->peekCmdBufferList().peekIsEmpty());

    auto cmdBuffer = mockedSubmissionsAggregator->peekCmdBufferList().peekHead();

    //Two more surfaces from preemptionAllocation and SipKernel
    size_t csrSurfaceCount = (pDevice->getPreemptionMode() == PreemptionMode::MidThread) ? 2 : 0;

    EXPECT_EQ(0, mockCsr->flushCalledCount);
    EXPECT_EQ(5u + csrSurfaceCount, cmdBuffer->surfaces.size());
}

HWTEST_F(EnqueueKernelTest, givenDefaultCommandStreamReceiverWhenClFlushIsCalledThenSuccessIsReturned) {
    MockKernelWithInternals mockKernel(*pDevice);
    size_t gws[3] = {1, 0, 0};
    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    auto ret = clFlush(pCmdQ);
    EXPECT_EQ(CL_SUCCESS, ret);
}

HWTEST_F(EnqueueKernelTest, givenCommandStreamReceiverInBatchingModeAndBatchedKernelWhenFlushIsCalledThenKernelIsSubmitted) {
    auto mockCsrmockCsr = new MockCsrHw2<FamilyType>(pDevice->getHardwareInfo(), *pDevice->executionEnvironment);
    mockCsrmockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);
    pDevice->resetCommandStreamReceiver(mockCsrmockCsr);

    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    mockCsrmockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    MockKernelWithInternals mockKernel(*pDevice);
    size_t gws[3] = {1, 0, 0};
    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    EXPECT_FALSE(mockedSubmissionsAggregator->peekCmdBufferList().peekIsEmpty());

    auto ret = clFlush(pCmdQ);
    EXPECT_EQ(CL_SUCCESS, ret);

    EXPECT_TRUE(mockedSubmissionsAggregator->peekCmdBufferList().peekIsEmpty());
    EXPECT_EQ(1, mockCsrmockCsr->flushCalledCount);
}

HWTEST_F(EnqueueKernelTest, givenCommandStreamReceiverInBatchingModeAndBatchedKernelWhenFlushIsCalledTwiceThenNothingChanges) {
    auto mockCsrmockCsr = new MockCsrHw2<FamilyType>(pDevice->getHardwareInfo(), *pDevice->executionEnvironment);
    mockCsrmockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);
    pDevice->resetCommandStreamReceiver(mockCsrmockCsr);

    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    mockCsrmockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    MockKernelWithInternals mockKernel(*pDevice);
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
    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    mockCsrmockCsr.submissionAggregator.reset(mockedSubmissionsAggregator);

    MockKernelWithInternals mockKernel(*pDevice, context);
    size_t gws[3] = {1, 0, 0};
    //make sure csr emits something
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
    auto mockCsr = new MockCsrHw2<FamilyType>(pDevice->getHardwareInfo(), *pDevice->executionEnvironment);
    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);
    pDevice->resetCommandStreamReceiver(mockCsr);

    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    MockKernelWithInternals mockKernel(*pDevice);
    size_t gws[3] = {1, 0, 0};
    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    pCmdQ->flush();

    EXPECT_TRUE(mockedSubmissionsAggregator->peekCmdBufferList().peekIsEmpty());
    EXPECT_EQ(1, mockCsr->flushCalledCount);
}

HWTEST_F(EnqueueKernelTest, givenCsrInBatchingModeWhenFinishIsCalledThenBatchesSubmissionsAreFlushed) {
    auto mockCsr = new MockCsrHw2<FamilyType>(pDevice->getHardwareInfo(), *pDevice->executionEnvironment);
    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);
    pDevice->resetCommandStreamReceiver(mockCsr);

    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    MockKernelWithInternals mockKernel(*pDevice);
    size_t gws[3] = {1, 0, 0};
    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    pCmdQ->finish(false);

    EXPECT_TRUE(mockedSubmissionsAggregator->peekCmdBufferList().peekIsEmpty());
    EXPECT_EQ(1, mockCsr->flushCalledCount);
}

HWTEST_F(EnqueueKernelTest, givenCsrInBatchingModeWhenThressEnqueueKernelsAreCalledThenBatchesSubmissionsAreFlushed) {
    auto mockCsr = new MockCsrHw2<FamilyType>(pDevice->getHardwareInfo(), *pDevice->executionEnvironment);
    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);
    pDevice->resetCommandStreamReceiver(mockCsr);

    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    MockKernelWithInternals mockKernel(*pDevice);
    size_t gws[3] = {1, 0, 0};
    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    pCmdQ->finish(false);

    EXPECT_TRUE(mockedSubmissionsAggregator->peekCmdBufferList().peekIsEmpty());
    EXPECT_EQ(1, mockCsr->flushCalledCount);
}

HWTEST_F(EnqueueKernelTest, givenCsrInBatchingModeWhenWaitForEventsIsCalledThenBatchedSubmissionsAreFlushed) {
    auto mockCsr = new MockCsrHw2<FamilyType>(pDevice->getHardwareInfo(), *pDevice->executionEnvironment);
    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);
    pDevice->resetCommandStreamReceiver(mockCsr);

    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    MockKernelWithInternals mockKernel(*pDevice);
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
    auto mockCsr = new MockCsrHw2<FamilyType>(pDevice->getHardwareInfo(), *pDevice->executionEnvironment);
    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);
    pDevice->resetCommandStreamReceiver(mockCsr);

    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    MockKernelWithInternals mockKernel(*pDevice);
    size_t gws[3] = {1, 0, 0};
    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    cl_event event;
    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, &event);
    auto neoEvent = castToObject<Event>(event);

    EXPECT_EQ(0u, mockCsr->flushStamp->peekStamp());
    EXPECT_EQ(0u, neoEvent->flushStamp->peekStamp());
    EXPECT_EQ(0u, pCmdQ->flushStamp->peekStamp());

    auto status = clWaitForEvents(1, &event);

    EXPECT_EQ(1, neoEvent->getRefInternalCount());
    EXPECT_EQ(1u, mockCsr->flushStamp->peekStamp());
    EXPECT_EQ(1u, neoEvent->flushStamp->peekStamp());
    EXPECT_EQ(1u, pCmdQ->flushStamp->peekStamp());

    status = clFinish(pCmdQ);
    EXPECT_EQ(1u, pCmdQ->flushStamp->peekStamp());

    status = clReleaseEvent(event);
}

HWTEST_F(EnqueueKernelTest, givenCsrInBatchingModeWhenCommandWithEventIsFollowedByCommandWithoutEventThenFlushStampIsUpdatedInCommandQueueCsrAndEvent) {
    auto mockCsr = new MockCsrHw2<FamilyType>(pDevice->getHardwareInfo(), *pDevice->executionEnvironment);
    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);
    pDevice->resetCommandStreamReceiver(mockCsr);

    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    MockKernelWithInternals mockKernel(*pDevice);
    size_t gws[3] = {1, 0, 0};
    cl_event event;
    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, &event);
    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    auto neoEvent = castToObject<Event>(event);

    EXPECT_EQ(0u, mockCsr->flushStamp->peekStamp());
    EXPECT_EQ(0u, neoEvent->flushStamp->peekStamp());
    EXPECT_EQ(0u, pCmdQ->flushStamp->peekStamp());

    auto status = clWaitForEvents(1, &event);

    EXPECT_EQ(1, neoEvent->getRefInternalCount());
    EXPECT_EQ(1u, mockCsr->flushStamp->peekStamp());
    EXPECT_EQ(1u, neoEvent->flushStamp->peekStamp());
    EXPECT_EQ(1u, pCmdQ->flushStamp->peekStamp());

    status = clFinish(pCmdQ);
    EXPECT_EQ(1u, pCmdQ->flushStamp->peekStamp());

    status = clReleaseEvent(event);
}

HWTEST_F(EnqueueKernelTest, givenCsrInBatchingModeWhenClFlushIsCalledThenQueueFlushStampIsUpdated) {
    auto mockCsr = new MockCsrHw2<FamilyType>(pDevice->getHardwareInfo(), *pDevice->executionEnvironment);
    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);
    pDevice->resetCommandStreamReceiver(mockCsr);

    MockKernelWithInternals mockKernel(*pDevice);
    size_t gws[3] = {1, 0, 0};
    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    EXPECT_EQ(0u, mockCsr->flushStamp->peekStamp());
    EXPECT_EQ(0u, pCmdQ->flushStamp->peekStamp());

    clFlush(pCmdQ);
    EXPECT_EQ(1u, mockCsr->flushStamp->peekStamp());
    EXPECT_EQ(1u, pCmdQ->flushStamp->peekStamp());
}

HWTEST_F(EnqueueKernelTest, givenCsrInBatchingModeWhenWaitForEventsIsCalledWithUnflushedTaskCountThenBatchedSubmissionsAreFlushed) {
    auto mockCsr = new MockCsrHw2<FamilyType>(pDevice->getHardwareInfo(), *pDevice->executionEnvironment);
    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);
    pDevice->resetCommandStreamReceiver(mockCsr);

    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    MockKernelWithInternals mockKernel(*pDevice);
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
    auto mockCsr = new MockCsrHw2<FamilyType>(pDevice->getHardwareInfo(), *pDevice->executionEnvironment);
    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);
    pDevice->resetCommandStreamReceiver(mockCsr);

    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    MockKernelWithInternals mockKernel(*pDevice);
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
    auto ooq = clCreateCommandQueueWithProperties(context, pDevice, props, nullptr);

    auto mockCsr = new MockCsrHw2<FamilyType>(pDevice->getHardwareInfo(), *pDevice->executionEnvironment);
    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);
    pDevice->resetCommandStreamReceiver(mockCsr);

    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    MockKernelWithInternals mockKernel(*pDevice, context);
    size_t gws[3] = {1, 0, 0};
    clEnqueueNDRangeKernel(ooq, mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    EXPECT_FALSE(mockedSubmissionsAggregator->peekCmdBufferList().peekIsEmpty());
    auto cmdBuffer = mockedSubmissionsAggregator->peekCmdBufferList().peekHead();
    EXPECT_NE(nullptr, cmdBuffer->pipeControlThatMayBeErasedLocation);

    clReleaseCommandQueue(ooq);
}

HWTEST_F(EnqueueKernelTest, givenInOrderCommandQueueWhenEnqueueKernelIsMadeThenPipeControlPositionIsRecorded) {
    const cl_queue_properties props[] = {0};
    auto inOrderQueue = clCreateCommandQueueWithProperties(context, pDevice, props, nullptr);

    auto &mockCsr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    mockCsr.overrideDispatchPolicy(DispatchMode::BatchedDispatch);
    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    mockCsr.submissionAggregator.reset(mockedSubmissionsAggregator);

    MockKernelWithInternals mockKernel(*pDevice, context);
    size_t gws[3] = {1, 0, 0};
    clEnqueueNDRangeKernel(inOrderQueue, mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    EXPECT_FALSE(mockedSubmissionsAggregator->peekCmdBufferList().peekIsEmpty());
    auto cmdBuffer = mockedSubmissionsAggregator->peekCmdBufferList().peekHead();
    EXPECT_NE(nullptr, cmdBuffer->pipeControlThatMayBeErasedLocation);

    clReleaseCommandQueue(inOrderQueue);
}

HWTEST_F(EnqueueKernelTest, givenInOrderCommandQueueWhenEnqueueKernelThatHasSharedObjectsAsArgIsMadeThenPipeControlPositionIsRecorded) {
    const cl_queue_properties props[] = {0};
    auto inOrderQueue = clCreateCommandQueueWithProperties(context, pDevice, props, nullptr);

    auto mockCsr = new MockCsrHw2<FamilyType>(pDevice->getHardwareInfo(), *pDevice->executionEnvironment);
    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);
    pDevice->resetCommandStreamReceiver(mockCsr);

    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    MockKernelWithInternals mockKernel(*pDevice, context);
    size_t gws[3] = {1, 0, 0};
    mockKernel.mockKernel->setUsingSharedArgs(true);
    clEnqueueNDRangeKernel(inOrderQueue, mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    EXPECT_FALSE(mockedSubmissionsAggregator->peekCmdBufferList().peekIsEmpty());
    auto cmdBuffer = mockedSubmissionsAggregator->peekCmdBufferList().peekHead();
    EXPECT_NE(nullptr, cmdBuffer->pipeControlThatMayBeErasedLocation);
    EXPECT_NE(nullptr, cmdBuffer->epiloguePipeControlLocation);

    clReleaseCommandQueue(inOrderQueue);
}

HWTEST_F(EnqueueKernelTest, givenInOrderCommandQueueWhenEnqueueKernelThatHasSharedObjectsAsArgIsMadeThenPipeControlDoesntHaveDcFlush) {
    auto mockCsr = new MockCsrHw2<FamilyType>(pDevice->getHardwareInfo(), *pDevice->executionEnvironment);
    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);
    pDevice->resetCommandStreamReceiver(mockCsr);

    MockKernelWithInternals mockKernel(*pDevice, context);
    size_t gws[3] = {1, 0, 0};
    mockKernel.mockKernel->setUsingSharedArgs(true);
    clEnqueueNDRangeKernel(this->pCmdQ, mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    EXPECT_FALSE(mockCsr->passedDispatchFlags.dcFlush);
}

HWTEST_F(EnqueueKernelTest, givenInOrderCommandQueueWhenEnqueueKernelReturningEventIsMadeThenPipeControlPositionIsNotRecorded) {
    const cl_queue_properties props[] = {0};
    auto inOrderQueue = clCreateCommandQueueWithProperties(context, pDevice, props, nullptr);

    auto mockCsr = new MockCsrHw2<FamilyType>(pDevice->getHardwareInfo(), *pDevice->executionEnvironment);
    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);
    pDevice->resetCommandStreamReceiver(mockCsr);

    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    MockKernelWithInternals mockKernel(*pDevice, context);
    size_t gws[3] = {1, 0, 0};
    cl_event event;

    clEnqueueNDRangeKernel(inOrderQueue, mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, &event);

    EXPECT_FALSE(mockedSubmissionsAggregator->peekCmdBufferList().peekIsEmpty());
    auto cmdBuffer = mockedSubmissionsAggregator->peekCmdBufferList().peekHead();
    EXPECT_EQ(nullptr, cmdBuffer->pipeControlThatMayBeErasedLocation);
    EXPECT_NE(nullptr, cmdBuffer->epiloguePipeControlLocation);

    clReleaseCommandQueue(inOrderQueue);
    clReleaseEvent(event);
}

HWTEST_F(EnqueueKernelTest, givenInOrderCommandQueueWhenEnqueueKernelReturningEventIsMadeAndCommandStreamReceiverIsInNTo1ModeThenPipeControlPositionIsRecorded) {
    const cl_queue_properties props[] = {0};
    auto inOrderQueue = clCreateCommandQueueWithProperties(context, pDevice, props, nullptr);

    auto mockCsr = new MockCsrHw2<FamilyType>(pDevice->getHardwareInfo(), *pDevice->executionEnvironment);
    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);
    pDevice->resetCommandStreamReceiver(mockCsr);
    mockCsr->enableNTo1SubmissionModel();

    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    MockKernelWithInternals mockKernel(*pDevice, context);
    size_t gws[3] = {1, 0, 0};
    cl_event event;

    clEnqueueNDRangeKernel(inOrderQueue, mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, &event);

    EXPECT_FALSE(mockedSubmissionsAggregator->peekCmdBufferList().peekIsEmpty());
    auto cmdBuffer = mockedSubmissionsAggregator->peekCmdBufferList().peekHead();
    EXPECT_NE(nullptr, cmdBuffer->pipeControlThatMayBeErasedLocation);
    EXPECT_NE(nullptr, cmdBuffer->epiloguePipeControlLocation);

    clReleaseCommandQueue(inOrderQueue);
    clReleaseEvent(event);
}

HWTEST_F(EnqueueKernelTest, givenOutOfOrderCommandQueueWhenEnqueueKernelReturningEventIsMadeThenPipeControlPositionIsRecorded) {
    auto mockCsr = new MockCsrHw2<FamilyType>(pDevice->getHardwareInfo(), *pDevice->executionEnvironment);
    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);
    pDevice->resetCommandStreamReceiver(mockCsr);

    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, 0};
    auto inOrderQueue = clCreateCommandQueueWithProperties(context, pDevice, props, nullptr);

    MockKernelWithInternals mockKernel(*pDevice, context);
    size_t gws[3] = {1, 0, 0};
    cl_event event;

    clEnqueueNDRangeKernel(inOrderQueue, mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, &event);

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
    auto mockCsr = new MockCsrHw2<FamilyType>(pDevice->getHardwareInfo(), *pDevice->executionEnvironment);
    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);
    pDevice->resetCommandStreamReceiver(mockCsr);

    MockKernelWithInternals mockKernel(*pDevice);
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
    auto mockCsr = new MockCsrHw2<FamilyType>(pDevice->getHardwareInfo(), *pDevice->executionEnvironment);
    pDevice->resetCommandStreamReceiver(mockCsr);

    MockKernelWithInternals mockKernel(*pDevice);
    size_t gws[3] = {1, 0, 0};
    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    EXPECT_EQ(1, mockCsr->flushCalledCount);

    auto csrTaskCount = mockCsr->peekTaskCount();
    auto &passedAllocationPack = mockCsr->copyOfAllocations;
    for (auto &allocation : passedAllocationPack) {
        EXPECT_EQ(csrTaskCount, allocation->taskCount);
    }
}

HWTEST_F(EnqueueKernelTest, givenKernelWhenItIsSubmittedFromTwoDifferentCommandQueuesThenCsrDoesntReloadAnyCommands) {
    auto &csr = this->pDevice->getUltCommandStreamReceiver<FamilyType>();
    MockKernelWithInternals mockKernel(*pDevice);
    size_t gws[3] = {1, 0, 0};
    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    auto currentUsed = csr.commandStream.getUsed();

    const cl_queue_properties props[] = {0};
    auto inOrderQueue = clCreateCommandQueueWithProperties(context, pDevice, props, nullptr);
    clEnqueueNDRangeKernel(inOrderQueue, mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    auto usedAfterSubmission = csr.commandStream.getUsed();

    EXPECT_EQ(usedAfterSubmission, currentUsed);
    clReleaseCommandQueue(inOrderQueue);
}

TEST_F(EnqueueKernelTest, givenKernelWhenAllArgsAreNotAndEventExistSetThenClEnqueueNDRangeKernelReturnsInvalidKernelArgsAndSetEventToNull) {
    const size_t n = 512;
    size_t globalWorkSize[3] = {n, 1, 1};
    size_t localWorkSize[3] = {256, 1, 1};
    cl_int retVal = CL_SUCCESS;
    CommandQueue *pCmdQ2 = createCommandQueue(pDevice, 0);

    std::unique_ptr<Kernel> kernel(Kernel::create(pProgram, *pProgram->getKernelInfo("CopyBuffer"), &retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_FALSE(kernel->isPatched());
    cl_event event;
    retVal = clEnqueueNDRangeKernel(pCmdQ2, kernel.get(), 1, nullptr, globalWorkSize, localWorkSize, 0, nullptr, &event);
    EXPECT_EQ(CL_INVALID_KERNEL_ARGS, retVal);

    clFlush(pCmdQ2);
    clReleaseCommandQueue(pCmdQ2);
}

TEST_F(EnqueueKernelTest, givenEnqueueCommandThatLwsExceedsDeviceCapabilitiesWhenEnqueueNDRangeKernelIsCalledThenErrorIsReturned) {
    auto maxWorkgroupSize = pDevice->getDeviceInfo().maxWorkGroupSize;
    size_t globalWorkSize[3] = {maxWorkgroupSize * 2, 1, 1};
    size_t localWorkSize[3] = {maxWorkgroupSize * 2, 1, 1};
    MockKernelWithInternals mockKernel(*pDevice);

    auto status = pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, globalWorkSize, localWorkSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_WORK_GROUP_SIZE, status);
}

TEST_F(EnqueueKernelTest, givenEnqueueCommandThatLocalWorkgroupSizeContainsZeroWhenEnqueueNDRangeKernelIsCalledThenClInvalidWorkGroupSizeIsReturned) {
    size_t globalWorkSize[3] = {1, 1, 1};
    size_t localWorkSize[3] = {1, 0, 1};
    MockKernelWithInternals mockKernel(*pDevice);

    auto status = pCmdQ->enqueueKernel(mockKernel.mockKernel, 3, nullptr, globalWorkSize, localWorkSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_WORK_GROUP_SIZE, status);
}

HWTEST_F(EnqueueKernelTest, givenVMEKernelWhenEnqueueKernelThenDispatchFlagsHaveMediaSamplerRequired) {
    auto mockCsr = new MockCsrHw2<FamilyType>(pDevice->getHardwareInfo(), *pDevice->executionEnvironment);
    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);
    pDevice->resetCommandStreamReceiver(mockCsr);

    MockKernelWithInternals mockKernel(*pDevice, context);
    size_t gws[3] = {1, 0, 0};
    mockKernel.kernelInfo.isVmeWorkload = true;
    clEnqueueNDRangeKernel(this->pCmdQ, mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    EXPECT_TRUE(mockCsr->passedDispatchFlags.mediaSamplerRequired);
}

HWTEST_F(EnqueueKernelTest, givenNonVMEKernelWhenEnqueueKernelThenDispatchFlagsDoesntHaveMediaSamplerRequired) {
    auto mockCsr = new MockCsrHw2<FamilyType>(pDevice->getHardwareInfo(), *pDevice->executionEnvironment);
    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);
    pDevice->resetCommandStreamReceiver(mockCsr);

    MockKernelWithInternals mockKernel(*pDevice, context);
    size_t gws[3] = {1, 0, 0};
    mockKernel.kernelInfo.isVmeWorkload = false;
    clEnqueueNDRangeKernel(this->pCmdQ, mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    EXPECT_FALSE(mockCsr->passedDispatchFlags.mediaSamplerRequired);
}

struct EnqueueAuxKernelTests : public EnqueueKernelTest {
    template <typename FamilyType>
    class MyCmdQ : public CommandQueueHw<FamilyType> {
      public:
        MyCmdQ(Context *context, Device *device) : CommandQueueHw<FamilyType>(context, device, nullptr) {}
        void dispatchAuxTranslation(MultiDispatchInfo &multiDispatchInfo, BuffersForAuxTranslation &buffersForAuxTranslation,
                                    AuxTranslationDirection auxTranslationDirection) override {
            CommandQueueHw<FamilyType>::dispatchAuxTranslation(multiDispatchInfo, buffersForAuxTranslation, auxTranslationDirection);
            Kernel *lastKernel = nullptr;
            for (const auto &dispatchInfo : multiDispatchInfo) {
                lastKernel = dispatchInfo.getKernel();
            }
            dispatchAuxTranslationInputs.emplace_back(lastKernel, multiDispatchInfo.size(), buffersForAuxTranslation, auxTranslationDirection);
        }

        void waitUntilComplete(uint32_t taskCountToWait, FlushStamp flushStampToWait, bool useQuickKmdSleep) override {
            waitCalled++;
            CommandQueueHw<FamilyType>::waitUntilComplete(taskCountToWait, flushStampToWait, useQuickKmdSleep);
        }

        std::vector<std::tuple<Kernel *, size_t, BuffersForAuxTranslation, AuxTranslationDirection>> dispatchAuxTranslationInputs;
        uint32_t waitCalled = 0;
    };
};

HWTEST_F(EnqueueAuxKernelTests, givenKernelWithRequiredAuxTranslationWhenEnqueuedThenGuardKernelWithAuxTranslations) {
    MockKernelWithInternals mockKernel(*pDevice, context);
    MyCmdQ<FamilyType> cmdQ(context, pDevice);
    size_t gws[3] = {1, 0, 0};

    mockKernel.mockKernel->auxTranslationRequired = true;
    cmdQ.enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(2u, cmdQ.dispatchAuxTranslationInputs.size());

    // before kernel
    EXPECT_EQ(0u, std::get<size_t>(cmdQ.dispatchAuxTranslationInputs.at(0)));
    EXPECT_EQ(AuxTranslationDirection::AuxToNonAux, std::get<AuxTranslationDirection>(cmdQ.dispatchAuxTranslationInputs.at(0)));

    // after kernel
    EXPECT_EQ(1u, std::get<size_t>(cmdQ.dispatchAuxTranslationInputs.at(1)));
    EXPECT_EQ(AuxTranslationDirection::NonAuxToAux, std::get<AuxTranslationDirection>(cmdQ.dispatchAuxTranslationInputs.at(1)));

    mockKernel.mockKernel->auxTranslationRequired = false;
    cmdQ.enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(2u, cmdQ.dispatchAuxTranslationInputs.size()); // not changed
}

HWTEST_F(EnqueueAuxKernelTests, givenMultipleArgsWhenAuxTranslationIsRequiredThenPickOnlyApplicableBuffers) {
    MyCmdQ<FamilyType> cmdQ(context, pDevice);
    size_t gws[3] = {1, 0, 0};
    MockBuffer buffer0, buffer1, buffer2, buffer3;
    cl_mem clMem0 = &buffer0;
    cl_mem clMem1 = &buffer1;
    cl_mem clMem2 = &buffer2;
    cl_mem clMem3 = &buffer3;
    buffer0.getGraphicsAllocation()->setAllocationType(GraphicsAllocation::AllocationType::BUFFER);
    buffer1.getGraphicsAllocation()->setAllocationType(GraphicsAllocation::AllocationType::BUFFER);
    buffer2.getGraphicsAllocation()->setAllocationType(GraphicsAllocation::AllocationType::BUFFER_COMPRESSED);
    buffer3.getGraphicsAllocation()->setAllocationType(GraphicsAllocation::AllocationType::BUFFER_COMPRESSED);

    MockKernelWithInternals mockKernel(*pDevice, context);
    mockKernel.mockKernel->auxTranslationRequired = true;
    mockKernel.kernelInfo.kernelArgInfo.resize(6);
    for (auto &kernelInfo : mockKernel.kernelInfo.kernelArgInfo) {
        kernelInfo.kernelArgPatchInfoVector.resize(1);
    }

    mockKernel.mockKernel->initialize();
    mockKernel.kernelInfo.kernelArgInfo.at(0).pureStatefulBufferAccess = false;
    mockKernel.kernelInfo.kernelArgInfo.at(1).pureStatefulBufferAccess = true;
    mockKernel.kernelInfo.kernelArgInfo.at(2).pureStatefulBufferAccess = false;
    mockKernel.kernelInfo.kernelArgInfo.at(3).pureStatefulBufferAccess = true;
    mockKernel.kernelInfo.kernelArgInfo.at(4).pureStatefulBufferAccess = false;
    mockKernel.kernelInfo.kernelArgInfo.at(5).pureStatefulBufferAccess = false;

    mockKernel.mockKernel->setArgBuffer(0, sizeof(cl_mem *), &clMem0);                    // stateless on regular buffer - dont insert
    mockKernel.mockKernel->setArgBuffer(1, sizeof(cl_mem *), &clMem1);                    // stateful on regular buffer - dont insert
    mockKernel.mockKernel->setArgBuffer(2, sizeof(cl_mem *), &clMem2);                    // stateless on BUFFER_COMPRESSED - insert
    mockKernel.mockKernel->setArgBuffer(3, sizeof(cl_mem *), &clMem3);                    // stateful on BUFFER_COMPRESSED - dont insert
    mockKernel.mockKernel->setArgBuffer(4, sizeof(cl_mem *), nullptr);                    // nullptr - dont insert
    mockKernel.mockKernel->kernelArguments.at(5).type = Kernel::kernelArgType::IMAGE_OBJ; // non-buffer arg - dont insert

    cmdQ.enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(2u, cmdQ.dispatchAuxTranslationInputs.size());
    EXPECT_EQ(1u, std::get<BuffersForAuxTranslation>(cmdQ.dispatchAuxTranslationInputs.at(0)).size()); // before kernel
    EXPECT_EQ(1u, std::get<BuffersForAuxTranslation>(cmdQ.dispatchAuxTranslationInputs.at(1)).size()); // after kernel

    EXPECT_EQ(&buffer2, *std::get<BuffersForAuxTranslation>(cmdQ.dispatchAuxTranslationInputs.at(0)).begin());
    EXPECT_EQ(&buffer2, *std::get<BuffersForAuxTranslation>(cmdQ.dispatchAuxTranslationInputs.at(1)).begin());
}

HWTEST_F(EnqueueAuxKernelTests, givenKernelWithRequiredAuxTranslationWhenEnqueuedThenDispatchAuxTranslationBuiltin) {
    MockKernelWithInternals mockKernel(*pDevice, context);
    MyCmdQ<FamilyType> cmdQ(context, pDevice);
    size_t gws[3] = {1, 0, 0};
    MockBuffer buffer;
    cl_mem clMem = &buffer;

    buffer.getGraphicsAllocation()->setAllocationType(GraphicsAllocation::AllocationType::BUFFER_COMPRESSED);
    mockKernel.kernelInfo.kernelArgInfo.resize(1);
    mockKernel.kernelInfo.kernelArgInfo.at(0).kernelArgPatchInfoVector.resize(1);
    mockKernel.kernelInfo.kernelArgInfo.at(0).pureStatefulBufferAccess = false;
    mockKernel.mockKernel->initialize();
    mockKernel.mockKernel->auxTranslationRequired = true;
    mockKernel.mockKernel->setArgBuffer(0, sizeof(cl_mem *), &clMem);

    cmdQ.enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(2u, cmdQ.dispatchAuxTranslationInputs.size());

    // before kernel
    EXPECT_EQ(1u, std::get<size_t>(cmdQ.dispatchAuxTranslationInputs.at(0))); // aux before NDR
    auto kernelBefore = std::get<Kernel *>(cmdQ.dispatchAuxTranslationInputs.at(0));
    EXPECT_EQ("fullCopy", kernelBefore->getKernelInfo().name);
    EXPECT_TRUE(kernelBefore->isBuiltIn);

    // after kernel
    EXPECT_EQ(3u, std::get<size_t>(cmdQ.dispatchAuxTranslationInputs.at(1))); // aux + NDR + aux
    auto kernelAfter = std::get<Kernel *>(cmdQ.dispatchAuxTranslationInputs.at(1));
    EXPECT_EQ("fullCopy", kernelAfter->getKernelInfo().name);
    EXPECT_TRUE(kernelAfter->isBuiltIn);
}

HWCMDTEST_F(IGFX_GEN8_CORE, EnqueueAuxKernelTests, givenParentKernelWhenAuxTranslationIsRequiredThenDontTranslateFromNonAuxToAux) {
    if (pDevice->getSupportedClVersion() >= 20) {
        MyCmdQ<FamilyType> cmdQ(context, pDevice);
        size_t gws[3] = {1, 0, 0};
        MockBuffer buffer0, buffer1, buffer2;
        cl_mem clMem0 = &buffer0;
        cl_mem clMem1 = &buffer1;
        cl_mem clMem2 = &buffer2;
        buffer0.getGraphicsAllocation()->setAllocationType(GraphicsAllocation::AllocationType::BUFFER_COMPRESSED);
        buffer1.getGraphicsAllocation()->setAllocationType(GraphicsAllocation::AllocationType::BUFFER_COMPRESSED);
        buffer2.getGraphicsAllocation()->setAllocationType(GraphicsAllocation::AllocationType::BUFFER_COMPRESSED);

        cl_queue_properties queueProperties = {};
        auto mockDevQueue = std::make_unique<MockDeviceQueueHw<FamilyType>>(context, pDevice, queueProperties);
        context->setDefaultDeviceQueue(mockDevQueue.get());
        std::unique_ptr<MockParentKernel> parentKernel(MockParentKernel::create(*context));

        parentKernel->auxTranslationRequired = true;
        parentKernel->mockKernelInfo->kernelArgInfo.resize(3);
        for (auto &kernelInfo : parentKernel->mockKernelInfo->kernelArgInfo) {
            kernelInfo.kernelArgPatchInfoVector.resize(1);
        }

        parentKernel->initialize();
        parentKernel->mockKernelInfo->kernelArgInfo.at(0).pureStatefulBufferAccess = false;
        parentKernel->mockKernelInfo->kernelArgInfo.at(1).pureStatefulBufferAccess = true;
        parentKernel->mockKernelInfo->kernelArgInfo.at(2).pureStatefulBufferAccess = false;

        parentKernel->setArgBuffer(0, sizeof(cl_mem *), &clMem0); // stateless on BUFFER_COMPRESSED - insert
        parentKernel->setArgBuffer(1, sizeof(cl_mem *), &clMem1); // stateful on BUFFER_COMPRESSED - dont insert
        parentKernel->setArgBuffer(2, sizeof(cl_mem *), &clMem2); // stateless on BUFFER_COMPRESSED - insert

        cmdQ.enqueueKernel(parentKernel.get(), 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
        EXPECT_EQ(1u, cmdQ.dispatchAuxTranslationInputs.size());
        EXPECT_EQ(2u, std::get<BuffersForAuxTranslation>(cmdQ.dispatchAuxTranslationInputs.at(0)).size()); // before kernel

        auto &dispatchedBuffers = std::get<BuffersForAuxTranslation>(cmdQ.dispatchAuxTranslationInputs.at(0));

        EXPECT_NE(dispatchedBuffers.end(), dispatchedBuffers.find(&buffer0));
        EXPECT_EQ(dispatchedBuffers.end(), dispatchedBuffers.find(&buffer1));
        EXPECT_NE(dispatchedBuffers.end(), dispatchedBuffers.find(&buffer2));

        EXPECT_EQ(GraphicsAllocation::AllocationType::BUFFER, buffer0.getGraphicsAllocation()->getAllocationType());
        EXPECT_EQ(GraphicsAllocation::AllocationType::BUFFER_COMPRESSED, buffer1.getGraphicsAllocation()->getAllocationType());
        EXPECT_EQ(GraphicsAllocation::AllocationType::BUFFER, buffer2.getGraphicsAllocation()->getAllocationType());
    }
}

HWCMDTEST_F(IGFX_GEN8_CORE, EnqueueAuxKernelTests, givenParentKernelWhenAuxTranslationIsRequiredThenMakeEnqueueBlocking) {
    if (pDevice->getSupportedClVersion() >= 20) {
        MyCmdQ<FamilyType> cmdQ(context, pDevice);
        size_t gws[3] = {1, 0, 0};

        cl_queue_properties queueProperties = {};
        auto mockDevQueue = std::make_unique<MockDeviceQueueHw<FamilyType>>(context, pDevice, queueProperties);
        context->setDefaultDeviceQueue(mockDevQueue.get());
        std::unique_ptr<MockParentKernel> parentKernel(MockParentKernel::create(*context, false, false, false, false, false));
        parentKernel->initialize();

        parentKernel->auxTranslationRequired = false;
        cmdQ.enqueueKernel(parentKernel.get(), 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
        EXPECT_EQ(0u, cmdQ.waitCalled);
        mockDevQueue->getIgilQueue()->m_controls.m_CriticalSection = 0;

        parentKernel->auxTranslationRequired = true;
        cmdQ.enqueueKernel(parentKernel.get(), 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
        EXPECT_EQ(1u, cmdQ.waitCalled);
    }
}
