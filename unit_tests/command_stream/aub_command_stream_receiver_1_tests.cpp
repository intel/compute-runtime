/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/aub_mem_dump/page_table_entry_bits.h"
#include "runtime/helpers/hardware_context_controller.h"
#include "runtime/helpers/hw_helper.h"
#include "runtime/os_interface/os_context.h"
#include "test.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/mocks/mock_aub_center.h"
#include "unit_tests/mocks/mock_aub_csr.h"
#include "unit_tests/mocks/mock_aub_manager.h"
#include "unit_tests/mocks/mock_aub_subcapture_manager.h"
#include "unit_tests/mocks/mock_gmm.h"
#include "unit_tests/mocks/mock_graphics_allocation.h"
#include "unit_tests/mocks/mock_kernel.h"
#include "unit_tests/mocks/mock_mdi.h"
#include "unit_tests/mocks/mock_os_context.h"

using namespace OCLRT;

typedef Test<DeviceFixture> AubCommandStreamReceiverTests;

template <typename GfxFamily>
struct MockAubCsrToTestDumpAubNonWritable : public AUBCommandStreamReceiverHw<GfxFamily> {
    using AUBCommandStreamReceiverHw<GfxFamily>::AUBCommandStreamReceiverHw;
    using AUBCommandStreamReceiverHw<GfxFamily>::dumpAubNonWritable;

    bool writeMemory(GraphicsAllocation &gfxAllocation) override {
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

TEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenItIsCreatedWithWrongGfxCoreFamilyThenNullPointerShouldBeReturned) {
    HardwareInfo hwInfo = *platformDevices[0];
    GFXCORE_FAMILY family = hwInfo.pPlatform->eRenderCoreFamily;

    const_cast<PLATFORM *>(hwInfo.pPlatform)->eRenderCoreFamily = GFXCORE_FAMILY_FORCE_ULONG; // wrong gfx core family

    CommandStreamReceiver *aubCsr = AUBCommandStreamReceiver::create(hwInfo, "", true, *pDevice->executionEnvironment);
    EXPECT_EQ(nullptr, aubCsr);

    const_cast<PLATFORM *>(hwInfo.pPlatform)->eRenderCoreFamily = family;
}

TEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenTypeIsCheckedThenAubCsrIsReturned) {
    HardwareInfo hwInfo = *platformDevices[0];
    std::unique_ptr<CommandStreamReceiver> aubCsr(AUBCommandStreamReceiver::create(hwInfo, "", true, *pDevice->executionEnvironment));
    EXPECT_NE(nullptr, aubCsr);
    EXPECT_EQ(CommandStreamReceiverType::CSR_AUB, aubCsr->getType());
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenItIsCreatedThenAubManagerAndHardwareContextAreNull) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.UseAubStream.set(false);

    HardwareInfo hwInfo = *platformDevices[0];
    auto aubCsr = std::make_unique<AUBCommandStreamReceiverHw<FamilyType>>(hwInfo, "", true, *pDevice->executionEnvironment);
    ASSERT_NE(nullptr, aubCsr);

    EXPECT_EQ(nullptr, aubCsr->aubManager);
    EXPECT_EQ(nullptr, aubCsr->hardwareContextController);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenGetEngineIndexFromInstanceIsCalledForGivenEngineInstanceThenEngineIndexForThatInstanceIsReturned) {
    auto aubCsr = std::make_unique<AUBCommandStreamReceiverHw<FamilyType>>(**platformDevices, "", true, *pDevice->executionEnvironment);
    EXPECT_NE(nullptr, aubCsr);

    EXPECT_TRUE(aubCsr->getEngineIndex(EngineInstanceT(EngineType::ENGINE_RCS, 0)) < allEngineInstances.size());
    EXPECT_TRUE(aubCsr->getEngineIndex(EngineInstanceT(EngineType::ENGINE_RCS, 1)) < allEngineInstances.size());
    EXPECT_TRUE(aubCsr->getEngineIndex(EngineInstanceT(EngineType::ENGINE_BCS, 0)) < allEngineInstances.size());
    EXPECT_TRUE(aubCsr->getEngineIndex(EngineInstanceT(EngineType::ENGINE_VCS, 0)) < allEngineInstances.size());
    EXPECT_TRUE(aubCsr->getEngineIndex(EngineInstanceT(EngineType::ENGINE_VECS, 0)) < allEngineInstances.size());

    EXPECT_THROW(aubCsr->getEngineIndex(EngineInstanceT(EngineType::ENGINE_RCS, 2)), std::exception);
    EXPECT_THROW(aubCsr->getEngineIndex(EngineInstanceT(EngineType::ENGINE_BCS, 1)), std::exception);
    EXPECT_THROW(aubCsr->getEngineIndex(EngineInstanceT(EngineType::ENGINE_VCS, 1)), std::exception);
    EXPECT_THROW(aubCsr->getEngineIndex(EngineInstanceT(EngineType::ENGINE_VECS, 1)), std::exception);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenGetEngineIndexIsCalledForGivenEngineTypeThenEngineIndexForThatTypeIsReturned) {
    auto aubCsr = std::make_unique<AUBCommandStreamReceiverHw<FamilyType>>(**platformDevices, "", true, *pDevice->executionEnvironment);
    EXPECT_NE(nullptr, aubCsr);

    auto engineIndex = aubCsr->getEngineIndex(EngineInstanceT(EngineType::ENGINE_RCS, 1));
    EXPECT_EQ(EngineType::ENGINE_RCS, allEngineInstances[engineIndex].type);
    EXPECT_EQ(1, allEngineInstances[engineIndex].id);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCsrWhenItIsCreatedWithDefaultSettingsThenItHasBatchedDispatchModeEnabled) {
    DebugManagerStateRestore stateRestore;
    DebugManager.flags.CsrDispatchMode.set(0);
    std::unique_ptr<MockAubCsr<FamilyType>> aubCsr(new MockAubCsr<FamilyType>(*platformDevices[0], "", true, *pDevice->executionEnvironment));
    EXPECT_EQ(DispatchMode::BatchedDispatch, aubCsr->peekDispatchMode());
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCsrWhenItIsCreatedWithDebugSettingsThenItHasProperDispatchModeEnabled) {
    DebugManagerStateRestore stateRestore;
    DebugManager.flags.CsrDispatchMode.set(static_cast<uint32_t>(DispatchMode::ImmediateDispatch));
    std::unique_ptr<MockAubCsr<FamilyType>> aubCsr(new MockAubCsr<FamilyType>(*platformDevices[0], "", true, *pDevice->executionEnvironment));
    EXPECT_EQ(DispatchMode::ImmediateDispatch, aubCsr->peekDispatchMode());
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenItIsCreatedThenMemoryManagerIsNotNull) {
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>(**platformDevices, "", true, *pDevice->executionEnvironment));
    std::unique_ptr<MemoryManager> memoryManager(new OsAgnosticMemoryManager(false, false, *pDevice->executionEnvironment));
    EXPECT_NE(nullptr, memoryManager.get());
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenMultipleInstancesAreCreatedThenTheyOperateOnSingleFileStream) {
    auto aubCsr1 = std::make_unique<AUBCommandStreamReceiverHw<FamilyType>>(**platformDevices, "", true, *pDevice->executionEnvironment);
    auto aubCsr2 = std::make_unique<AUBCommandStreamReceiverHw<FamilyType>>(**platformDevices, "", true, *pDevice->executionEnvironment);
    EXPECT_EQ(aubCsr1->stream, aubCsr2->stream);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenMultipleInstancesAreCreatedThenTheyUseTheSameFileStream) {
    auto aubCsr1 = std::make_unique<AUBCommandStreamReceiverHw<FamilyType>>(**platformDevices, "", true, *pDevice->executionEnvironment);
    auto streamProvider1 = pDevice->executionEnvironment->aubCenter->getStreamProvider();
    EXPECT_NE(nullptr, streamProvider1);
    auto aubCsr2 = std::make_unique<AUBCommandStreamReceiverHw<FamilyType>>(**platformDevices, "", true, *pDevice->executionEnvironment);
    auto streamProvider2 = pDevice->executionEnvironment->aubCenter->getStreamProvider();
    EXPECT_NE(nullptr, streamProvider2);
    EXPECT_EQ(streamProvider1, streamProvider2);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenMultipleInstancesAreCreatedThenTheyUseTheSamePhysicalAddressAllocator) {
    auto aubCsr1 = std::make_unique<AUBCommandStreamReceiverHw<FamilyType>>(**platformDevices, "", true, *pDevice->executionEnvironment);
    auto physicalAddressAlocator1 = pDevice->executionEnvironment->aubCenter->getPhysicalAddressAllocator();
    EXPECT_NE(nullptr, physicalAddressAlocator1);
    auto aubCsr2 = std::make_unique<AUBCommandStreamReceiverHw<FamilyType>>(**platformDevices, "", true, *pDevice->executionEnvironment);
    auto physicalAddressAlocator2 = pDevice->executionEnvironment->aubCenter->getPhysicalAddressAllocator();
    EXPECT_NE(nullptr, physicalAddressAlocator2);
    EXPECT_EQ(physicalAddressAlocator1, physicalAddressAlocator2);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenMultipleInstancesAreCreatedThenTheyUseTheSameAddressMapper) {
    auto aubCsr1 = std::make_unique<AUBCommandStreamReceiverHw<FamilyType>>(**platformDevices, "", true, *pDevice->executionEnvironment);
    auto addressMapper1 = pDevice->executionEnvironment->aubCenter->getAddressMapper();
    EXPECT_NE(nullptr, addressMapper1);
    auto aubCsr2 = std::make_unique<AUBCommandStreamReceiverHw<FamilyType>>(**platformDevices, "", true, *pDevice->executionEnvironment);
    auto addressMapper2 = pDevice->executionEnvironment->aubCenter->getAddressMapper();
    EXPECT_NE(nullptr, addressMapper2);
    EXPECT_EQ(addressMapper1, addressMapper2);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenMultipleInstancesAreCreatedThenTheyUseTheSameSubCaptureManager) {
    auto aubCsr1 = std::make_unique<AUBCommandStreamReceiverHw<FamilyType>>(**platformDevices, "", true, *pDevice->executionEnvironment);
    auto subCaptureManager1 = pDevice->executionEnvironment->aubCenter->getSubCaptureManager();
    EXPECT_NE(nullptr, subCaptureManager1);
    auto aubCsr2 = std::make_unique<AUBCommandStreamReceiverHw<FamilyType>>(**platformDevices, "", true, *pDevice->executionEnvironment);
    auto subCaptureManager2 = pDevice->executionEnvironment->aubCenter->getSubCaptureManager();
    EXPECT_NE(nullptr, subCaptureManager2);
    EXPECT_EQ(subCaptureManager1, subCaptureManager2);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWithAubManagerWhenItIsCreatedThenFileIsCreated) {
    HardwareInfo hwInfo = *platformDevices[0];
    std::string fileName = "file_name.aub";
    MockAubManager *mockManager = new MockAubManager();
    MockAubCenter *mockAubCenter = new MockAubCenter(&hwInfo, false, fileName, CommandStreamReceiverType::CSR_AUB);
    mockAubCenter->aubManager = std::unique_ptr<MockAubManager>(mockManager);
    ExecutionEnvironment executionEnvironment;
    executionEnvironment.aubCenter = std::unique_ptr<MockAubCenter>(mockAubCenter);

    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(reinterpret_cast<AUBCommandStreamReceiverHw<FamilyType> *>(AUBCommandStreamReceiver::create(hwInfo, fileName, true, executionEnvironment)));
    ASSERT_NE(nullptr, aubCsr);

    EXPECT_TRUE(aubCsr->isFileOpen());
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCsrWhenOsContextIsSetThenCreateHardwareContext) {
    uint32_t engineIndex = 2;
    uint32_t deviceIndex = 3;

    MockOsContext osContext(nullptr, 0, 8, allEngineInstances[engineIndex], PreemptionMode::Disabled);
    std::string fileName = "file_name.aub";
    MockAubManager *mockManager = new MockAubManager();
    MockAubCenter *mockAubCenter = new MockAubCenter(platformDevices[0], false, fileName, CommandStreamReceiverType::CSR_AUB);
    mockAubCenter->aubManager = std::unique_ptr<MockAubManager>(mockManager);
    ExecutionEnvironment executionEnvironment;
    executionEnvironment.aubCenter = std::unique_ptr<MockAubCenter>(mockAubCenter);

    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(reinterpret_cast<AUBCommandStreamReceiverHw<FamilyType> *>(AUBCommandStreamReceiver::create(*platformDevices[0], fileName, true, executionEnvironment)));
    aubCsr->setDeviceIndex(deviceIndex);
    EXPECT_EQ(nullptr, aubCsr->hardwareContextController.get());

    aubCsr->setupContext(osContext);
    EXPECT_NE(nullptr, aubCsr->hardwareContextController.get());
    auto mockHardwareContext = static_cast<MockHardwareContext *>(aubCsr->hardwareContextController->hardwareContexts[0].get());
    EXPECT_EQ(deviceIndex, mockHardwareContext->deviceIndex);
    EXPECT_EQ(engineIndex, mockHardwareContext->engineIndex);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCsrWhenLowPriorityOsContextIsSetThenDontCreateHardwareContext) {
    MockOsContext osContext(nullptr, 0, 1, lowPriorityGpgpuEngine, PreemptionMode::Disabled);
    std::string fileName = "file_name.aub";
    MockAubManager *mockManager = new MockAubManager();
    MockAubCenter *mockAubCenter = new MockAubCenter(platformDevices[0], false, fileName, CommandStreamReceiverType::CSR_AUB);
    mockAubCenter->aubManager = std::unique_ptr<MockAubManager>(mockManager);
    ExecutionEnvironment executionEnvironment;
    executionEnvironment.aubCenter = std::unique_ptr<MockAubCenter>(mockAubCenter);

    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(reinterpret_cast<AUBCommandStreamReceiverHw<FamilyType> *>(AUBCommandStreamReceiver::create(*platformDevices[0], fileName, true, executionEnvironment)));
    EXPECT_EQ(nullptr, aubCsr->hardwareContextController.get());

    aubCsr->setupContext(osContext);
    EXPECT_EQ(nullptr, aubCsr->hardwareContextController.get());
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverInSubCaptureModeWhenItIsCreatedThenFileIsNotCreated) {
    DebugManagerStateRestore stateRestore;
    DebugManager.flags.AUBDumpSubCaptureMode.set(static_cast<int32_t>(AubSubCaptureManager::SubCaptureMode::Filter));
    HardwareInfo hwInfo = *platformDevices[0];
    std::string fileName = "file_name.aub";
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(reinterpret_cast<AUBCommandStreamReceiverHw<FamilyType> *>(AUBCommandStreamReceiver::create(hwInfo, fileName, true, *pDevice->executionEnvironment)));
    EXPECT_NE(nullptr, aubCsr);
    EXPECT_FALSE(aubCsr->isFileOpen());
}

HWTEST_F(AubCommandStreamReceiverTests, givenGraphicsAllocationWhenMakeResidentCalledMultipleTimesAffectsResidencyOnce) {
    std::unique_ptr<MemoryManager> memoryManager(nullptr);
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>(*platformDevices[0], "", true, *pDevice->executionEnvironment));
    memoryManager.reset(new OsAgnosticMemoryManager(false, false, *pDevice->executionEnvironment));
    aubCsr->setupContext(*pDevice->getDefaultEngine().osContext);
    auto gfxAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});

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

    memoryManager->freeGraphicsMemoryImpl(gfxAllocation);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenMultipleInstancesInitializeTheirEnginesThenUniqueGlobalGttAdressesAreGenerated) {
    ExecutionEnvironment executionEnvironment;
    executionEnvironment.aubCenter.reset(new AubCenter());

    auto engineInstance = HwHelper::get(platformDevices[0]->pPlatform->eRenderCoreFamily).getGpgpuEngineInstances()[0];
    MockOsContext osContext(nullptr, 0, 1, engineInstance, PreemptionMode::Disabled);

    auto aubCsr1 = std::make_unique<AUBCommandStreamReceiverHw<FamilyType>>(**platformDevices, "", true, executionEnvironment);
    auto aubCsr2 = std::make_unique<AUBCommandStreamReceiverHw<FamilyType>>(**platformDevices, "", true, executionEnvironment);
    auto engineType = HwHelper::get(platformDevices[0]->pPlatform->eRenderCoreFamily).getGpgpuEngineInstances()[0].type;

    aubCsr1->setupContext(osContext);
    aubCsr1->initializeEngine();
    EXPECT_NE(0u, aubCsr1->engineInfoTable[engineType].ggttLRCA);
    EXPECT_NE(0u, aubCsr1->engineInfoTable[engineType].ggttHWSP);
    EXPECT_NE(0u, aubCsr1->engineInfoTable[engineType].ggttRingBuffer);

    aubCsr2->setupContext(osContext);
    aubCsr2->initializeEngine();
    EXPECT_NE(aubCsr1->engineInfoTable[engineType].ggttLRCA, aubCsr2->engineInfoTable[engineType].ggttLRCA);
    EXPECT_NE(aubCsr1->engineInfoTable[engineType].ggttHWSP, aubCsr2->engineInfoTable[engineType].ggttHWSP);
    EXPECT_NE(aubCsr1->engineInfoTable[engineType].ggttRingBuffer, aubCsr2->engineInfoTable[engineType].ggttRingBuffer);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenFlushIsCalledThenItShouldInitializeEngineInfoTable) {
    auto aubExecutionEnvironment = getEnvironment<AUBCommandStreamReceiverHw<FamilyType>>(true, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<AUBCommandStreamReceiverHw<FamilyType>>();
    aubCsr->hardwareContextController.reset(nullptr);
    LinearStream cs(aubExecutionEnvironment->commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    ResidencyContainer allocationsForResidency = {};
    auto engineIndex = aubCsr->getEngineIndex(aubCsr->getOsContext().getEngineType());
    aubCsr->flush(batchBuffer, allocationsForResidency);
    EXPECT_NE(nullptr, aubCsr->engineInfoTable[engineIndex].pLRCA);
    EXPECT_NE(nullptr, aubCsr->engineInfoTable[engineIndex].pGlobalHWStatusPage);
    EXPECT_NE(nullptr, aubCsr->engineInfoTable[engineIndex].pRingBuffer);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverInSubCaptureModeWhenProcessResidencyIsCalledButSubCaptureIsDisabledThenItShouldntWriteMemory) {
    auto aubExecutionEnvironment = getEnvironment<MockAubCsr<FamilyType>>(true, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<MockAubCsr<FamilyType>>();

    auto aubSubCaptureManagerMock = std::unique_ptr<AubSubCaptureManagerMock>(new AubSubCaptureManagerMock(""));
    aubSubCaptureManagerMock->subCaptureMode = AubSubCaptureManager::SubCaptureMode::Toggle;
    aubSubCaptureManagerMock->disableSubCapture();
    aubCsr->subCaptureManager = aubSubCaptureManagerMock.get();
    ASSERT_FALSE(aubCsr->subCaptureManager->isSubCaptureEnabled());

    MockGraphicsAllocation allocation(reinterpret_cast<void *>(0x1000), 0x1000);
    ResidencyContainer allocationsForResidency = {&allocation};
    aubCsr->processResidency(allocationsForResidency);

    EXPECT_FALSE(aubCsr->writeMemoryCalled);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverInSubCaptureModeWhenProcessResidencyIsCalledButAllocationSizeIsZeroThenItShouldntWriteMemory) {
    DebugManagerStateRestore stateRestore;
    auto aubExecutionEnvironment = getEnvironment<MockAubCsr<FamilyType>>(true, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<MockAubCsr<FamilyType>>();

    const DispatchInfo dispatchInfo;
    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.push(dispatchInfo);
    auto aubSubCaptureManagerMock = std::unique_ptr<AubSubCaptureManagerMock>(new AubSubCaptureManagerMock(""));
    aubSubCaptureManagerMock->subCaptureMode = AubSubCaptureManager::SubCaptureMode::Toggle;
    aubSubCaptureManagerMock->setSubCaptureToggleActive(true);
    aubSubCaptureManagerMock->activateSubCapture(multiDispatchInfo);
    aubCsr->subCaptureManager = aubSubCaptureManagerMock.get();
    ASSERT_TRUE(aubCsr->subCaptureManager->isSubCaptureEnabled());

    MockGraphicsAllocation allocation(reinterpret_cast<void *>(0x1000), 0);
    ResidencyContainer allocationsForResidency = {&allocation};
    aubCsr->processResidency(allocationsForResidency);

    EXPECT_FALSE(aubCsr->writeMemoryCalled);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverInSubCaptureModeWhenFlushIsCalledButSubCaptureIsDisabledThenItShouldntInitializeEngineInfoTable) {
    auto aubExecutionEnvironment = getEnvironment<AUBCommandStreamReceiverHw<FamilyType>>(true, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<AUBCommandStreamReceiverHw<FamilyType>>();
    LinearStream cs(aubExecutionEnvironment->commandBuffer);

    auto aubSubCaptureManagerMock = std::unique_ptr<AubSubCaptureManagerMock>(new AubSubCaptureManagerMock(""));
    aubSubCaptureManagerMock->subCaptureMode = AubSubCaptureManager::SubCaptureMode::Toggle;
    aubSubCaptureManagerMock->disableSubCapture();
    aubCsr->subCaptureManager = aubSubCaptureManagerMock.get();
    ASSERT_FALSE(aubCsr->subCaptureManager->isSubCaptureEnabled());

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    ResidencyContainer allocationsForResidency = {};
    auto engineType = aubCsr->getOsContext().getEngineType().type;
    aubCsr->flush(batchBuffer, allocationsForResidency);
    EXPECT_EQ(nullptr, aubCsr->engineInfoTable[engineType].pLRCA);
    EXPECT_EQ(nullptr, aubCsr->engineInfoTable[engineType].pGlobalHWStatusPage);
    EXPECT_EQ(nullptr, aubCsr->engineInfoTable[engineType].pRingBuffer);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenFlushIsCalledThenItShouldLeaveProperRingTailAlignment) {
    auto aubExecutionEnvironment = getEnvironment<AUBCommandStreamReceiverHw<FamilyType>>(true, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<AUBCommandStreamReceiverHw<FamilyType>>();
    auto allocationsForResidency = aubCsr->getResidencyAllocations();
    LinearStream cs(aubExecutionEnvironment->commandBuffer);

    auto ringTailAlignment = sizeof(uint64_t);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    auto engineType = aubCsr->getOsContext().getEngineType().type;
    // First flush typically includes a preamble and chain to command buffer
    aubCsr->overrideDispatchPolicy(DispatchMode::ImmediateDispatch);
    aubCsr->flush(batchBuffer, allocationsForResidency);
    EXPECT_EQ(0ull, aubCsr->engineInfoTable[engineType].tailRingBuffer % ringTailAlignment);

    // Second flush should just submit command buffer
    cs.getSpace(sizeof(uint64_t));
    aubCsr->flush(batchBuffer, allocationsForResidency);
    EXPECT_EQ(0ull, aubCsr->engineInfoTable[engineType].tailRingBuffer % ringTailAlignment);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverInNonStandaloneModeWhenFlushIsCalledThenItShouldNotUpdateHwTagWithLatestSentTaskCount) {
    auto aubExecutionEnvironment = getEnvironment<MockAubCsr<FamilyType>>(true, true, false);
    auto aubCsr = aubExecutionEnvironment->template getCsr<MockAubCsr<FamilyType>>();
    LinearStream cs(aubExecutionEnvironment->commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
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

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};

    ResidencyContainer allocationsForResidency = {};

    EXPECT_EQ(initialHardwareTag, *aubCsr->getTagAddress());

    aubCsr->setLatestSentTaskCount(aubCsr->peekTaskCount() + 1);

    EXPECT_NE(aubCsr->peekLatestSentTaskCount(), *aubCsr->getTagAddress());

    aubCsr->flush(batchBuffer, allocationsForResidency);

    EXPECT_EQ(aubCsr->peekLatestSentTaskCount(), *aubCsr->getTagAddress());
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverInStandaloneAndSubCaptureModeWhenFlushIsCalledButSubCaptureIsDisabledThenItShouldUpdateHwTagWithLatestSentTaskCount) {
    DebugManagerStateRestore stateRestore;
    auto aubExecutionEnvironment = getEnvironment<MockAubCsr<FamilyType>>(true, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<MockAubCsr<FamilyType>>();
    LinearStream cs(aubExecutionEnvironment->commandBuffer);

    auto aubSubCaptureManagerMock = std::unique_ptr<AubSubCaptureManagerMock>(new AubSubCaptureManagerMock(""));
    aubSubCaptureManagerMock->subCaptureMode = AubSubCaptureManager::SubCaptureMode::Toggle;
    aubSubCaptureManagerMock->disableSubCapture();
    aubCsr->subCaptureManager = aubSubCaptureManagerMock.get();
    ASSERT_FALSE(aubCsr->subCaptureManager->isSubCaptureEnabled());

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};

    ResidencyContainer allocationsForResidency = {};

    aubCsr->setLatestSentTaskCount(aubCsr->peekTaskCount() + 1);

    EXPECT_NE(aubCsr->peekLatestSentTaskCount(), *aubCsr->getTagAddress());

    aubCsr->flush(batchBuffer, allocationsForResidency);

    EXPECT_EQ(aubCsr->peekLatestSentTaskCount(), *aubCsr->getTagAddress());
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverInNonStandaloneAndSubCaptureModeWhenFlushIsCalledButSubCaptureIsDisabledThenItShouldNotUpdateHwTagWithLatestSentTaskCount) {
    DebugManagerStateRestore stateRestore;
    auto aubExecutionEnvironment = getEnvironment<MockAubCsr<FamilyType>>(true, true, false);
    auto aubCsr = aubExecutionEnvironment->template getCsr<MockAubCsr<FamilyType>>();
    LinearStream cs(aubExecutionEnvironment->commandBuffer);

    auto aubSubCaptureManagerMock = std::unique_ptr<AubSubCaptureManagerMock>(new AubSubCaptureManagerMock(""));
    aubSubCaptureManagerMock->subCaptureMode = AubSubCaptureManager::SubCaptureMode::Toggle;
    aubSubCaptureManagerMock->disableSubCapture();
    aubCsr->subCaptureManager = aubSubCaptureManagerMock.get();
    ASSERT_FALSE(aubCsr->subCaptureManager->isSubCaptureEnabled());

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};

    ResidencyContainer allocationsForResidency = {};

    aubCsr->setLatestSentTaskCount(aubCsr->peekTaskCount() + 1);

    EXPECT_NE(aubCsr->peekLatestSentTaskCount(), *aubCsr->getTagAddress());

    aubCsr->flush(batchBuffer, allocationsForResidency);

    EXPECT_NE(aubCsr->peekLatestSentTaskCount(), *aubCsr->getTagAddress());
    EXPECT_EQ(initialHardwareTag, *aubCsr->getTagAddress());
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverInSubCaptureModeWhenFlushIsCalledAndSubCaptureIsEnabledThenItShouldDeactivateSubCapture) {
    DebugManagerStateRestore stateRestore;

    auto aubExecutionEnvironment = getEnvironment<MockAubCsr<FamilyType>>(true, true, false);
    auto aubCsr = aubExecutionEnvironment->template getCsr<MockAubCsr<FamilyType>>();
    LinearStream cs(aubExecutionEnvironment->commandBuffer);

    const DispatchInfo dispatchInfo;
    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.push(dispatchInfo);
    auto aubSubCaptureManagerMock = std::unique_ptr<AubSubCaptureManagerMock>(new AubSubCaptureManagerMock(""));
    aubSubCaptureManagerMock->subCaptureMode = AubSubCaptureManager::SubCaptureMode::Toggle;
    aubSubCaptureManagerMock->setSubCaptureToggleActive(true);
    aubSubCaptureManagerMock->activateSubCapture(multiDispatchInfo);
    aubCsr->subCaptureManager = aubSubCaptureManagerMock.get();
    ASSERT_TRUE(aubCsr->subCaptureManager->isSubCaptureEnabled());

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};

    ResidencyContainer allocationsForResidency = {};

    aubCsr->flush(batchBuffer, allocationsForResidency);

    EXPECT_FALSE(aubCsr->subCaptureManager->isSubCaptureEnabled());
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverInStandaloneModeWhenFlushIsCalledThenItShouldCallMakeResidentOnCommandBufferAllocation) {
    auto aubExecutionEnvironment = getEnvironment<MockAubCsr<FamilyType>>(true, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<MockAubCsr<FamilyType>>();
    auto allocationsForResidency = aubCsr->getResidencyAllocations();
    auto commandBuffer = aubExecutionEnvironment->commandBuffer;
    LinearStream cs(commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};

    EXPECT_FALSE(commandBuffer->isResident(aubCsr->getOsContext().getContextId()));

    aubCsr->overrideDispatchPolicy(DispatchMode::ImmediateDispatch);
    aubCsr->flush(batchBuffer, allocationsForResidency);

    EXPECT_TRUE(commandBuffer->isResident(aubCsr->getOsContext().getContextId()));
    EXPECT_EQ(aubCsr->peekTaskCount() + 1, commandBuffer->getResidencyTaskCount(aubCsr->getOsContext().getContextId()));

    aubCsr->makeSurfacePackNonResident(aubCsr->getResidencyAllocations());

    EXPECT_FALSE(commandBuffer->isResident(aubCsr->getOsContext().getContextId()));
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverInNonStandaloneModeWhenFlushIsCalledThenItShouldNotCallMakeResidentOnCommandBufferAllocation) {
    auto aubExecutionEnvironment = getEnvironment<AUBCommandStreamReceiverHw<FamilyType>>(false, true, false);
    auto aubCsr = aubExecutionEnvironment->template getCsr<AUBCommandStreamReceiverHw<FamilyType>>();
    auto allocationsForResidency = aubCsr->getResidencyAllocations();
    LinearStream cs(aubExecutionEnvironment->commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};

    EXPECT_FALSE(aubExecutionEnvironment->commandBuffer->isResident(aubCsr->getOsContext().getContextId()));

    aubCsr->flush(batchBuffer, allocationsForResidency);

    EXPECT_FALSE(aubExecutionEnvironment->commandBuffer->isResident(aubCsr->getOsContext().getContextId()));
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverInStandaloneModeWhenFlushIsCalledThenItShouldCallMakeResidentOnResidencyAllocations) {
    auto aubExecutionEnvironment = getEnvironment<MockAubCsr<FamilyType>>(true, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<MockAubCsr<FamilyType>>();
    auto memoryManager = aubExecutionEnvironment->executionEnvironment->memoryManager.get();
    auto commandBuffer = aubExecutionEnvironment->commandBuffer;
    LinearStream cs(commandBuffer);

    auto gfxAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});
    ASSERT_NE(nullptr, gfxAllocation);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};

    ResidencyContainer allocationsForResidency = {gfxAllocation};

    EXPECT_FALSE(gfxAllocation->isResident(aubCsr->getOsContext().getContextId()));
    EXPECT_FALSE(commandBuffer->isResident(aubCsr->getOsContext().getContextId()));

    aubCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);
    aubCsr->flush(batchBuffer, allocationsForResidency);

    EXPECT_TRUE(gfxAllocation->isResident(aubCsr->getOsContext().getContextId()));
    EXPECT_EQ(aubCsr->peekTaskCount() + 1, gfxAllocation->getResidencyTaskCount(aubCsr->getOsContext().getContextId()));

    EXPECT_TRUE(commandBuffer->isResident(aubCsr->getOsContext().getContextId()));
    EXPECT_EQ(aubCsr->peekTaskCount() + 1, commandBuffer->getResidencyTaskCount(aubCsr->getOsContext().getContextId()));

    aubCsr->makeSurfacePackNonResident(allocationsForResidency);

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

    auto gfxAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};

    ResidencyContainer allocationsForResidency = {gfxAllocation};

    EXPECT_FALSE(gfxAllocation->isResident(aubCsr->getOsContext().getContextId()));
    EXPECT_FALSE(commandBuffer->isResident(aubCsr->getOsContext().getContextId()));

    aubCsr->flush(batchBuffer, allocationsForResidency);

    EXPECT_TRUE(gfxAllocation->isResident(aubCsr->getOsContext().getContextId()));
    EXPECT_EQ(aubCsr->peekTaskCount() + 1, gfxAllocation->getResidencyTaskCount(aubCsr->getOsContext().getContextId()));

    EXPECT_FALSE(commandBuffer->isResident(aubCsr->getOsContext().getContextId()));

    memoryManager->freeGraphicsMemoryImpl(gfxAllocation);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverInStandaloneAndSubCaptureModeWhenFlushIsCalledAndSubCaptureIsEnabledThenItShouldCallMakeResidentOnCommandBufferAndResidencyAllocations) {
    DebugManagerStateRestore stateRestore;
    auto aubExecutionEnvironment = getEnvironment<MockAubCsr<FamilyType>>(true, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<MockAubCsr<FamilyType>>();
    auto memoryManager = aubExecutionEnvironment->executionEnvironment->memoryManager.get();
    auto commandBuffer = aubExecutionEnvironment->commandBuffer;
    LinearStream cs(commandBuffer);

    const DispatchInfo dispatchInfo;
    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.push(dispatchInfo);
    auto aubSubCaptureManagerMock = std::unique_ptr<AubSubCaptureManagerMock>(new AubSubCaptureManagerMock(""));
    aubSubCaptureManagerMock->subCaptureMode = AubSubCaptureManager::SubCaptureMode::Toggle;
    aubSubCaptureManagerMock->setSubCaptureToggleActive(true);
    aubSubCaptureManagerMock->activateSubCapture(multiDispatchInfo);
    aubCsr->subCaptureManager = aubSubCaptureManagerMock.get();
    ASSERT_TRUE(aubCsr->subCaptureManager->isSubCaptureEnabled());

    auto gfxAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});
    ASSERT_NE(nullptr, gfxAllocation);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};

    ResidencyContainer allocationsForResidency = {gfxAllocation};

    EXPECT_FALSE(gfxAllocation->isResident(aubCsr->getOsContext().getContextId()));
    EXPECT_FALSE(commandBuffer->isResident(aubCsr->getOsContext().getContextId()));

    aubCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);
    aubCsr->flush(batchBuffer, allocationsForResidency);

    EXPECT_TRUE(gfxAllocation->isResident(aubCsr->getOsContext().getContextId()));
    EXPECT_EQ(aubCsr->peekTaskCount() + 1, gfxAllocation->getResidencyTaskCount(aubCsr->getOsContext().getContextId()));

    EXPECT_TRUE(commandBuffer->isResident(aubCsr->getOsContext().getContextId()));
    EXPECT_EQ(aubCsr->peekTaskCount() + 1, commandBuffer->getResidencyTaskCount(aubCsr->getOsContext().getContextId()));

    aubCsr->makeSurfacePackNonResident(allocationsForResidency);

    EXPECT_FALSE(gfxAllocation->isResident(aubCsr->getOsContext().getContextId()));
    EXPECT_FALSE(commandBuffer->isResident(aubCsr->getOsContext().getContextId()));

    memoryManager->freeGraphicsMemory(gfxAllocation);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenGraphicsAllocationIsCreatedThenItDoesntHaveTypeNonAubWritable) {
    auto aubExecutionEnvironment = getEnvironment<AUBCommandStreamReceiverHw<FamilyType>>(false, false, true);
    auto memoryManager = aubExecutionEnvironment->executionEnvironment->memoryManager.get();

    auto gfxAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});

    EXPECT_TRUE(gfxAllocation->isAubWritable());

    memoryManager->freeGraphicsMemory(gfxAllocation);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenProcessResidencyIsCalledOnDefaultAllocationThenAllocationTypeShouldNotBeMadeNonAubWritable) {
    auto aubExecutionEnvironment = getEnvironment<AUBCommandStreamReceiverHw<FamilyType>>(false, false, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<AUBCommandStreamReceiverHw<FamilyType>>();
    auto memoryManager = aubExecutionEnvironment->executionEnvironment->memoryManager.get();

    auto gfxDefaultAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});

    ResidencyContainer allocationsForResidency = {gfxDefaultAllocation};
    aubCsr->processResidency(allocationsForResidency);

    EXPECT_TRUE(gfxDefaultAllocation->isAubWritable());

    memoryManager->freeGraphicsMemory(gfxDefaultAllocation);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenWriteMemoryIsCalledOnBufferAndImageTypeAllocationsThenAllocationsHaveAubWritableSetToFalse) {
    std::unique_ptr<MemoryManager> memoryManager(nullptr);

    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>(*platformDevices[0], "", true, *pDevice->executionEnvironment));
    aubCsr->setupContext(*pDevice->getDefaultEngine().osContext);
    memoryManager.reset(new OsAgnosticMemoryManager(false, false, *pDevice->executionEnvironment));

    auto gfxAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});

    const GraphicsAllocation::AllocationType onlyOneTimeAubWritableTypes[] = {
        GraphicsAllocation::AllocationType::BUFFER,
        GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY,
        GraphicsAllocation::AllocationType::BUFFER_COMPRESSED,
        GraphicsAllocation::AllocationType::IMAGE,
        GraphicsAllocation::AllocationType::TIMESTAMP_PACKET_TAG_BUFFER};

    for (auto allocationType : onlyOneTimeAubWritableTypes) {
        gfxAllocation->setAubWritable(true);
        gfxAllocation->setAllocationType(allocationType);
        aubCsr->writeMemory(*gfxAllocation);

        EXPECT_FALSE(gfxAllocation->isAubWritable());
    }
    memoryManager->freeGraphicsMemory(gfxAllocation);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenProcessResidencyIsCalledOnBufferAndImageAllocationsThenAllocationsTypesShouldBeMadeNonAubWritable) {
    std::unique_ptr<MemoryManager> memoryManager(nullptr);
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>(*platformDevices[0], "", true, *pDevice->executionEnvironment));
    memoryManager.reset(new OsAgnosticMemoryManager(false, false, *pDevice->executionEnvironment));
    aubCsr->setupContext(*pDevice->getDefaultEngine().osContext);

    auto gfxBufferAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize, GraphicsAllocation::AllocationType::BUFFER});

    auto gfxImageAllocation = MockGmm::allocateImage2d(*memoryManager);

    ResidencyContainer allocationsForResidency = {gfxBufferAllocation, gfxImageAllocation};
    aubCsr->processResidency(allocationsForResidency);

    EXPECT_FALSE(gfxBufferAllocation->isAubWritable());
    EXPECT_FALSE(gfxImageAllocation->isAubWritable());

    memoryManager->freeGraphicsMemory(gfxBufferAllocation);
    memoryManager->freeGraphicsMemory(gfxImageAllocation);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverInSubCaptureModWhenProcessResidencyIsCalledWithDumpAubNonWritableFlagThenAllocationsTypesShouldBeMadeAubWritable) {
    DebugManagerStateRestore stateRestore;
    std::unique_ptr<MemoryManager> memoryManager(nullptr);
    std::unique_ptr<MockAubCsrToTestDumpAubNonWritable<FamilyType>> aubCsr(new MockAubCsrToTestDumpAubNonWritable<FamilyType>(*platformDevices[0], "", true, *pDevice->executionEnvironment));
    memoryManager.reset(new OsAgnosticMemoryManager(false, false, *pDevice->executionEnvironment));
    aubCsr->setupContext(*pDevice->getDefaultEngine().osContext);

    auto gfxBufferAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize, GraphicsAllocation::AllocationType::BUFFER});
    gfxBufferAllocation->setAubWritable(false);

    auto gfxImageAllocation = MockGmm::allocateImage2d(*memoryManager);
    gfxImageAllocation->setAubWritable(false);

    aubCsr->dumpAubNonWritable = true;

    ResidencyContainer allocationsForResidency = {gfxBufferAllocation, gfxImageAllocation};
    aubCsr->processResidency(allocationsForResidency);

    EXPECT_TRUE(gfxBufferAllocation->isAubWritable());
    EXPECT_TRUE(gfxImageAllocation->isAubWritable());

    memoryManager->freeGraphicsMemory(gfxBufferAllocation);
    memoryManager->freeGraphicsMemory(gfxImageAllocation);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenProcessResidencyIsCalledWithoutDumpAubWritableFlagThenAllocationsTypesShouldBeKeptNonAubWritable) {
    DebugManagerStateRestore stateRestore;
    std::unique_ptr<MemoryManager> memoryManager(nullptr);
    std::unique_ptr<MockAubCsrToTestDumpAubNonWritable<FamilyType>> aubCsr(new MockAubCsrToTestDumpAubNonWritable<FamilyType>(*platformDevices[0], "", true, *pDevice->executionEnvironment));
    memoryManager.reset(new OsAgnosticMemoryManager(false, false, *pDevice->executionEnvironment));
    aubCsr->setupContext(*pDevice->getDefaultEngine().osContext);

    auto gfxBufferAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize, GraphicsAllocation::AllocationType::BUFFER});
    gfxBufferAllocation->setAubWritable(false);

    auto gfxImageAllocation = MockGmm::allocateImage2d(*memoryManager);
    gfxImageAllocation->setAubWritable(false);

    aubCsr->dumpAubNonWritable = false;

    ResidencyContainer allocationsForResidency = {gfxBufferAllocation, gfxImageAllocation};
    aubCsr->processResidency(allocationsForResidency);

    EXPECT_FALSE(gfxBufferAllocation->isAubWritable());
    EXPECT_FALSE(gfxImageAllocation->isAubWritable());

    memoryManager->freeGraphicsMemory(gfxBufferAllocation);
    memoryManager->freeGraphicsMemory(gfxImageAllocation);
}

HWTEST_F(AubCommandStreamReceiverTests, givenOsContextWithMultipleDevicesSupportedWhenSetupIsCalledThenCreateMultipleHardwareContexts) {
    MockAubCenter *mockAubCenter = new MockAubCenter(platformDevices[0], false, "", CommandStreamReceiverType::CSR_AUB);
    mockAubCenter->aubManager = std::make_unique<MockAubManager>();
    pDevice->executionEnvironment->aubCenter.reset(mockAubCenter);

    uint32_t deviceBitfield = 3;
    MockOsContext osContext(nullptr, 1, deviceBitfield, {EngineType::ENGINE_RCS, 0}, PreemptionMode::Disabled);
    auto aubCsr = std::make_unique<AUBCommandStreamReceiverHw<FamilyType>>(*platformDevices[0], "", true, *pDevice->executionEnvironment);
    aubCsr->setupContext(osContext);

    EXPECT_EQ(2u, aubCsr->hardwareContextController->hardwareContexts.size());
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenGraphicsAllocationTypeIsntNonAubWritableThenWriteMemoryIsAllowed) {
    std::unique_ptr<MemoryManager> memoryManager(nullptr);
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>(*platformDevices[0], "", true, *pDevice->executionEnvironment));
    aubCsr->setupContext(*pDevice->getDefaultEngine().osContext);

    memoryManager.reset(new OsAgnosticMemoryManager(false, false, *pDevice->executionEnvironment));

    auto gfxAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});

    EXPECT_TRUE(aubCsr->writeMemory(*gfxAllocation));

    memoryManager->freeGraphicsMemory(gfxAllocation);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenGraphicsAllocationTypeIsNonAubWritableThenWriteMemoryIsNotAllowed) {
    std::unique_ptr<MemoryManager> memoryManager(nullptr);
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>(*platformDevices[0], "", true, *pDevice->executionEnvironment));
    memoryManager.reset(new OsAgnosticMemoryManager(false, false, *pDevice->executionEnvironment));

    auto gfxAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});

    gfxAllocation->setAubWritable(false);
    EXPECT_FALSE(aubCsr->writeMemory(*gfxAllocation));

    memoryManager->freeGraphicsMemory(gfxAllocation);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenGraphicsAllocationSizeIsZeroThenWriteMemoryIsNotAllowed) {
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>(*platformDevices[0], "", true, *pDevice->executionEnvironment));
    MockGraphicsAllocation gfxAllocation((void *)0x1234, 0);

    EXPECT_FALSE(aubCsr->writeMemory(gfxAllocation));
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenAllocationDataIsPassedInAllocationViewThenWriteMemoryIsAllowed) {
    auto aubCsr = std::make_unique<AUBCommandStreamReceiverHw<FamilyType>>(*platformDevices[0], "", true, *pDevice->executionEnvironment);
    aubCsr->setupContext(*pDevice->getDefaultEngine().osContext);
    size_t size = 100;
    auto ptr = std::make_unique<char[]>(size);
    auto addr = reinterpret_cast<uint64_t>(ptr.get());
    AllocationView allocationView(addr, size);

    EXPECT_TRUE(aubCsr->writeMemory(allocationView));
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenAllocationSizeInAllocationViewIsZeroThenWriteMemoryIsNotAllowed) {
    auto aubCsr = std::make_unique<AUBCommandStreamReceiverHw<FamilyType>>(*platformDevices[0], "", true, *pDevice->executionEnvironment);
    AllocationView allocationView(0x1234, 0);

    EXPECT_FALSE(aubCsr->writeMemory(allocationView));
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenAUBDumpCaptureFileNameHasBeenSpecifiedThenItShouldBeUsedToOpenTheFileWithAubCapture) {
    DebugManagerStateRestore stateRestore;
    DebugManager.flags.AUBDumpCaptureFileName.set("file_name.aub");
    pDevice->executionEnvironment->aubCenter.reset(new AubCenter());

    std::unique_ptr<MockAubCsr<FamilyType>> aubCsr(static_cast<MockAubCsr<FamilyType> *>(AUBCommandStreamReceiver::create(*platformDevices[0], "", true, *pDevice->executionEnvironment)));
    EXPECT_NE(nullptr, aubCsr);

    EXPECT_TRUE(aubCsr->isFileOpen());
    EXPECT_STREQ(DebugManager.flags.AUBDumpCaptureFileName.get().c_str(), aubCsr->getFileName().c_str());
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverInSubCaptureModeWhenAubSubCaptureIsActivatedThenFileIsOpened) {
    DebugManagerStateRestore stateRestore;
    std::unique_ptr<MockAubCsr<FamilyType>> aubCsr(new MockAubCsr<FamilyType>(*platformDevices[0], "", false, *pDevice->executionEnvironment));

    auto subCaptureManagerMock = std::unique_ptr<AubSubCaptureManagerMock>(new AubSubCaptureManagerMock(""));
    subCaptureManagerMock->subCaptureMode = AubSubCaptureManager::SubCaptureMode::Toggle;
    subCaptureManagerMock->setSubCaptureIsActive(false);
    subCaptureManagerMock->setSubCaptureToggleActive(true);
    aubCsr->subCaptureManager = subCaptureManagerMock.get();

    MockKernelWithInternals kernelInternals(*pDevice);
    Kernel *kernel = kernelInternals.mockKernel;
    MockMultiDispatchInfo multiDispatchInfo(kernel);

    ASSERT_FALSE(aubCsr->isFileOpen());

    aubCsr->activateAubSubCapture(multiDispatchInfo);

    EXPECT_TRUE(aubCsr->isFileOpen());
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverInSubCaptureModeWhenAubSubCaptureRemainsActivedThenTheSameFileShouldBeKeptOpened) {
    DebugManagerStateRestore stateRestore;
    std::unique_ptr<MockAubCsr<FamilyType>> aubCsr(new MockAubCsr<FamilyType>(*platformDevices[0], "", false, *pDevice->executionEnvironment));

    auto subCaptureManagerMock = std::unique_ptr<AubSubCaptureManagerMock>(new AubSubCaptureManagerMock(""));
    subCaptureManagerMock->subCaptureMode = AubSubCaptureManager::SubCaptureMode::Toggle;
    subCaptureManagerMock->setSubCaptureIsActive(false);
    subCaptureManagerMock->setSubCaptureToggleActive(true);
    aubCsr->subCaptureManager = subCaptureManagerMock.get();

    MockKernelWithInternals kernelInternals(*pDevice);
    Kernel *kernel = kernelInternals.mockKernel;
    MockMultiDispatchInfo multiDispatchInfo(kernel);

    std::string fileName = aubCsr->subCaptureManager->getSubCaptureFileName(multiDispatchInfo);
    aubCsr->initFile(fileName);
    ASSERT_TRUE(aubCsr->isFileOpen());

    aubCsr->activateAubSubCapture(multiDispatchInfo);

    EXPECT_TRUE(aubCsr->isFileOpen());
    EXPECT_STREQ(fileName.c_str(), aubCsr->getFileName().c_str());
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverInSubCaptureModeWhenAubSubCaptureIsActivatedWithNewFileNameThenNewFileShouldBeReOpened) {
    DebugManagerStateRestore stateRestore;
    std::unique_ptr<MockAubCsr<FamilyType>> aubCsr(new MockAubCsr<FamilyType>(*platformDevices[0], "", false, *pDevice->executionEnvironment));
    std::string newFileName = "new_file_name.aub";

    auto subCaptureManagerMock = std::unique_ptr<AubSubCaptureManagerMock>(new AubSubCaptureManagerMock(""));
    subCaptureManagerMock->subCaptureMode = AubSubCaptureManager::SubCaptureMode::Toggle;
    subCaptureManagerMock->setSubCaptureIsActive(false);
    subCaptureManagerMock->setSubCaptureToggleActive(true);
    subCaptureManagerMock->setExternalFileName(newFileName);
    aubCsr->subCaptureManager = subCaptureManagerMock.get();

    MockKernelWithInternals kernelInternals(*pDevice);
    Kernel *kernel = kernelInternals.mockKernel;
    MockMultiDispatchInfo multiDispatchInfo(kernel);

    std::string fileName = "file_name.aub";
    aubCsr->initFile(fileName);
    ASSERT_TRUE(aubCsr->isFileOpen());
    ASSERT_STREQ(fileName.c_str(), aubCsr->getFileName().c_str());

    aubCsr->activateAubSubCapture(multiDispatchInfo);

    EXPECT_TRUE(aubCsr->isFileOpen());
    EXPECT_STRNE(fileName.c_str(), aubCsr->getFileName().c_str());
    EXPECT_STREQ(newFileName.c_str(), aubCsr->getFileName().c_str());
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverInSubCaptureModeWhenAubSubCaptureIsActivatedForNewFileThenOldEngineInfoTableShouldBeFreed) {
    DebugManagerStateRestore stateRestore;
    std::unique_ptr<MockAubCsr<FamilyType>> aubCsr(new MockAubCsr<FamilyType>(*platformDevices[0], "", false, *pDevice->executionEnvironment));
    std::string newFileName = "new_file_name.aub";

    auto subCaptureManagerMock = std::unique_ptr<AubSubCaptureManagerMock>(new AubSubCaptureManagerMock(""));
    subCaptureManagerMock->subCaptureMode = AubSubCaptureManager::SubCaptureMode::Toggle;
    subCaptureManagerMock->setSubCaptureIsActive(false);
    subCaptureManagerMock->setSubCaptureToggleActive(true);
    subCaptureManagerMock->setExternalFileName(newFileName);
    aubCsr->subCaptureManager = subCaptureManagerMock.get();

    MockKernelWithInternals kernelInternals(*pDevice);
    Kernel *kernel = kernelInternals.mockKernel;
    MockMultiDispatchInfo multiDispatchInfo(kernel);

    std::string fileName = "file_name.aub";
    aubCsr->initFile(fileName);
    ASSERT_STREQ(fileName.c_str(), aubCsr->getFileName().c_str());

    aubCsr->activateAubSubCapture(multiDispatchInfo);
    ASSERT_STREQ(newFileName.c_str(), aubCsr->getFileName().c_str());

    for (auto &engineInfo : aubCsr->engineInfoTable) {
        EXPECT_EQ(nullptr, engineInfo.pLRCA);
        EXPECT_EQ(nullptr, engineInfo.pGlobalHWStatusPage);
        EXPECT_EQ(nullptr, engineInfo.pRingBuffer);
    }
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverInSubCaptureModeWhenAubSubCaptureIsActivatedThenForceDumpingAllocationsAubNonWritable) {
    DebugManagerStateRestore stateRestore;
    std::unique_ptr<MockAubCsrToTestDumpAubNonWritable<FamilyType>> aubCsr(new MockAubCsrToTestDumpAubNonWritable<FamilyType>(*platformDevices[0], "", false, *pDevice->executionEnvironment));

    auto subCaptureManagerMock = std::unique_ptr<AubSubCaptureManagerMock>(new AubSubCaptureManagerMock(""));
    subCaptureManagerMock->subCaptureMode = AubSubCaptureManager::SubCaptureMode::Toggle;
    subCaptureManagerMock->setSubCaptureIsActive(false);
    subCaptureManagerMock->setSubCaptureToggleActive(true);
    aubCsr->subCaptureManager = subCaptureManagerMock.get();

    MockKernelWithInternals kernelInternals(*pDevice);
    Kernel *kernel = kernelInternals.mockKernel;
    MockMultiDispatchInfo multiDispatchInfo(kernel);

    aubCsr->activateAubSubCapture(multiDispatchInfo);

    EXPECT_TRUE(aubCsr->dumpAubNonWritable);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverInSubCaptureModeWhenAubSubCaptureRemainsActivatedThenDontForceDumpingAllocationsAubNonWritable) {
    DebugManagerStateRestore stateRestore;
    std::unique_ptr<MockAubCsrToTestDumpAubNonWritable<FamilyType>> aubCsr(new MockAubCsrToTestDumpAubNonWritable<FamilyType>(*platformDevices[0], "", false, *pDevice->executionEnvironment));

    auto subCaptureManagerMock = std::unique_ptr<AubSubCaptureManagerMock>(new AubSubCaptureManagerMock(""));
    subCaptureManagerMock->subCaptureMode = AubSubCaptureManager::SubCaptureMode::Toggle;
    subCaptureManagerMock->setSubCaptureIsActive(false);
    subCaptureManagerMock->setSubCaptureToggleActive(true);
    aubCsr->subCaptureManager = subCaptureManagerMock.get();

    MockKernelWithInternals kernelInternals(*pDevice);
    Kernel *kernel = kernelInternals.mockKernel;
    MockMultiDispatchInfo multiDispatchInfo(kernel);

    aubCsr->initFile(aubCsr->subCaptureManager->getSubCaptureFileName(multiDispatchInfo));
    aubCsr->activateAubSubCapture(multiDispatchInfo);

    EXPECT_FALSE(aubCsr->dumpAubNonWritable);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverInSubCaptureModeWhenSubCaptureModeRemainsDeactivatedThenSubCaptureIsDisabled) {
    DebugManagerStateRestore stateRestore;
    std::unique_ptr<MockAubCsr<FamilyType>> aubCsr(new MockAubCsr<FamilyType>(*platformDevices[0], "", false, *pDevice->executionEnvironment));

    auto subCaptureManagerMock = std::unique_ptr<AubSubCaptureManagerMock>(new AubSubCaptureManagerMock(""));
    subCaptureManagerMock->subCaptureMode = AubSubCaptureManager::SubCaptureMode::Toggle;
    subCaptureManagerMock->setSubCaptureIsActive(false);
    subCaptureManagerMock->setSubCaptureToggleActive(false);
    aubCsr->subCaptureManager = subCaptureManagerMock.get();

    const DispatchInfo dispatchInfo;
    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.push(dispatchInfo);
    aubCsr->activateAubSubCapture(multiDispatchInfo);

    EXPECT_FALSE(aubCsr->subCaptureManager->isSubCaptureEnabled());
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverInSubCaptureModeWhenSubCaptureIsToggledOnThenSubCaptureGetsEnabled) {
    DebugManagerStateRestore stateRestore;
    std::unique_ptr<MockAubCsr<FamilyType>> aubCsr(new MockAubCsr<FamilyType>(*platformDevices[0], "", false, *pDevice->executionEnvironment));

    auto subCaptureManagerMock = std::unique_ptr<AubSubCaptureManagerMock>(new AubSubCaptureManagerMock(""));
    subCaptureManagerMock->subCaptureMode = AubSubCaptureManager::SubCaptureMode::Toggle;
    subCaptureManagerMock->setSubCaptureIsActive(false);
    subCaptureManagerMock->setSubCaptureToggleActive(true);
    aubCsr->subCaptureManager = subCaptureManagerMock.get();

    MockKernelWithInternals kernelInternals(*pDevice);
    Kernel *kernel = kernelInternals.mockKernel;
    MockMultiDispatchInfo multiDispatchInfo(kernel);
    aubCsr->activateAubSubCapture(multiDispatchInfo);

    EXPECT_TRUE(aubCsr->subCaptureManager->isSubCaptureEnabled());
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverInStandaloneAndSubCaptureModeWhenSubCaptureRemainsDeactivatedThenNeitherProgrammingFlagsAreInitializedNorCsrIsFlushed) {
    DebugManagerStateRestore stateRestore;
    std::unique_ptr<MockAubCsr<FamilyType>> aubCsr(new MockAubCsr<FamilyType>(*platformDevices[0], "", true, *pDevice->executionEnvironment));

    auto subCaptureManagerMock = std::unique_ptr<AubSubCaptureManagerMock>(new AubSubCaptureManagerMock(""));
    subCaptureManagerMock->subCaptureMode = AubSubCaptureManager::SubCaptureMode::Toggle;
    subCaptureManagerMock->setSubCaptureIsActive(false);
    subCaptureManagerMock->setSubCaptureToggleActive(false);
    aubCsr->subCaptureManager = subCaptureManagerMock.get();

    MockKernelWithInternals kernelInternals(*pDevice);
    Kernel *kernel = kernelInternals.mockKernel;
    MockMultiDispatchInfo multiDispatchInfo(kernel);
    aubCsr->activateAubSubCapture(multiDispatchInfo);

    EXPECT_FALSE(aubCsr->flushBatchedSubmissionsCalled);
    EXPECT_FALSE(aubCsr->initProgrammingFlagsCalled);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverInStandaloneAndSubCaptureModeWhenSubCaptureRemainsActivatedThenNeitherProgrammingFlagsAreInitializedNorCsrIsFlushed) {
    DebugManagerStateRestore stateRestore;
    std::unique_ptr<MockAubCsr<FamilyType>> aubCsr(new MockAubCsr<FamilyType>(*platformDevices[0], "", true, *pDevice->executionEnvironment));

    auto subCaptureManagerMock = std::unique_ptr<AubSubCaptureManagerMock>(new AubSubCaptureManagerMock(""));
    subCaptureManagerMock->subCaptureMode = AubSubCaptureManager::SubCaptureMode::Toggle;
    subCaptureManagerMock->setSubCaptureIsActive(true);
    subCaptureManagerMock->setSubCaptureToggleActive(true);
    aubCsr->subCaptureManager = subCaptureManagerMock.get();

    MockKernelWithInternals kernelInternals(*pDevice);
    Kernel *kernel = kernelInternals.mockKernel;
    MockMultiDispatchInfo multiDispatchInfo(kernel);
    aubCsr->activateAubSubCapture(multiDispatchInfo);

    EXPECT_FALSE(aubCsr->flushBatchedSubmissionsCalled);
    EXPECT_FALSE(aubCsr->initProgrammingFlagsCalled);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverInStandaloneAndSubCaptureModeWhenSubCaptureGetsActivatedThenProgrammingFlagsAreInitialized) {
    DebugManagerStateRestore stateRestore;
    std::unique_ptr<MockAubCsr<FamilyType>> aubCsr(new MockAubCsr<FamilyType>(*platformDevices[0], "", true, *pDevice->executionEnvironment));

    auto subCaptureManagerMock = std::unique_ptr<AubSubCaptureManagerMock>(new AubSubCaptureManagerMock(""));
    subCaptureManagerMock->subCaptureMode = AubSubCaptureManager::SubCaptureMode::Toggle;
    subCaptureManagerMock->setSubCaptureIsActive(false);
    subCaptureManagerMock->setSubCaptureToggleActive(true);
    aubCsr->subCaptureManager = subCaptureManagerMock.get();

    MockKernelWithInternals kernelInternals(*pDevice);
    Kernel *kernel = kernelInternals.mockKernel;
    MockMultiDispatchInfo multiDispatchInfo(kernel);
    aubCsr->activateAubSubCapture(multiDispatchInfo);

    EXPECT_FALSE(aubCsr->flushBatchedSubmissionsCalled);
    EXPECT_TRUE(aubCsr->initProgrammingFlagsCalled);
}

using HardwareContextContainerTests = ::testing::Test;

TEST_F(HardwareContextContainerTests, givenOsContextWithMultipleDevicesSupportedThenInitialzeHwContextsWithValidIndexes) {
    MockAubManager aubManager;
    uint32_t deviceBitfield = 3;
    MockOsContext osContext(nullptr, 1, deviceBitfield, {EngineType::ENGINE_RCS, 0}, PreemptionMode::Disabled);

    HardwareContextController hwContextControler(aubManager, osContext, 0, 0);
    EXPECT_EQ(2u, hwContextControler.hardwareContexts.size());
    EXPECT_EQ(2u, osContext.getNumSupportedDevices());
    auto mockHwContext0 = static_cast<MockHardwareContext *>(hwContextControler.hardwareContexts[0].get());
    auto mockHwContext1 = static_cast<MockHardwareContext *>(hwContextControler.hardwareContexts[1].get());
    EXPECT_EQ(0u, mockHwContext0->deviceIndex);
    EXPECT_EQ(1u, mockHwContext1->deviceIndex);
}

TEST_F(HardwareContextContainerTests, givenMultipleHwContextWhenSingleMethodIsCalledThenUseAllContexts) {
    MockAubManager aubManager;
    uint32_t deviceBitfield = 3;
    MockOsContext osContext(nullptr, 1, deviceBitfield, {EngineType::ENGINE_RCS, 0}, PreemptionMode::Disabled);
    HardwareContextController hwContextContainer(aubManager, osContext, deviceBitfield, 0);
    EXPECT_EQ(2u, hwContextContainer.hardwareContexts.size());

    auto mockHwContext0 = static_cast<MockHardwareContext *>(hwContextContainer.hardwareContexts[0].get());
    auto mockHwContext1 = static_cast<MockHardwareContext *>(hwContextContainer.hardwareContexts[1].get());

    EXPECT_FALSE(mockHwContext0->initializeCalled);
    EXPECT_FALSE(mockHwContext1->initializeCalled);
    EXPECT_FALSE(mockHwContext0->pollForCompletionCalled);
    EXPECT_FALSE(mockHwContext1->pollForCompletionCalled);
    EXPECT_FALSE(mockHwContext0->expectMemoryCalled);
    EXPECT_FALSE(mockHwContext1->expectMemoryCalled);
    EXPECT_FALSE(mockHwContext0->submitCalled);
    EXPECT_FALSE(mockHwContext1->submitCalled);
    EXPECT_FALSE(mockHwContext0->writeMemoryCalled);
    EXPECT_FALSE(mockHwContext1->writeMemoryCalled);

    hwContextContainer.initialize();
    hwContextContainer.pollForCompletion();
    hwContextContainer.expectMemory(1, reinterpret_cast<const void *>(0x123), 2, 0);
    hwContextContainer.submit(1, reinterpret_cast<const void *>(0x123), 2, 0, 1);
    hwContextContainer.writeMemory(1, reinterpret_cast<const void *>(0x123), 2, 3, 4, 5);

    EXPECT_TRUE(mockHwContext0->initializeCalled);
    EXPECT_TRUE(mockHwContext1->initializeCalled);
    EXPECT_TRUE(mockHwContext0->pollForCompletionCalled);
    EXPECT_TRUE(mockHwContext1->pollForCompletionCalled);
    EXPECT_TRUE(mockHwContext0->expectMemoryCalled);
    EXPECT_TRUE(mockHwContext1->expectMemoryCalled);
    EXPECT_TRUE(mockHwContext0->submitCalled);
    EXPECT_TRUE(mockHwContext1->submitCalled);
    EXPECT_TRUE(mockHwContext0->writeMemoryCalled);
    EXPECT_TRUE(mockHwContext1->writeMemoryCalled);
}

TEST_F(HardwareContextContainerTests, givenMultipleHwContextWhenSingleMethodIsCalledThenUseFirstContext) {
    MockAubManager aubManager;
    uint32_t deviceBitfield = 3;
    MockOsContext osContext(nullptr, 1, deviceBitfield, {EngineType::ENGINE_RCS, 0}, PreemptionMode::Disabled);
    HardwareContextController hwContextContainer(aubManager, osContext, 2, 0);
    EXPECT_EQ(2u, hwContextContainer.hardwareContexts.size());

    auto mockHwContext0 = static_cast<MockHardwareContext *>(hwContextContainer.hardwareContexts[0].get());
    auto mockHwContext1 = static_cast<MockHardwareContext *>(hwContextContainer.hardwareContexts[1].get());

    EXPECT_FALSE(mockHwContext0->dumpBufferBINCalled);
    EXPECT_FALSE(mockHwContext1->dumpBufferBINCalled);
    EXPECT_FALSE(mockHwContext0->readMemoryCalled);
    EXPECT_FALSE(mockHwContext1->readMemoryCalled);

    hwContextContainer.dumpBufferBIN(1, 2);
    hwContextContainer.readMemory(1, reinterpret_cast<void *>(0x123), 1, 2, 0);

    EXPECT_TRUE(mockHwContext0->dumpBufferBINCalled);
    EXPECT_FALSE(mockHwContext1->dumpBufferBINCalled);
    EXPECT_TRUE(mockHwContext0->readMemoryCalled);
    EXPECT_FALSE(mockHwContext1->readMemoryCalled);
}
