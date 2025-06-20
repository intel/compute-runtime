/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub_mem_dump/page_table_entry_bits.h"
#include "shared/source/command_stream/aub_command_stream_receiver.h"
#include "shared/source/helpers/blit_properties.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hardware_context_controller.h"
#include "shared/source/memory_manager/os_agnostic_memory_manager.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/test/common/fixtures/aub_command_stream_receiver_fixture.h"
#include "shared/test/common/fixtures/mock_aub_center_fixture.h"
#include "shared/test/common/helpers/batch_buffer_helper.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/libult/ult_aub_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_aub_center.h"
#include "shared/test/common/mocks/mock_aub_csr.h"
#include "shared/test/common/mocks/mock_aub_manager.h"
#include "shared/test/common/mocks/mock_aub_memory_operations_handler.h"
#include "shared/test/common/mocks/mock_aub_subcapture_manager.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_gmm.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_os_context.h"
#include "shared/test/common/test_macros/hw_test.h"

using namespace NEO;

using AubCommandStreamReceiverTests = Test<AubCommandStreamReceiverFixture>;
using AubCommandStreamReceiverWithoutAubStreamTests = Test<AubCommandStreamReceiverWithoutAubStreamFixture>;
using AubCsrTests = Test<AubCommandStreamReceiverFixture>;

template <typename GfxFamily>
struct MockAubCsrToTestDumpAubNonWritable : public AUBCommandStreamReceiverHw<GfxFamily> {
    using AUBCommandStreamReceiverHw<GfxFamily>::AUBCommandStreamReceiverHw;
    using AUBCommandStreamReceiverHw<GfxFamily>::dumpAubNonWritable;

    bool writeMemory(GraphicsAllocation &gfxAllocation, bool isChunkCopy, uint64_t gpuVaChunkOffset, size_t chunkSize) override {
        return true;
    }
};

TEST_F(AubCommandStreamReceiverTests, givenStructureWhenMisalignedUint64ThenUseSetterGetterFunctionsToSetGetValue) {
    const uint64_t value = 0x0123456789ABCDEFu;
    AubMemDump::AubCaptureBinaryDumpHD aubCaptureBinaryDumpHD{};
    aubCaptureBinaryDumpHD.setBaseAddr(value);
    EXPECT_EQ(value, aubCaptureBinaryDumpHD.getBaseAddr());
    aubCaptureBinaryDumpHD.setWidth(value);
    EXPECT_EQ(value, aubCaptureBinaryDumpHD.getWidth());
    aubCaptureBinaryDumpHD.setHeight(value);
    EXPECT_EQ(value, aubCaptureBinaryDumpHD.getHeight());
    aubCaptureBinaryDumpHD.setPitch(value);
    EXPECT_EQ(value, aubCaptureBinaryDumpHD.getPitch());

    AubMemDump::AubCmdDumpBmpHd aubCmdDumpBmpHd{};
    aubCmdDumpBmpHd.setBaseAddr(value);
    EXPECT_EQ(value, aubCmdDumpBmpHd.getBaseAddr());

    AubMemDump::CmdServicesMemTraceDumpCompress cmdServicesMemTraceDumpCompress{};
    cmdServicesMemTraceDumpCompress.setSurfaceAddress(value);
    EXPECT_EQ(value, cmdServicesMemTraceDumpCompress.getSurfaceAddress());
}

HWTEST_F(AubCommandStreamReceiverTests, givenDebugFlagSetWhenCreatingContextThenAppendFlags) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.AppendAubStreamContextFlags.set(0x123);

    std::unique_ptr<MemoryManager> memoryManager(nullptr);
    MockAubManager mockAubManager;
    auto aubCsr = std::make_unique<AUBCommandStreamReceiverHw<FamilyType>>("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    memoryManager.reset(new OsAgnosticMemoryManager(*pDevice->executionEnvironment));
    aubCsr->aubManager = &mockAubManager;

    aubCsr->setupContext(*pDevice->getDefaultEngine().osContext);
    EXPECT_EQ(0x123u, mockAubManager.contextFlags & 0x123);
}

TEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenItIsCreatedWithWrongGfxCoreFamilyThenNullPointerShouldBeReturned) {
    HardwareInfo *hwInfo = pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();

    hwInfo->platform.eRenderCoreFamily = GFXCORE_FAMILY_FORCE_ULONG; // wrong gfx core family

    CommandStreamReceiver *aubCsr = AUBCommandStreamReceiver::create("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    EXPECT_EQ(nullptr, aubCsr);
}

TEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenTypeIsCheckedThenAubCsrIsReturned) {
    std::unique_ptr<CommandStreamReceiver> aubCsr(AUBCommandStreamReceiver::create("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield()));
    EXPECT_NE(nullptr, aubCsr);
    EXPECT_EQ(CommandStreamReceiverType::aub, aubCsr->getType());
}

HWTEST_F(AubCommandStreamReceiverWithoutAubStreamTests, givenAubCommandStreamReceiverWhenItIsCreatedThenAubManagerAndHardwareContextAreNull) {

    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get(), false, 1);
    executionEnvironment.initGmm();
    executionEnvironment.initializeMemoryManager();

    executionEnvironment.rootDeviceEnvironments[0]->initAubCenter(false, "", CommandStreamReceiverType::aub);
    executionEnvironment.rootDeviceEnvironments[0]->aubCenter->getAubManager();

    DeviceBitfield deviceBitfield(1);
    auto aubCsr = std::make_unique<AUBCommandStreamReceiverHw<FamilyType>>("", true, executionEnvironment, 0, deviceBitfield);

    ASSERT_NE(nullptr, aubCsr);
    EXPECT_EQ(nullptr, aubCsr->aubManager);
    EXPECT_EQ(nullptr, aubCsr->hardwareContextController);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCsrWhenItIsCreatedWithDefaultSettingsThenItHasImmediateDispatchModeEnabled) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.CsrDispatchMode.set(0);
    std::unique_ptr<MockAubCsr<FamilyType>> aubCsr(new MockAubCsr<FamilyType>("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield()));
    EXPECT_EQ(DispatchMode::immediateDispatch, aubCsr->peekDispatchMode());
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCsrWhenItIsCreatedWithDebugSettingsThenItHasProperDispatchModeEnabled) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.CsrDispatchMode.set(static_cast<uint32_t>(DispatchMode::batchedDispatch));
    std::unique_ptr<MockAubCsr<FamilyType>> aubCsr(new MockAubCsr<FamilyType>("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield()));
    EXPECT_EQ(DispatchMode::batchedDispatch, aubCsr->peekDispatchMode());
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenItIsCreatedThenMemoryManagerIsNotNull) {
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield()));
    std::unique_ptr<MemoryManager> memoryManager(new OsAgnosticMemoryManager(*pDevice->executionEnvironment));
    EXPECT_NE(nullptr, memoryManager.get());
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenMultipleInstancesAreCreatedThenTheyUseTheSameSubCaptureCommon) {
    auto aubCsr1 = std::make_unique<AUBCommandStreamReceiverHw<FamilyType>>("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    auto subCaptureCommon1 = pDevice->executionEnvironment->rootDeviceEnvironments[0]->aubCenter->getSubCaptureCommon();
    EXPECT_NE(nullptr, subCaptureCommon1);
    auto aubCsr2 = std::make_unique<AUBCommandStreamReceiverHw<FamilyType>>("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    auto subCaptureCommon2 = pDevice->executionEnvironment->rootDeviceEnvironments[0]->aubCenter->getSubCaptureCommon();
    EXPECT_NE(nullptr, subCaptureCommon2);
    EXPECT_EQ(subCaptureCommon1, subCaptureCommon2);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWithAubManagerWhenItIsCreatedThenFileIsCreated) {
    std::string fileName = "file_name.aub";
    MockAubManager *mockManager = new MockAubManager();
    MockAubCenter *mockAubCenter = new MockAubCenter(pDevice->getRootDeviceEnvironment(), false, fileName, CommandStreamReceiverType::aub);
    mockAubCenter->aubManager = std::unique_ptr<MockAubManager>(mockManager);
    pDevice->executionEnvironment->rootDeviceEnvironments[0]->aubCenter = std::unique_ptr<MockAubCenter>(mockAubCenter);

    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(static_cast<AUBCommandStreamReceiverHw<FamilyType> *>(AUBCommandStreamReceiver::create(fileName, true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield())));
    ASSERT_NE(nullptr, aubCsr);

    EXPECT_TRUE(aubCsr->isFileOpen());
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCsrWhenOsContextIsSetThenCreateHardwareContext) {
    uint32_t deviceIndex = 3;

    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS, EngineUsage::regular}, DeviceBitfield(8)));
    std::string fileName = "file_name.aub";
    MockAubManager *mockManager = new MockAubManager();
    MockAubCenter *mockAubCenter = new MockAubCenter(pDevice->getRootDeviceEnvironment(), false, fileName, CommandStreamReceiverType::aub);
    mockAubCenter->aubManager = std::unique_ptr<MockAubManager>(mockManager);
    pDevice->executionEnvironment->rootDeviceEnvironments[0]->aubCenter = std::unique_ptr<MockAubCenter>(mockAubCenter);

    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(static_cast<AUBCommandStreamReceiverHw<FamilyType> *>(AUBCommandStreamReceiver::create(fileName, true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield())));
    EXPECT_EQ(nullptr, aubCsr->hardwareContextController.get());

    aubCsr->setupContext(osContext);
    EXPECT_NE(nullptr, aubCsr->hardwareContextController.get());
    auto mockHardwareContext = static_cast<MockHardwareContext *>(aubCsr->hardwareContextController->hardwareContexts[0].get());
    EXPECT_EQ(deviceIndex, mockHardwareContext->deviceIndex);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCsrAndHighPriorityContextWhenOsContextIsSetThenCorrectFlagIsPassed) {
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::highPriority}, DeviceBitfield(1)));
    std::string fileName = "file_name.aub";
    MockAubManager *mockManager = new MockAubManager();
    MockAubCenter *mockAubCenter = new MockAubCenter(pDevice->getRootDeviceEnvironment(), false, fileName, CommandStreamReceiverType::aub);
    mockAubCenter->aubManager = std::unique_ptr<MockAubManager>(mockManager);
    pDevice->executionEnvironment->rootDeviceEnvironments[0]->aubCenter = std::unique_ptr<MockAubCenter>(mockAubCenter);

    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(static_cast<AUBCommandStreamReceiverHw<FamilyType> *>(AUBCommandStreamReceiver::create(fileName, true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield())));
    EXPECT_EQ(nullptr, aubCsr->hardwareContextController.get());

    aubCsr->setupContext(osContext);
    EXPECT_TRUE(aub_stream::hardwareContextFlags::highPriority & mockManager->contextFlags);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCsrWhenLowPriorityOsContextIsSetThenCreateHardwareContext) {
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::lowPriority}));
    std::string fileName = "file_name.aub";
    MockAubManager *mockManager = new MockAubManager();
    MockAubCenter *mockAubCenter = new MockAubCenter(pDevice->getRootDeviceEnvironment(), false, fileName, CommandStreamReceiverType::aub);
    mockAubCenter->aubManager = std::unique_ptr<MockAubManager>(mockManager);
    pDevice->executionEnvironment->rootDeviceEnvironments[0]->aubCenter = std::unique_ptr<MockAubCenter>(mockAubCenter);

    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(static_cast<AUBCommandStreamReceiverHw<FamilyType> *>(AUBCommandStreamReceiver::create(fileName, true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield())));
    EXPECT_EQ(nullptr, aubCsr->hardwareContextController.get());

    aubCsr->setupContext(osContext);
    EXPECT_NE(nullptr, aubCsr->hardwareContextController.get());
    EXPECT_TRUE(aub_stream::hardwareContextFlags::lowPriority & mockManager->contextFlags);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverInSubCaptureModeWhenItIsCreatedThenFileIsNotCreated) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.AUBDumpSubCaptureMode.set(static_cast<int32_t>(AubSubCaptureManager::SubCaptureMode::filter));
    std::string fileName = "file_name.aub";
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    executionEnvironment.initializeMemoryManager();
    DeviceBitfield deviceBitfield(1);
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(static_cast<AUBCommandStreamReceiverHw<FamilyType> *>(AUBCommandStreamReceiver::create(fileName, true, executionEnvironment, 0, deviceBitfield)));
    EXPECT_NE(nullptr, aubCsr);
    EXPECT_FALSE(aubCsr->isFileOpen());
}

HWTEST_F(AubCommandStreamReceiverTests, givenGraphicsAllocationWhenMakeResidentCalledMultipleTimesThenAffectsResidencyOnce) {
    std::unique_ptr<MemoryManager> memoryManager(nullptr);
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield()));
    memoryManager.reset(new OsAgnosticMemoryManager(*pDevice->executionEnvironment));
    aubCsr->setupContext(*pDevice->getDefaultEngine().osContext);
    auto gfxAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{pDevice->getRootDeviceIndex(), MemoryConstants::pageSize});

    auto osContextId = aubCsr->getOsContext().getContextId();

    // First makeResident marks the allocation resident
    aubCsr->makeResident(*gfxAllocation);
    EXPECT_TRUE(gfxAllocation->isResident(osContextId));
    EXPECT_EQ(aubCsr->peekTaskCount() + 1, gfxAllocation->getTaskCount(osContextId));
    EXPECT_EQ(aubCsr->peekTaskCount() + 1, gfxAllocation->getResidencyTaskCount(osContextId));
    EXPECT_EQ(1u, aubCsr->getResidencyAllocations().size());

    // Second makeResident should have no impact
    aubCsr->makeResident(*gfxAllocation);
    EXPECT_TRUE(gfxAllocation->isResident(osContextId));
    EXPECT_EQ(aubCsr->peekTaskCount() + 1, gfxAllocation->getTaskCount(osContextId));
    EXPECT_EQ(aubCsr->peekTaskCount() + 1, gfxAllocation->getResidencyTaskCount(osContextId));
    EXPECT_EQ(1u, aubCsr->getResidencyAllocations().size());

    // First makeNonResident marks the allocation as nonresident
    aubCsr->makeNonResident(*gfxAllocation);
    EXPECT_FALSE(gfxAllocation->isResident(osContextId));
    EXPECT_EQ(1u, aubCsr->getEvictionAllocations().size());

    // Second makeNonResident should have no impact
    aubCsr->makeNonResident(*gfxAllocation);
    EXPECT_FALSE(gfxAllocation->isResident(osContextId));
    EXPECT_EQ(1u, aubCsr->getEvictionAllocations().size());

    memoryManager->freeGraphicsMemory(gfxAllocation);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverInSubCaptureModeWhenProcessResidencyIsCalledButSubCaptureIsDisabledThenItShouldntWriteMemory) {
    AubSubCaptureCommon aubSubCaptureCommon;
    auto aubSubCaptureManagerMock = new AubSubCaptureManagerMock("", aubSubCaptureCommon);
    auto aubExecutionEnvironment = getEnvironment<MockAubCsr<FamilyType>>(true, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<MockAubCsr<FamilyType>>();

    aubSubCaptureCommon.subCaptureMode = AubSubCaptureManager::SubCaptureMode::toggle;
    aubSubCaptureManagerMock->disableSubCapture();
    aubCsr->subCaptureManager = std::unique_ptr<AubSubCaptureManagerMock>(aubSubCaptureManagerMock);
    ASSERT_FALSE(aubCsr->subCaptureManager->isSubCaptureEnabled());

    MockGraphicsAllocation allocation(reinterpret_cast<void *>(0x1000), 0x1000);
    ResidencyContainer allocationsForResidency = {&allocation};
    aubCsr->processResidency(allocationsForResidency, 0u);

    EXPECT_FALSE(aubCsr->writeMemoryCalled);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCsrAndResidentAllocationWhenProcessResidencyIsCalledThenWriteMemoryIsCalledOnResidentAllocations) {

    auto mockManager = new MockAubManager();
    auto mockAubCenter = new MockAubCenter(*pDevice->getExecutionEnvironment()->rootDeviceEnvironments[0], false, "aubfile", CommandStreamReceiverType::aub);
    mockAubCenter->aubManager.reset(mockManager);
    pDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->aubCenter.reset(mockAubCenter);

    auto memoryOperationsHandler = new NEO::MockAubMemoryOperationsHandler(mockManager);
    pDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->memoryOperationsInterface.reset(memoryOperationsHandler);

    auto commandStreamReceiver = std::make_unique<MockAubCsr<FamilyType>>("", true, *pDevice->getExecutionEnvironment(), 0, pDevice->getDeviceBitfield());

    pDevice->getExecutionEnvironment()->memoryManager->reInitLatestContextId();
    auto osContext = pDevice->getExecutionEnvironment()->memoryManager->createAndRegisterOsContext(commandStreamReceiver.get(),
                                                                                                   EngineDescriptorHelper::getDefaultDescriptor({getChosenEngineType(*defaultHwInfo), EngineUsage::regular},
                                                                                                                                                PreemptionHelper::getDefaultPreemptionMode(*defaultHwInfo)));
    commandStreamReceiver->setupContext(*osContext);
    commandStreamReceiver->initializeTagAllocation();
    commandStreamReceiver->createGlobalFenceAllocation();

    MockGraphicsAllocation allocation(reinterpret_cast<void *>(0x1000), 0x1000);
    ResidencyContainer allocationsForResidency = {&allocation};

    MockGraphicsAllocation allocation2(reinterpret_cast<void *>(0x5000), 0x5000, 0x1000);
    GraphicsAllocation *allocPtr = &allocation2;
    memoryOperationsHandler->makeResident(pDevice, ArrayRef<GraphicsAllocation *>(&allocPtr, 1), false, false);
    EXPECT_TRUE(mockManager->writeMemory2Called);

    mockManager->storeAllocationParams = true;
    commandStreamReceiver->writeMemoryGfxAllocCalled = false;
    mockManager->writeMemory2Called = false;

    commandStreamReceiver->processResidency(allocationsForResidency, 0u);
    EXPECT_TRUE(mockManager->writeMemory2Called);
    EXPECT_TRUE(commandStreamReceiver->writeMemoryGfxAllocCalled);
    ASSERT_EQ(2u, mockManager->storedAllocationParams.size());
    EXPECT_EQ(allocation2.getGpuAddress(), mockManager->storedAllocationParams[1].gfxAddress);
    ASSERT_EQ(1u, memoryOperationsHandler->residentAllocations.size());
    EXPECT_EQ(&allocation2, memoryOperationsHandler->residentAllocations[0]);
}

HWTEST_F(AubCommandStreamReceiverWithoutAubStreamTests, givenAubCommandStreamReceiverInSubCaptureModeWhenProcessResidencyIsCalledButAllocationSizeIsZeroThenItShouldntWriteMemory) {

    AubSubCaptureCommon aubSubCaptureCommon;
    auto aubSubCaptureManagerMock = new AubSubCaptureManagerMock("", aubSubCaptureCommon);
    auto aubExecutionEnvironment = getEnvironment<MockAubCsr<FamilyType>>(true, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<MockAubCsr<FamilyType>>();
    aubCsr->initializeEngine();

    aubSubCaptureCommon.subCaptureMode = AubSubCaptureManager::SubCaptureMode::toggle;
    aubSubCaptureManagerMock->setSubCaptureToggleActive(true);
    aubSubCaptureManagerMock->checkAndActivateSubCapture("kernelName");
    aubCsr->subCaptureManager = std::unique_ptr<AubSubCaptureManagerMock>(aubSubCaptureManagerMock);
    ASSERT_TRUE(aubCsr->subCaptureManager->isSubCaptureEnabled());

    MockGraphicsAllocation allocation(reinterpret_cast<void *>(0x1000), 0);
    ResidencyContainer allocationsForResidency = {&allocation};
    aubCsr->processResidency(allocationsForResidency, 0u);

    EXPECT_FALSE(aubCsr->writeMemoryCalled);
}

HWTEST_F(AubCommandStreamReceiverWithoutAubStreamTests, givenAubCommandStreamReceiverInSubCaptureModeWhenFlushIsCalledButSubCaptureIsDisabledThenItShouldntInitializeEngineInfo) {

    AubSubCaptureCommon aubSubCaptureCommon;
    auto aubSubCaptureManagerMock = new AubSubCaptureManagerMock("", aubSubCaptureCommon);
    auto aubExecutionEnvironment = getEnvironment<AUBCommandStreamReceiverHw<FamilyType>>(true, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<AUBCommandStreamReceiverHw<FamilyType>>();
    LinearStream cs(aubExecutionEnvironment->commandBuffer);

    aubSubCaptureCommon.subCaptureMode = AubSubCaptureManager::SubCaptureMode::toggle;
    aubSubCaptureManagerMock->disableSubCapture();
    aubCsr->subCaptureManager = std::unique_ptr<AubSubCaptureManagerMock>(aubSubCaptureManagerMock);
    ASSERT_FALSE(aubCsr->subCaptureManager->isSubCaptureEnabled());

    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());
    ResidencyContainer allocationsForResidency = {};
    aubCsr->flush(batchBuffer, allocationsForResidency);
    EXPECT_EQ(nullptr, aubCsr->engineInfo.pLRCA);
    EXPECT_EQ(nullptr, aubCsr->engineInfo.pGlobalHWStatusPage);
    EXPECT_EQ(nullptr, aubCsr->engineInfo.pRingBuffer);
}

HWTEST_F(AubCommandStreamReceiverWithoutAubStreamTests, givenAubCommandStreamReceiverWhenFlushIsCalledThenItShouldLeaveProperRingTailAlignment) {

    auto aubExecutionEnvironment = getEnvironment<AUBCommandStreamReceiverHw<FamilyType>>(true, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<AUBCommandStreamReceiverHw<FamilyType>>();
    auto allocationsForResidency = aubCsr->getResidencyAllocations();
    LinearStream cs(aubExecutionEnvironment->commandBuffer);

    auto ringTailAlignment = sizeof(uint64_t);

    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());
    // First flush typically includes a preamble and chain to command buffer
    aubCsr->overrideDispatchPolicy(DispatchMode::immediateDispatch);
    aubCsr->flush(batchBuffer, allocationsForResidency);
    EXPECT_EQ(0ull, aubCsr->engineInfo.tailRingBuffer % ringTailAlignment);

    // Second flush should just submit command buffer
    cs.getSpace(sizeof(uint64_t));
    aubCsr->flush(batchBuffer, allocationsForResidency);
    EXPECT_EQ(0ull, aubCsr->engineInfo.tailRingBuffer % ringTailAlignment);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverInNonStandaloneModeWhenFlushIsCalledThenItShouldNotUpdateHwTagWithLatestSentTaskCount) {
    auto aubExecutionEnvironment = getEnvironment<MockAubCsr<FamilyType>>(true, true, false);
    auto aubCsr = aubExecutionEnvironment->template getCsr<MockAubCsr<FamilyType>>();
    LinearStream cs(aubExecutionEnvironment->commandBuffer);

    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());
    ResidencyContainer allocationsForResidency = {};

    EXPECT_EQ(initialHardwareTag, *aubCsr->getTagAddress());

    aubCsr->setLatestSentTaskCount(aubCsr->peekTaskCount() + 1);

    EXPECT_NE(aubCsr->peekLatestSentTaskCount(), *aubCsr->getTagAddress());

    aubCsr->flush(batchBuffer, allocationsForResidency);

    EXPECT_NE(aubCsr->peekLatestSentTaskCount(), *aubCsr->getTagAddress());
    EXPECT_EQ(initialHardwareTag, *aubCsr->getTagAddress());
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverInStandaloneModeWhenFlushIsCalledThenItShouldUpdateHwTagWithLatestSentTaskCount) {
    auto aubExecutionEnvironment = getEnvironment<MockAubCsr<FamilyType>>(true, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<MockAubCsr<FamilyType>>();
    LinearStream cs(aubExecutionEnvironment->commandBuffer);

    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());

    ResidencyContainer allocationsForResidency = {};

    EXPECT_EQ(initialHardwareTag, *aubCsr->getTagAddress());

    aubCsr->setLatestSentTaskCount(aubCsr->peekTaskCount() + 1);

    EXPECT_NE(aubCsr->peekLatestSentTaskCount(), *aubCsr->getTagAddress());

    aubCsr->flush(batchBuffer, allocationsForResidency);

    EXPECT_EQ(aubCsr->peekLatestSentTaskCount(), *aubCsr->getTagAddress());
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverInStandaloneModeWhenFlushIsCalledThenItShouldUpdateAllocationsForResidencyWithCommandBufferAllocation) {
    auto aubExecutionEnvironment = getEnvironment<MockAubCsr<FamilyType>>(true, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<MockAubCsr<FamilyType>>();
    LinearStream cs(aubExecutionEnvironment->commandBuffer);

    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());

    ResidencyContainer allocationsForResidency = {};

    aubCsr->flush(batchBuffer, allocationsForResidency);

    EXPECT_EQ(1u, allocationsForResidency.size());
    EXPECT_EQ(cs.getGraphicsAllocation(), allocationsForResidency[0]);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverInNonStandaloneModeWhenFlushIsCalledThenItShouldNotUpdateAllocationsForResidencyWithCommandBufferAllocation) {
    auto aubExecutionEnvironment = getEnvironment<MockAubCsr<FamilyType>>(true, true, false);
    auto aubCsr = aubExecutionEnvironment->template getCsr<MockAubCsr<FamilyType>>();
    LinearStream cs(aubExecutionEnvironment->commandBuffer);

    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());

    ResidencyContainer allocationsForResidency = {};

    aubCsr->flush(batchBuffer, allocationsForResidency);

    EXPECT_EQ(0u, allocationsForResidency.size());
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverInStandaloneAndSubCaptureModeWhenFlushIsCalledButSubCaptureIsDisabledThenItShouldUpdateHwTagWithLatestSentTaskCount) {

    AubSubCaptureCommon aubSubCaptureCommon;
    auto aubSubCaptureManagerMock = new AubSubCaptureManagerMock("", aubSubCaptureCommon);
    auto aubExecutionEnvironment = getEnvironment<MockAubCsr<FamilyType>>(true, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<MockAubCsr<FamilyType>>();
    LinearStream cs(aubExecutionEnvironment->commandBuffer);

    aubSubCaptureCommon.subCaptureMode = AubSubCaptureManager::SubCaptureMode::toggle;
    aubSubCaptureManagerMock->disableSubCapture();
    aubCsr->subCaptureManager = std::unique_ptr<AubSubCaptureManagerMock>(aubSubCaptureManagerMock);
    ASSERT_FALSE(aubCsr->subCaptureManager->isSubCaptureEnabled());

    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());

    ResidencyContainer allocationsForResidency = {};

    aubCsr->setLatestSentTaskCount(aubCsr->peekTaskCount() + 1);

    EXPECT_NE(aubCsr->peekLatestSentTaskCount(), *aubCsr->getTagAddress());

    aubCsr->flush(batchBuffer, allocationsForResidency);

    EXPECT_EQ(aubCsr->peekLatestSentTaskCount(), *aubCsr->getTagAddress());
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverInNonStandaloneAndSubCaptureModeWhenFlushIsCalledButSubCaptureIsDisabledThenItShouldNotUpdateHwTagWithLatestSentTaskCount) {

    AubSubCaptureCommon aubSubCaptureCommon;
    auto aubSubCaptureManagerMock = new AubSubCaptureManagerMock("", aubSubCaptureCommon);
    auto aubExecutionEnvironment = getEnvironment<MockAubCsr<FamilyType>>(true, true, false);
    auto aubCsr = aubExecutionEnvironment->template getCsr<MockAubCsr<FamilyType>>();
    LinearStream cs(aubExecutionEnvironment->commandBuffer);

    aubSubCaptureCommon.subCaptureMode = AubSubCaptureManager::SubCaptureMode::toggle;
    aubSubCaptureManagerMock->disableSubCapture();
    aubCsr->subCaptureManager = std::unique_ptr<AubSubCaptureManagerMock>(aubSubCaptureManagerMock);
    ASSERT_FALSE(aubCsr->subCaptureManager->isSubCaptureEnabled());

    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());

    ResidencyContainer allocationsForResidency = {};

    aubCsr->setLatestSentTaskCount(aubCsr->peekTaskCount() + 1);

    EXPECT_NE(aubCsr->peekLatestSentTaskCount(), *aubCsr->getTagAddress());

    aubCsr->flush(batchBuffer, allocationsForResidency);

    EXPECT_NE(aubCsr->peekLatestSentTaskCount(), *aubCsr->getTagAddress());
    EXPECT_EQ(initialHardwareTag, *aubCsr->getTagAddress());
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverInSubCaptureModeWhenFlushIsCalledAndSubCaptureIsEnabledThenItShouldDeactivateSubCapture) {

    AubSubCaptureCommon aubSubCaptureCommon;
    auto aubSubCaptureManagerMock = new AubSubCaptureManagerMock("", aubSubCaptureCommon);
    auto aubExecutionEnvironment = getEnvironment<MockAubCsr<FamilyType>>(true, true, false);
    auto aubCsr = aubExecutionEnvironment->template getCsr<MockAubCsr<FamilyType>>();
    LinearStream cs(aubExecutionEnvironment->commandBuffer);

    aubSubCaptureCommon.subCaptureMode = AubSubCaptureManager::SubCaptureMode::toggle;
    aubSubCaptureManagerMock->setSubCaptureToggleActive(true);
    aubSubCaptureManagerMock->checkAndActivateSubCapture("kernelName");
    aubCsr->subCaptureManager = std::unique_ptr<AubSubCaptureManagerMock>(aubSubCaptureManagerMock);
    ASSERT_TRUE(aubCsr->subCaptureManager->isSubCaptureEnabled());

    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());

    ResidencyContainer allocationsForResidency = {};

    aubCsr->flush(batchBuffer, allocationsForResidency);

    EXPECT_FALSE(aubCsr->subCaptureManager->isSubCaptureEnabled());
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverInSubCaptureModeWhenFlushIsCalledAndSubCaptureIsEnabledThenItShouldCallPollForCompletion) {

    AubSubCaptureCommon aubSubCaptureCommon;
    auto aubSubCaptureManagerMock = new AubSubCaptureManagerMock("", aubSubCaptureCommon);
    auto aubExecutionEnvironment = getEnvironment<MockAubCsr<FamilyType>>(true, true, false);
    auto aubCsr = aubExecutionEnvironment->template getCsr<MockAubCsr<FamilyType>>();
    LinearStream cs(aubExecutionEnvironment->commandBuffer);

    aubSubCaptureCommon.subCaptureMode = AubSubCaptureManager::SubCaptureMode::toggle;
    aubSubCaptureManagerMock->setSubCaptureToggleActive(true);
    aubSubCaptureManagerMock->checkAndActivateSubCapture("kernelName");
    aubCsr->subCaptureManager = std::unique_ptr<AubSubCaptureManagerMock>(aubSubCaptureManagerMock);
    ASSERT_TRUE(aubCsr->subCaptureManager->isSubCaptureEnabled());

    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());

    ResidencyContainer allocationsForResidency = {};

    aubCsr->flush(batchBuffer, allocationsForResidency);

    EXPECT_TRUE(aubCsr->pollForCompletionCalled);
}

HWTEST_F(AubCommandStreamReceiverTests, whenPollForAubCompletionCalledThenInsertPoll) {
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_CCS, EngineUsage::regular}));
    std::string fileName = "file_name.aub";
    MockAubManager *mockManager = new MockAubManager();
    MockAubCenter *mockAubCenter = new MockAubCenter(pDevice->getRootDeviceEnvironment(), false, fileName, CommandStreamReceiverType::aub);
    mockAubCenter->aubManager = std::unique_ptr<MockAubManager>(mockManager);
    pDevice->executionEnvironment->rootDeviceEnvironments[0]->aubCenter = std::unique_ptr<MockAubCenter>(mockAubCenter);

    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(static_cast<AUBCommandStreamReceiverHw<FamilyType> *>(AUBCommandStreamReceiver::create(fileName, true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield())));

    aubCsr->setupContext(osContext);
    auto mockHardwareContext = static_cast<MockHardwareContext *>(aubCsr->hardwareContextController->hardwareContexts[0].get());

    aubCsr->pollForAubCompletion();
    EXPECT_TRUE(mockHardwareContext->pollForCompletionCalled);

    mockHardwareContext->pollForCompletionCalled = false;

    aubCsr->pollForAubCompletion();

    EXPECT_TRUE(mockHardwareContext->pollForCompletionCalled);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverInStandaloneModeWhenFlushIsCalledThenItShouldCallMakeResidentOnCommandBufferAllocation) {
    auto aubExecutionEnvironment = getEnvironment<MockAubCsr<FamilyType>>(true, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<MockAubCsr<FamilyType>>();
    auto allocationsForResidency = aubCsr->getResidencyAllocations();
    auto commandBuffer = aubExecutionEnvironment->commandBuffer;
    LinearStream cs(commandBuffer);

    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());

    EXPECT_FALSE(commandBuffer->isResident(aubCsr->getOsContext().getContextId()));

    aubCsr->overrideDispatchPolicy(DispatchMode::immediateDispatch);
    aubCsr->flush(batchBuffer, allocationsForResidency);

    EXPECT_TRUE(commandBuffer->isResident(aubCsr->getOsContext().getContextId()));
    EXPECT_EQ(aubCsr->peekTaskCount() + 1, commandBuffer->getResidencyTaskCount(aubCsr->getOsContext().getContextId()));

    aubCsr->makeSurfacePackNonResident(allocationsForResidency, true);

    EXPECT_FALSE(commandBuffer->isResident(aubCsr->getOsContext().getContextId()));
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverInNonStandaloneModeWhenFlushIsCalledThenItShouldCallMakeResidentOnCommandBufferAllocation) {
    auto aubExecutionEnvironment = getEnvironment<AUBCommandStreamReceiverHw<FamilyType>>(false, true, false);
    auto aubCsr = aubExecutionEnvironment->template getCsr<AUBCommandStreamReceiverHw<FamilyType>>();
    auto allocationsForResidency = aubCsr->getResidencyAllocations();
    LinearStream cs(aubExecutionEnvironment->commandBuffer);

    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());

    EXPECT_FALSE(aubExecutionEnvironment->commandBuffer->isResident(aubCsr->getOsContext().getContextId()));

    aubCsr->flush(batchBuffer, allocationsForResidency);

    EXPECT_TRUE(aubExecutionEnvironment->commandBuffer->isResident(aubCsr->getOsContext().getContextId()));
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverInStandaloneModeWhenFlushIsCalledThenItShouldCallMakeResidentOnResidencyAllocations) {
    auto aubExecutionEnvironment = getEnvironment<MockAubCsr<FamilyType>>(true, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<MockAubCsr<FamilyType>>();
    auto memoryManager = aubExecutionEnvironment->executionEnvironment->memoryManager.get();
    auto commandBuffer = aubExecutionEnvironment->commandBuffer;
    LinearStream cs(commandBuffer);

    auto gfxAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{pDevice->getRootDeviceIndex(), MemoryConstants::pageSize});
    ASSERT_NE(nullptr, gfxAllocation);

    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());

    ResidencyContainer allocationsForResidency = {gfxAllocation};

    EXPECT_FALSE(gfxAllocation->isResident(aubCsr->getOsContext().getContextId()));
    EXPECT_FALSE(commandBuffer->isResident(aubCsr->getOsContext().getContextId()));

    aubCsr->overrideDispatchPolicy(DispatchMode::batchedDispatch);
    aubCsr->flush(batchBuffer, allocationsForResidency);

    EXPECT_TRUE(gfxAllocation->isResident(aubCsr->getOsContext().getContextId()));
    EXPECT_EQ(aubCsr->peekTaskCount() + 1, gfxAllocation->getResidencyTaskCount(aubCsr->getOsContext().getContextId()));

    EXPECT_TRUE(commandBuffer->isResident(aubCsr->getOsContext().getContextId()));
    EXPECT_EQ(aubCsr->peekTaskCount() + 1, commandBuffer->getResidencyTaskCount(aubCsr->getOsContext().getContextId()));

    aubCsr->makeSurfacePackNonResident(allocationsForResidency, true);

    EXPECT_FALSE(gfxAllocation->isResident(aubCsr->getOsContext().getContextId()));
    EXPECT_FALSE(commandBuffer->isResident(aubCsr->getOsContext().getContextId()));

    memoryManager->freeGraphicsMemory(gfxAllocation);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverInNonStandaloneModeWhenFlushIsCalledThenItShouldCallMakeResidentOnResidencyAllocations) {
    auto aubExecutionEnvironment = getEnvironment<AUBCommandStreamReceiverHw<FamilyType>>(true, true, false);
    auto aubCsr = aubExecutionEnvironment->template getCsr<AUBCommandStreamReceiverHw<FamilyType>>();
    auto memoryManager = aubExecutionEnvironment->executionEnvironment->memoryManager.get();
    auto commandBuffer = aubExecutionEnvironment->commandBuffer;
    LinearStream cs(commandBuffer);

    auto gfxAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{pDevice->getRootDeviceIndex(), MemoryConstants::pageSize});

    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());

    ResidencyContainer allocationsForResidency = {gfxAllocation};

    EXPECT_FALSE(gfxAllocation->isResident(aubCsr->getOsContext().getContextId()));
    EXPECT_FALSE(commandBuffer->isResident(aubCsr->getOsContext().getContextId()));

    aubCsr->flush(batchBuffer, allocationsForResidency);

    EXPECT_TRUE(gfxAllocation->isResident(aubCsr->getOsContext().getContextId()));
    EXPECT_EQ(aubCsr->peekTaskCount() + 1, gfxAllocation->getResidencyTaskCount(aubCsr->getOsContext().getContextId()));

    EXPECT_TRUE(commandBuffer->isResident(aubCsr->getOsContext().getContextId()));

    memoryManager->freeGraphicsMemory(gfxAllocation);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverInStandaloneAndSubCaptureModeWhenFlushIsCalledAndSubCaptureIsEnabledThenItShouldCallMakeResidentOnCommandBufferAndResidencyAllocations) {
    DebugManagerStateRestore stateRestore;

    AubSubCaptureCommon aubSubCaptureCommon;
    auto aubSubCaptureManagerMock = new AubSubCaptureManagerMock("", aubSubCaptureCommon);
    auto aubExecutionEnvironment = getEnvironment<MockAubCsr<FamilyType>>(true, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<MockAubCsr<FamilyType>>();
    auto memoryManager = aubExecutionEnvironment->executionEnvironment->memoryManager.get();
    auto commandBuffer = aubExecutionEnvironment->commandBuffer;
    LinearStream cs(commandBuffer);

    aubSubCaptureCommon.subCaptureMode = AubSubCaptureManager::SubCaptureMode::toggle;
    aubSubCaptureManagerMock->setSubCaptureToggleActive(true);
    aubSubCaptureManagerMock->checkAndActivateSubCapture("kernelName");
    aubCsr->subCaptureManager = std::unique_ptr<AubSubCaptureManagerMock>(aubSubCaptureManagerMock);
    ASSERT_TRUE(aubCsr->subCaptureManager->isSubCaptureEnabled());

    auto gfxAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{pDevice->getRootDeviceIndex(), MemoryConstants::pageSize});
    ASSERT_NE(nullptr, gfxAllocation);

    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());

    ResidencyContainer allocationsForResidency = {gfxAllocation};

    EXPECT_FALSE(gfxAllocation->isResident(aubCsr->getOsContext().getContextId()));
    EXPECT_FALSE(commandBuffer->isResident(aubCsr->getOsContext().getContextId()));

    aubCsr->overrideDispatchPolicy(DispatchMode::batchedDispatch);
    aubCsr->flush(batchBuffer, allocationsForResidency);

    EXPECT_TRUE(gfxAllocation->isResident(aubCsr->getOsContext().getContextId()));
    EXPECT_EQ(aubCsr->peekTaskCount() + 1, gfxAllocation->getResidencyTaskCount(aubCsr->getOsContext().getContextId()));

    EXPECT_TRUE(commandBuffer->isResident(aubCsr->getOsContext().getContextId()));
    EXPECT_EQ(aubCsr->peekTaskCount() + 1, commandBuffer->getResidencyTaskCount(aubCsr->getOsContext().getContextId()));

    aubCsr->makeSurfacePackNonResident(allocationsForResidency, true);

    EXPECT_FALSE(gfxAllocation->isResident(aubCsr->getOsContext().getContextId()));
    EXPECT_FALSE(commandBuffer->isResident(aubCsr->getOsContext().getContextId()));

    memoryManager->freeGraphicsMemory(gfxAllocation);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenGraphicsAllocationIsCreatedThenItDoesntHaveTypeNonAubWritable) {
    auto aubExecutionEnvironment = getEnvironment<AUBCommandStreamReceiverHw<FamilyType>>(false, false, true);
    auto memoryManager = aubExecutionEnvironment->executionEnvironment->memoryManager.get();

    auto gfxAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{pDevice->getRootDeviceIndex(), MemoryConstants::pageSize});

    EXPECT_TRUE(gfxAllocation->isAubWritable(GraphicsAllocation::defaultBank));

    memoryManager->freeGraphicsMemory(gfxAllocation);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenProcessResidencyIsCalledOnDefaultAllocationThenAllocationTypeShouldNotBeMadeNonAubWritable) {
    auto aubExecutionEnvironment = getEnvironment<AUBCommandStreamReceiverHw<FamilyType>>(false, false, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<AUBCommandStreamReceiverHw<FamilyType>>();
    aubCsr->initializeEngine();
    auto memoryManager = aubExecutionEnvironment->executionEnvironment->memoryManager.get();

    auto gfxDefaultAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{pDevice->getRootDeviceIndex(), MemoryConstants::pageSize});

    ResidencyContainer allocationsForResidency = {gfxDefaultAllocation};
    aubCsr->processResidency(allocationsForResidency, 0u);

    EXPECT_TRUE(aubCsr->isAubWritable(*gfxDefaultAllocation));

    memoryManager->freeGraphicsMemory(gfxDefaultAllocation);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenWriteMemoryIsCalledOnBufferAndImageTypeAllocationsThenAllocationsHaveAubWritableSetToFalse) {
    std::unique_ptr<MemoryManager> memoryManager(nullptr);

    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield()));
    aubCsr->setupContext(*pDevice->getDefaultEngine().osContext);
    memoryManager.reset(new OsAgnosticMemoryManager(*pDevice->executionEnvironment));
    aubCsr->initializeEngine();

    auto gfxAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{pDevice->getRootDeviceIndex(), MemoryConstants::pageSize});

    const AllocationType onlyOneTimeAubWritableTypes[] = {
        AllocationType::pipe,
        AllocationType::constantSurface,
        AllocationType::globalSurface,
        AllocationType::kernelIsa,
        AllocationType::kernelIsaInternal,
        AllocationType::privateSurface,
        AllocationType::scratchSurface,
        AllocationType::workPartitionSurface,
        AllocationType::buffer,
        AllocationType::bufferHostMemory,
        AllocationType::image,
        AllocationType::timestampPacketTagBuffer,
        AllocationType::mapAllocation,
        AllocationType::svmGpu,
        AllocationType::externalHostPtr,
        AllocationType::assertBuffer};

    for (auto allocationType : onlyOneTimeAubWritableTypes) {
        gfxAllocation->setAubWritable(true, GraphicsAllocation::defaultBank);
        gfxAllocation->setAllocationType(allocationType);
        aubCsr->writeMemory(*gfxAllocation);

        EXPECT_FALSE(aubCsr->isAubWritable(*gfxAllocation));
    }
    memoryManager->freeGraphicsMemory(gfxAllocation);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenProcessResidencyIsCalledOnBufferAndImageAllocationsThenAllocationsTypesShouldBeMadeNonAubWritable) {
    std::unique_ptr<MemoryManager> memoryManager(nullptr);
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield()));
    memoryManager.reset(new OsAgnosticMemoryManager(*pDevice->executionEnvironment));
    aubCsr->setupContext(*pDevice->getDefaultEngine().osContext);
    aubCsr->initializeEngine();

    auto gfxBufferAllocation = memoryManager->allocateGraphicsMemoryWithProperties({pDevice->getRootDeviceIndex(), MemoryConstants::pageSize, AllocationType::buffer, pDevice->getDeviceBitfield()});

    auto gfxImageAllocation = MockGmm::allocateImage2d(*memoryManager);

    ResidencyContainer allocationsForResidency = {gfxBufferAllocation, gfxImageAllocation};
    aubCsr->processResidency(allocationsForResidency, 0u);

    EXPECT_FALSE(aubCsr->isAubWritable(*gfxBufferAllocation));
    EXPECT_FALSE(aubCsr->isAubWritable(*gfxImageAllocation));

    memoryManager->freeGraphicsMemory(gfxBufferAllocation);
    memoryManager->freeGraphicsMemory(gfxImageAllocation);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverInSubCaptureModWhenProcessResidencyIsCalledWithDumpAubNonWritableFlagThenAllocationsTypesShouldBeMadeAubWritable) {
    DebugManagerStateRestore stateRestore;
    std::unique_ptr<MemoryManager> memoryManager(nullptr);
    std::unique_ptr<MockAubCsrToTestDumpAubNonWritable<FamilyType>> aubCsr(new MockAubCsrToTestDumpAubNonWritable<FamilyType>("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield()));
    memoryManager.reset(new OsAgnosticMemoryManager(*pDevice->executionEnvironment));
    aubCsr->setupContext(*pDevice->getDefaultEngine().osContext);

    auto gfxBufferAllocation = memoryManager->allocateGraphicsMemoryWithProperties({pDevice->getRootDeviceIndex(), MemoryConstants::pageSize, AllocationType::buffer, pDevice->getDeviceBitfield()});
    aubCsr->setAubWritable(false, *gfxBufferAllocation);

    auto gfxImageAllocation = MockGmm::allocateImage2d(*memoryManager);
    aubCsr->setAubWritable(false, *gfxImageAllocation);

    aubCsr->dumpAubNonWritable = true;

    ResidencyContainer allocationsForResidency = {gfxBufferAllocation, gfxImageAllocation};
    aubCsr->processResidency(allocationsForResidency, 0u);

    EXPECT_TRUE(aubCsr->isAubWritable(*gfxBufferAllocation));
    EXPECT_TRUE(aubCsr->isAubWritable(*gfxImageAllocation));

    memoryManager->freeGraphicsMemory(gfxBufferAllocation);
    memoryManager->freeGraphicsMemory(gfxImageAllocation);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenProcessResidencyIsCalledWithoutDumpAubWritableFlagThenAllocationsTypesShouldBeKeptNonAubWritable) {
    DebugManagerStateRestore stateRestore;
    std::unique_ptr<MemoryManager> memoryManager(nullptr);
    std::unique_ptr<MockAubCsrToTestDumpAubNonWritable<FamilyType>> aubCsr(new MockAubCsrToTestDumpAubNonWritable<FamilyType>("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield()));
    memoryManager.reset(new OsAgnosticMemoryManager(*pDevice->executionEnvironment));
    aubCsr->setupContext(*pDevice->getDefaultEngine().osContext);

    auto gfxBufferAllocation = memoryManager->allocateGraphicsMemoryWithProperties({pDevice->getRootDeviceIndex(), MemoryConstants::pageSize, AllocationType::buffer, pDevice->getDeviceBitfield()});
    aubCsr->setAubWritable(false, *gfxBufferAllocation);

    auto gfxImageAllocation = MockGmm::allocateImage2d(*memoryManager);
    aubCsr->setAubWritable(false, *gfxImageAllocation);

    aubCsr->dumpAubNonWritable = false;

    ResidencyContainer allocationsForResidency = {gfxBufferAllocation, gfxImageAllocation};
    aubCsr->processResidency(allocationsForResidency, 0u);

    EXPECT_FALSE(aubCsr->isAubWritable(*gfxBufferAllocation));
    EXPECT_FALSE(aubCsr->isAubWritable(*gfxImageAllocation));

    memoryManager->freeGraphicsMemory(gfxBufferAllocation);
    memoryManager->freeGraphicsMemory(gfxImageAllocation);
}

HWTEST_F(AubCommandStreamReceiverTests, givenOsContextWithMultipleDevicesSupportedWhenSetupIsCalledThenCreateMultipleHardwareContexts) {
    MockOsContext osContext(1, EngineDescriptorHelper::getDefaultDescriptor(0b11));
    auto aubCsr = std::make_unique<AUBCommandStreamReceiverHw<FamilyType>>("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    aubCsr->setupContext(osContext);

    EXPECT_EQ(2u, aubCsr->hardwareContextController->hardwareContexts.size());
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenGraphicsAllocationTypeIsntNonAubWritableThenWriteMemoryIsAllowed) {
    std::unique_ptr<MemoryManager> memoryManager(nullptr);
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield()));
    aubCsr->setupContext(*pDevice->getDefaultEngine().osContext);
    aubCsr->initializeEngine();

    memoryManager.reset(new OsAgnosticMemoryManager(*pDevice->executionEnvironment));

    auto gfxAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{pDevice->getRootDeviceIndex(), MemoryConstants::pageSize});

    EXPECT_TRUE(aubCsr->writeMemory(*gfxAllocation));

    memoryManager->freeGraphicsMemory(gfxAllocation);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenGraphicsAllocationTypeIsNonAubWritableThenWriteMemoryIsNotAllowed) {
    std::unique_ptr<MemoryManager> memoryManager(nullptr);
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield()));
    memoryManager.reset(new OsAgnosticMemoryManager(*pDevice->executionEnvironment));
    aubCsr->setupContext(*pDevice->getDefaultEngine().osContext);
    aubCsr->initializeEngine();
    auto gfxAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{pDevice->getRootDeviceIndex(), MemoryConstants::pageSize});

    aubCsr->setAubWritable(false, *gfxAllocation);
    EXPECT_FALSE(aubCsr->writeMemory(*gfxAllocation));

    memoryManager->freeGraphicsMemory(gfxAllocation);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenGraphicsAllocationSizeIsZeroThenWriteMemoryIsNotAllowed) {
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield()));
    MockGraphicsAllocation gfxAllocation((void *)0x1234, 0);
    aubCsr->setupContext(*pDevice->getDefaultEngine().osContext);
    aubCsr->initializeEngine();
    EXPECT_FALSE(aubCsr->writeMemory(gfxAllocation));
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenAllocationDataIsPassedInAllocationViewThenWriteMemoryIsAllowed) {
    auto aubCsr = std::make_unique<AUBCommandStreamReceiverHw<FamilyType>>("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    aubCsr->setupContext(*pDevice->getDefaultEngine().osContext);
    aubCsr->initializeEngine();
    size_t size = 100;
    auto ptr = std::make_unique<char[]>(size);
    auto addr = reinterpret_cast<uint64_t>(ptr.get());
    AllocationView allocationView(addr, size);

    EXPECT_TRUE(aubCsr->writeMemory(allocationView));
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenAllocationSizeInAllocationViewIsZeroThenWriteMemoryIsNotAllowed) {
    auto aubCsr = std::make_unique<AUBCommandStreamReceiverHw<FamilyType>>("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    AllocationView allocationView(0x1234, 0);
    aubCsr->setupContext(*pDevice->getDefaultEngine().osContext);
    aubCsr->initializeEngine();
    EXPECT_FALSE(aubCsr->writeMemory(allocationView));
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenAUBDumpCaptureFileNameHasBeenSpecifiedThenItShouldBeUsedToOpenTheFileWithAubCapture) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.AUBDumpCaptureFileName.set("file_name.aub");

    std::unique_ptr<MockAubCsr<FamilyType>> aubCsr(static_cast<MockAubCsr<FamilyType> *>(AUBCommandStreamReceiver::create("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield())));
    EXPECT_NE(nullptr, aubCsr);

    EXPECT_TRUE(aubCsr->isFileOpen());
    EXPECT_STREQ(debugManager.flags.AUBDumpCaptureFileName.get().c_str(), aubCsr->getFileName().c_str());
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverInSubCaptureModeWhenAubSubCaptureIsActivatedThenFileIsOpened) {
    DebugManagerStateRestore stateRestore;
    std::unique_ptr<MockAubCsr<FamilyType>> aubCsr(new MockAubCsr<FamilyType>("", false, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield()));

    AubSubCaptureCommon aubSubCaptureCommon;
    auto subCaptureManagerMock = new AubSubCaptureManagerMock("", aubSubCaptureCommon);
    aubSubCaptureCommon.subCaptureMode = AubSubCaptureManager::SubCaptureMode::toggle;
    subCaptureManagerMock->setSubCaptureIsActive(false);
    subCaptureManagerMock->setSubCaptureToggleActive(true);
    aubCsr->subCaptureManager = std::unique_ptr<AubSubCaptureManagerMock>(subCaptureManagerMock);

    ASSERT_FALSE(aubCsr->isFileOpen());

    aubCsr->checkAndActivateAubSubCapture("kernelName");

    EXPECT_TRUE(aubCsr->isFileOpen());
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverInSubCaptureModeWhenAubSubCaptureRemainsActivedThenTheSameFileShouldBeKeptOpened) {
    DebugManagerStateRestore stateRestore;
    std::unique_ptr<MockAubCsr<FamilyType>> aubCsr(new MockAubCsr<FamilyType>("", false, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield()));

    AubSubCaptureCommon aubSubCaptureCommon;
    auto subCaptureManagerMock = new AubSubCaptureManagerMock("", aubSubCaptureCommon);
    aubSubCaptureCommon.subCaptureMode = AubSubCaptureManager::SubCaptureMode::toggle;
    subCaptureManagerMock->setSubCaptureIsActive(false);
    subCaptureManagerMock->setSubCaptureToggleActive(true);
    aubCsr->subCaptureManager = std::unique_ptr<AubSubCaptureManagerMock>(subCaptureManagerMock);

    std::string kernelName = "";
    std::string fileName = aubCsr->subCaptureManager->getSubCaptureFileName(kernelName);
    aubCsr->initFile(fileName);
    ASSERT_TRUE(aubCsr->isFileOpen());

    aubCsr->checkAndActivateAubSubCapture(kernelName);

    EXPECT_TRUE(aubCsr->isFileOpen());
    EXPECT_STREQ(fileName.c_str(), aubCsr->getFileName().c_str());
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverInSubCaptureModeWhenAubSubCaptureIsActivatedWithNewFileNameThenNewFileShouldBeReOpened) {
    DebugManagerStateRestore stateRestore;
    std::unique_ptr<MockAubCsr<FamilyType>> aubCsr(new MockAubCsr<FamilyType>("", false, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield()));
    std::string newFileName = "new_file_name.aub";

    AubSubCaptureCommon aubSubCaptureCommon;
    auto subCaptureManagerMock = new AubSubCaptureManagerMock("", aubSubCaptureCommon);
    aubSubCaptureCommon.subCaptureMode = AubSubCaptureManager::SubCaptureMode::toggle;
    subCaptureManagerMock->setSubCaptureIsActive(false);
    subCaptureManagerMock->setSubCaptureToggleActive(true);
    subCaptureManagerMock->setToggleFileName(newFileName);
    aubCsr->subCaptureManager = std::unique_ptr<AubSubCaptureManagerMock>(subCaptureManagerMock);

    std::string fileName = "file_name.aub";
    aubCsr->initFile(fileName);
    ASSERT_TRUE(aubCsr->isFileOpen());
    ASSERT_STREQ(fileName.c_str(), aubCsr->getFileName().c_str());

    aubCsr->checkAndActivateAubSubCapture("kernelName");

    EXPECT_TRUE(aubCsr->isFileOpen());
    EXPECT_STRNE(fileName.c_str(), aubCsr->getFileName().c_str());
    EXPECT_STREQ(newFileName.c_str(), aubCsr->getFileName().c_str());
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverInSubCaptureModeWhenAubSubCaptureIsActivatedForNewFileThenOldEngineInfoShouldBeFreed) {
    DebugManagerStateRestore stateRestore;
    std::unique_ptr<MockAubCsr<FamilyType>> aubCsr(new MockAubCsr<FamilyType>("", false, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield()));
    std::string newFileName = "new_file_name.aub";

    AubSubCaptureCommon aubSubCaptureCommon;
    auto subCaptureManagerMock = new AubSubCaptureManagerMock("", aubSubCaptureCommon);
    aubSubCaptureCommon.subCaptureMode = AubSubCaptureManager::SubCaptureMode::toggle;
    subCaptureManagerMock->setSubCaptureIsActive(false);
    subCaptureManagerMock->setSubCaptureToggleActive(true);
    subCaptureManagerMock->setToggleFileName(newFileName);
    aubCsr->subCaptureManager = std::unique_ptr<AubSubCaptureManagerMock>(subCaptureManagerMock);

    std::string fileName = "file_name.aub";
    aubCsr->initFile(fileName);
    ASSERT_STREQ(fileName.c_str(), aubCsr->getFileName().c_str());

    aubCsr->checkAndActivateAubSubCapture("kernelName");
    ASSERT_STREQ(newFileName.c_str(), aubCsr->getFileName().c_str());

    EXPECT_EQ(nullptr, aubCsr->engineInfo.pLRCA);
    EXPECT_EQ(nullptr, aubCsr->engineInfo.pGlobalHWStatusPage);
    EXPECT_EQ(nullptr, aubCsr->engineInfo.pRingBuffer);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverInSubCaptureModeWhenAubSubCaptureIsActivatedThenForceDumpingAllocationsAubNonWritable) {
    DebugManagerStateRestore stateRestore;
    std::unique_ptr<MockAubCsrToTestDumpAubNonWritable<FamilyType>> aubCsr(new MockAubCsrToTestDumpAubNonWritable<FamilyType>("", false, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield()));

    AubSubCaptureCommon aubSubCaptureCommon;
    auto subCaptureManagerMock = new AubSubCaptureManagerMock("", aubSubCaptureCommon);
    aubSubCaptureCommon.subCaptureMode = AubSubCaptureManager::SubCaptureMode::toggle;
    subCaptureManagerMock->setSubCaptureIsActive(false);
    subCaptureManagerMock->setSubCaptureToggleActive(true);
    aubCsr->subCaptureManager = std::unique_ptr<AubSubCaptureManagerMock>(subCaptureManagerMock);

    aubCsr->checkAndActivateAubSubCapture("kernelName");

    EXPECT_TRUE(aubCsr->dumpAubNonWritable);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverInSubCaptureModeWhenAubSubCaptureRemainsActivatedThenDontForceDumpingAllocationsAubNonWritable) {
    DebugManagerStateRestore stateRestore;
    std::unique_ptr<MockAubCsrToTestDumpAubNonWritable<FamilyType>> aubCsr(new MockAubCsrToTestDumpAubNonWritable<FamilyType>("", false, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield()));

    AubSubCaptureCommon aubSubCaptureCommon;
    auto subCaptureManagerMock = new AubSubCaptureManagerMock("", aubSubCaptureCommon);
    aubSubCaptureCommon.subCaptureMode = AubSubCaptureManager::SubCaptureMode::toggle;
    subCaptureManagerMock->setSubCaptureIsActive(false);
    subCaptureManagerMock->setSubCaptureToggleActive(true);
    aubCsr->subCaptureManager = std::unique_ptr<AubSubCaptureManagerMock>(subCaptureManagerMock);

    std::string kernelName = "";

    aubCsr->initFile(aubCsr->subCaptureManager->getSubCaptureFileName(kernelName));
    aubCsr->checkAndActivateAubSubCapture(kernelName);

    EXPECT_FALSE(aubCsr->dumpAubNonWritable);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverInSubCaptureModeWhenSubCaptureModeRemainsDeactivatedThenSubCaptureIsDisabled) {
    DebugManagerStateRestore stateRestore;
    std::unique_ptr<MockAubCsr<FamilyType>> aubCsr(new MockAubCsr<FamilyType>("", false, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield()));

    AubSubCaptureCommon aubSubCaptureCommon;
    auto subCaptureManagerMock = new AubSubCaptureManagerMock("", aubSubCaptureCommon);
    aubSubCaptureCommon.subCaptureMode = AubSubCaptureManager::SubCaptureMode::toggle;
    subCaptureManagerMock->setSubCaptureIsActive(false);
    subCaptureManagerMock->setSubCaptureToggleActive(false);
    aubCsr->subCaptureManager = std::unique_ptr<AubSubCaptureManagerMock>(subCaptureManagerMock);

    aubCsr->checkAndActivateAubSubCapture("kernelName");

    EXPECT_FALSE(aubCsr->subCaptureManager->isSubCaptureEnabled());
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverInSubCaptureModeWhenSubCaptureIsToggledOnThenSubCaptureGetsEnabled) {
    DebugManagerStateRestore stateRestore;
    std::unique_ptr<MockAubCsr<FamilyType>> aubCsr(new MockAubCsr<FamilyType>("", false, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield()));

    AubSubCaptureCommon aubSubCaptureCommon;
    auto subCaptureManagerMock = new AubSubCaptureManagerMock("", aubSubCaptureCommon);
    aubSubCaptureCommon.subCaptureMode = AubSubCaptureManager::SubCaptureMode::toggle;
    subCaptureManagerMock->setSubCaptureIsActive(false);
    subCaptureManagerMock->setSubCaptureToggleActive(true);
    aubCsr->subCaptureManager = std::unique_ptr<AubSubCaptureManagerMock>(subCaptureManagerMock);

    aubCsr->checkAndActivateAubSubCapture("kernelName");

    EXPECT_TRUE(aubCsr->subCaptureManager->isSubCaptureEnabled());
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverInStandaloneAndSubCaptureModeWhenSubCaptureRemainsDeactivatedThenNeitherProgrammingFlagsAreInitializedNorCsrIsFlushed) {
    DebugManagerStateRestore stateRestore;
    std::unique_ptr<MockAubCsr<FamilyType>> aubCsr(new MockAubCsr<FamilyType>("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield()));

    AubSubCaptureCommon aubSubCaptureCommon;
    auto subCaptureManagerMock = new AubSubCaptureManagerMock("", aubSubCaptureCommon);
    aubSubCaptureCommon.subCaptureMode = AubSubCaptureManager::SubCaptureMode::toggle;
    subCaptureManagerMock->setSubCaptureIsActive(false);
    subCaptureManagerMock->setSubCaptureToggleActive(false);
    aubCsr->subCaptureManager = std::unique_ptr<AubSubCaptureManagerMock>(subCaptureManagerMock);

    aubCsr->checkAndActivateAubSubCapture("");

    EXPECT_FALSE(aubCsr->flushBatchedSubmissionsCalled);
    EXPECT_FALSE(aubCsr->initProgrammingFlagsCalled);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverInStandaloneAndSubCaptureModeWhenSubCaptureRemainsActivatedThenNeitherProgrammingFlagsAreInitializedNorCsrIsFlushed) {
    DebugManagerStateRestore stateRestore;
    std::unique_ptr<MockAubCsr<FamilyType>> aubCsr(new MockAubCsr<FamilyType>("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield()));

    AubSubCaptureCommon aubSubCaptureCommon;
    auto subCaptureManagerMock = new AubSubCaptureManagerMock("", aubSubCaptureCommon);
    aubSubCaptureCommon.subCaptureMode = AubSubCaptureManager::SubCaptureMode::toggle;
    subCaptureManagerMock->setSubCaptureIsActive(true);
    subCaptureManagerMock->setSubCaptureToggleActive(true);
    aubCsr->subCaptureManager = std::unique_ptr<AubSubCaptureManagerMock>(subCaptureManagerMock);

    aubCsr->checkAndActivateAubSubCapture("");

    EXPECT_FALSE(aubCsr->flushBatchedSubmissionsCalled);
    EXPECT_FALSE(aubCsr->initProgrammingFlagsCalled);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverInStandaloneAndSubCaptureModeWhenSubCaptureGetsActivatedThenProgrammingFlagsAreInitialized) {
    DebugManagerStateRestore stateRestore;
    std::unique_ptr<MockAubCsr<FamilyType>> aubCsr(new MockAubCsr<FamilyType>("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield()));

    AubSubCaptureCommon aubSubCaptureCommon;
    auto subCaptureManagerMock = new AubSubCaptureManagerMock("", aubSubCaptureCommon);
    aubSubCaptureCommon.subCaptureMode = AubSubCaptureManager::SubCaptureMode::toggle;
    subCaptureManagerMock->setSubCaptureIsActive(false);
    subCaptureManagerMock->setSubCaptureToggleActive(true);
    aubCsr->subCaptureManager = std::unique_ptr<AubSubCaptureManagerMock>(subCaptureManagerMock);

    aubCsr->checkAndActivateAubSubCapture("kernelName");

    EXPECT_FALSE(aubCsr->flushBatchedSubmissionsCalled);
    EXPECT_TRUE(aubCsr->initProgrammingFlagsCalled);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverInStandaloneAndSubCaptureModeWhenSubCaptureGetsDeactivatedThenCsrIsFlushed) {
    DebugManagerStateRestore stateRestore;
    std::unique_ptr<MockAubCsr<FamilyType>> aubCsr(new MockAubCsr<FamilyType>("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield()));

    AubSubCaptureCommon aubSubCaptureCommon;
    auto subCaptureManagerMock = new AubSubCaptureManagerMock("", aubSubCaptureCommon);
    aubSubCaptureCommon.subCaptureMode = AubSubCaptureManager::SubCaptureMode::toggle;
    subCaptureManagerMock->setSubCaptureIsActive(true);
    subCaptureManagerMock->setSubCaptureToggleActive(false);
    aubCsr->subCaptureManager = std::unique_ptr<AubSubCaptureManagerMock>(subCaptureManagerMock);

    aubCsr->checkAndActivateAubSubCapture("kernelName");

    EXPECT_TRUE(aubCsr->flushBatchedSubmissionsCalled);
    EXPECT_FALSE(aubCsr->initProgrammingFlagsCalled);
}

HWTEST_F(AubCommandStreamReceiverTests, WhenBlitBufferIsCalledThenCounterIsCorrectlyIncremented) {
    auto aubExecutionEnvironment = getEnvironment<UltAubCommandStreamReceiver<FamilyType>>(true, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<UltAubCommandStreamReceiver<FamilyType>>();
    auto osContext = static_cast<MockOsContext *>(aubCsr->osContext);
    osContext->engineType = aub_stream::ENGINE_BCS;
    EXPECT_EQ(0u, aubCsr->blitBufferCalled);

    MockGraphicsAllocation allocation(reinterpret_cast<void *>(0x1000), 0);
    BlitProperties blitProperties = BlitProperties::constructPropertiesForCopy(&allocation, &allocation, 0, 0, 0, 0, 0, 0, 0, aubCsr->getClearColorAllocation());
    BlitPropertiesContainer blitPropertiesContainer;
    blitPropertiesContainer.push_back(blitProperties);
    aubCsr->flushBcsTask(blitPropertiesContainer, true, *pDevice);
    EXPECT_EQ(1u, aubCsr->blitBufferCalled);
}

HWTEST_F(AubCommandStreamReceiverTests, givenDebugOverwritesForImplicitFlushesWhenTheyAreUsedThenTheyDoNotAffectAubCapture) {
    DebugManagerStateRestore restorer;
    debugManager.flags.PerformImplicitFlushForIdleGpu.set(1);
    debugManager.flags.PerformImplicitFlushForNewResource.set(1);

    auto aubExecutionEnvironment = getEnvironment<UltAubCommandStreamReceiver<FamilyType>>(true, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<UltAubCommandStreamReceiver<FamilyType>>();
    EXPECT_FALSE(aubCsr->useGpuIdleImplicitFlush);
    EXPECT_FALSE(aubCsr->useNewResourceImplicitFlush);
}