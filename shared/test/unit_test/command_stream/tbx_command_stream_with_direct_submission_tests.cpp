/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver_with_aub_dump.h"
#include "shared/source/command_stream/command_stream_receiver_with_aub_dump.inl"
#include "shared/source/direct_submission/direct_submission_hw.h"
#include "shared/source/direct_submission/dispatchers/render_dispatcher.h"
#include "shared/source/helpers/hardware_context_controller.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/fixtures/mock_aub_center_fixture.h"
#include "shared/test/common/fixtures/tbx_command_stream_fixture.h"
#include "shared/test/common/helpers/batch_buffer_helper.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/direct_submission_upload_helper.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/mocks/mock_aub_center.h"
#include "shared/test/common/mocks/mock_aub_manager.h"
#include "shared/test/common/mocks/mock_aub_memory_operations_handler.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_os_context.h"
#include "shared/test/common/mocks/mock_tbx_csr.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/unit_test/mocks/mock_os_agnostic_direct_submission.h"

#include <algorithm>
#include <cstdint>

using namespace NEO;

struct TbxDirectSubmissionFixture : public DeviceFixture {
    TbxDirectSubmissionFixture() {}

    template <typename FamilyType>
    void setUpT() {
        setUpForEngine<FamilyType>(getChosenEngineType(*defaultHwInfo));
        ASSERT_TRUE(commandStreamReceiver->isDirectSubmissionEnabled());
    }

    template <typename FamilyType>
    void setUpForEngine(aub_stream::EngineType engineType) {
        debugManager.flags.EnableDirectSubmission.set(true);
        debugManager.flags.EnableDirectSubmissionInSimulationMode.set(true);

        DeviceFixture::setUp();

        std::string fileName = "file_name.aub";
        mockAubManager = new MockAubManager();
        MockAubCenter *mockAubCenter = new MockAubCenter(pDevice->getRootDeviceEnvironment(), false, fileName, CommandStreamReceiverType::aub);
        mockAubCenter->aubManager = std::unique_ptr<MockAubManager>(mockAubManager);
        pDevice->executionEnvironment->rootDeviceEnvironments[0]->aubCenter = std::unique_ptr<MockAubCenter>(mockAubCenter);

        memoryOperationsHandler = new NEO::MockAubMemoryOperationsHandler(mockAubManager);
        pDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->memoryOperationsInterface.reset(memoryOperationsHandler);

        commandStreamReceiver = std::make_unique<MockTbxCsr<FamilyType>>(*pDevice->getExecutionEnvironment(), pDevice->getDeviceBitfield());

        pDevice->getExecutionEnvironment()->memoryManager->reInitLatestContextId();
        auto osContext = pDevice->getExecutionEnvironment()->memoryManager->createAndRegisterOsContext(commandStreamReceiver.get(),
                                                                                                       EngineDescriptorHelper::getDefaultDescriptor({engineType, EngineUsage::regular},
                                                                                                                                                    PreemptionHelper::getDefaultPreemptionMode(*defaultHwInfo)));
        osContext->setDefaultContext(true);
        commandStreamReceiver->setupContext(*osContext);
        commandStreamReceiver->initializeTagAllocation();
        commandStreamReceiver->createGlobalFenceAllocation();

        commandStreamReceiver->initDirectSubmission();
        ASSERT_TRUE(osContext->isDirectSubmissionActive());
    }

    template <typename FamilyType>
    void tearDownT() {
        commandStreamReceiver.reset(nullptr);
        mockAubManager = nullptr;
        DeviceFixture::tearDown();
    }

    template <typename FamilyType>
    MockTbxCsr<FamilyType> *getCsr() {
        return static_cast<MockTbxCsr<FamilyType> *>(commandStreamReceiver.get());
    }
    void setUp() {}
    void tearDown() {}

    DebugManagerStateRestore debugManagerStateRestore;
    MockAubManager *mockAubManager = nullptr;
    MockAubMemoryOperationsHandler *memoryOperationsHandler = nullptr;
    std::unique_ptr<CommandStreamReceiver> commandStreamReceiver;
};

struct TbxBlitterDirectSubmissionFixture : public TbxDirectSubmissionFixture {
    template <typename FamilyType>
    void setUpT() {
        debugManager.flags.DirectSubmissionOverrideBlitterSupport.set(2);
        setUpForEngine<FamilyType>(aub_stream::ENGINE_BCS);
        ASSERT_TRUE(commandStreamReceiver->isBlitterDirectSubmissionEnabled());
    }
};

template <typename FamilyType>
void flushAppendOnStartedDirectSubmission(MockTbxCsr<FamilyType> &tbxCsr, MemoryManager &memoryManager, const DeviceBitfield deviceBitfield) {
    auto commandBuffer = memoryManager.allocateGraphicsMemoryWithProperties(MockAllocationProperties{tbxCsr.getRootDeviceIndex(), MemoryConstants::pageSize, deviceBitfield});

    LinearStream commandStream(commandBuffer);
    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(commandStream.getGraphicsAllocation(), &commandStream, commandStream.getUsed());
    batchBuffer.endCmdPtr = commandStream.getCpuBase();
    ResidencyContainer allocationsForResidency;
    tbxCsr.flush(batchBuffer, allocationsForResidency);

    auto mockHardwareContext = static_cast<MockHardwareContext *>(tbxCsr.hardwareContextController->hardwareContexts[0].get());
    mockHardwareContext->submitCalled = false;
    mockHardwareContext->resubmitCalled = false;
    tbxCsr.allocationTypesWrittenInChunksWithAubManager.clear();
    allocationsForResidency.clear();
    commandStream.getSpace(256);
    batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(commandStream.getGraphicsAllocation(), &commandStream, commandStream.getUsed());
    batchBuffer.endCmdPtr = ptrOffset(commandStream.getCpuBase(), 128);
    tbxCsr.flush(batchBuffer, allocationsForResidency);

    memoryManager.freeGraphicsMemory(commandBuffer);
}

template <typename FamilyType>
bool isRingBufferChunkUploaded(const MockTbxCsr<FamilyType> &tbxCsr) {
    const auto &writtenAllocationTypes = tbxCsr.allocationTypesWrittenInChunksWithAubManager;
    return std::find(writtenAllocationTypes.begin(), writtenAllocationTypes.end(), AllocationType::ringBuffer) != writtenAllocationTypes.end();
}

template <typename FamilyType>
bool isCommandBufferTbxWritableAfterFlush(MockTbxCsr<FamilyType> &tbxCsr, MemoryManager &memoryManager, const DeviceBitfield deviceBitfield) {
    auto commandBuffer = memoryManager.allocateGraphicsMemoryWithProperties(MockAllocationProperties{tbxCsr.getRootDeviceIndex(), MemoryConstants::pageSize, deviceBitfield});

    LinearStream commandStream(commandBuffer);
    commandStream.getSpace(256);
    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(commandStream.getGraphicsAllocation(), &commandStream, commandStream.getUsed());
    batchBuffer.endCmdPtr = ptrOffset(commandStream.getCpuBase(), 128);
    ResidencyContainer allocationsForResidency;
    tbxCsr.flush(batchBuffer, allocationsForResidency);

    auto isTbxWritable = tbxCsr.isTbxWritable(*commandBuffer);
    memoryManager.freeGraphicsMemory(commandBuffer);
    return isTbxWritable;
}

template <typename FamilyType, typename Dispatcher>
bool isPreviousRingBufferUploadedWhenRingBuffersAreSwitched(MockTbxCsr<FamilyType> &tbxCsr, std::unique_ptr<DirectSubmissionHw<FamilyType, Dispatcher>> &csrDirectSubmission, MockAubManager &mockAubManager, MemoryManager &memoryManager, const DeviceBitfield deviceBitfield) {
    auto directSubmission = new MockOsAgnosticDirectSubmission<FamilyType, Dispatcher>(tbxCsr);
    csrDirectSubmission.reset(directSubmission);
    tbxCsr.completionFenceValuePointer = csrDirectSubmission->getCompletionValuePointer();
    EXPECT_TRUE(directSubmission->initialize(false));

    auto commandBuffer = memoryManager.allocateGraphicsMemoryWithProperties(MockAllocationProperties{tbxCsr.getRootDeviceIndex(), MemoryConstants::pageSize, deviceBitfield});
    LinearStream commandStream(commandBuffer);
    commandStream.getSpace(256);
    ResidencyContainer allocationsForResidency;

    auto flushCommandBuffer = [&]() {
        BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(commandStream.getGraphicsAllocation(), &commandStream, commandStream.getUsed());
        batchBuffer.endCmdPtr = ptrOffset(commandStream.getCpuBase(), 128);
        allocationsForResidency.clear();
        tbxCsr.flush(batchBuffer, allocationsForResidency);
    };
    flushCommandBuffer();

    auto previousRingBuffer = directSubmission->ringBuffers[directSubmission->currentRingBuffer].ringBuffer;
    directSubmission->ringCommandStream.getSpace(directSubmission->ringCommandStream.getAvailableSpace() - directSubmission->getSizeSwitchRingBufferSection());
    auto switchSectionStart = previousRingBuffer->getGpuAddress() + directSubmission->ringCommandStream.getUsed();
    auto switchSectionEnd = switchSectionStart + directSubmission->getSizeSwitchRingBufferSection();

    mockAubManager.storeAllocationParams = true;
    mockAubManager.storedAllocationParams.clear();
    flushCommandBuffer();

    auto ringBufferSwitched = directSubmission->ringBuffers[directSubmission->currentRingBuffer].ringBuffer != previousRingBuffer;
    auto previousRingBufferUploaded = std::any_of(mockAubManager.storedAllocationParams.begin(), mockAubManager.storedAllocationParams.end(), [&](const auto &params) {
        return params.gfxAddress <= switchSectionStart && params.gfxAddress + params.size >= switchSectionEnd;
    });

    memoryManager.freeGraphicsMemory(commandBuffer);
    return ringBufferSwitched && previousRingBufferUploaded;
}

using TbxCommandStreamDirectSubmissionTests = Test<TbxDirectSubmissionFixture>;
using TbxCommandStreamBlitterDirectSubmissionTests = Test<TbxBlitterDirectSubmissionFixture>;

HWTEST_TEMPLATED_F(TbxCommandStreamDirectSubmissionTests, givenStartedDirectSubmissionWhenFlushIsCalledThenRingBufferChunkIsUploaded) {
    auto *tbxCsr = getCsr<FamilyType>();

    flushAppendOnStartedDirectSubmission(*tbxCsr, *pDevice->executionEnvironment->memoryManager, pDevice->getDeviceBitfield());

    EXPECT_TRUE(isRingBufferChunkUploaded(*tbxCsr));
}

HWTEST_TEMPLATED_F(TbxCommandStreamBlitterDirectSubmissionTests, givenStartedBlitterDirectSubmissionWhenFlushIsCalledThenRingBufferChunkIsUploaded) {
    auto *tbxCsr = getCsr<FamilyType>();

    flushAppendOnStartedDirectSubmission(*tbxCsr, *pDevice->executionEnvironment->memoryManager, pDevice->getDeviceBitfield());

    EXPECT_TRUE(isRingBufferChunkUploaded(*tbxCsr));
}

HWTEST_TEMPLATED_F(TbxCommandStreamDirectSubmissionTests, givenTbxCommandStreamReceiverWhenFlushIsCalledThenItShouldProcessAllocationsForResidency) {
    auto *tbxCsr = getCsr<FamilyType>();
    MemoryManager *memoryManager = tbxCsr->getMemoryManager();
    ASSERT_NE(nullptr, memoryManager);

    auto graphicsAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{tbxCsr->getRootDeviceIndex(), MemoryConstants::pageSize});
    ASSERT_NE(nullptr, graphicsAllocation);

    GraphicsAllocation *commandBuffer = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{tbxCsr->getRootDeviceIndex(), MemoryConstants::pageSize});
    ASSERT_NE(nullptr, commandBuffer);

    LinearStream cs(commandBuffer);
    cs.getSpace(256);
    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());
    batchBuffer.endCmdPtr = ptrOffset(cs.getCpuBase(), 128);
    ResidencyContainer allocationsForResidency = {graphicsAllocation};

    EXPECT_FALSE(graphicsAllocation->isResident(tbxCsr->getOsContext().getContextId()));

    *tbxCsr->writeMemoryWithAubManagerCalled = false;
    mockAubManager->writeMemory2Called = false;

    tbxCsr->flush(batchBuffer, allocationsForResidency);

    EXPECT_TRUE(*tbxCsr->writeMemoryWithAubManagerCalled);
    EXPECT_TRUE(mockAubManager->writeMemory2Called);

    EXPECT_TRUE(graphicsAllocation->isResident(tbxCsr->getOsContext().getContextId()));
    EXPECT_EQ(tbxCsr->peekTaskCount() + 1, graphicsAllocation->getResidencyTaskCount(tbxCsr->getOsContext().getContextId()));

    ASSERT_EQ(2u, allocationsForResidency.size());
    EXPECT_EQ(commandBuffer, allocationsForResidency[1]);

    EXPECT_EQ(tbxCsr->peekTaskCount() + 1, commandBuffer->getResidencyTaskCount(tbxCsr->getOsContext().getContextId()));

    memoryManager->freeGraphicsMemory(commandBuffer);
    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

HWTEST_TEMPLATED_F(TbxCommandStreamDirectSubmissionTests, whenPollForAubCompletionCalledThenDontInsertPoll) {
    auto *tbxCsr = getCsr<FamilyType>();
    auto mockHardwareContext = static_cast<MockHardwareContext *>(tbxCsr->hardwareContextController->hardwareContexts[0].get());

    tbxCsr->pollForAubCompletion();
    EXPECT_FALSE(tbxCsr->pollForCompletionCalled);
    EXPECT_FALSE(mockHardwareContext->pollForCompletionCalled);
}

HWTEST_TEMPLATED_F(TbxCommandStreamDirectSubmissionTests, whenPollForCompletionCalledThenDontInsertPoll) {
    auto *tbxCsr = getCsr<FamilyType>();
    auto mockHardwareContext = static_cast<MockHardwareContext *>(tbxCsr->hardwareContextController->hardwareContexts[0].get());

    tbxCsr->pollForCompletion(true);
    EXPECT_TRUE(tbxCsr->pollForCompletionCalled);
    EXPECT_FALSE(mockHardwareContext->pollForCompletionCalled);
}

HWTEST_TEMPLATED_F(TbxCommandStreamDirectSubmissionTests, givenTbxWithAubCsrWhenPollForAubCompletionCalledThenDoNotInsertPoll) {
    auto *tbxCsr = getCsr<FamilyType>();
    auto csr = std::make_unique<CommandStreamReceiverWithAUBDump<MockTbxCsr<FamilyType>>>("",
                                                                                          *pDevice->executionEnvironment,
                                                                                          0,
                                                                                          pDevice->getDeviceBitfield());

    csr->setupContext(tbxCsr->getOsContext());
    csr->initializeEngine();

    auto mockHardwareContext = static_cast<MockHardwareContext *>(csr->hardwareContextController->hardwareContexts[0].get());

    csr->pollForAubCompletion();
    EXPECT_FALSE(mockHardwareContext->pollForCompletionCalled);
}

HWTEST_TEMPLATED_F(TbxCommandStreamDirectSubmissionTests, givenNotStartedDirectSubmissionWhenFlushIsCalledWithZeroSizedBufferThenSubmitIsCalledOnHwContext) {
    auto *tbxCsr = getCsr<FamilyType>();

    auto mockHardwareContext = static_cast<MockHardwareContext *>(tbxCsr->hardwareContextController->hardwareContexts[0].get());

    auto commandBuffer = pDevice->executionEnvironment->memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{pDevice->getRootDeviceIndex(), MemoryConstants::pageSize, pDevice->getDeviceBitfield()});

    LinearStream cs(commandBuffer);
    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());
    batchBuffer.endCmdPtr = cs.getCpuBase();
    ResidencyContainer allocationsForResidency;

    tbxCsr->flush(batchBuffer, allocationsForResidency);

    EXPECT_TRUE(mockHardwareContext->submitCalled);
    pDevice->executionEnvironment->memoryManager->freeGraphicsMemory(commandBuffer);
}

HWTEST_TEMPLATED_F(TbxCommandStreamDirectSubmissionTests, givenStartedDirectSubmissionWhenFlushIsCalledThenSubmitIsNotCalledOnHwContext) {
    auto *tbxCsr = getCsr<FamilyType>();

    auto mockHardwareContext = static_cast<MockHardwareContext *>(tbxCsr->hardwareContextController->hardwareContexts[0].get());

    auto commandBuffer = pDevice->executionEnvironment->memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{pDevice->getRootDeviceIndex(), MemoryConstants::pageSize, pDevice->getDeviceBitfield()});

    LinearStream cs(commandBuffer);
    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());
    batchBuffer.endCmdPtr = cs.getCpuBase();
    ResidencyContainer allocationsForResidency;

    tbxCsr->flush(batchBuffer, allocationsForResidency);

    EXPECT_TRUE(mockHardwareContext->submitCalled);
    mockHardwareContext->submitCalled = false;
    allocationsForResidency.clear();
    cs.getSpace(256);
    batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());
    batchBuffer.endCmdPtr = ptrOffset(cs.getCpuBase(), 128);

    tbxCsr->flush(batchBuffer, allocationsForResidency);

    EXPECT_FALSE(mockHardwareContext->submitCalled);

    pDevice->executionEnvironment->memoryManager->freeGraphicsMemory(commandBuffer);
}

HWTEST_TEMPLATED_F(TbxCommandStreamDirectSubmissionTests, givenStartedDirectSubmissionAndInactiveHwContextWhenFlushIsCalledThenHwContextIsResubmittedWithoutNewSubmission) {
    auto *tbxCsr = getCsr<FamilyType>();
    auto mockHardwareContext = static_cast<MockHardwareContext *>(tbxCsr->hardwareContextController->hardwareContexts[0].get());
    mockHardwareContext->isActiveReturnValue = false;

    flushAppendOnStartedDirectSubmission(*tbxCsr, *pDevice->executionEnvironment->memoryManager, pDevice->getDeviceBitfield());

    EXPECT_TRUE(mockHardwareContext->resubmitCalled);
    EXPECT_FALSE(mockHardwareContext->submitCalled);
}

HWTEST_TEMPLATED_F(TbxCommandStreamDirectSubmissionTests, givenStartedDirectSubmissionAndActiveHwContextWhenFlushIsCalledThenHwContextIsNotResubmitted) {
    auto *tbxCsr = getCsr<FamilyType>();
    auto mockHardwareContext = static_cast<MockHardwareContext *>(tbxCsr->hardwareContextController->hardwareContexts[0].get());
    mockHardwareContext->isActiveReturnValue = true;

    flushAppendOnStartedDirectSubmission(*tbxCsr, *pDevice->executionEnvironment->memoryManager, pDevice->getDeviceBitfield());

    EXPECT_FALSE(mockHardwareContext->resubmitCalled);
    EXPECT_FALSE(mockHardwareContext->submitCalled);
}

HWTEST_TEMPLATED_F(TbxCommandStreamBlitterDirectSubmissionTests, givenStartedBlitterDirectSubmissionAndInactiveHwContextWhenFlushIsCalledThenHwContextIsResubmittedWithoutNewSubmission) {
    auto *tbxCsr = getCsr<FamilyType>();
    auto mockHardwareContext = static_cast<MockHardwareContext *>(tbxCsr->hardwareContextController->hardwareContexts[0].get());
    mockHardwareContext->isActiveReturnValue = false;

    flushAppendOnStartedDirectSubmission(*tbxCsr, *pDevice->executionEnvironment->memoryManager, pDevice->getDeviceBitfield());

    EXPECT_TRUE(mockHardwareContext->resubmitCalled);
    EXPECT_FALSE(mockHardwareContext->submitCalled);
}

HWTEST_TEMPLATED_F(TbxCommandStreamDirectSubmissionTests, givenStartedDirectSubmissionAndInactiveHwContextWhenDirectSubmissionIsStoppedThenHwContextIsNotResubmitted) {
    auto *tbxCsr = getCsr<FamilyType>();
    flushAppendOnStartedDirectSubmission(*tbxCsr, *pDevice->executionEnvironment->memoryManager, pDevice->getDeviceBitfield());

    auto mockHardwareContext = static_cast<MockHardwareContext *>(tbxCsr->hardwareContextController->hardwareContexts[0].get());
    mockHardwareContext->isActiveReturnValue = false;
    mockHardwareContext->resubmitCalled = false;

    tbxCsr->stopDirectSubmission(false, false);

    EXPECT_FALSE(mockHardwareContext->resubmitCalled);
}

HWTEST_TEMPLATED_F(TbxCommandStreamDirectSubmissionTests, givenStartedDirectSubmissionWhenFlushIsCalledThenUsedCommandBufferRangeAndReturnPatchAreUploaded) {
    auto uploadResult = flushCommandBufferOnStartedDirectSubmission<FamilyType>(*getCsr<FamilyType>(), *mockAubManager, *pDevice->executionEnvironment->memoryManager, pDevice->getDeviceBitfield());

    EXPECT_TRUE(uploadResult.usedCommandBufferRangeUploaded);
    EXPECT_TRUE(uploadResult.returnPatchUploaded);
    EXPECT_FALSE(uploadResult.tbxWritableAfterFlush);
}

HWTEST_TEMPLATED_F(TbxCommandStreamBlitterDirectSubmissionTests, givenStartedBlitterDirectSubmissionWhenFlushIsCalledThenUsedCommandBufferRangeAndReturnPatchAreUploaded) {
    auto uploadResult = flushCommandBufferOnStartedDirectSubmission<FamilyType>(*getCsr<FamilyType>(), *mockAubManager, *pDevice->executionEnvironment->memoryManager, pDevice->getDeviceBitfield());

    EXPECT_TRUE(uploadResult.usedCommandBufferRangeUploaded);
    EXPECT_TRUE(uploadResult.returnPatchUploaded);
    EXPECT_FALSE(uploadResult.tbxWritableAfterFlush);
}

HWTEST_TEMPLATED_F(TbxCommandStreamDirectSubmissionTests, givenRingBufferSectionsWhenHandlingResidencyThenOnlyNotUploadedSectionsAreWritten) {
    MockOsAgnosticDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*commandStreamReceiver);
    ASSERT_TRUE(directSubmission.initialize(false));
    auto ringBufferAllocation = directSubmission.ringCommandStream.getGraphicsAllocation();
    ResidencyContainer allocationsForResidency;
    mockAubManager->storedAllocationParams.clear();
    mockAubManager->storeAllocationParams = true;

    directSubmission.ringCommandStream.getSpace(64);
    auto firstSectionEnd = directSubmission.ringCommandStream.getUsed();
    directSubmission.handleResidency(&allocationsForResidency);
    directSubmission.ringCommandStream.getSpace(32);
    auto secondSectionEnd = directSubmission.ringCommandStream.getUsed();
    directSubmission.handleResidency(&allocationsForResidency);
    directSubmission.handleResidency(&allocationsForResidency);
    directSubmission.ringCommandStream.getSpace(16);
    directSubmission.handleResidency(nullptr);

    UploadedSections expectedSections = {{0u, firstSectionEnd}, {firstSectionEnd, secondSectionEnd - firstSectionEnd}};
    EXPECT_EQ(expectedSections, getUploadedSectionsOfAllocation(*mockAubManager, *ringBufferAllocation));
    EXPECT_EQ(ringBufferAllocation, directSubmission.uploadedRingBufferAllocation);
    EXPECT_EQ(secondSectionEnd, directSubmission.uploadedRingBufferOffset);
}

HWTEST_TEMPLATED_F(TbxCommandStreamDirectSubmissionTests, givenTrackedSectionOfOtherRingBufferWhenHandlingResidencyThenCurrentRingBufferIsUploadedFromStart) {
    MockOsAgnosticDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*commandStreamReceiver);
    ASSERT_TRUE(directSubmission.initialize(false));
    auto ringBufferAllocation = directSubmission.ringCommandStream.getGraphicsAllocation();
    MockGraphicsAllocation otherRingBufferAllocation;
    directSubmission.uploadedRingBufferAllocation = &otherRingBufferAllocation;
    directSubmission.uploadedRingBufferOffset = 32u;
    ResidencyContainer allocationsForResidency;
    mockAubManager->storedAllocationParams.clear();
    mockAubManager->storeAllocationParams = true;

    directSubmission.ringCommandStream.getSpace(64);
    auto sectionEnd = directSubmission.ringCommandStream.getUsed();
    directSubmission.handleResidency(&allocationsForResidency);

    UploadedSections expectedSections = {{0u, sectionEnd}};
    EXPECT_EQ(expectedSections, getUploadedSectionsOfAllocation(*mockAubManager, *ringBufferAllocation));
    EXPECT_EQ(ringBufferAllocation, directSubmission.uploadedRingBufferAllocation);
    EXPECT_EQ(sectionEnd, directSubmission.uploadedRingBufferOffset);
}

HWTEST_TEMPLATED_F(TbxCommandStreamDirectSubmissionTests, givenRingBufferSubmissionWhenSubmitIsCalledThenSubmittedSectionIsTrackedAsUploaded) {
    MockOsAgnosticDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*commandStreamReceiver);
    ASSERT_TRUE(directSubmission.initialize(false));
    auto ringBufferAllocation = directSubmission.ringCommandStream.getGraphicsAllocation();

    directSubmission.ringCommandStream.getSpace(64);
    auto submittedSectionEnd = directSubmission.ringCommandStream.getUsed();
    directSubmission.submit(ringBufferAllocation->getGpuAddress(), submittedSectionEnd, nullptr);

    EXPECT_EQ(ringBufferAllocation, directSubmission.uploadedRingBufferAllocation);
    EXPECT_EQ(submittedSectionEnd, directSubmission.uploadedRingBufferOffset);

    directSubmission.ringCommandStream.getSpace(16);
    directSubmission.submit(ringBufferAllocation->getGpuAddress() + submittedSectionEnd, 0u, nullptr);

    EXPECT_EQ(submittedSectionEnd, directSubmission.uploadedRingBufferOffset);
}

HWTEST_TEMPLATED_F(TbxCommandStreamDirectSubmissionTests, givenNotUploadedRingBufferSectionWhenUnblockingGpuThenSectionIsUploaded) {
    MockOsAgnosticDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*commandStreamReceiver);
    ASSERT_TRUE(directSubmission.initialize(false));
    auto ringBufferAllocation = directSubmission.ringCommandStream.getGraphicsAllocation();
    mockAubManager->storedAllocationParams.clear();
    mockAubManager->storeAllocationParams = true;

    directSubmission.ringCommandStream.getSpace(48);
    auto sectionEnd = directSubmission.ringCommandStream.getUsed();
    directSubmission.unblockGpu();

    UploadedSections expectedSections = {{0u, sectionEnd}};
    EXPECT_EQ(expectedSections, getUploadedSectionsOfAllocation(*mockAubManager, *ringBufferAllocation));
    EXPECT_EQ(sectionEnd, directSubmission.uploadedRingBufferOffset);
}

HWTEST_TEMPLATED_F(TbxCommandStreamDirectSubmissionTests, givenStartedRingWithTrackedSectionWhenHandlingSwitchRingBuffersThenRemainingPartOfRingBufferIsUploaded) {
    MockOsAgnosticDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*commandStreamReceiver);
    ASSERT_TRUE(directSubmission.initialize(false));
    auto ringBufferAllocation = directSubmission.ringCommandStream.getGraphicsAllocation();
    ResidencyContainer allocationsForResidency;
    directSubmission.ringCommandStream.getSpace(64);
    auto trackedSectionEnd = directSubmission.ringCommandStream.getUsed();
    directSubmission.handleResidency(&allocationsForResidency);
    mockAubManager->storedAllocationParams.clear();
    mockAubManager->storeAllocationParams = true;

    directSubmission.ringStart = true;
    directSubmission.handleSwitchRingBuffers(nullptr);
    directSubmission.ringStart = false;

    UploadedSections expectedSections = {{trackedSectionEnd, directSubmission.getRingBufferUsableSize(*ringBufferAllocation) - trackedSectionEnd}};
    EXPECT_EQ(expectedSections, getUploadedSectionsOfAllocation(*mockAubManager, *ringBufferAllocation));
    EXPECT_EQ(ringBufferAllocation, directSubmission.uploadedRingBufferAllocation);
    EXPECT_EQ(0u, directSubmission.uploadedRingBufferOffset);
}

HWTEST_TEMPLATED_F(TbxCommandStreamDirectSubmissionTests, givenNotStartedRingWhenHandlingSwitchRingBuffersThenNothingIsUploadedAndTrackingIsReset) {
    MockOsAgnosticDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*commandStreamReceiver);
    ASSERT_TRUE(directSubmission.initialize(false));
    auto ringBufferAllocation = directSubmission.ringCommandStream.getGraphicsAllocation();
    ResidencyContainer allocationsForResidency;
    directSubmission.ringCommandStream.getSpace(64);
    directSubmission.handleResidency(&allocationsForResidency);
    mockAubManager->storedAllocationParams.clear();
    mockAubManager->storeAllocationParams = true;

    directSubmission.handleSwitchRingBuffers(nullptr);

    EXPECT_TRUE(getUploadedSectionsOfAllocation(*mockAubManager, *ringBufferAllocation).empty());
    EXPECT_EQ(ringBufferAllocation, directSubmission.uploadedRingBufferAllocation);
    EXPECT_EQ(0u, directSubmission.uploadedRingBufferOffset);
}

HWTEST_TEMPLATED_F(TbxCommandStreamDirectSubmissionTests, givenStartedRingWithoutTrackedSectionWhenHandlingSwitchRingBuffersThenNothingIsUploaded) {
    MockOsAgnosticDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*commandStreamReceiver);
    ASSERT_TRUE(directSubmission.initialize(false));
    auto ringBufferAllocation = directSubmission.ringCommandStream.getGraphicsAllocation();
    mockAubManager->storedAllocationParams.clear();
    mockAubManager->storeAllocationParams = true;

    directSubmission.ringStart = true;
    directSubmission.handleSwitchRingBuffers(nullptr);
    directSubmission.ringStart = false;

    EXPECT_TRUE(getUploadedSectionsOfAllocation(*mockAubManager, *ringBufferAllocation).empty());
    EXPECT_EQ(ringBufferAllocation, directSubmission.uploadedRingBufferAllocation);
    EXPECT_EQ(0u, directSubmission.uploadedRingBufferOffset);
}

HWTEST_TEMPLATED_F(TbxCommandStreamDirectSubmissionTests, givenStartedDirectSubmissionWhenRingBuffersAreSwitchedThenPreviousRingBufferIsUploaded) {
    auto *tbxCsr = getCsr<FamilyType>();

    EXPECT_TRUE(isPreviousRingBufferUploadedWhenRingBuffersAreSwitched(*tbxCsr, tbxCsr->directSubmission, *mockAubManager, *pDevice->executionEnvironment->memoryManager, pDevice->getDeviceBitfield()));
}

HWTEST_TEMPLATED_F(TbxCommandStreamBlitterDirectSubmissionTests, givenStartedBlitterDirectSubmissionWhenRingBuffersAreSwitchedThenPreviousRingBufferIsUploaded) {
    auto *tbxCsr = getCsr<FamilyType>();

    EXPECT_TRUE(isPreviousRingBufferUploadedWhenRingBuffersAreSwitched(*tbxCsr, tbxCsr->blitterDirectSubmission, *mockAubManager, *pDevice->executionEnvironment->memoryManager, pDevice->getDeviceBitfield()));
}

HWTEST_TEMPLATED_F(TbxCommandStreamDirectSubmissionTests, givenRelaxedOrderingNotInitializedWhenSubmittingThenSchedulerIsNotUploaded) {
    debugManager.flags.DirectSubmissionRelaxedOrdering.set(1);
    MockOsAgnosticDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*commandStreamReceiver);
    ASSERT_TRUE(directSubmission.initialize(false));
    ASSERT_NE(nullptr, directSubmission.relaxedOrderingSchedulerAllocation);
    mockAubManager->storedAllocationParams.clear();
    mockAubManager->storeAllocationParams = true;

    directSubmission.submit(directSubmission.ringCommandStream.getGraphicsAllocation()->getGpuAddress(), 0u, nullptr);
    directSubmission.unblockGpu();

    EXPECT_TRUE(getUploadedSectionsOfAllocation(*mockAubManager, *directSubmission.relaxedOrderingSchedulerAllocation).empty());
    EXPECT_FALSE(directSubmission.relaxedOrderingSchedulerUploaded);
}

HWTEST_TEMPLATED_F(TbxCommandStreamDirectSubmissionTests, givenRelaxedOrderingInitializedWhenSubmittingTwiceThenSchedulerIsUploadedOnce) {
    debugManager.flags.DirectSubmissionRelaxedOrdering.set(1);
    MockOsAgnosticDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*commandStreamReceiver);
    ASSERT_TRUE(directSubmission.initialize(false));
    auto schedulerAllocation = directSubmission.relaxedOrderingSchedulerAllocation;
    ASSERT_NE(nullptr, schedulerAllocation);
    mockAubManager->storedAllocationParams.clear();
    mockAubManager->storeAllocationParams = true;
    directSubmission.relaxedOrderingInitialized = true;

    directSubmission.submit(directSubmission.ringCommandStream.getGraphicsAllocation()->getGpuAddress(), 0u, nullptr);
    directSubmission.submit(directSubmission.ringCommandStream.getGraphicsAllocation()->getGpuAddress(), 0u, nullptr);

    UploadedSections expectedSections = {{0u, schedulerAllocation->getUnderlyingBufferSize()}};
    EXPECT_EQ(expectedSections, getUploadedSectionsOfAllocation(*mockAubManager, *schedulerAllocation));
    EXPECT_TRUE(directSubmission.relaxedOrderingSchedulerUploaded);
}

HWTEST_TEMPLATED_F(TbxCommandStreamDirectSubmissionTests, givenRelaxedOrderingInitializedWhenUnblockingGpuTwiceThenSchedulerIsUploadedOnce) {
    debugManager.flags.DirectSubmissionRelaxedOrdering.set(1);
    MockOsAgnosticDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*commandStreamReceiver);
    ASSERT_TRUE(directSubmission.initialize(false));
    auto schedulerAllocation = directSubmission.relaxedOrderingSchedulerAllocation;
    ASSERT_NE(nullptr, schedulerAllocation);
    mockAubManager->storedAllocationParams.clear();
    mockAubManager->storeAllocationParams = true;
    directSubmission.relaxedOrderingInitialized = true;

    directSubmission.unblockGpu();
    directSubmission.unblockGpu();

    UploadedSections expectedSections = {{0u, schedulerAllocation->getUnderlyingBufferSize()}};
    EXPECT_EQ(expectedSections, getUploadedSectionsOfAllocation(*mockAubManager, *schedulerAllocation));
    EXPECT_TRUE(directSubmission.relaxedOrderingSchedulerUploaded);
}
