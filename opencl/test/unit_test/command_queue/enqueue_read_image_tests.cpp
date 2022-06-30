/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/allocations_list.h"
#include "shared/source/memory_manager/migration_sync_data.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/mocks/mock_builtins.h"
#include "shared/test/common/mocks/mock_gmm_resource_info.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/source/built_ins/builtins_dispatch_builder.h"
#include "opencl/test/unit_test/command_queue/enqueue_read_image_fixture.h"
#include "opencl/test/unit_test/fixtures/one_mip_level_image_fixture.h"
#include "opencl/test/unit_test/gen_common/gen_commands_common_validation.h"
#include "opencl/test/unit_test/mocks/mock_builtin_dispatch_info_builder.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"

#include "reg_configs_common.h"

using namespace NEO;

HWCMDTEST_F(IGFX_GEN8_CORE, EnqueueReadImageTest, WhenReadingImageThenGpgpuWalkerIsProgrammedCorrectly) {
    typedef typename FamilyType::GPGPU_WALKER GPGPU_WALKER;
    enqueueReadImage<FamilyType>();

    auto *cmd = reinterpret_cast<GPGPU_WALKER *>(cmdWalker);
    ASSERT_NE(nullptr, cmd);

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
        cmd->getSimdSize() == GPGPU_WALKER::SIMD_SIZE_SIMD32 ? 32 : cmd->getSimdSize() == GPGPU_WALKER::SIMD_SIZE_SIMD16 ? 16
                                                                                                                         : 8;
    uint64_t simdMask = maxNBitValue(simd);

    // Mask off lanes based on the execution masks
    auto laneMaskRight = cmd->getRightExecutionMask() & simdMask;
    auto lanesPerThreadX = 0;
    while (laneMaskRight) {
        lanesPerThreadX += laneMaskRight & 1;
        laneMaskRight >>= 1;
    }
}

HWTEST_F(EnqueueReadImageTest, GivenBlockingEnqueueWhenReadingImageThenTaskLevelIsNotIncremented) {
    //this test case assumes IOQ
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.taskCount = pCmdQ->taskCount + 100;
    csr.taskLevel = pCmdQ->taskLevel + 50;
    auto oldCsrTaskLevel = csr.peekTaskLevel();

    EnqueueReadImageHelper<>::enqueueReadImage(pCmdQ, srcImage, CL_TRUE);
    EXPECT_EQ(csr.peekTaskCount(), pCmdQ->taskCount);
    EXPECT_EQ(oldCsrTaskLevel, pCmdQ->taskLevel);
}

HWTEST_F(EnqueueReadImageTest, whenEnqueueReadImageThenBuiltinKernelIsResolved) {

    UserEvent userEvent{};
    cl_event inputEvent = &userEvent;
    cl_event outputEvent{};

    EnqueueReadImageHelper<>::enqueueReadImage(pCmdQ, srcImage, CL_FALSE,
                                               EnqueueReadImageTraits::origin,
                                               EnqueueReadImageTraits::region,
                                               EnqueueReadImageTraits::rowPitch,
                                               EnqueueReadImageTraits::slicePitch,
                                               EnqueueReadImageTraits::hostPtr,
                                               EnqueueReadImageTraits::mapAllocation,
                                               1u,
                                               &inputEvent,
                                               &outputEvent);

    auto pEvent = castToObject<Event>(outputEvent);
    auto pCommand = static_cast<CommandComputeKernel *>(pEvent->peekCommand());
    EXPECT_FALSE(pCommand->peekKernel()->Kernel::canTransformImages());
    EXPECT_TRUE(pCommand->peekKernel()->isPatched());
    userEvent.setStatus(CL_COMPLETE);
    pEvent->release();
    pCmdQ->finish();
}

template <typename GfxFamily>
struct CreateAllocationForHostSurfaceFailCsr : public CommandStreamReceiverHw<GfxFamily> {
    using CommandStreamReceiverHw<GfxFamily>::CommandStreamReceiverHw;

    bool createAllocationForHostSurface(HostPtrSurface &surface, bool requiresL3Flush) override {
        return CL_FALSE;
    }
};

HWTEST_F(EnqueueReadImageTest, givenCommandQueueAndFailingAllocationForHostSurfaceWhenEnqueueReadImageThenOutOfResourceIsReturned) {
    MockCommandQueueHw<FamilyType> cmdQ(context, pClDevice, nullptr);
    auto failCsr = std::make_unique<CreateAllocationForHostSurfaceFailCsr<FamilyType>>(*pDevice->getExecutionEnvironment(), pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());

    failCsr->setupContext(*pDevice->getDefaultEngine().osContext);
    CommandStreamReceiver *oldCommandStreamReceiver = &cmdQ.getGpgpuCommandStreamReceiver();
    cmdQ.gpgpuEngine->commandStreamReceiver = failCsr.get();

    auto srcImage = Image2dHelper<>::create(context);
    auto retVal = cmdQ.enqueueReadImage(srcImage, CL_FALSE,
                                        EnqueueReadImageTraits::origin,
                                        EnqueueReadImageTraits::region,
                                        EnqueueReadImageTraits::rowPitch,
                                        EnqueueReadImageTraits::slicePitch,
                                        EnqueueReadImageTraits::hostPtr,
                                        EnqueueReadImageTraits::mapAllocation,
                                        0u,
                                        nullptr,
                                        nullptr);
    EXPECT_EQ(CL_OUT_OF_RESOURCES, retVal);
    cmdQ.gpgpuEngine->commandStreamReceiver = oldCommandStreamReceiver;
    srcImage->release();
}

HWTEST_F(EnqueueReadImageTest, givenCommandQueueAndFailingAllocationForHostSurfaceWhenBlockingEnqueueReadImageThenOutOfResourceIsReturned) {
    MockCommandQueueHw<FamilyType> cmdQ(context, pClDevice, nullptr);
    auto failCsr = std::make_unique<CreateAllocationForHostSurfaceFailCsr<FamilyType>>(*pDevice->getExecutionEnvironment(), pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());

    failCsr->setupContext(*pDevice->getDefaultEngine().osContext);
    CommandStreamReceiver *oldCommandStreamReceiver = &cmdQ.getGpgpuCommandStreamReceiver();
    cmdQ.gpgpuEngine->commandStreamReceiver = failCsr.get();

    auto srcImage = Image2dHelper<>::create(context);
    auto retVal = cmdQ.enqueueReadImage(srcImage, CL_TRUE,
                                        EnqueueReadImageTraits::origin,
                                        EnqueueReadImageTraits::region,
                                        EnqueueReadImageTraits::rowPitch,
                                        EnqueueReadImageTraits::slicePitch,
                                        EnqueueReadImageTraits::hostPtr,
                                        EnqueueReadImageTraits::mapAllocation,
                                        0u,
                                        nullptr,
                                        nullptr);
    EXPECT_EQ(CL_OUT_OF_RESOURCES, retVal);
    cmdQ.gpgpuEngine->commandStreamReceiver = oldCommandStreamReceiver;
    srcImage->release();
}

template <typename GfxFamily>
struct CreateAllocationForHostSurfaceCsr : public CommandStreamReceiverHw<GfxFamily> {
    using CommandStreamReceiverHw<GfxFamily>::CommandStreamReceiverHw;

    bool createAllocationForHostSurface(HostPtrSurface &surface, bool requiresL3Flush) override {
        if (surface.peekIsPtrCopyAllowed()) {
            return CommandStreamReceiverHw<GfxFamily>::createAllocationForHostSurface(surface, requiresL3Flush);
        } else {
            return CL_FALSE;
        }
    }

    CompletionStamp flushTask(LinearStream &commandStream, size_t commandStreamStart,
                              const IndirectHeap *dsh, const IndirectHeap *ioh, const IndirectHeap *ssh,
                              uint32_t taskLevel, DispatchFlags &dispatchFlags, Device &device) override {
        return CompletionStamp{0u, 0u, static_cast<FlushStamp>(0u)};
    }
};

HWTEST_F(EnqueueReadImageTest, givenCommandQueueAndPtrCopyAllowedForHostSurfaceWhenBlockingEnqueueReadImageThenSuccessIsReturned) {
    auto csr = std::make_unique<CreateAllocationForHostSurfaceCsr<FamilyType>>(*pDevice->getExecutionEnvironment(), pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context, pClDevice, nullptr);

    csr->setupContext(*pDevice->getDefaultEngine().osContext);
    CommandStreamReceiver *oldCommandStreamReceiver = &cmdQ->getGpgpuCommandStreamReceiver();
    cmdQ->gpgpuEngine->commandStreamReceiver = csr.get();
    csr->initializeTagAllocation();

    auto retVal = cmdQ->enqueueReadImage(srcImage, CL_TRUE,
                                         EnqueueReadImageTraits::origin,
                                         EnqueueReadImageTraits::region,
                                         EnqueueReadImageTraits::rowPitch,
                                         EnqueueReadImageTraits::slicePitch,
                                         EnqueueReadImageTraits::hostPtr,
                                         EnqueueReadImageTraits::mapAllocation,
                                         0u,
                                         nullptr,
                                         nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    cmdQ->gpgpuEngine->commandStreamReceiver = oldCommandStreamReceiver;
}

HWTEST_F(EnqueueReadImageTest, givenGpuHangAndCommandQueueAndPtrCopyAllowedForHostSurfaceWhenBlockingEnqueueReadImageThenOutOfResourcesIsReturned) {
    auto csr = std::make_unique<CreateAllocationForHostSurfaceCsr<FamilyType>>(*pDevice->getExecutionEnvironment(), pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context, pClDevice, nullptr);
    cmdQ->waitForAllEnginesReturnValue = WaitStatus::GpuHang;

    csr->setupContext(*pDevice->getDefaultEngine().osContext);
    CommandStreamReceiver *oldCommandStreamReceiver = &cmdQ->getGpgpuCommandStreamReceiver();
    cmdQ->gpgpuEngine->commandStreamReceiver = csr.get();
    csr->initializeTagAllocation();

    auto retVal = cmdQ->enqueueReadImage(srcImage, CL_TRUE,
                                         EnqueueReadImageTraits::origin,
                                         EnqueueReadImageTraits::region,
                                         EnqueueReadImageTraits::rowPitch,
                                         EnqueueReadImageTraits::slicePitch,
                                         EnqueueReadImageTraits::hostPtr,
                                         EnqueueReadImageTraits::mapAllocation,
                                         0u,
                                         nullptr,
                                         nullptr);
    EXPECT_EQ(CL_OUT_OF_RESOURCES, retVal);
    cmdQ->gpgpuEngine->commandStreamReceiver = oldCommandStreamReceiver;
}

HWTEST_F(EnqueueReadImageTest, givenMultiRootDeviceImageWhenEnqueueReadImageThenKernelRequiresMigration) {

    MockDefaultContext context;

    auto pCmdQ1 = createCommandQueue(context.getDevice(0), nullptr, &context);

    auto pImage = Image2dHelper<>::create(&context);
    EXPECT_TRUE(pImage->getMultiGraphicsAllocation().requiresMigrations());

    UserEvent userEvent{};
    cl_event inputEvent = &userEvent;
    cl_event outputEvent{};

    EnqueueReadImageHelper<>::enqueueReadImage(pCmdQ1, pImage, CL_FALSE,
                                               EnqueueReadImageTraits::origin,
                                               EnqueueReadImageTraits::region,
                                               EnqueueReadImageTraits::rowPitch,
                                               EnqueueReadImageTraits::slicePitch,
                                               EnqueueReadImageTraits::hostPtr,
                                               EnqueueReadImageTraits::mapAllocation,
                                               1u,
                                               &inputEvent,
                                               &outputEvent);

    auto pEvent = castToObject<Event>(outputEvent);
    auto pCommand = static_cast<CommandComputeKernel *>(pEvent->peekCommand());
    auto pKernel = pCommand->peekKernel();
    EXPECT_FALSE(pKernel->Kernel::canTransformImages());
    EXPECT_TRUE(pKernel->isPatched());
    EXPECT_TRUE(pKernel->requiresMemoryMigration());

    auto &memObjectsForMigration = pKernel->getMemObjectsToMigrate();
    ASSERT_EQ(1u, memObjectsForMigration.size());
    auto memObj = memObjectsForMigration.begin()->second;
    for (auto &rootDeviceIndex : context.getRootDeviceIndices()) {
        EXPECT_EQ(pImage->getMultiGraphicsAllocation().getGraphicsAllocation(rootDeviceIndex), memObj->getMultiGraphicsAllocation().getGraphicsAllocation(rootDeviceIndex));
    }

    EXPECT_TRUE(memObj->getMultiGraphicsAllocation().requiresMigrations());

    EXPECT_EQ(MigrationSyncData::locationUndefined, pImage->getMultiGraphicsAllocation().getMigrationSyncData()->getCurrentLocation());

    userEvent.setStatus(CL_COMPLETE);

    EXPECT_EQ(0u, pImage->getMultiGraphicsAllocation().getMigrationSyncData()->getCurrentLocation());
    pEvent->release();
    pCmdQ1->finish();
    pCmdQ1->release();
    pImage->release();
}

HWTEST_F(EnqueueReadImageTest, givenMultiRootDeviceImageWhenEnqueueReadImageIsCalledMultipleTimesThenEachKernelUsesDifferentImage) {

    MockDefaultContext context;

    auto pCmdQ1 = createCommandQueue(context.getDevice(0), nullptr, &context);

    auto pImage = Image2dHelper<>::create(&context);
    EXPECT_TRUE(pImage->getMultiGraphicsAllocation().requiresMigrations());

    UserEvent userEvent{};
    cl_event inputEvent = &userEvent;
    cl_event outputEvent0{};
    cl_event outputEvent1{};

    EXPECT_EQ(MigrationSyncData::locationUndefined, pImage->getMultiGraphicsAllocation().getMigrationSyncData()->getCurrentLocation());
    EnqueueReadImageHelper<>::enqueueReadImage(pCmdQ1, pImage, CL_FALSE,
                                               EnqueueReadImageTraits::origin,
                                               EnqueueReadImageTraits::region,
                                               EnqueueReadImageTraits::rowPitch,
                                               EnqueueReadImageTraits::slicePitch,
                                               EnqueueReadImageTraits::hostPtr,
                                               EnqueueReadImageTraits::mapAllocation,
                                               1u,
                                               &inputEvent,
                                               &outputEvent0);
    EXPECT_EQ(MigrationSyncData::locationUndefined, pImage->getMultiGraphicsAllocation().getMigrationSyncData()->getCurrentLocation());

    auto pEvent0 = castToObject<Event>(outputEvent0);
    auto pCommand0 = static_cast<CommandComputeKernel *>(pEvent0->peekCommand());
    auto pKernel0 = pCommand0->peekKernel();
    EXPECT_FALSE(pKernel0->Kernel::canTransformImages());
    EXPECT_TRUE(pKernel0->isPatched());
    EXPECT_TRUE(pKernel0->requiresMemoryMigration());

    auto &memObjectsForMigration0 = pKernel0->getMemObjectsToMigrate();
    ASSERT_EQ(1u, memObjectsForMigration0.size());
    auto memObj0 = memObjectsForMigration0.begin()->second;
    for (auto &rootDeviceIndex : context.getRootDeviceIndices()) {
        EXPECT_EQ(pImage->getMultiGraphicsAllocation().getGraphicsAllocation(rootDeviceIndex), memObj0->getMultiGraphicsAllocation().getGraphicsAllocation(rootDeviceIndex));
    }

    EXPECT_TRUE(memObj0->getMultiGraphicsAllocation().requiresMigrations());

    EnqueueReadImageHelper<>::enqueueReadImage(pCmdQ1, pImage, CL_FALSE,
                                               EnqueueReadImageTraits::origin,
                                               EnqueueReadImageTraits::region,
                                               EnqueueReadImageTraits::rowPitch,
                                               EnqueueReadImageTraits::slicePitch,
                                               EnqueueReadImageTraits::hostPtr,
                                               EnqueueReadImageTraits::mapAllocation,
                                               1u,
                                               &outputEvent0,
                                               &outputEvent1);
    EXPECT_EQ(MigrationSyncData::locationUndefined, pImage->getMultiGraphicsAllocation().getMigrationSyncData()->getCurrentLocation());

    auto pEvent1 = castToObject<Event>(outputEvent1);
    auto pCommand1 = static_cast<CommandComputeKernel *>(pEvent1->peekCommand());
    auto pKernel1 = pCommand1->peekKernel();
    EXPECT_FALSE(pKernel1->Kernel::canTransformImages());
    EXPECT_TRUE(pKernel1->isPatched());
    EXPECT_TRUE(pKernel1->requiresMemoryMigration());

    auto &memObjectsForMigration1 = pKernel1->getMemObjectsToMigrate();
    ASSERT_EQ(1u, memObjectsForMigration1.size());
    auto memObj1 = memObjectsForMigration1.begin()->second;
    for (auto &rootDeviceIndex : context.getRootDeviceIndices()) {
        EXPECT_EQ(pImage->getMultiGraphicsAllocation().getGraphicsAllocation(rootDeviceIndex), memObj1->getMultiGraphicsAllocation().getGraphicsAllocation(rootDeviceIndex));
    }

    EXPECT_TRUE(memObj1->getMultiGraphicsAllocation().requiresMigrations());

    EXPECT_NE(memObj0, memObj1);

    userEvent.setStatus(CL_COMPLETE);

    EXPECT_EQ(0u, pImage->getMultiGraphicsAllocation().getMigrationSyncData()->getCurrentLocation());
    pEvent0->release();
    pEvent1->release();
    pCmdQ1->finish();
    pCmdQ1->release();
    pImage->release();
}

HWTEST_F(EnqueueReadImageTest, givenMultiRootDeviceImageWhenNonBlockedEnqueueReadImageIsCalledThenCommandQueueIsFlushed) {
    MockDefaultContext context;

    auto pCmdQ1 = createCommandQueue(context.getDevice(0), nullptr, &context);

    auto pImage = Image2dHelper<>::create(&context);
    EXPECT_TRUE(pImage->getMultiGraphicsAllocation().requiresMigrations());
    auto &ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> &>(pCmdQ1->getGpgpuCommandStreamReceiver());

    EXPECT_FALSE(ultCsr.flushBatchedSubmissionsCalled);
    EXPECT_EQ(MigrationSyncData::locationUndefined, pImage->getMultiGraphicsAllocation().getMigrationSyncData()->getCurrentLocation());
    EnqueueReadImageHelper<>::enqueueReadImage(pCmdQ1, pImage, CL_FALSE);

    EXPECT_EQ(0u, pImage->getMultiGraphicsAllocation().getMigrationSyncData()->getCurrentLocation());
    EXPECT_TRUE(ultCsr.flushBatchedSubmissionsCalled);
    pCmdQ1->finish();
    pCmdQ1->release();
    pImage->release();
}

HWTEST_F(EnqueueReadImageTest, givenMultiRootDeviceImageWhenNonBlockedEnqueueReadImageIsCalledThenTlbCacheIsInvalidated) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    MockDefaultContext context;

    auto pCmdQ1 = createCommandQueue(context.getDevice(0), nullptr, &context);

    auto pImage = Image2dHelper<>::create(&context);
    EXPECT_TRUE(pImage->getMultiGraphicsAllocation().requiresMigrations());

    EXPECT_EQ(MigrationSyncData::locationUndefined, pImage->getMultiGraphicsAllocation().getMigrationSyncData()->getCurrentLocation());
    EnqueueReadImageHelper<>::enqueueReadImage(pCmdQ1, pImage, CL_FALSE);

    EXPECT_EQ(0u, pImage->getMultiGraphicsAllocation().getMigrationSyncData()->getCurrentLocation());
    pCmdQ1->finish();

    {
        HardwareParse hwParser;
        hwParser.parseCommands<FamilyType>(pCmdQ1->getCS(0), 0);
        auto pipeControls = findAll<PIPE_CONTROL *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
        EXPECT_LT(0u, pipeControls.size());
        bool pipeControlWithTlbInvalidateFound = false;
        for (auto &pipeControl : pipeControls) {
            auto pipeControlCmd = genCmdCast<PIPE_CONTROL *>(*pipeControl);
            if (pipeControlCmd->getTlbInvalidate()) {
                EXPECT_TRUE(pipeControlCmd->getCommandStreamerStallEnable());
                pipeControlWithTlbInvalidateFound = true;
            }
        }
        EXPECT_TRUE(pipeControlWithTlbInvalidateFound);
    }

    pCmdQ1->release();
    pImage->release();
}

HWTEST_F(EnqueueReadImageTest, givenMultiRootDeviceImageWhenEnqueueReadImageIsCalledToDifferentDevicesThenCorrectLocationIsSet) {

    MockDefaultContext context;

    auto pCmdQ1 = createCommandQueue(context.getDevice(0), nullptr, &context);
    auto pCmdQ2 = createCommandQueue(context.getDevice(1), nullptr, &context);

    auto pImage = Image2dHelper<>::create(&context);
    EXPECT_TRUE(pImage->getMultiGraphicsAllocation().requiresMigrations());
    auto &ultCsr1 = static_cast<UltCommandStreamReceiver<FamilyType> &>(pCmdQ1->getGpgpuCommandStreamReceiver());
    auto &ultCsr2 = static_cast<UltCommandStreamReceiver<FamilyType> &>(pCmdQ2->getGpgpuCommandStreamReceiver());

    EXPECT_FALSE(ultCsr1.flushBatchedSubmissionsCalled);
    EXPECT_FALSE(ultCsr2.flushBatchedSubmissionsCalled);
    EXPECT_EQ(MigrationSyncData::locationUndefined, pImage->getMultiGraphicsAllocation().getMigrationSyncData()->getCurrentLocation());
    EnqueueReadImageHelper<>::enqueueReadImage(pCmdQ1, pImage, CL_FALSE,
                                               EnqueueReadImageTraits::origin,
                                               EnqueueReadImageTraits::region,
                                               EnqueueReadImageTraits::rowPitch,
                                               EnqueueReadImageTraits::slicePitch,
                                               EnqueueReadImageTraits::hostPtr,
                                               EnqueueReadImageTraits::mapAllocation,
                                               0u,
                                               nullptr,
                                               nullptr);

    EXPECT_EQ(0u, pImage->getMultiGraphicsAllocation().getMigrationSyncData()->getCurrentLocation());
    EXPECT_TRUE(ultCsr1.flushBatchedSubmissionsCalled);
    EXPECT_FALSE(ultCsr2.flushBatchedSubmissionsCalled);
    pCmdQ1->finish();

    EnqueueReadImageHelper<>::enqueueReadImage(pCmdQ2, pImage, CL_FALSE,
                                               EnqueueReadImageTraits::origin,
                                               EnqueueReadImageTraits::region,
                                               EnqueueReadImageTraits::rowPitch,
                                               EnqueueReadImageTraits::slicePitch,
                                               EnqueueReadImageTraits::hostPtr,
                                               EnqueueReadImageTraits::mapAllocation,
                                               0u,
                                               nullptr,
                                               nullptr);

    EXPECT_EQ(1u, pImage->getMultiGraphicsAllocation().getMigrationSyncData()->getCurrentLocation());
    EXPECT_TRUE(ultCsr2.flushBatchedSubmissionsCalled);
    pCmdQ2->finish();

    EnqueueReadImageHelper<>::enqueueReadImage(pCmdQ1, pImage, CL_FALSE,
                                               EnqueueReadImageTraits::origin,
                                               EnqueueReadImageTraits::region,
                                               EnqueueReadImageTraits::rowPitch,
                                               EnqueueReadImageTraits::slicePitch,
                                               EnqueueReadImageTraits::hostPtr,
                                               EnqueueReadImageTraits::mapAllocation,
                                               0u,
                                               nullptr,
                                               nullptr);

    EXPECT_EQ(0u, pImage->getMultiGraphicsAllocation().getMigrationSyncData()->getCurrentLocation());
    pCmdQ1->finish();
    pCmdQ1->release();
    pCmdQ2->release();
    pImage->release();
}

HWTEST_F(EnqueueReadImageTest, givenImageFromBufferThatRequiresMigrationWhenEnqueueReadImageThenBufferObjectIsTakenForMigration) {

    MockDefaultContext context;

    auto pCmdQ1 = createCommandQueue(context.getDevice(0), nullptr, &context);

    auto pBuffer = BufferHelper<>::create(&context);
    auto imageDesc = Image2dDefaults::imageDesc;

    cl_mem clBuffer = pBuffer;
    imageDesc.mem_object = clBuffer;

    const_cast<MultiGraphicsAllocation &>(pBuffer->getMultiGraphicsAllocation()).setMultiStorage(true);

    EXPECT_TRUE(pBuffer->getMultiGraphicsAllocation().requiresMigrations());
    auto pImage = Image2dHelper<>::create(&context, &imageDesc);
    EXPECT_TRUE(pImage->getMultiGraphicsAllocation().requiresMigrations());

    UserEvent userEvent{};
    cl_event inputEvent = &userEvent;
    cl_event outputEvent{};

    EnqueueReadImageHelper<>::enqueueReadImage(pCmdQ1, pImage, CL_FALSE,
                                               EnqueueReadImageTraits::origin,
                                               EnqueueReadImageTraits::region,
                                               EnqueueReadImageTraits::rowPitch,
                                               EnqueueReadImageTraits::slicePitch,
                                               EnqueueReadImageTraits::hostPtr,
                                               EnqueueReadImageTraits::mapAllocation,
                                               1u,
                                               &inputEvent,
                                               &outputEvent);

    auto pEvent = castToObject<Event>(outputEvent);
    auto pCommand = static_cast<CommandComputeKernel *>(pEvent->peekCommand());
    auto pKernel = pCommand->peekKernel();
    EXPECT_FALSE(pKernel->Kernel::canTransformImages());
    EXPECT_TRUE(pKernel->isPatched());
    EXPECT_TRUE(pKernel->requiresMemoryMigration());

    auto &memObjectsForMigration = pKernel->getMemObjectsToMigrate();
    ASSERT_EQ(1u, memObjectsForMigration.size());
    auto memObj = memObjectsForMigration.begin()->second;
    EXPECT_EQ(static_cast<MemObj *>(pBuffer), memObj);

    EXPECT_TRUE(memObj->getMultiGraphicsAllocation().requiresMigrations());

    EXPECT_EQ(MigrationSyncData::locationUndefined, pBuffer->getMultiGraphicsAllocation().getMigrationSyncData()->getCurrentLocation());

    userEvent.setStatus(CL_COMPLETE);

    EXPECT_EQ(0u, pBuffer->getMultiGraphicsAllocation().getMigrationSyncData()->getCurrentLocation());
    pEvent->release();
    pCmdQ1->finish();
    pCmdQ1->release();
    pImage->release();
    pBuffer->release();
}

HWTEST_F(EnqueueReadImageTest, GivenNonBlockingEnqueueWhenReadingImageThenTaskLevelIsIncremented) {
    //this test case assumes IOQ
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.taskCount = pCmdQ->taskCount + 100;
    csr.taskLevel = pCmdQ->taskLevel + 50;

    EnqueueReadImageHelper<>::enqueueReadImage(pCmdQ, srcImage, CL_FALSE);
    EXPECT_EQ(csr.peekTaskCount(), pCmdQ->taskCount);
    EXPECT_EQ(csr.peekTaskLevel(), pCmdQ->taskLevel + 1);
}

HWTEST_F(EnqueueReadImageTest, WhenReadingImageThenTaskLevelIsIncremented) {
    auto taskLevelBefore = pCmdQ->taskLevel;

    EnqueueReadImageHelper<>::enqueueReadImage(pCmdQ, srcImage, EnqueueReadImageTraits::blocking);
    EXPECT_GT(pCmdQ->taskLevel, taskLevelBefore);
}

HWTEST_F(EnqueueReadImageTest, WhenReadingImageThenCommandsAreAdded) {
    auto usedCmdBufferBefore = pCS->getUsed();

    EnqueueReadImageHelper<>::enqueueReadImage(pCmdQ, srcImage, EnqueueReadImageTraits::blocking);
    EXPECT_NE(usedCmdBufferBefore, pCS->getUsed());
}

HWTEST_F(EnqueueReadImageTest, WhenReadingImageThenIndirectDataIsAdded) {
    auto dshBefore = pDSH->getUsed();
    auto iohBefore = pIOH->getUsed();
    auto sshBefore = pSSH->getUsed();

    EnqueueReadImageHelper<>::enqueueReadImage(pCmdQ, srcImage, EnqueueReadImageTraits::blocking);
    EXPECT_TRUE(UnitTestHelper<FamilyType>::evaluateDshUsage(dshBefore, pDSH->getUsed(), nullptr, rootDeviceIndex));
    EXPECT_NE(iohBefore, pIOH->getUsed());
    EXPECT_NE(sshBefore, pSSH->getUsed());
}

HWTEST_F(EnqueueReadImageTest, WhenReadingImageThenL3ProgrammingIsCorrect) {
    enqueueReadImage<FamilyType>();
    validateL3Programming<FamilyType>(cmdList, itorWalker);
}

HWCMDTEST_F(IGFX_GEN8_CORE, EnqueueReadImageTest, WhenEnqueueIsDoneThenStateBaseAddressIsProperlyProgrammed) {
    enqueueReadImage<FamilyType>();
    auto &ultCsr = this->pDevice->getUltCommandStreamReceiver<FamilyType>();

    auto &hwHelper = HwHelper::get(pDevice->getHardwareInfo().platform.eRenderCoreFamily);

    validateStateBaseAddress<FamilyType>(ultCsr.getMemoryManager()->getInternalHeapBaseAddress(ultCsr.rootDeviceIndex, pIOH->getGraphicsAllocation()->isAllocatedInLocalMemoryPool()),
                                         ultCsr.getMemoryManager()->getInternalHeapBaseAddress(ultCsr.rootDeviceIndex, !hwHelper.useSystemMemoryPlacementForISA(pDevice->getHardwareInfo())),
                                         pDSH, pIOH, pSSH, itorPipelineSelect, itorWalker, cmdList, 0llu);
}

HWCMDTEST_F(IGFX_GEN8_CORE, EnqueueReadImageTest, WhenReadingImageThenMediaInterfaceDescriptorIsCorrect) {
    typedef typename FamilyType::MEDIA_INTERFACE_DESCRIPTOR_LOAD MEDIA_INTERFACE_DESCRIPTOR_LOAD;
    typedef typename FamilyType::INTERFACE_DESCRIPTOR_DATA INTERFACE_DESCRIPTOR_DATA;

    enqueueReadImage<FamilyType>();

    // All state should be programmed before walker
    auto cmd = reinterpret_cast<MEDIA_INTERFACE_DESCRIPTOR_LOAD *>(cmdMediaInterfaceDescriptorLoad);
    ASSERT_NE(nullptr, cmd);

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
    FamilyType::PARSE::template validateCommand<MEDIA_INTERFACE_DESCRIPTOR_LOAD *>(cmdList.begin(), itorMediaInterfaceDescriptorLoad);
}

HWCMDTEST_F(IGFX_GEN8_CORE, EnqueueReadImageTest, WhenReadingImageThenInterfaceDescriptorData) {
    typedef typename FamilyType::STATE_BASE_ADDRESS STATE_BASE_ADDRESS;
    typedef typename FamilyType::INTERFACE_DESCRIPTOR_DATA INTERFACE_DESCRIPTOR_DATA;

    enqueueReadImage<FamilyType>();

    // Extract the interfaceDescriptorData
    auto cmdSBA = (STATE_BASE_ADDRESS *)cmdStateBaseAddress;
    auto &interfaceDescriptorData = *(INTERFACE_DESCRIPTOR_DATA *)cmdInterfaceDescriptorData;

    // Validate the kernel start pointer.  Technically, a kernel can start at address 0 but let's force a value.
    auto kernelStartPointer = ((uint64_t)interfaceDescriptorData.getKernelStartPointerHigh() << 32) + interfaceDescriptorData.getKernelStartPointer();
    EXPECT_LE(kernelStartPointer, cmdSBA->getInstructionBufferSize() * MemoryConstants::pageSize);

    auto localWorkSize = 4u;
    auto simd = 32u;
    auto numThreadsPerThreadGroup = Math::divideAndRoundUp(localWorkSize, simd);
    EXPECT_EQ(numThreadsPerThreadGroup, interfaceDescriptorData.getNumberOfThreadsInGpgpuThreadGroup());
    EXPECT_NE(0u, interfaceDescriptorData.getCrossThreadConstantDataReadLength());
    EXPECT_NE(0u, interfaceDescriptorData.getConstantIndirectUrbEntryReadLength());

    // We shouldn't have these pointers the same.
    EXPECT_NE(kernelStartPointer, interfaceDescriptorData.getBindingTablePointer());
}

HWTEST_F(EnqueueReadImageTest, WhenReadingImageThenSurfaceStateIsCorrect) {
    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;

    enqueueReadImage<FamilyType>();

    // BufferToImage kernel uses BTI=1 for destSurface
    uint32_t bindingTableIndex = 0;
    const auto surfaceState = getSurfaceState<FamilyType>(&pCmdQ->getIndirectHeap(IndirectHeap::Type::SURFACE_STATE, 0), bindingTableIndex);

    // EnqueueReadImage uses multi-byte copies depending on per-pixel-size-in-bytes
    const auto &imageDesc = srcImage->getImageDesc();
    EXPECT_EQ(imageDesc.image_width, surfaceState->getWidth());
    EXPECT_EQ(imageDesc.image_height, surfaceState->getHeight());
    EXPECT_NE(0u, surfaceState->getSurfacePitch());
    EXPECT_NE(0u, surfaceState->getSurfaceType());
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R32_UINT, surfaceState->getSurfaceFormat());
    EXPECT_EQ(MockGmmResourceInfo::getHAlignSurfaceStateResult, surfaceState->getSurfaceHorizontalAlignment());
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_VERTICAL_ALIGNMENT_VALIGN_4, surfaceState->getSurfaceVerticalAlignment());
    EXPECT_EQ(srcAllocation->getGpuAddress(), surfaceState->getSurfaceBaseAddress());
}

HWTEST2_F(EnqueueReadImageTest, WhenReadingImageThenPipelineSelectIsProgrammed, IsAtMostXeHpcCore) {
    enqueueReadImage<FamilyType>();
    int numCommands = getNumberOfPipelineSelectsThatEnablePipelineSelect<FamilyType>();
    EXPECT_EQ(1, numCommands);
}

HWCMDTEST_F(IGFX_GEN8_CORE, EnqueueReadImageTest, WhenReadingImageThenMediaVfeStateIsCorrect) {
    enqueueReadImage<FamilyType>();
    validateMediaVFEState<FamilyType>(&pDevice->getHardwareInfo(), cmdMediaVfeState, cmdList, itorMediaVfeState);
}

HWCMDTEST_F(IGFX_GEN8_CORE, EnqueueReadImageTest, GivenBlockingEnqueueWhenReadingImageThenPipeControlWithDcFlushIsSetAfterWalker) {
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;

    bool blocking = true;
    enqueueReadImage<FamilyType>(blocking);

    auto itorWalker = find<typename FamilyType::GPGPU_WALKER *>(cmdList.begin(), cmdList.end());

    auto itorCmd = find<PIPE_CONTROL *>(itorWalker, cmdList.end());
    auto *cmd = (PIPE_CONTROL *)*itorCmd;
    EXPECT_NE(cmdList.end(), itorCmd);

    if (UnitTestHelper<FamilyType>::isPipeControlWArequired(pDevice->getHardwareInfo())) {
        // SKL: two PIPE_CONTROLs following GPGPU_WALKER: first has DcFlush and second has Write HwTag
        EXPECT_FALSE(cmd->getDcFlushEnable());
        // Move to next PPC
        auto itorCmdP = ++((GenCmdList::iterator)itorCmd);
        EXPECT_NE(cmdList.end(), itorCmdP);
        auto itorCmd2 = find<PIPE_CONTROL *>(itorCmdP, cmdList.end());
        cmd = (PIPE_CONTROL *)*itorCmd2;
        EXPECT_TRUE(cmd->getDcFlushEnable());
    } else {
        // BDW: single PIPE_CONTROL following GPGPU_WALKER has DcFlush and Write HwTag
        EXPECT_TRUE(cmd->getDcFlushEnable());
    }
}

HWTEST_F(EnqueueReadImageTest, GivenImage1DarrayWhenReadImageIsCalledThenHostPtrSizeIsCalculatedProperly) {
    std::unique_ptr<Image> srcImage(Image1dArrayHelper<>::create(context));
    auto &imageDesc = srcImage->getImageDesc();
    auto imageSize = imageDesc.image_width * imageDesc.image_array_size * 4;
    size_t origin[] = {0, 0, 0};
    size_t region[] = {imageDesc.image_width, imageDesc.image_array_size, 1};

    EnqueueReadImageHelper<>::enqueueReadImage(pCmdQ, srcImage.get(), CL_FALSE, origin, region);

    auto &csr = pCmdQ->getGpgpuCommandStreamReceiver();

    auto temporaryAllocation = csr.getTemporaryAllocations().peekHead();
    ASSERT_NE(nullptr, temporaryAllocation);

    EXPECT_EQ(temporaryAllocation->getUnderlyingBufferSize(), imageSize);
}

HWTEST_F(EnqueueReadImageTest, GivenImage1DarrayWhenReadImageIsCalledThenRowPitchIsSetToSlicePitch) {
    auto builtIns = new MockBuiltins();
    pCmdQ->getDevice().getExecutionEnvironment()->rootDeviceEnvironments[pCmdQ->getDevice().getRootDeviceIndex()]->builtins.reset(builtIns);
    EBuiltInOps::Type copyBuiltIn = EBuiltInOps::CopyImage3dToBuffer;
    auto &origBuilder = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(
        copyBuiltIn,
        pCmdQ->getClDevice());

    // substitute original builder with mock builder
    auto oldBuilder = pClExecutionEnvironment->setBuiltinDispatchInfoBuilder(
        rootDeviceIndex,
        copyBuiltIn,
        std::unique_ptr<NEO::BuiltinDispatchInfoBuilder>(new MockBuiltinDispatchInfoBuilder(*builtIns, pCmdQ->getClDevice(), &origBuilder)));

    std::unique_ptr<Image> srcImage(Image1dArrayHelper<>::create(context));
    auto &imageDesc = srcImage->getImageDesc();
    size_t origin[] = {0, 0, 0};
    size_t region[] = {imageDesc.image_width, imageDesc.image_array_size, 1};
    size_t rowPitch = 64;
    size_t slicePitch = 128;

    EnqueueReadImageHelper<>::enqueueReadImage(pCmdQ, srcImage.get(), CL_TRUE, origin, region, rowPitch, slicePitch);

    auto &mockBuilder = static_cast<MockBuiltinDispatchInfoBuilder &>(BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(copyBuiltIn,
                                                                                                                              pCmdQ->getClDevice()));
    auto params = mockBuilder.getBuiltinOpParams();
    EXPECT_EQ(params->dstRowPitch, slicePitch);

    // restore original builder and retrieve mock builder
    auto newBuilder = pClExecutionEnvironment->setBuiltinDispatchInfoBuilder(
        rootDeviceIndex,
        copyBuiltIn,
        std::move(oldBuilder));
    EXPECT_NE(nullptr, newBuilder);
}

HWTEST_F(EnqueueReadImageTest, GivenImage2DarrayWhenReadImageIsCalledThenHostPtrSizeIsCalculatedProperly) {
    std::unique_ptr<Image> srcImage(Image2dArrayHelper<>::create(context));
    auto &imageDesc = srcImage->getImageDesc();
    auto imageSize = imageDesc.image_width * imageDesc.image_height * imageDesc.image_array_size * 4;
    size_t origin[] = {0, 0, 0};
    size_t region[] = {imageDesc.image_width, imageDesc.image_height, imageDesc.image_array_size};

    EnqueueReadImageHelper<>::enqueueReadImage(pCmdQ, srcImage.get(), CL_FALSE, origin, region);

    auto &csr = pCmdQ->getGpgpuCommandStreamReceiver();

    auto temporaryAllocation = csr.getTemporaryAllocations().peekHead();
    ASSERT_NE(nullptr, temporaryAllocation);

    EXPECT_EQ(temporaryAllocation->getUnderlyingBufferSize(), imageSize);
}

HWTEST_F(EnqueueReadImageTest, GivenImage1DAndImageShareTheSameStorageWithHostPtrWhenReadReadImageIsCalledThenImageIsNotRead) {
    cl_int retVal = CL_SUCCESS;
    std::unique_ptr<Image> dstImage2(Image1dHelper<>::create(context));
    auto &imageDesc = dstImage2->getImageDesc();
    std::unique_ptr<CommandQueue> pCmdOOQ(createCommandQueue(pClDevice, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE));
    size_t origin[] = {0, 0, 0};
    size_t region[] = {imageDesc.image_width, 1, 1};
    void *ptr = dstImage2->getCpuAddressForMemoryTransfer();

    size_t rowPitch = dstImage2->getHostPtrRowPitch();
    size_t slicePitch = dstImage2->getHostPtrSlicePitch();
    retVal = pCmdOOQ->enqueueReadImage(dstImage2.get(),
                                       CL_FALSE,
                                       origin,
                                       region,
                                       rowPitch,
                                       slicePitch,
                                       ptr,
                                       nullptr,
                                       0,
                                       nullptr,
                                       nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(pCmdOOQ->taskLevel, 0u);
}

HWTEST_F(EnqueueReadImageTest, GivenImage1DArrayAndImageShareTheSameStorageWithHostPtrWhenReadReadImageIsCalledThenImageIsNotRead) {
    cl_int retVal = CL_SUCCESS;
    std::unique_ptr<Image> dstImage2(Image1dArrayHelper<>::create(context));
    auto &imageDesc = dstImage2->getImageDesc();
    size_t origin[] = {imageDesc.image_width / 2, imageDesc.image_array_size / 2, 0};
    size_t region[] = {imageDesc.image_width - (imageDesc.image_width / 2), imageDesc.image_array_size - (imageDesc.image_array_size / 2), 1};
    void *ptr = dstImage2->getCpuAddressForMemoryTransfer();
    auto bytesPerPixel = 4;
    size_t rowPitch = dstImage2->getHostPtrRowPitch();
    size_t slicePitch = dstImage2->getHostPtrSlicePitch();
    auto pOffset = origin[2] * rowPitch + origin[1] * slicePitch + origin[0] * bytesPerPixel;
    void *ptrStorage = ptrOffset(ptr, pOffset);
    retVal = pCmdQ->enqueueReadImage(dstImage2.get(),
                                     CL_FALSE,
                                     origin,
                                     region,
                                     rowPitch,
                                     slicePitch,
                                     ptrStorage,
                                     nullptr,
                                     0,
                                     nullptr,
                                     nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(pCmdQ->taskLevel, 0u);
}

HWTEST_F(EnqueueReadImageTest, GivenSharedContextZeroCopy2DImageWhenEnqueueReadImageWithMappedPointerIsCalledThenImageIsNotRead) {
    cl_int retVal = CL_SUCCESS;
    context->isSharedContext = true;

    std::unique_ptr<Image> dstImage(ImageHelper<ImageUseHostPtr<Image2dDefaults>>::create(context));
    EXPECT_TRUE(dstImage->isMemObjZeroCopy());

    auto &imageDesc = dstImage->getImageDesc();
    size_t origin[] = {0, 0, 0};
    size_t region[] = {imageDesc.image_width, imageDesc.image_height, 1};
    void *ptr = dstImage->getCpuAddressForMemoryTransfer();

    size_t rowPitch = dstImage->getHostPtrRowPitch();
    size_t slicePitch = dstImage->getHostPtrSlicePitch();
    retVal = pCmdQ->enqueueReadImage(dstImage.get(),
                                     CL_FALSE,
                                     origin,
                                     region,
                                     rowPitch,
                                     slicePitch,
                                     ptr,
                                     nullptr,
                                     0,
                                     nullptr,
                                     nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(pCmdQ->taskLevel, 0u);
}

HWTEST_F(EnqueueReadImageTest, GivenImage1DThatIsZeroCopyWhenReadImageWithTheSamePointerAndOutputEventIsPassedThenEventHasCorrectCommandTypeSet) {
    cl_int retVal = CL_SUCCESS;
    std::unique_ptr<Image> dstImage(Image1dHelper<>::create(context));
    auto &imageDesc = dstImage->getImageDesc();
    size_t origin[] = {0, 0, 0};
    size_t region[] = {imageDesc.image_width, imageDesc.image_height, 1};
    void *ptr = dstImage->getCpuAddressForMemoryTransfer();
    size_t rowPitch = dstImage->getHostPtrRowPitch();
    size_t slicePitch = dstImage->getHostPtrSlicePitch();

    cl_uint numEventsInWaitList = 0;
    cl_event event = nullptr;

    retVal = pCmdQ->enqueueReadImage(dstImage.get(),
                                     CL_FALSE,
                                     origin,
                                     region,
                                     rowPitch,
                                     slicePitch,
                                     ptr,
                                     nullptr,
                                     numEventsInWaitList,
                                     nullptr,
                                     &event);

    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, event);

    auto pEvent = static_cast<Event *>(event);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_READ_IMAGE), pEvent->getCommandType());

    pEvent->release();
}

struct EnqueueReadImageTestWithBcs : EnqueueReadImageTest {
    void SetUp() override {
        VariableBackup<HardwareInfo> backupHwInfo(defaultHwInfo.get());
        defaultHwInfo->capabilityTable.blitterOperationsSupported = true;
        EnqueueReadImageTest::SetUp();
    }
};

HWTEST_F(EnqueueReadImageTest, givenCommandQueueWhenEnqueueReadImageIsCalledThenItCallsNotifyFunction) {
    auto mockCmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context, pClDevice, nullptr);
    std::unique_ptr<Image> srcImage(Image2dArrayHelper<>::create(context));
    auto &imageDesc = srcImage->getImageDesc();
    size_t origin[] = {0, 0, 0};
    size_t region[] = {imageDesc.image_width, imageDesc.image_height, imageDesc.image_array_size};

    EnqueueReadImageHelper<>::enqueueReadImage(mockCmdQ.get(), srcImage.get(), CL_TRUE, origin, region);

    EXPECT_TRUE(mockCmdQ->notifyEnqueueReadImageCalled);
}

HWTEST_F(EnqueueReadImageTest, givenCommandQueueWhenEnqueueReadImageWithMapAllocationIsCalledThenItDoesntCallNotifyFunction) {
    auto mockCmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context, pClDevice, nullptr);
    std::unique_ptr<Image> srcImage(Image2dArrayHelper<>::create(context));
    auto &imageDesc = srcImage->getImageDesc();
    size_t origin[] = {0, 0, 0};
    size_t region[] = {imageDesc.image_width, imageDesc.image_height, imageDesc.image_array_size};
    size_t rowPitch = srcImage->getHostPtrRowPitch();
    size_t slicePitch = srcImage->getHostPtrSlicePitch();
    GraphicsAllocation mapAllocation{0, AllocationType::UNKNOWN, nullptr, 0, 0, MemoryPool::MemoryNull, MemoryManager::maxOsContextCount, 0llu};

    EnqueueReadImageHelper<>::enqueueReadImage(mockCmdQ.get(), srcImage.get(), CL_TRUE, origin, region, rowPitch, slicePitch, dstPtr, &mapAllocation);

    EXPECT_FALSE(mockCmdQ->notifyEnqueueReadImageCalled);
}

HWTEST_F(EnqueueReadImageTest, givenEnqueueReadImageBlockingWhenAUBDumpAllocsOnEnqueueReadOnlyIsOnThenImageShouldBeSetDumpable) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.AUBDumpAllocsOnEnqueueReadOnly.set(true);

    std::unique_ptr<Image> srcImage(Image2dArrayHelper<>::create(context));
    srcAllocation = srcImage->getGraphicsAllocation(pClDevice->getRootDeviceIndex());
    auto &imageDesc = srcImage->getImageDesc();
    size_t origin[] = {0, 0, 0};
    size_t region[] = {imageDesc.image_width, imageDesc.image_height, imageDesc.image_array_size};

    ASSERT_FALSE(srcAllocation->isAllocDumpable());

    EnqueueReadImageHelper<>::enqueueReadImage(pCmdQ, srcImage.get(), CL_TRUE, origin, region);

    EXPECT_TRUE(srcAllocation->isAllocDumpable());
}

HWTEST_F(EnqueueReadImageTest, givenEnqueueReadImageNonBlockingWhenAUBDumpAllocsOnEnqueueReadOnlyIsOnThenImageShouldntBeSetDumpable) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.AUBDumpAllocsOnEnqueueReadOnly.set(true);

    std::unique_ptr<Image> srcImage(Image2dArrayHelper<>::create(context));
    srcAllocation = srcImage->getGraphicsAllocation(pClDevice->getRootDeviceIndex());
    auto &imageDesc = srcImage->getImageDesc();
    size_t origin[] = {0, 0, 0};
    size_t region[] = {imageDesc.image_width, imageDesc.image_height, imageDesc.image_array_size};

    ASSERT_FALSE(srcAllocation->isAllocDumpable());

    EnqueueReadImageHelper<>::enqueueReadImage(pCmdQ, srcImage.get(), CL_FALSE, origin, region);

    EXPECT_FALSE(srcAllocation->isAllocDumpable());
}

typedef EnqueueReadImageMipMapTest MipMapReadImageTest;

HWTEST_P(MipMapReadImageTest, GivenImageWithMipLevelNonZeroWhenReadImageIsCalledThenProperMipLevelIsSet) {
    auto builtIns = new MockBuiltins();
    pCmdQ->getDevice().getExecutionEnvironment()->rootDeviceEnvironments[pCmdQ->getDevice().getRootDeviceIndex()]->builtins.reset(builtIns);
    auto imageType = (cl_mem_object_type)GetParam();
    auto &origBuilder = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(
        EBuiltInOps::CopyImage3dToBuffer,
        pCmdQ->getClDevice());

    // substitute original builder with mock builder
    auto oldBuilder = pClExecutionEnvironment->setBuiltinDispatchInfoBuilder(
        rootDeviceIndex,
        EBuiltInOps::CopyImage3dToBuffer,
        std::unique_ptr<NEO::BuiltinDispatchInfoBuilder>(new MockBuiltinDispatchInfoBuilder(*builtIns, pCmdQ->getClDevice(), &origBuilder)));

    cl_int retVal = CL_SUCCESS;
    cl_image_desc imageDesc = {};
    uint32_t expectedMipLevel = 3;
    imageDesc.image_type = imageType;
    imageDesc.num_mip_levels = 10;
    imageDesc.image_width = 4;
    imageDesc.image_height = 1;
    imageDesc.image_depth = 1;
    size_t origin[] = {0, 0, 0, 0};
    size_t region[] = {imageDesc.image_width, 1, 1};
    std::unique_ptr<Image> image;
    switch (imageType) {
    case CL_MEM_OBJECT_IMAGE1D:
        origin[1] = expectedMipLevel;
        image = std::unique_ptr<Image>(ImageHelper<Image1dDefaults>::create(context, &imageDesc));
        break;
    case CL_MEM_OBJECT_IMAGE1D_ARRAY:
        imageDesc.image_array_size = 2;
        origin[2] = expectedMipLevel;
        image = std::unique_ptr<Image>(ImageHelper<Image1dArrayDefaults>::create(context, &imageDesc));
        break;
    case CL_MEM_OBJECT_IMAGE2D:
        origin[2] = expectedMipLevel;
        image = std::unique_ptr<Image>(ImageHelper<Image2dDefaults>::create(context, &imageDesc));
        break;
    case CL_MEM_OBJECT_IMAGE2D_ARRAY:
        imageDesc.image_array_size = 2;
        origin[3] = expectedMipLevel;
        image = std::unique_ptr<Image>(ImageHelper<Image2dArrayDefaults>::create(context, &imageDesc));
        break;
    case CL_MEM_OBJECT_IMAGE3D:
        origin[3] = expectedMipLevel;
        image = std::unique_ptr<Image>(ImageHelper<Image3dDefaults>::create(context, &imageDesc));
        break;
    }
    EXPECT_NE(nullptr, image.get());

    std::unique_ptr<uint32_t[]> ptr = std::unique_ptr<uint32_t[]>(new uint32_t[3]);
    retVal = pCmdQ->enqueueReadImage(image.get(),
                                     CL_FALSE,
                                     origin,
                                     region,
                                     0,
                                     0,
                                     ptr.get(),
                                     nullptr,
                                     0,
                                     nullptr,
                                     nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);

    auto &mockBuilder = static_cast<MockBuiltinDispatchInfoBuilder &>(BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(EBuiltInOps::CopyImage3dToBuffer,
                                                                                                                              pCmdQ->getClDevice()));
    auto params = mockBuilder.getBuiltinOpParams();

    EXPECT_EQ(expectedMipLevel, params->srcMipLevel);

    // restore original builder and retrieve mock builder
    auto newBuilder = pClExecutionEnvironment->setBuiltinDispatchInfoBuilder(
        rootDeviceIndex,
        EBuiltInOps::CopyImage3dToBuffer,
        std::move(oldBuilder));
    EXPECT_NE(nullptr, newBuilder);
}

INSTANTIATE_TEST_CASE_P(MipMapReadImageTest_GivenImageWithMipLevelNonZeroWhenWriteImageIsCalledThenProperMipLevelIsSet,
                        MipMapReadImageTest, ::testing::Values(CL_MEM_OBJECT_IMAGE1D, CL_MEM_OBJECT_IMAGE1D_ARRAY, CL_MEM_OBJECT_IMAGE2D, CL_MEM_OBJECT_IMAGE2D_ARRAY, CL_MEM_OBJECT_IMAGE3D));

using NegativeFailAllocationTest = Test<NegativeFailAllocationCommandEnqueueBaseFixture>;

HWTEST_F(NegativeFailAllocationTest, givenEnqueueWriteImageWhenHostPtrAllocationCreationFailsThenReturnOutOfResource) {
    cl_int retVal = CL_SUCCESS;
    auto &imageDesc = image->getImageDesc();

    size_t origin[] = {0, 0, 0};
    size_t region[] = {imageDesc.image_width, imageDesc.image_height, 1};

    size_t rowPitch = image->getHostPtrRowPitch();
    size_t slicePitch = image->getHostPtrSlicePitch();

    retVal = pCmdQ->enqueueWriteImage(image.get(),
                                      CL_FALSE,
                                      origin,
                                      region,
                                      rowPitch,
                                      slicePitch,
                                      ptr,
                                      nullptr,
                                      0,
                                      nullptr,
                                      nullptr);

    EXPECT_EQ(CL_OUT_OF_RESOURCES, retVal);
}

using OneMipLevelReadImageTests = Test<OneMipLevelImageFixture>;

HWTEST_F(OneMipLevelReadImageTests, GivenNotMippedImageWhenReadingImageThenDoNotProgramSourceMipLevel) {
    auto queue = createQueue<FamilyType>();
    auto retVal = queue->enqueueReadImage(
        image.get(),
        CL_TRUE,
        origin,
        region,
        0,
        0,
        cpuPtr,
        nullptr,
        0,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(builtinOpsParamsCaptured);
    EXPECT_EQ(0u, usedBuiltinOpsParams.srcMipLevel);
}
