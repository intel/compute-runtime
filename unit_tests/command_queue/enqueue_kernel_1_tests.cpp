/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/built_ins/builtins_dispatch_builder.h"
#include "unit_tests/command_queue/enqueue_fixture.h"
#include "unit_tests/fixtures/hello_world_fixture.h"
#include "unit_tests/helpers/hw_parse.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/helpers/unit_test_helper.h"
#include "unit_tests/mocks/mock_csr.h"
#include "unit_tests/mocks/mock_submissions_aggregator.h"

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
    size_t timestampPacketSurfacesCount = mockCsr->peekTimestampPacketWriteEnabled() ? 1 : 0;

    EXPECT_EQ(0, mockCsr->flushCalledCount);
    EXPECT_EQ(5u + csrSurfaceCount + timestampPacketSurfacesCount, cmdBuffer->surfaces.size());
}

HWTEST_F(EnqueueKernelTest, givenReducedAddressSpaceGraphicsAllocationForHostPtrWithL3FlushRequiredWhenEnqueueKernelIsCalledThenFlushIsCalledForReducedAddressSpacePlatforms) {
    std::unique_ptr<MockDevice> device;
    std::unique_ptr<CommandQueue> cmdQ;
    auto hwInfoToModify = *platformDevices[0];
    hwInfoToModify.capabilityTable.gpuAddressSpace = MemoryConstants::max32BitAddress;
    device.reset(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfoToModify));
    auto mockCsr = new MockCsrHw2<FamilyType>(device->getHardwareInfo(), *device->executionEnvironment);
    device->resetCommandStreamReceiver(mockCsr);
    auto memoryManager = mockCsr->getMemoryManager();
    uint32_t hostPtr[10]{};

    auto allocation = memoryManager->allocateGraphicsMemoryForHostPtr(1, hostPtr, device->isFullRangeSvm(), true);
    MockKernelWithInternals mockKernel(*device, context);
    size_t gws[3] = {1, 0, 0};
    mockCsr->makeResident(*allocation);
    cmdQ.reset(createCommandQueue(device.get(), 0));
    auto ret = cmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, ret);
    EXPECT_TRUE(mockCsr->passedDispatchFlags.dcFlush);
    memoryManager->freeGraphicsMemory(allocation);
}

HWTEST_F(EnqueueKernelTest, givenReducedAddressSpaceGraphicsAllocationForHostPtrWithL3FlushUnrequiredWhenEnqueueKernelIsCalledThenFlushIsNotForcedByGraphicsAllocation) {
    std::unique_ptr<MockDevice> device;
    std::unique_ptr<CommandQueue> cmdQ;
    auto hwInfoToModify = *platformDevices[0];
    hwInfoToModify.capabilityTable.gpuAddressSpace = MemoryConstants::max32BitAddress;
    device.reset(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfoToModify));
    auto mockCsr = new MockCsrHw2<FamilyType>(device->getHardwareInfo(), *device->executionEnvironment);
    device->resetCommandStreamReceiver(mockCsr);
    auto memoryManager = mockCsr->getMemoryManager();
    uint32_t hostPtr[10]{};

    auto allocation = memoryManager->allocateGraphicsMemoryForHostPtr(1, hostPtr, device->isFullRangeSvm(), false);
    MockKernelWithInternals mockKernel(*device, context);
    size_t gws[3] = {1, 0, 0};
    mockCsr->makeResident(*allocation);
    cmdQ.reset(createCommandQueue(device.get(), 0));
    auto ret = cmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, ret);
    EXPECT_FALSE(mockCsr->passedDispatchFlags.dcFlush);
    memoryManager->freeGraphicsMemory(allocation);
}

HWTEST_F(EnqueueKernelTest, givenFullAddressSpaceGraphicsAllocationWhenEnqueueKernelIsCalledThenFlushIsNotForcedByGraphicsAllocation) {
    HardwareInfo hwInfoToModify;
    std::unique_ptr<MockDevice> device;
    std::unique_ptr<CommandQueue> cmdQ;
    hwInfoToModify = *platformDevices[0];
    hwInfoToModify.capabilityTable.gpuAddressSpace = MemoryConstants::max48BitAddress;
    device.reset(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfoToModify));
    auto mockCsr = new MockCsrHw2<FamilyType>(device->getHardwareInfo(), *device->executionEnvironment);
    device->resetCommandStreamReceiver(mockCsr);
    auto memoryManager = mockCsr->getMemoryManager();
    uint32_t hostPtr[10]{};

    auto allocation = memoryManager->allocateGraphicsMemoryForHostPtr(1, hostPtr, device->isFullRangeSvm(), false);
    MockKernelWithInternals mockKernel(*device, context);
    size_t gws[3] = {1, 0, 0};
    mockCsr->makeResident(*allocation);
    cmdQ.reset(createCommandQueue(device.get(), 0));
    auto ret = cmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, ret);
    EXPECT_FALSE(mockCsr->passedDispatchFlags.dcFlush);
    memoryManager->freeGraphicsMemory(allocation);

    allocation = (memoryManager->allocateGraphicsMemoryForHostPtr(1, hostPtr, device->isFullRangeSvm(), true));
    mockCsr->makeResident(*allocation);
    cmdQ.reset(createCommandQueue(device.get(), 0));
    ret = cmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, ret);
    EXPECT_FALSE(mockCsr->passedDispatchFlags.dcFlush);
    memoryManager->freeGraphicsMemory(allocation);
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
    mockCsr->timestampPacketWriteEnabled = false;
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
        EXPECT_EQ(csrTaskCount, allocation->getTaskCount(mockCsr->getOsContext().getContextId()));
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
