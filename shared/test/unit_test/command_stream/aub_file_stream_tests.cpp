/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub_mem_dump/page_table_entry_bits.h"
#include "shared/source/command_stream/aub_command_stream_receiver.h"
#include "shared/source/command_stream/aub_command_stream_receiver_hw.h"
#include "shared/source/helpers/address_patch.h"
#include "shared/source/helpers/flat_batch_buffer_helper.h"
#include "shared/source/helpers/hardware_context_controller.h"
#include "shared/source/helpers/neo_driver_version.h"
#include "shared/source/memory_manager/page_table.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/test/common/fixtures/aub_command_stream_receiver_fixture.h"
#include "shared/test/common/helpers/batch_buffer_helper.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/mocks/mock_aub_center.h"
#include "shared/test/common/mocks/mock_aub_csr.h"
#include "shared/test/common/mocks/mock_aub_manager.h"
#include "shared/test/common/mocks/mock_aub_subcapture_manager.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_os_context.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "driver_version.h"
#include "gtest/gtest.h"
#include "sys_calls.h"

#include <fstream>
#include <memory>

using namespace NEO;

using AubFileStreamTests = Test<AubCommandStreamReceiverFixture>;
using AubFileStreamWithoutAubStreamTests = Test<AubCommandStreamReceiverWithoutAubStreamFixture>;
using AubCsrTests = Test<AubCommandStreamReceiverFixture>;

HWTEST_F(AubFileStreamTests, givenAubCommandStreamReceiverWhenInitFileIsCalledWithInvalidFileNameThenFileIsNotOpened) {
    AUBCommandStreamReceiverHw<FamilyType> aubCsr("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    std::string invalidFileName = "";

    EXPECT_THROW(aubCsr.initFile(invalidFileName), std::exception);
}

HWTEST_F(AubCsrTests, givenAubCommandStreamReceiverWhenInitFileIsCalledThenFileIsOpenedAndFileNameIsStored) {
    AUBCommandStreamReceiverHw<FamilyType> aubCsr("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    std::string fileName = "file_name.aub";

    aubCsr.initFile(fileName);
    EXPECT_TRUE(aubCsr.isFileOpen());
    EXPECT_STREQ(fileName.c_str(), aubCsr.getFileName().c_str());

    aubCsr.closeFile();
    EXPECT_FALSE(aubCsr.isFileOpen());
    EXPECT_TRUE(aubCsr.getFileName().empty());
}

HWTEST_F(AubCsrTests, givenAubCommandStreamReceiverWhenReopenFileIsCalledThenFileWithSpecifiedNameIsReopened) {
    AUBCommandStreamReceiverHw<FamilyType> aubCsr("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    std::string fileName = "file_name.aub";
    std::string newFileName = "new_file_name.aub";

    aubCsr.reopenFile(fileName);
    EXPECT_TRUE(aubCsr.isFileOpen());
    EXPECT_STREQ(fileName.c_str(), aubCsr.getFileName().c_str());

    aubCsr.reopenFile(newFileName);
    EXPECT_TRUE(aubCsr.isFileOpen());
    EXPECT_STREQ(newFileName.c_str(), aubCsr.getFileName().c_str());
}

HWTEST_F(AubFileStreamTests, givenAubCommandStreamReceiverWithAubManagerWhenInitFileIsCalledThenFileShouldBeInitializedOnce) {
    auto mockAubManager = std::make_unique<MockAubManager>();
    AUBCommandStreamReceiverHw<FamilyType> aubCsr("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    std::string fileName = "file_name.aub";
    aubCsr.aubManager = mockAubManager.get();

    aubCsr.initFile(fileName);
    aubCsr.initFile(fileName);

    EXPECT_EQ(1u, mockAubManager->openCalledCnt);
}

HWTEST_F(AubCsrTests, givenAubCommandStreamReceiverWithAubManagerWhenFileFunctionsAreCalledThenTheyShouldCallTheExpectedAubManagerFunctions) {
    auto mockAubManager = std::make_unique<MockAubManager>();
    AUBCommandStreamReceiverHw<FamilyType> aubCsr("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    std::string fileName = "file_name.aub";
    aubCsr.aubManager = mockAubManager.get();

    aubCsr.initFile(fileName);
    EXPECT_EQ(1u, mockAubManager->openCalledCnt);

    EXPECT_TRUE(aubCsr.isFileOpen());
    EXPECT_TRUE(mockAubManager->isOpenCalled);

    EXPECT_STREQ(fileName.c_str(), aubCsr.getFileName().c_str());
    EXPECT_TRUE(mockAubManager->getFileNameCalled);

    aubCsr.closeFile();
    EXPECT_FALSE(aubCsr.isFileOpen());
    EXPECT_TRUE(aubCsr.getFileName().empty());
    EXPECT_TRUE(mockAubManager->closeCalled);
}

HWTEST_F(AubFileStreamTests, givenAubCommandStreamReceiverWhenOpenFileIsCalledThenFileStreamShouldBeLocked) {
    auto aubCsr = std::make_unique<MockAubCsr<FamilyType>>("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    std::string fileName = "file_name.aub";

    aubCsr->openFile(fileName);

    EXPECT_TRUE(aubCsr->lockStreamCalled);
}

HWTEST_F(AubFileStreamTests, givenAubCommandStreamReceiverWhenReopenFileIsCalledThenFileStreamShouldBeLocked) {
    auto aubCsr = std::make_unique<MockAubCsr<FamilyType>>("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    std::string fileName = "file_name.aub";

    aubCsr->reopenFile(fileName);
    EXPECT_TRUE(aubCsr->lockStreamCalled);
}

HWTEST_F(AubFileStreamWithoutAubStreamTests, givenAubCommandStreamReceiverWhenInitializeEngineIsCalledThenFileStreamShouldBeLocked) {
    auto aubExecutionEnvironment = getEnvironment<MockAubCsr<FamilyType>>(true, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<MockAubCsr<FamilyType>>();

    aubCsr->initializeEngine();
    EXPECT_TRUE(aubCsr->lockStreamCalled);
}

HWTEST_F(AubFileStreamWithoutAubStreamTests, givenAubCommandStreamReceiverWhenSubmitBatchBufferIsCalledThenFileStreamShouldBeLocked) {
    auto aubExecutionEnvironment = getEnvironment<MockAubCsr<FamilyType>>(true, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<MockAubCsr<FamilyType>>();
    LinearStream cs(aubExecutionEnvironment->commandBuffer);
    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());

    auto pBatchBuffer = ptrOffset(batchBuffer.commandBufferAllocation->getUnderlyingBuffer(), batchBuffer.startOffset);
    auto batchBufferGpuAddress = ptrOffset(batchBuffer.commandBufferAllocation->getGpuAddress(), batchBuffer.startOffset);
    auto currentOffset = batchBuffer.usedSize;
    auto sizeBatchBuffer = currentOffset - batchBuffer.startOffset;

    aubCsr->initializeEngine();
    aubCsr->lockStreamCalled = false;

    aubCsr->submitBatchBufferAub(batchBufferGpuAddress, pBatchBuffer, sizeBatchBuffer, aubCsr->getMemoryBank(batchBuffer.commandBufferAllocation),
                                 aubCsr->getPPGTTAdditionalBits(batchBuffer.commandBufferAllocation));
    EXPECT_TRUE(aubCsr->lockStreamCalled);
}

HWTEST_F(AubFileStreamWithoutAubStreamTests, givenAubCommandStreamReceiverWhenWriteMemoryIsCalledThenFileStreamShouldBeLocked) {
    auto aubExecutionEnvironment = getEnvironment<MockAubCsr<FamilyType>>(true, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<MockAubCsr<FamilyType>>();

    aubCsr->initializeEngine();

    MockGraphicsAllocation allocation(reinterpret_cast<void *>(0x1000), 0x1000);

    aubCsr->writeMemory(allocation);
    EXPECT_TRUE(aubCsr->lockStreamCalled);
}

HWTEST_F(AubFileStreamWithoutAubStreamTests, givenAubCommandStreamReceiverWhenPollForCompletionIsCalledThenFileStreamShouldBeLocked) {
    auto aubExecutionEnvironment = getEnvironment<MockAubCsr<FamilyType>>(true, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<MockAubCsr<FamilyType>>();

    aubCsr->latestSentTaskCount = 1;
    aubCsr->pollForCompletionTaskCount = 0;

    aubCsr->pollForCompletion();
    EXPECT_TRUE(aubCsr->lockStreamCalled);
}

HWTEST_F(AubFileStreamWithoutAubStreamTests, givenAubCommandStreamReceiverWhenExpectMemoryEqualIsCalledThenFileStreamShouldBeLocked) {
    auto aubExecutionEnvironment = getEnvironment<MockAubCsr<FamilyType>>(true, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<MockAubCsr<FamilyType>>();

    aubCsr->expectMemoryEqual(reinterpret_cast<void *>(0x1000), reinterpret_cast<void *>(0x1000), 0x1000);
    EXPECT_TRUE(aubCsr->lockStreamCalled);
}

HWTEST_F(AubFileStreamWithoutAubStreamTests, givenAubCommandStreamReceiverWhenAddAubCommentIsCalledThenFileStreamShouldBeLocked) {
    auto aubExecutionEnvironment = getEnvironment<MockAubCsr<FamilyType>>(true, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<MockAubCsr<FamilyType>>();

    aubCsr->addAubComment("comment");
    EXPECT_TRUE(aubCsr->lockStreamCalled);
}

HWTEST_F(AubFileStreamWithoutAubStreamTests, givenAubCommandStreamReceiverWhenDumpAllocationIsCalledThenFileStreamShouldBeLocked) {
    auto aubExecutionEnvironment = getEnvironment<MockAubCsr<FamilyType>>(true, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<MockAubCsr<FamilyType>>();
    GraphicsAllocation allocation{0, 1u /*num gmms*/, AllocationType::unknown, nullptr, 0, 0, 0, MemoryPool::memoryNull, MemoryManager::maxOsContextCount};

    aubCsr->dumpAllocation(allocation);
    EXPECT_TRUE(aubCsr->lockStreamCalled);
}
using AubCsrTests = Test<AubCommandStreamReceiverFixture>;
HWTEST_F(AubCsrTests, givenAubCommandStreamReceiverWithAubManagerWhenCallingAddAubCommentThenCallAddCommentOnAubManager) {
    MockAubCsr<FamilyType> aubCsr("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor());
    aubCsr.setupContext(osContext);
    auto mockAubManager = static_cast<MockAubManager *>(aubCsr.aubManager);
    ASSERT_NE(nullptr, mockAubManager);

    const char *comment = "message";
    aubCsr.addAubComment(comment);

    EXPECT_TRUE(aubCsr.addAubCommentCalled);
    EXPECT_TRUE(mockAubManager->addCommentCalled);
    EXPECT_STREQ(comment, mockAubManager->receivedComments.c_str());
}

HWTEST_F(AubFileStreamWithoutAubStreamTests, givenAubCommandStreamReceiverWhenCallingInsertAubWaitInstructionThenCallPollForCompletion) {
    auto aubExecutionEnvironment = getEnvironment<MockAubCsr<FamilyType>>(true, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<MockAubCsr<FamilyType>>();
    ASSERT_FALSE(aubCsr->pollForCompletionCalled);
    aubCsr->waitForTaskCountWithKmdNotifyFallback(0, 0, false, QueueThrottle::MEDIUM);
    EXPECT_TRUE(aubCsr->pollForCompletionCalled);
}

HWTEST_F(AubCsrTests, givenNewTasksAndHardwareContextPresentWhenCallingPollForCompletionThenCallPollForCompletion) {
    MockAubCsr<FamilyType> aubCsr("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor());
    aubCsr.setupContext(osContext);
    auto hardwareContext = static_cast<MockHardwareContext *>(aubCsr.hardwareContextController->hardwareContexts[0].get());

    aubCsr.latestSentTaskCount = 50;
    aubCsr.pollForCompletionTaskCount = 49;
    ASSERT_FALSE(hardwareContext->pollForCompletionCalled);

    aubCsr.pollForCompletion();
    EXPECT_TRUE(hardwareContext->pollForCompletionCalled);
}

HWTEST_F(AubCsrTests, givenNoNewTasksAndHardwareContextPresentWhenCallingPollForCompletionThenDontCallPollForCompletion) {
    MockAubCsr<FamilyType> aubCsr("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor());
    aubCsr.setupContext(osContext);
    auto hardwareContext = static_cast<MockHardwareContext *>(aubCsr.hardwareContextController->hardwareContexts[0].get());

    aubCsr.latestSentTaskCount = 50;
    aubCsr.pollForCompletionTaskCount = 50;
    ASSERT_FALSE(hardwareContext->pollForCompletionCalled);

    aubCsr.pollForCompletion();
    EXPECT_FALSE(hardwareContext->pollForCompletionCalled);
}

HWTEST_F(AubCsrTests, givenAubCommandStreamReceiverWithHardwareContextInSubCaptureModeWhenPollForCompletionIsCalledAndSubCaptureIsEnabledThenItShouldCallPollForCompletionOnHwContext) {
    DebugManagerStateRestore stateRestore;
    AubSubCaptureCommon aubSubCaptureCommon;
    auto aubSubCaptureManagerMock = new AubSubCaptureManagerMock("", aubSubCaptureCommon);
    MockAubCsr<FamilyType> aubCsr("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor());
    aubCsr.setupContext(osContext);
    auto hardwareContext = static_cast<MockHardwareContext *>(aubCsr.hardwareContextController->hardwareContexts[0].get());

    aubSubCaptureCommon.subCaptureMode = AubSubCaptureManager::SubCaptureMode::toggle;
    aubSubCaptureManagerMock->setSubCaptureToggleActive(true);
    aubSubCaptureManagerMock->checkAndActivateSubCapture("kernelName");
    aubCsr.subCaptureManager = std::unique_ptr<AubSubCaptureManagerMock>(aubSubCaptureManagerMock);
    ASSERT_TRUE(aubCsr.subCaptureManager->isSubCaptureEnabled());

    aubCsr.latestSentTaskCount = 50;
    aubCsr.pollForCompletionTaskCount = 49;
    ASSERT_FALSE(hardwareContext->pollForCompletionCalled);

    aubCsr.pollForCompletion();

    EXPECT_TRUE(hardwareContext->pollForCompletionCalled);
}

HWTEST_F(AubFileStreamTests, givenAubCommandStreamReceiverWithHardwareContextInSubCaptureModeWhenPollForCompletionIsCalledButSubCaptureIsDisabledThenItShouldntCallPollForCompletionOnHwContext) {
    DebugManagerStateRestore stateRestore;
    AubSubCaptureCommon aubSubCaptureCommon;
    auto aubSubCaptureManagerMock = new AubSubCaptureManagerMock("", aubSubCaptureCommon);
    MockAubCsr<FamilyType> aubCsr("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor());
    aubCsr.setupContext(osContext);
    auto hardwareContext = static_cast<MockHardwareContext *>(aubCsr.hardwareContextController->hardwareContexts[0].get());

    aubSubCaptureCommon.subCaptureMode = AubSubCaptureManager::SubCaptureMode::toggle;
    aubSubCaptureManagerMock->disableSubCapture();
    aubCsr.subCaptureManager = std::unique_ptr<AubSubCaptureManagerMock>(aubSubCaptureManagerMock);
    ASSERT_FALSE(aubCsr.subCaptureManager->isSubCaptureEnabled());

    aubCsr.latestSentTaskCount = 50;
    aubCsr.pollForCompletionTaskCount = 49;
    ASSERT_FALSE(hardwareContext->pollForCompletionCalled);

    aubCsr.pollForCompletion();

    EXPECT_FALSE(hardwareContext->pollForCompletionCalled);
}

HWTEST_F(AubFileStreamWithoutAubStreamTests, givenAubCommandStreamReceiverWhenExpectMemoryEqualIsCalledThenItShouldCallTheExpectedFunctions) {
    auto aubExecutionEnvironment = getEnvironment<MockAubCsr<FamilyType>>(true, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<MockAubCsr<FamilyType>>();

    MockGraphicsAllocation allocation(reinterpret_cast<void *>(0x1000), 0x1000);
    aubCsr->expectMemoryEqual(reinterpret_cast<void *>(0x1000), reinterpret_cast<void *>(0x1000), 0x1000);

    EXPECT_TRUE(aubCsr->expectMemoryEqualCalled);
}

HWTEST_F(AubFileStreamWithoutAubStreamTests, givenAubCommandStreamReceiverWhenExpectMemoryNotEqualIsCalledThenItShouldCallTheExpectedFunctions) {
    auto aubExecutionEnvironment = getEnvironment<MockAubCsr<FamilyType>>(true, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<MockAubCsr<FamilyType>>();

    MockGraphicsAllocation allocation(reinterpret_cast<void *>(0x1000), 0x1000);
    aubCsr->expectMemoryNotEqual(reinterpret_cast<void *>(0x1000), reinterpret_cast<void *>(0x1000), 0x1000);

    EXPECT_TRUE(aubCsr->expectMemoryNotEqualCalled);
}

HWTEST_F(AubFileStreamWithoutAubStreamTests, givenAubCommandStreamReceiverWhenExpectMemoryCompressedIsCalledThenItShouldCallTheExpectedFunctions) {
    auto aubExecutionEnvironment = getEnvironment<MockAubCsr<FamilyType>>(true, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<MockAubCsr<FamilyType>>();

    aubCsr->expectMemoryCompressed(reinterpret_cast<void *>(0x1000), reinterpret_cast<void *>(0x1000), 0x1000);

    EXPECT_TRUE(aubCsr->expectMemoryCompressedCalled);
}

HWTEST_F(AubFileStreamTests, givenAubCommandStreamReceiverWhenFlushIsCalledThenItShouldCallTheExpectedHwContextFunctions) {
    MockAubCsr<FamilyType> aubCsr("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor());
    aubCsr.setupContext(osContext);
    auto mockHardwareContext = static_cast<MockHardwareContext *>(aubCsr.hardwareContextController->hardwareContexts[0].get());

    auto commandBuffer = pDevice->executionEnvironment->memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{pDevice->getRootDeviceIndex(), MemoryConstants::pageSize});

    aubCsr.initializeTagAllocation();

    LinearStream cs(commandBuffer);
    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());
    batchBuffer.startOffset = 1;

    ResidencyContainer allocationsForResidency;

    aubCsr.flush(batchBuffer, allocationsForResidency);

    EXPECT_TRUE(mockHardwareContext->initializeCalled);
    EXPECT_TRUE(mockHardwareContext->submitCalled);
    EXPECT_FALSE(mockHardwareContext->writeAndSubmitCalled);
    EXPECT_FALSE(mockHardwareContext->pollForCompletionCalled);

    EXPECT_TRUE(aubCsr.writeMemoryWithAubManagerCalled);
    pDevice->executionEnvironment->memoryManager->freeGraphicsMemory(commandBuffer);
}

HWTEST_F(AubFileStreamTests, givenAubCommandStreamReceiverWhenFlushIsCalledWithZeroSizedBufferThenSubmitIsNotCalledOnHwContext) {
    MockAubCsr<FamilyType> aubCsr("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor());
    aubCsr.setupContext(osContext);
    auto mockHardwareContext = static_cast<MockHardwareContext *>(aubCsr.hardwareContextController->hardwareContexts[0].get());

    auto commandBuffer = pDevice->executionEnvironment->memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{pDevice->getRootDeviceIndex(), MemoryConstants::pageSize});

    aubCsr.initializeTagAllocation();

    LinearStream cs(commandBuffer);
    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());
    ResidencyContainer allocationsForResidency;

    aubCsr.flush(batchBuffer, allocationsForResidency);

    EXPECT_FALSE(mockHardwareContext->writeAndSubmitCalled);
    EXPECT_FALSE(mockHardwareContext->submitCalled);
    pDevice->executionEnvironment->memoryManager->freeGraphicsMemory(commandBuffer);
}

HWTEST_F(AubFileStreamTests, givenAubCommandStreamReceiverWhenMakeResidentIsCalledThenItShouldCallTheExpectedHwContextFunctions) {
    MockAubCsr<FamilyType> aubCsr("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor());
    aubCsr.setupContext(osContext);
    aubCsr.initializeEngine();

    MockGraphicsAllocation allocation(reinterpret_cast<void *>(0x1000), 0x1000);
    ResidencyContainer allocationsForResidency = {&allocation};
    aubCsr.processResidency(allocationsForResidency, 0u);

    EXPECT_TRUE(aubCsr.writeMemoryWithAubManagerCalled);
}

HWTEST_F(AubFileStreamTests, givenAubCommandStreamReceiverWhenExpectMemoryEqualIsCalledThenItShouldCallTheExpectedHwContextFunctions) {
    MockAubCsr<FamilyType> aubCsr("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor());
    aubCsr.setupContext(osContext);
    auto mockHardwareContext = static_cast<MockHardwareContext *>(aubCsr.hardwareContextController->hardwareContexts[0].get());

    MockGraphicsAllocation allocation(reinterpret_cast<void *>(0x1000), 0x1000);
    aubCsr.expectMemoryEqual(reinterpret_cast<void *>(0x1000), reinterpret_cast<void *>(0x1000), 0x1000);

    EXPECT_TRUE(mockHardwareContext->expectMemoryCalled);
}

HWTEST_F(AubFileStreamTests, givenAubCommandStreamReceiverWhenExpectMemoryNotEqualIsCalledThenItShouldCallTheExpectedHwContextFunctions) {
    MockAubCsr<FamilyType> aubCsr("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor());
    aubCsr.setupContext(osContext);
    auto mockHardwareContext = static_cast<MockHardwareContext *>(aubCsr.hardwareContextController->hardwareContexts[0].get());

    MockGraphicsAllocation allocation(reinterpret_cast<void *>(0x1000), 0x1000);
    aubCsr.expectMemoryNotEqual(reinterpret_cast<void *>(0x1000), reinterpret_cast<void *>(0x1000), 0x1000);

    EXPECT_TRUE(mockHardwareContext->expectMemoryCalled);
}

HWTEST_F(AubFileStreamTests, givenAubCommandStreamReceiverWithAubManagerWhenInitFileIsCalledThenMemTraceCommentWithDriverVersionIsPutIntoAubStream) {
    DebugManagerStateRestore stateRestore;
    auto mockAubManager = std::make_unique<MockAubManager>();

    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    executionEnvironment.initializeMemoryManager();
    DeviceBitfield deviceBitfield(1);

    AUBCommandStreamReceiverHw<FamilyType> aubCsr("", true, executionEnvironment, 0, deviceBitfield);

    aubCsr.aubManager = mockAubManager.get();
    ASSERT_NE(nullptr, aubCsr.aubManager);

    std::string fileName = "file_name.aub";
    aubCsr.initFile(fileName);

    std::string commentWithDriverVersion = "driver version: " + std::string(driverVersion);
    EXPECT_EQ(mockAubManager->receivedComments, commentWithDriverVersion);

    aubCsr.aubManager = nullptr;
}

HWTEST2_F(AubFileStreamTests, givenAubCommandStreamReceiverWhenCreateFullFilePathIsCalledForMultipleDevicesThenFileNameIsExtendedWithSuffixToIndicateMultipleDevices, IsAtMostXeCore) {
    DebugManagerStateRestore stateRestore;

    debugManager.flags.CreateMultipleSubDevices.set(1);
    auto fullName = AUBCommandStreamReceiver::createFullFilePath(*defaultHwInfo, "aubfile", mockRootDeviceIndex);
    EXPECT_EQ(std::string::npos, fullName.find("tx"));

    debugManager.flags.CreateMultipleSubDevices.set(2);
    fullName = AUBCommandStreamReceiver::createFullFilePath(*defaultHwInfo, "aubfile", mockRootDeviceIndex);
    EXPECT_NE(std::string::npos, fullName.find("2tx"));
}

HWTEST_F(AubFileStreamTests, givenDisabledGeneratingPerProcessAubNameAndAubCommandStreamReceiverWhenCreateFullFilePathIsCalledThenFileNameIsExtendedWithRootDeviceIndex) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.GenerateAubFilePerProcessId.set(0);

    uint32_t rootDeviceIndex = 123u;
    auto fullName = AUBCommandStreamReceiver::createFullFilePath(*defaultHwInfo, "aubfile", rootDeviceIndex);
    EXPECT_NE(std::string::npos, fullName.find("_123_aubfile.aub"));
}

HWTEST_F(AubFileStreamTests, givenEnabledGeneratingAubFilePerProcessIdDebugFlagAndAubCommandStreamReceiverWhenCreateFullFilePathIsCalledThenFileNameIsExtendedRootDeviceIndex) {
    DebugManagerStateRestore stateRestore;

    debugManager.flags.GenerateAubFilePerProcessId.set(1);
    auto fullName = AUBCommandStreamReceiver::createFullFilePath(*defaultHwInfo, "aubfile", 1u);
    std::stringstream strExtendedFileName;
    strExtendedFileName << "_1_aubfile_PID_" << SysCalls::getProcessId() << ".aub";
    EXPECT_NE(std::string::npos, fullName.find(strExtendedFileName.str()));
}

HWTEST_F(AubFileStreamTests, givenAUBDumpCaptureDirPathAndGeneratingAubFilePerProcessIdWhenCreateFullFilePathIsCalledThenFileNameIsExtendedRootDeviceIndex) {
    DebugManagerStateRestore stateRestore;

    std::string expectedDirPath = "/tmp/path";
    debugManager.flags.AUBDumpCaptureDirPath.set(expectedDirPath);
    debugManager.flags.GenerateAubFilePerProcessId.set(1);
    auto fullName = AUBCommandStreamReceiver::createFullFilePath(*defaultHwInfo, "aubfile", 1u);

    std::stringstream expectedPerProcessPart;
    expectedPerProcessPart << "_1_aubfile_PID_" << SysCalls::getProcessId() << ".aub";

    EXPECT_NE(std::string::npos, fullName.find(expectedDirPath));
    EXPECT_NE(std::string::npos, fullName.find(expectedPerProcessPart.str()));
}

HWTEST_F(AubFileStreamTests, givenAndAubCommandStreamReceiverWhenCreateFullFilePathIsCalledThenFileNameIsExtendedRootDeviceIndexAndPid) {
    auto fullName = AUBCommandStreamReceiver::createFullFilePath(*defaultHwInfo, "aubfile", 1u);
    std::stringstream strExtendedFileName;
    strExtendedFileName << "_1_aubfile_PID_" << SysCalls::getProcessId() << ".aub";
    EXPECT_NE(std::string::npos, fullName.find(strExtendedFileName.str()));
}

HWTEST_F(AubFileStreamTests, givenAubCommandStreamReceiverWithAubManagerWhenInitFileIsCalledThenCommentWithNonDefaultFlagsAreAdded) {
    DebugManagerStateRestore stateRestore;

    debugManager.flags.MakeAllBuffersResident.set(1);
    debugManager.flags.ZE_AFFINITY_MASK.set("non-default");

    auto mockAubManager = std::make_unique<MockAubManager>();
    auto aubExecutionEnvironment = getEnvironment<AUBCommandStreamReceiverHw<FamilyType>>(false, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<AUBCommandStreamReceiverHw<FamilyType>>();

    aubCsr->aubManager = mockAubManager.get();

    std::string fileName = "file_name.aub";
    aubCsr->initFile(fileName);

    std::string expectedAddedComments = std::string("driver version: ") + std::string(driverVersion) +
                                        std::string("Non-default value of debug variable: MakeAllBuffersResident = 1") +
                                        std::string("Non-default value of debug variable: ZE_AFFINITY_MASK = non-default");

    EXPECT_EQ(expectedAddedComments, mockAubManager->receivedComments);
}