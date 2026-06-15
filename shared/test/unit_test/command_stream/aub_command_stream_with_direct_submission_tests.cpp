/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hardware_context_controller.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/batch_buffer_helper.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/mocks/mock_aub_center.h"
#include "shared/test/common/mocks/mock_aub_csr.h"
#include "shared/test/common/mocks/mock_aub_manager.h"
#include "shared/test/common/mocks/mock_aub_memory_operations_handler.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/hw_test.h"

#include <cstdint>

using namespace NEO;

struct AubDirectSubmissionFixture : public DeviceFixture {
    AubDirectSubmissionFixture() {}

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

        // standalone == true mirrors SetCommandStreamReceiver=1 (pure AUB capture, getType() == aub)
        commandStreamReceiver = std::make_unique<MockAubCsr<FamilyType>>(fileName, true, *pDevice->getExecutionEnvironment(),
                                                                         pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());

        pDevice->getExecutionEnvironment()->memoryManager->reInitLatestContextId();
        auto osContext = pDevice->getExecutionEnvironment()->memoryManager->createAndRegisterOsContext(commandStreamReceiver.get(),
                                                                                                       EngineDescriptorHelper::getDefaultDescriptor({getChosenEngineType(*defaultHwInfo), EngineUsage::regular},
                                                                                                                                                    PreemptionHelper::getDefaultPreemptionMode(*defaultHwInfo)));
        osContext->setDefaultContext(true);
        commandStreamReceiver->setupContext(*osContext);
        commandStreamReceiver->initializeTagAllocation();
        commandStreamReceiver->createGlobalFenceAllocation();
        commandStreamReceiver->initializeEngine();

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
    MockAubCsr<FamilyType> *getCsr() {
        return static_cast<MockAubCsr<FamilyType> *>(commandStreamReceiver.get());
    }
    void setUp() {}
    void tearDown() {}

    DebugManagerStateRestore debugManagerStateRestore;
    MockAubManager *mockAubManager = nullptr;
    MockAubMemoryOperationsHandler *memoryOperationsHandler = nullptr;
    std::unique_ptr<CommandStreamReceiver> commandStreamReceiver;
};

using AubCommandStreamDirectSubmissionTests = Test<AubDirectSubmissionFixture>;

HWTEST_TEMPLATED_F(AubCommandStreamDirectSubmissionTests, givenNotStartedDirectSubmissionWhenFlushIsCalledThenSubmitIsCalledOnHwContext) {
    auto *aubCsr = getCsr<FamilyType>();

    auto mockHardwareContext = static_cast<MockHardwareContext *>(aubCsr->hardwareContextController->hardwareContexts[0].get());

    auto commandBuffer = pDevice->executionEnvironment->memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{pDevice->getRootDeviceIndex(), MemoryConstants::pageSize, pDevice->getDeviceBitfield()});

    LinearStream cs(commandBuffer);
    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());
    batchBuffer.endCmdPtr = cs.getCpuBase();
    ResidencyContainer allocationsForResidency;

    aubCsr->flush(batchBuffer, allocationsForResidency);

    EXPECT_TRUE(mockHardwareContext->submitCalled);
    pDevice->executionEnvironment->memoryManager->freeGraphicsMemory(commandBuffer);
}

HWTEST_TEMPLATED_F(AubCommandStreamDirectSubmissionTests, givenStartedDirectSubmissionInAubModeWhenFlushIsCalledOnAppendThenSubmitIsCalledOnHwContext) {
    auto *aubCsr = getCsr<FamilyType>();

    auto mockHardwareContext = static_cast<MockHardwareContext *>(aubCsr->hardwareContextController->hardwareContexts[0].get());

    auto commandBuffer = pDevice->executionEnvironment->memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{pDevice->getRootDeviceIndex(), MemoryConstants::pageSize, pDevice->getDeviceBitfield()});

    LinearStream cs(commandBuffer);
    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());
    batchBuffer.endCmdPtr = cs.getCpuBase();
    ResidencyContainer allocationsForResidency;

    aubCsr->flush(batchBuffer, allocationsForResidency);

    EXPECT_TRUE(mockHardwareContext->submitCalled);
    mockHardwareContext->submitCalled = false;
    allocationsForResidency.clear();
    cs.getSpace(256);
    batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());
    batchBuffer.endCmdPtr = ptrOffset(cs.getCpuBase(), 128);

    aubCsr->flush(batchBuffer, allocationsForResidency);

    // Unlike live TBX, pure AUB must record execution of each appended ring section.
    EXPECT_TRUE(mockHardwareContext->submitCalled);

    pDevice->executionEnvironment->memoryManager->freeGraphicsMemory(commandBuffer);
}
