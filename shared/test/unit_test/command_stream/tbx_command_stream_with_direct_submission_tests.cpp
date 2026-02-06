/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver_with_aub_dump.h"
#include "shared/source/command_stream/command_stream_receiver_with_aub_dump.inl"
#include "shared/source/helpers/hardware_context_controller.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/fixtures/mock_aub_center_fixture.h"
#include "shared/test/common/fixtures/tbx_command_stream_fixture.h"
#include "shared/test/common/helpers/batch_buffer_helper.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
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

#include <cstdint>

using namespace NEO;

struct TbxDirectSubmissionFixture : public DeviceFixture {
    TbxDirectSubmissionFixture() {}

    template <typename FamilyType>
    void setUpT() {
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
                                                                                                       EngineDescriptorHelper::getDefaultDescriptor({getChosenEngineType(*defaultHwInfo), EngineUsage::regular},
                                                                                                                                                    PreemptionHelper::getDefaultPreemptionMode(*defaultHwInfo)));
        osContext->setDefaultContext(true);
        commandStreamReceiver->setupContext(*osContext);
        commandStreamReceiver->initializeTagAllocation();
        commandStreamReceiver->createGlobalFenceAllocation();

        commandStreamReceiver->initDirectSubmission();
        ASSERT_TRUE(osContext->isDirectSubmissionActive());
        ASSERT_TRUE(commandStreamReceiver->isDirectSubmissionEnabled());
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

using TbxCommandStreamDirectSubmissionTests = Test<TbxDirectSubmissionFixture>;

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
