/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/aub_mem_dump/aub_mem_dump.h"
#include "runtime/aub_mem_dump/page_table_entry_bits.h"
#include "runtime/helpers/array_count.h"
#include "runtime/helpers/dispatch_info.h"
#include "runtime/helpers/flat_batch_buffer_helper_hw.h"
#include "runtime/memory_manager/memory_banks.h"
#include "runtime/memory_manager/memory_manager.h"
#include "runtime/os_interface/debug_settings_manager.h"
#include "test.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/mocks/mock_aub_csr.h"
#include "unit_tests/mocks/mock_aub_file_stream.h"
#include "unit_tests/mocks/mock_aub_subcapture_manager.h"
#include "unit_tests/mocks/mock_csr.h"
#include "unit_tests/mocks/mock_gmm.h"
#include "unit_tests/mocks/mock_kernel.h"
#include "unit_tests/mocks/mock_mdi.h"

#include <fstream>
#include <memory>

using namespace OCLRT;

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

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

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenGetEngineIndexIsCalledForGivenEngineTypeThenEngineIndexForThatTypeIsReturned) {
    auto aubCsr = std::make_unique<AUBCommandStreamReceiverHw<FamilyType>>(**platformDevices, "", true, executionEnvironment);
    EXPECT_NE(nullptr, aubCsr);

    auto engineIndex = aubCsr->getEngineIndex(EngineType::ENGINE_RCS);
    EXPECT_EQ(EngineType::ENGINE_RCS, allEngineInstances[engineIndex].type);

    engineIndex = aubCsr->getEngineIndex(EngineType::ENGINE_BCS);
    EXPECT_EQ(EngineType::ENGINE_BCS, allEngineInstances[engineIndex].type);

    engineIndex = aubCsr->getEngineIndex(EngineType::ENGINE_VCS);
    EXPECT_EQ(EngineType::ENGINE_VCS, allEngineInstances[engineIndex].type);

    engineIndex = aubCsr->getEngineIndex(EngineType::ENGINE_VECS);
    EXPECT_EQ(EngineType::ENGINE_VECS, allEngineInstances[engineIndex].type);
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
    std::unique_ptr<MemoryManager> memoryManager(aubCsr->createMemoryManager(false, false));
    EXPECT_NE(nullptr, memoryManager.get());
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenMultipleInstancesAreCreatedThenTheyOperateOnSingleFileStream) {
    ExecutionEnvironment executionEnvironment;
    auto aubCsr1 = std::make_unique<AUBCommandStreamReceiverHw<FamilyType>>(**platformDevices, "", true, executionEnvironment);
    auto aubCsr2 = std::make_unique<AUBCommandStreamReceiverHw<FamilyType>>(**platformDevices, "", true, executionEnvironment);
    EXPECT_EQ(aubCsr1->stream, aubCsr2->stream);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenMultipleInstancesAreCreatedThenTheyUseTheSameFileStream) {
    ExecutionEnvironment executionEnvironment;
    auto aubCsr1 = std::make_unique<AUBCommandStreamReceiverHw<FamilyType>>(**platformDevices, "", true, executionEnvironment);
    auto streamProvider1 = executionEnvironment.aubCenter->getStreamProvider();
    EXPECT_NE(nullptr, streamProvider1);
    auto aubCsr2 = std::make_unique<AUBCommandStreamReceiverHw<FamilyType>>(**platformDevices, "", true, executionEnvironment);
    auto streamProvider2 = executionEnvironment.aubCenter->getStreamProvider();
    EXPECT_NE(nullptr, streamProvider2);
    EXPECT_EQ(streamProvider1, streamProvider2);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenMultipleInstancesAreCreatedThenTheyUseTheSamePhysicalAddressAllocator) {
    ExecutionEnvironment executionEnvironment;
    auto aubCsr1 = std::make_unique<AUBCommandStreamReceiverHw<FamilyType>>(**platformDevices, "", true, executionEnvironment);
    auto physicalAddressAlocator1 = executionEnvironment.aubCenter->getPhysicalAddressAllocator();
    EXPECT_NE(nullptr, physicalAddressAlocator1);
    auto aubCsr2 = std::make_unique<AUBCommandStreamReceiverHw<FamilyType>>(**platformDevices, "", true, executionEnvironment);
    auto physicalAddressAlocator2 = executionEnvironment.aubCenter->getPhysicalAddressAllocator();
    EXPECT_NE(nullptr, physicalAddressAlocator2);
    EXPECT_EQ(physicalAddressAlocator1, physicalAddressAlocator2);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenMultipleInstancesAreCreatedThenTheyUseTheSameAddressMapper) {
    ExecutionEnvironment executionEnvironment;
    auto aubCsr1 = std::make_unique<AUBCommandStreamReceiverHw<FamilyType>>(**platformDevices, "", true, executionEnvironment);
    auto addressMapper1 = executionEnvironment.aubCenter->getAddressMapper();
    EXPECT_NE(nullptr, addressMapper1);
    auto aubCsr2 = std::make_unique<AUBCommandStreamReceiverHw<FamilyType>>(**platformDevices, "", true, executionEnvironment);
    auto addressMapper2 = executionEnvironment.aubCenter->getAddressMapper();
    EXPECT_NE(nullptr, addressMapper2);
    EXPECT_EQ(addressMapper1, addressMapper2);
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

HWTEST_F(AubCommandStreamReceiverTests, givenAubCsrInSubCaptureModeWhenItIsCreatedWithoutDebugFilterSettingsThenItInitializesSubCaptureFiltersWithDefaults) {
    DebugManagerStateRestore stateRestore;
    DebugManager.flags.AUBDumpSubCaptureMode.set(static_cast<int32_t>(AubSubCaptureManager::SubCaptureMode::Filter));
    std::unique_ptr<MockAubCsr<FamilyType>> aubCsr(new MockAubCsr<FamilyType>(*platformDevices[0], "", true, *pDevice->executionEnvironment));
    EXPECT_EQ(0u, aubCsr->subCaptureManager->subCaptureFilter.dumpKernelStartIdx);
    EXPECT_EQ(static_cast<uint32_t>(-1), aubCsr->subCaptureManager->subCaptureFilter.dumpKernelEndIdx);
    EXPECT_STREQ("", aubCsr->subCaptureManager->subCaptureFilter.dumpKernelName.c_str());
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCsrInSubCaptureModeWhenItIsCreatedWithDebugFilterSettingsThenItInitializesSubCaptureFiltersWithDebugFilterSettings) {
    DebugManagerStateRestore stateRestore;
    DebugManager.flags.AUBDumpSubCaptureMode.set(static_cast<int32_t>(AubSubCaptureManager::SubCaptureMode::Filter));
    DebugManager.flags.AUBDumpFilterKernelStartIdx.set(10);
    DebugManager.flags.AUBDumpFilterKernelEndIdx.set(100);
    DebugManager.flags.AUBDumpFilterKernelName.set("kernel_name");
    std::unique_ptr<MockAubCsr<FamilyType>> aubCsr(new MockAubCsr<FamilyType>(*platformDevices[0], "", true, *pDevice->executionEnvironment));
    EXPECT_EQ(static_cast<uint32_t>(DebugManager.flags.AUBDumpFilterKernelStartIdx.get()), aubCsr->subCaptureManager->subCaptureFilter.dumpKernelStartIdx);
    EXPECT_EQ(static_cast<uint32_t>(DebugManager.flags.AUBDumpFilterKernelEndIdx.get()), aubCsr->subCaptureManager->subCaptureFilter.dumpKernelEndIdx);
    EXPECT_STREQ(DebugManager.flags.AUBDumpFilterKernelName.get().c_str(), aubCsr->subCaptureManager->subCaptureFilter.dumpKernelName.c_str());
}

HWTEST_F(AubCommandStreamReceiverTests, givenGraphicsAllocationWhenMakeResidentCalledMultipleTimesAffectsResidencyOnce) {
    std::unique_ptr<MemoryManager> memoryManager(nullptr);
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>(*platformDevices[0], "", true, *pDevice->executionEnvironment));
    memoryManager.reset(aubCsr->createMemoryManager(false, false));
    auto gfxAllocation = memoryManager->allocateGraphicsMemory(sizeof(uint32_t), sizeof(uint32_t), false, false);

    // First makeResident marks the allocation resident
    aubCsr->makeResident(*gfxAllocation);
    EXPECT_NE(ObjectNotResident, gfxAllocation->residencyTaskCount[0u]);
    EXPECT_EQ(aubCsr->peekTaskCount() + 1, gfxAllocation->taskCount);
    EXPECT_EQ((int)aubCsr->peekTaskCount() + 1, gfxAllocation->residencyTaskCount[0u]);
    EXPECT_EQ(1u, aubCsr->getResidencyAllocations().size());

    // Second makeResident should have no impact
    aubCsr->makeResident(*gfxAllocation);
    EXPECT_NE(ObjectNotResident, gfxAllocation->residencyTaskCount[0u]);
    EXPECT_EQ(aubCsr->peekTaskCount() + 1, gfxAllocation->taskCount);
    EXPECT_EQ((int)aubCsr->peekTaskCount() + 1, gfxAllocation->residencyTaskCount[0u]);
    EXPECT_EQ(1u, aubCsr->getResidencyAllocations().size());

    // First makeNonResident marks the allocation as nonresident
    aubCsr->makeNonResident(*gfxAllocation);
    EXPECT_EQ(ObjectNotResident, gfxAllocation->residencyTaskCount[0u]);
    EXPECT_EQ(1u, aubCsr->getEvictionAllocations().size());

    // Second makeNonResident should have no impact
    aubCsr->makeNonResident(*gfxAllocation);
    EXPECT_EQ(ObjectNotResident, gfxAllocation->residencyTaskCount[0u]);
    EXPECT_EQ(1u, aubCsr->getEvictionAllocations().size());

    memoryManager->freeGraphicsMemoryImpl(gfxAllocation);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenMultipleInstancesInitializeTheirEnginesThenUniqueGlobalGttAdressesAreGenerated) {
    ExecutionEnvironment executionEnvironment;
    auto aubCsr1 = std::make_unique<AUBCommandStreamReceiverHw<FamilyType>>(**platformDevices, "", true, executionEnvironment);
    auto aubCsr2 = std::make_unique<AUBCommandStreamReceiverHw<FamilyType>>(**platformDevices, "", true, executionEnvironment);
    auto engineType = OCLRT::ENGINE_RCS;
    auto engineIndex = aubCsr1->getEngineIndex(engineType);

    aubCsr1->initializeEngine(engineIndex);
    EXPECT_NE(0u, aubCsr1->engineInfoTable[engineType].ggttLRCA);
    EXPECT_NE(0u, aubCsr1->engineInfoTable[engineType].ggttHWSP);
    EXPECT_NE(0u, aubCsr1->engineInfoTable[engineType].ggttRingBuffer);

    aubCsr2->initializeEngine(engineIndex);
    EXPECT_NE(aubCsr1->engineInfoTable[engineType].ggttLRCA, aubCsr2->engineInfoTable[engineType].ggttLRCA);
    EXPECT_NE(aubCsr1->engineInfoTable[engineType].ggttHWSP, aubCsr2->engineInfoTable[engineType].ggttHWSP);
    EXPECT_NE(aubCsr1->engineInfoTable[engineType].ggttRingBuffer, aubCsr2->engineInfoTable[engineType].ggttRingBuffer);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenFlushIsCalledThenItShouldInitializeEngineInfoTable) {
    auto aubExecutionEnvironment = getEnvironment<AUBCommandStreamReceiverHw<FamilyType>>(true, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<AUBCommandStreamReceiverHw<FamilyType>>();
    LinearStream cs(aubExecutionEnvironment->commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    auto engineType = OCLRT::ENGINE_RCS;
    ResidencyContainer allocationsForResidency = {};

    aubCsr->flush(batchBuffer, engineType, allocationsForResidency, *pDevice->getOsContext());
    EXPECT_NE(nullptr, aubCsr->engineInfoTable[engineType].pLRCA);
    EXPECT_NE(nullptr, aubCsr->engineInfoTable[engineType].pGlobalHWStatusPage);
    EXPECT_NE(nullptr, aubCsr->engineInfoTable[engineType].pRingBuffer);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverInSubCaptureModeWhenFlushIsCalledButSubCaptureIsDisabledThenItShouldntInitializeEngineInfoTable) {
    auto aubExecutionEnvironment = getEnvironment<AUBCommandStreamReceiverHw<FamilyType>>(true, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<AUBCommandStreamReceiverHw<FamilyType>>();
    LinearStream cs(aubExecutionEnvironment->commandBuffer);

    auto aubSubCaptureManagerMock = new AubSubCaptureManagerMock("");
    aubSubCaptureManagerMock->subCaptureMode = AubSubCaptureManager::SubCaptureMode::Toggle;
    aubSubCaptureManagerMock->disableSubCapture();
    aubCsr->subCaptureManager.reset(aubSubCaptureManagerMock);
    ASSERT_FALSE(aubCsr->subCaptureManager->isSubCaptureEnabled());

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    auto engineType = OCLRT::ENGINE_RCS;
    ResidencyContainer allocationsForResidency = {};

    aubCsr->flush(batchBuffer, engineType, allocationsForResidency, *pDevice->getOsContext());
    EXPECT_EQ(nullptr, aubCsr->engineInfoTable[engineType].pLRCA);
    EXPECT_EQ(nullptr, aubCsr->engineInfoTable[engineType].pGlobalHWStatusPage);
    EXPECT_EQ(nullptr, aubCsr->engineInfoTable[engineType].pRingBuffer);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenFlushIsCalledThenItShouldLeaveProperRingTailAlignment) {
    auto aubExecutionEnvironment = getEnvironment<AUBCommandStreamReceiverHw<FamilyType>>(true, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<AUBCommandStreamReceiverHw<FamilyType>>();
    auto allocationsForResidency = aubCsr->getResidencyAllocations();
    LinearStream cs(aubExecutionEnvironment->commandBuffer);

    auto engineType = OCLRT::ENGINE_RCS;
    auto ringTailAlignment = sizeof(uint64_t);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};

    // First flush typically includes a preamble and chain to command buffer
    aubCsr->overrideDispatchPolicy(DispatchMode::ImmediateDispatch);
    aubCsr->flush(batchBuffer, engineType, allocationsForResidency, *pDevice->getOsContext());
    EXPECT_EQ(0ull, aubCsr->engineInfoTable[engineType].tailRingBuffer % ringTailAlignment);

    // Second flush should just submit command buffer
    cs.getSpace(sizeof(uint64_t));
    aubCsr->flush(batchBuffer, engineType, allocationsForResidency, *pDevice->getOsContext());
    EXPECT_EQ(0ull, aubCsr->engineInfoTable[engineType].tailRingBuffer % ringTailAlignment);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverInNonStandaloneModeWhenFlushIsCalledThenItShouldNotUpdateHwTagWithLatestSentTaskCount) {
    auto aubExecutionEnvironment = getEnvironment<MockAubCsr<FamilyType>>(true, true, false);
    auto aubCsr = aubExecutionEnvironment->template getCsr<MockAubCsr<FamilyType>>();
    LinearStream cs(aubExecutionEnvironment->commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    auto engineType = OCLRT::ENGINE_RCS;
    ResidencyContainer allocationsForResidency = {};

    EXPECT_EQ(initialHardwareTag, *aubCsr->getTagAddress());

    aubCsr->setLatestSentTaskCount(aubCsr->peekTaskCount() + 1);

    EXPECT_NE(aubCsr->peekLatestSentTaskCount(), *aubCsr->getTagAddress());

    aubCsr->flush(batchBuffer, engineType, allocationsForResidency, *pDevice->getOsContext());

    EXPECT_NE(aubCsr->peekLatestSentTaskCount(), *aubCsr->getTagAddress());
    EXPECT_EQ(initialHardwareTag, *aubCsr->getTagAddress());
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverInStandaloneModeWhenFlushIsCalledThenItShouldUpdateHwTagWithLatestSentTaskCount) {
    auto aubExecutionEnvironment = getEnvironment<MockAubCsr<FamilyType>>(true, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<MockAubCsr<FamilyType>>();
    LinearStream cs(aubExecutionEnvironment->commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    auto engineType = OCLRT::ENGINE_RCS;
    ResidencyContainer allocationsForResidency = {};

    EXPECT_EQ(initialHardwareTag, *aubCsr->getTagAddress());

    aubCsr->setLatestSentTaskCount(aubCsr->peekTaskCount() + 1);

    EXPECT_NE(aubCsr->peekLatestSentTaskCount(), *aubCsr->getTagAddress());

    aubCsr->flush(batchBuffer, engineType, allocationsForResidency, *pDevice->getOsContext());

    EXPECT_EQ(aubCsr->peekLatestSentTaskCount(), *aubCsr->getTagAddress());
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverInStandaloneAndSubCaptureModeWhenFlushIsCalledButSubCaptureIsDisabledThenItShouldUpdateHwTagWithLatestSentTaskCount) {
    DebugManagerStateRestore stateRestore;
    auto aubExecutionEnvironment = getEnvironment<MockAubCsr<FamilyType>>(true, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<MockAubCsr<FamilyType>>();
    LinearStream cs(aubExecutionEnvironment->commandBuffer);

    auto aubSubCaptureManagerMock = new AubSubCaptureManagerMock("");
    aubSubCaptureManagerMock->subCaptureMode = AubSubCaptureManager::SubCaptureMode::Toggle;
    aubSubCaptureManagerMock->disableSubCapture();
    aubCsr->subCaptureManager.reset(aubSubCaptureManagerMock);
    ASSERT_FALSE(aubCsr->subCaptureManager->isSubCaptureEnabled());

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    auto engineType = OCLRT::ENGINE_RCS;
    ResidencyContainer allocationsForResidency = {};

    aubCsr->setLatestSentTaskCount(aubCsr->peekTaskCount() + 1);

    EXPECT_NE(aubCsr->peekLatestSentTaskCount(), *aubCsr->getTagAddress());

    aubCsr->flush(batchBuffer, engineType, allocationsForResidency, *pDevice->getOsContext());

    EXPECT_EQ(aubCsr->peekLatestSentTaskCount(), *aubCsr->getTagAddress());
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverInNonStandaloneAndSubCaptureModeWhenFlushIsCalledButSubCaptureIsDisabledThenItShouldNotUpdateHwTagWithLatestSentTaskCount) {
    DebugManagerStateRestore stateRestore;
    auto aubExecutionEnvironment = getEnvironment<MockAubCsr<FamilyType>>(true, true, false);
    auto aubCsr = aubExecutionEnvironment->template getCsr<MockAubCsr<FamilyType>>();
    LinearStream cs(aubExecutionEnvironment->commandBuffer);

    auto aubSubCaptureManagerMock = new AubSubCaptureManagerMock("");
    aubSubCaptureManagerMock->subCaptureMode = AubSubCaptureManager::SubCaptureMode::Toggle;
    aubSubCaptureManagerMock->disableSubCapture();
    aubCsr->subCaptureManager.reset(aubSubCaptureManagerMock);
    ASSERT_FALSE(aubCsr->subCaptureManager->isSubCaptureEnabled());

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    auto engineType = OCLRT::ENGINE_RCS;
    ResidencyContainer allocationsForResidency = {};

    aubCsr->setLatestSentTaskCount(aubCsr->peekTaskCount() + 1);

    EXPECT_NE(aubCsr->peekLatestSentTaskCount(), *aubCsr->getTagAddress());

    aubCsr->flush(batchBuffer, engineType, allocationsForResidency, *pDevice->getOsContext());

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
    auto aubSubCaptureManagerMock = new AubSubCaptureManagerMock("");
    aubSubCaptureManagerMock->subCaptureMode = AubSubCaptureManager::SubCaptureMode::Toggle;
    aubSubCaptureManagerMock->setSubCaptureToggleActive(true);
    aubSubCaptureManagerMock->activateSubCapture(multiDispatchInfo);
    aubCsr->subCaptureManager.reset(aubSubCaptureManagerMock);
    ASSERT_TRUE(aubCsr->subCaptureManager->isSubCaptureEnabled());

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    auto engineType = OCLRT::ENGINE_RCS;
    ResidencyContainer allocationsForResidency = {};

    aubCsr->flush(batchBuffer, engineType, allocationsForResidency, *pDevice->getOsContext());

    EXPECT_FALSE(aubCsr->subCaptureManager->isSubCaptureEnabled());
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverInStandaloneModeWhenFlushIsCalledThenItShouldCallMakeResidentOnCommandBufferAllocation) {
    auto aubExecutionEnvironment = getEnvironment<MockAubCsr<FamilyType>>(true, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<MockAubCsr<FamilyType>>();
    auto allocationsForResidency = aubCsr->getResidencyAllocations();
    auto commandBuffer = aubExecutionEnvironment->commandBuffer;
    LinearStream cs(commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    auto engineType = OCLRT::ENGINE_RCS;

    EXPECT_EQ(ObjectNotResident, commandBuffer->residencyTaskCount[0u]);

    aubCsr->overrideDispatchPolicy(DispatchMode::ImmediateDispatch);
    aubCsr->flush(batchBuffer, engineType, allocationsForResidency, *pDevice->getOsContext());

    EXPECT_NE(ObjectNotResident, commandBuffer->residencyTaskCount[0u]);
    EXPECT_EQ((int)aubCsr->peekTaskCount() + 1, commandBuffer->residencyTaskCount[0u]);

    aubCsr->makeSurfacePackNonResident(aubCsr->getResidencyAllocations(), *pDevice->getOsContext());

    EXPECT_EQ(ObjectNotResident, commandBuffer->residencyTaskCount[0u]);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverInNonStandaloneModeWhenFlushIsCalledThenItShouldNotCallMakeResidentOnCommandBufferAllocation) {
    auto aubExecutionEnvironment = getEnvironment<AUBCommandStreamReceiverHw<FamilyType>>(false, true, false);
    auto aubCsr = aubExecutionEnvironment->template getCsr<AUBCommandStreamReceiverHw<FamilyType>>();
    auto allocationsForResidency = aubCsr->getResidencyAllocations();
    LinearStream cs(aubExecutionEnvironment->commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    auto engineType = OCLRT::ENGINE_RCS;

    EXPECT_EQ(ObjectNotResident, aubExecutionEnvironment->commandBuffer->residencyTaskCount[0u]);

    aubCsr->flush(batchBuffer, engineType, allocationsForResidency, *pDevice->getOsContext());

    EXPECT_EQ(ObjectNotResident, aubExecutionEnvironment->commandBuffer->residencyTaskCount[0u]);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverInStandaloneModeWhenFlushIsCalledThenItShouldCallMakeResidentOnResidencyAllocations) {
    auto aubExecutionEnvironment = getEnvironment<MockAubCsr<FamilyType>>(true, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<MockAubCsr<FamilyType>>();
    auto memoryManager = aubExecutionEnvironment->executionEnvironment->memoryManager.get();
    auto commandBuffer = aubExecutionEnvironment->commandBuffer;
    LinearStream cs(commandBuffer);

    auto gfxAllocation = memoryManager->allocateGraphicsMemory(sizeof(uint32_t), sizeof(uint32_t), false, false);
    ASSERT_NE(nullptr, gfxAllocation);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    auto engineType = OCLRT::ENGINE_RCS;
    ResidencyContainer allocationsForResidency = {gfxAllocation};

    EXPECT_EQ(ObjectNotResident, gfxAllocation->residencyTaskCount[0u]);
    EXPECT_EQ(ObjectNotResident, commandBuffer->residencyTaskCount[0u]);

    aubCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);
    aubCsr->flush(batchBuffer, engineType, allocationsForResidency, *pDevice->getOsContext());

    EXPECT_NE(ObjectNotResident, gfxAllocation->residencyTaskCount[0u]);
    EXPECT_EQ((int)aubCsr->peekTaskCount() + 1, gfxAllocation->residencyTaskCount[0u]);

    EXPECT_NE(ObjectNotResident, commandBuffer->residencyTaskCount[0u]);
    EXPECT_EQ((int)aubCsr->peekTaskCount() + 1, commandBuffer->residencyTaskCount[0u]);

    aubCsr->makeSurfacePackNonResident(allocationsForResidency, *pDevice->getOsContext());

    EXPECT_EQ(ObjectNotResident, gfxAllocation->residencyTaskCount[0u]);
    EXPECT_EQ(ObjectNotResident, commandBuffer->residencyTaskCount[0u]);

    memoryManager->freeGraphicsMemory(gfxAllocation);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverInNonStandaloneModeWhenFlushIsCalledThenItShouldCallMakeResidentOnResidencyAllocations) {
    auto aubExecutionEnvironment = getEnvironment<AUBCommandStreamReceiverHw<FamilyType>>(true, true, false);
    auto aubCsr = aubExecutionEnvironment->template getCsr<AUBCommandStreamReceiverHw<FamilyType>>();
    auto memoryManager = aubExecutionEnvironment->executionEnvironment->memoryManager.get();
    auto commandBuffer = aubExecutionEnvironment->commandBuffer;
    LinearStream cs(commandBuffer);

    auto gfxAllocation = memoryManager->allocateGraphicsMemory(sizeof(uint32_t), sizeof(uint32_t), false, false);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    auto engineType = OCLRT::ENGINE_RCS;
    ResidencyContainer allocationsForResidency = {gfxAllocation};

    EXPECT_EQ(ObjectNotResident, gfxAllocation->residencyTaskCount[0u]);
    EXPECT_EQ(ObjectNotResident, commandBuffer->residencyTaskCount[0u]);

    aubCsr->flush(batchBuffer, engineType, allocationsForResidency, *pDevice->getOsContext());

    EXPECT_NE(ObjectNotResident, gfxAllocation->residencyTaskCount[0u]);
    EXPECT_EQ((int)aubCsr->peekTaskCount() + 1, gfxAllocation->residencyTaskCount[0u]);

    EXPECT_EQ(ObjectNotResident, commandBuffer->residencyTaskCount[0u]);

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
    auto aubSubCaptureManagerMock = new AubSubCaptureManagerMock("");
    aubSubCaptureManagerMock->subCaptureMode = AubSubCaptureManager::SubCaptureMode::Toggle;
    aubSubCaptureManagerMock->setSubCaptureToggleActive(true);
    aubSubCaptureManagerMock->activateSubCapture(multiDispatchInfo);
    aubCsr->subCaptureManager.reset(aubSubCaptureManagerMock);
    ASSERT_TRUE(aubCsr->subCaptureManager->isSubCaptureEnabled());

    auto gfxAllocation = memoryManager->allocateGraphicsMemory(sizeof(uint32_t), sizeof(uint32_t), false, false);
    ASSERT_NE(nullptr, gfxAllocation);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    auto engineType = OCLRT::ENGINE_RCS;
    ResidencyContainer allocationsForResidency = {gfxAllocation};

    EXPECT_EQ(ObjectNotResident, gfxAllocation->residencyTaskCount[0u]);
    EXPECT_EQ(ObjectNotResident, commandBuffer->residencyTaskCount[0u]);

    aubCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);
    aubCsr->flush(batchBuffer, engineType, allocationsForResidency, *pDevice->getOsContext());

    EXPECT_NE(ObjectNotResident, gfxAllocation->residencyTaskCount[0u]);
    EXPECT_EQ((int)aubCsr->peekTaskCount() + 1, gfxAllocation->residencyTaskCount[0u]);

    EXPECT_NE(ObjectNotResident, commandBuffer->residencyTaskCount[0u]);
    EXPECT_EQ((int)aubCsr->peekTaskCount() + 1, commandBuffer->residencyTaskCount[0u]);

    aubCsr->makeSurfacePackNonResident(allocationsForResidency, *pDevice->getOsContext());

    EXPECT_EQ(ObjectNotResident, gfxAllocation->residencyTaskCount[0u]);
    EXPECT_EQ(ObjectNotResident, commandBuffer->residencyTaskCount[0u]);

    memoryManager->freeGraphicsMemory(gfxAllocation);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenGraphicsAllocationIsCreatedThenItDoesntHaveTypeNonAubWritable) {
    auto aubExecutionEnvironment = getEnvironment<AUBCommandStreamReceiverHw<FamilyType>>(false, false, true);
    auto memoryManager = aubExecutionEnvironment->executionEnvironment->memoryManager.get();

    auto gfxAllocation = memoryManager->allocateGraphicsMemory(sizeof(uint32_t), sizeof(uint32_t), false, false);

    EXPECT_TRUE(gfxAllocation->isAubWritable());

    memoryManager->freeGraphicsMemory(gfxAllocation);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenProcessResidencyIsCalledOnDefaultAllocationThenAllocationTypeShouldNotBeMadeNonAubWritable) {
    auto aubExecutionEnvironment = getEnvironment<AUBCommandStreamReceiverHw<FamilyType>>(false, false, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<AUBCommandStreamReceiverHw<FamilyType>>();
    auto memoryManager = aubExecutionEnvironment->executionEnvironment->memoryManager.get();

    auto gfxDefaultAllocation = memoryManager->allocateGraphicsMemory(sizeof(uint32_t), sizeof(uint32_t), false, false);

    ResidencyContainer allocationsForResidency = {gfxDefaultAllocation};
    aubCsr->processResidency(allocationsForResidency, *pDevice->getOsContext());

    EXPECT_TRUE(gfxDefaultAllocation->isAubWritable());

    memoryManager->freeGraphicsMemory(gfxDefaultAllocation);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenWriteMemoryIsCalledOnBufferAndImageTypeAllocationsThenAllocationsHaveAubWritableSetToFalse) {
    std::unique_ptr<MemoryManager> memoryManager(nullptr);
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>(*platformDevices[0], "", true, *pDevice->executionEnvironment));
    memoryManager.reset(aubCsr->createMemoryManager(false, false));

    auto gfxAllocation = memoryManager->allocateGraphicsMemory(sizeof(uint32_t), sizeof(uint32_t), false, false);

    GraphicsAllocation::AllocationType onlyOneTimeAubWritableTypes[] = {
        GraphicsAllocation::AllocationType::BUFFER,
        GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY,
        GraphicsAllocation::AllocationType::BUFFER_COMPRESSED,
        GraphicsAllocation::AllocationType::IMAGE};

    for (size_t i = 0; i < arrayCount(onlyOneTimeAubWritableTypes); i++) {
        gfxAllocation->setAubWritable(true);
        gfxAllocation->setAllocationType(onlyOneTimeAubWritableTypes[i]);
        aubCsr->writeMemory(*gfxAllocation);

        EXPECT_FALSE(gfxAllocation->isAubWritable());
    }
    memoryManager->freeGraphicsMemory(gfxAllocation);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenProcessResidencyIsCalledOnBufferAndImageAllocationsThenAllocationsTypesShouldBeMadeNonAubWritable) {
    std::unique_ptr<MemoryManager> memoryManager(nullptr);
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>(*platformDevices[0], "", true, *pDevice->executionEnvironment));
    memoryManager.reset(aubCsr->createMemoryManager(false, false));

    auto gfxBufferAllocation = memoryManager->allocateGraphicsMemory(sizeof(uint32_t), sizeof(uint32_t), false, false);
    gfxBufferAllocation->setAllocationType(GraphicsAllocation::AllocationType::BUFFER);

    auto gfxImageAllocation = memoryManager->allocateGraphicsMemory(sizeof(uint32_t), sizeof(uint32_t), false, false);
    gfxImageAllocation->setAllocationType(GraphicsAllocation::AllocationType::IMAGE);

    ResidencyContainer allocationsForResidency = {gfxBufferAllocation, gfxImageAllocation};
    aubCsr->processResidency(allocationsForResidency, *pDevice->getOsContext());

    EXPECT_FALSE(gfxBufferAllocation->isAubWritable());
    EXPECT_FALSE(gfxImageAllocation->isAubWritable());

    memoryManager->freeGraphicsMemory(gfxBufferAllocation);
    memoryManager->freeGraphicsMemory(gfxImageAllocation);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverInSubCaptureModWhenProcessResidencyIsCalledWithDumpAubNonWritableFlagThenAllocationsTypesShouldBeMadeAubWritable) {
    DebugManagerStateRestore stateRestore;
    std::unique_ptr<MemoryManager> memoryManager(nullptr);
    std::unique_ptr<MockAubCsrToTestDumpAubNonWritable<FamilyType>> aubCsr(new MockAubCsrToTestDumpAubNonWritable<FamilyType>(*platformDevices[0], "", true, *pDevice->executionEnvironment));
    memoryManager.reset(aubCsr->createMemoryManager(false, false));

    auto gfxBufferAllocation = memoryManager->allocateGraphicsMemory(sizeof(uint32_t), sizeof(uint32_t), false, false);
    gfxBufferAllocation->setAllocationType(GraphicsAllocation::AllocationType::BUFFER);
    gfxBufferAllocation->setAubWritable(false);

    auto gfxImageAllocation = memoryManager->allocateGraphicsMemory(sizeof(uint32_t), sizeof(uint32_t), false, false);
    gfxImageAllocation->setAllocationType(GraphicsAllocation::AllocationType::IMAGE);
    gfxImageAllocation->setAubWritable(false);

    aubCsr->dumpAubNonWritable = true;

    ResidencyContainer allocationsForResidency = {gfxBufferAllocation, gfxImageAllocation};
    aubCsr->processResidency(allocationsForResidency, *pDevice->getOsContext());

    EXPECT_TRUE(gfxBufferAllocation->isAubWritable());
    EXPECT_TRUE(gfxImageAllocation->isAubWritable());

    memoryManager->freeGraphicsMemory(gfxBufferAllocation);
    memoryManager->freeGraphicsMemory(gfxImageAllocation);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenProcessResidencyIsCalledWithoutDumpAubWritableFlagThenAllocationsTypesShouldBeKeptNonAubWritable) {
    DebugManagerStateRestore stateRestore;
    std::unique_ptr<MemoryManager> memoryManager(nullptr);
    std::unique_ptr<MockAubCsrToTestDumpAubNonWritable<FamilyType>> aubCsr(new MockAubCsrToTestDumpAubNonWritable<FamilyType>(*platformDevices[0], "", true, *pDevice->executionEnvironment));
    memoryManager.reset(aubCsr->createMemoryManager(false, false));

    auto gfxBufferAllocation = memoryManager->allocateGraphicsMemory(sizeof(uint32_t), sizeof(uint32_t), false, false);
    gfxBufferAllocation->setAllocationType(GraphicsAllocation::AllocationType::BUFFER);
    gfxBufferAllocation->setAubWritable(false);

    auto gfxImageAllocation = memoryManager->allocateGraphicsMemory(sizeof(uint32_t), sizeof(uint32_t), false, false);
    gfxImageAllocation->setAllocationType(GraphicsAllocation::AllocationType::IMAGE);
    gfxImageAllocation->setAubWritable(false);

    aubCsr->dumpAubNonWritable = false;

    ResidencyContainer allocationsForResidency = {gfxBufferAllocation, gfxImageAllocation};
    aubCsr->processResidency(allocationsForResidency, *pDevice->getOsContext());

    EXPECT_FALSE(gfxBufferAllocation->isAubWritable());
    EXPECT_FALSE(gfxImageAllocation->isAubWritable());

    memoryManager->freeGraphicsMemory(gfxBufferAllocation);
    memoryManager->freeGraphicsMemory(gfxImageAllocation);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenGraphicsAllocationTypeIsntNonAubWritableThenWriteMemoryIsAllowed) {
    std::unique_ptr<MemoryManager> memoryManager(nullptr);
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>(*platformDevices[0], "", true, *pDevice->executionEnvironment));
    memoryManager.reset(aubCsr->createMemoryManager(false, false));

    auto gfxAllocation = memoryManager->allocateGraphicsMemory(sizeof(uint32_t), sizeof(uint32_t), false, false);

    EXPECT_TRUE(aubCsr->writeMemory(*gfxAllocation));

    memoryManager->freeGraphicsMemory(gfxAllocation);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenGraphicsAllocationTypeIsNonAubWritableThenWriteMemoryIsNotAllowed) {
    std::unique_ptr<MemoryManager> memoryManager(nullptr);
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>(*platformDevices[0], "", true, *pDevice->executionEnvironment));
    memoryManager.reset(aubCsr->createMemoryManager(false, false));

    auto gfxAllocation = memoryManager->allocateGraphicsMemory(sizeof(uint32_t), sizeof(uint32_t), false, false);

    gfxAllocation->setAubWritable(false);
    EXPECT_FALSE(aubCsr->writeMemory(*gfxAllocation));

    memoryManager->freeGraphicsMemory(gfxAllocation);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenGraphicsAllocationSizeIsZeroThenWriteMemoryIsNotAllowed) {
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>(*platformDevices[0], "", true, *pDevice->executionEnvironment));
    GraphicsAllocation gfxAllocation((void *)0x1234, 0);

    EXPECT_FALSE(aubCsr->writeMemory(gfxAllocation));
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenAllocationDataIsPassedInAllocationViewThenWriteMemoryIsAllowed) {
    auto aubCsr = std::make_unique<AUBCommandStreamReceiverHw<FamilyType>>(*platformDevices[0], "", true, *pDevice->executionEnvironment);
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

    std::unique_ptr<MockAubCsr<FamilyType>> aubCsr(static_cast<MockAubCsr<FamilyType> *>(AUBCommandStreamReceiver::create(*platformDevices[0], "", true, *pDevice->executionEnvironment)));
    EXPECT_NE(nullptr, aubCsr);

    EXPECT_TRUE(aubCsr->isFileOpen());
    EXPECT_STREQ(DebugManager.flags.AUBDumpCaptureFileName.get().c_str(), aubCsr->getFileName().c_str());
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverInSubCaptureModeWhenAubSubCaptureIsActivatedThenFileIsOpened) {
    DebugManagerStateRestore stateRestore;
    std::unique_ptr<MockAubCsr<FamilyType>> aubCsr(new MockAubCsr<FamilyType>(*platformDevices[0], "", false, *pDevice->executionEnvironment));

    auto subCaptureManagerMock = new AubSubCaptureManagerMock("");
    subCaptureManagerMock->subCaptureMode = AubSubCaptureManager::SubCaptureMode::Toggle;
    subCaptureManagerMock->setSubCaptureIsActive(false);
    subCaptureManagerMock->setSubCaptureToggleActive(true);
    aubCsr->subCaptureManager.reset(subCaptureManagerMock);

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

    auto subCaptureManagerMock = new AubSubCaptureManagerMock("");
    subCaptureManagerMock->subCaptureMode = AubSubCaptureManager::SubCaptureMode::Toggle;
    subCaptureManagerMock->setSubCaptureIsActive(false);
    subCaptureManagerMock->setSubCaptureToggleActive(true);
    aubCsr->subCaptureManager.reset(subCaptureManagerMock);

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

    auto subCaptureManagerMock = new AubSubCaptureManagerMock("");
    subCaptureManagerMock->subCaptureMode = AubSubCaptureManager::SubCaptureMode::Toggle;
    subCaptureManagerMock->setSubCaptureIsActive(false);
    subCaptureManagerMock->setSubCaptureToggleActive(true);
    subCaptureManagerMock->setExternalFileName(newFileName);
    aubCsr->subCaptureManager.reset(subCaptureManagerMock);

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

    auto subCaptureManagerMock = new AubSubCaptureManagerMock("");
    subCaptureManagerMock->subCaptureMode = AubSubCaptureManager::SubCaptureMode::Toggle;
    subCaptureManagerMock->setSubCaptureIsActive(false);
    subCaptureManagerMock->setSubCaptureToggleActive(true);
    subCaptureManagerMock->setExternalFileName(newFileName);
    aubCsr->subCaptureManager.reset(subCaptureManagerMock);

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

    auto subCaptureManagerMock = new AubSubCaptureManagerMock("");
    subCaptureManagerMock->subCaptureMode = AubSubCaptureManager::SubCaptureMode::Toggle;
    subCaptureManagerMock->setSubCaptureIsActive(false);
    subCaptureManagerMock->setSubCaptureToggleActive(true);
    aubCsr->subCaptureManager.reset(subCaptureManagerMock);

    MockKernelWithInternals kernelInternals(*pDevice);
    Kernel *kernel = kernelInternals.mockKernel;
    MockMultiDispatchInfo multiDispatchInfo(kernel);

    aubCsr->activateAubSubCapture(multiDispatchInfo);

    EXPECT_TRUE(aubCsr->dumpAubNonWritable);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverInSubCaptureModeWhenAubSubCaptureRemainsActivatedThenDontForceDumpingAllocationsAubNonWritable) {
    DebugManagerStateRestore stateRestore;
    std::unique_ptr<MockAubCsrToTestDumpAubNonWritable<FamilyType>> aubCsr(new MockAubCsrToTestDumpAubNonWritable<FamilyType>(*platformDevices[0], "", false, *pDevice->executionEnvironment));

    auto subCaptureManagerMock = new AubSubCaptureManagerMock("");
    subCaptureManagerMock->subCaptureMode = AubSubCaptureManager::SubCaptureMode::Toggle;
    subCaptureManagerMock->setSubCaptureIsActive(false);
    subCaptureManagerMock->setSubCaptureToggleActive(true);
    aubCsr->subCaptureManager.reset(subCaptureManagerMock);

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

    auto subCaptureManagerMock = new AubSubCaptureManagerMock("");
    subCaptureManagerMock->subCaptureMode = AubSubCaptureManager::SubCaptureMode::Toggle;
    subCaptureManagerMock->setSubCaptureIsActive(false);
    subCaptureManagerMock->setSubCaptureToggleActive(false);
    aubCsr->subCaptureManager.reset(subCaptureManagerMock);

    const DispatchInfo dispatchInfo;
    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.push(dispatchInfo);
    aubCsr->activateAubSubCapture(multiDispatchInfo);

    EXPECT_FALSE(aubCsr->subCaptureManager->isSubCaptureEnabled());
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverInSubCaptureModeWhenSubCaptureIsToggledOnThenSubCaptureGetsEnabled) {
    DebugManagerStateRestore stateRestore;
    std::unique_ptr<MockAubCsr<FamilyType>> aubCsr(new MockAubCsr<FamilyType>(*platformDevices[0], "", false, *pDevice->executionEnvironment));

    auto subCaptureManagerMock = new AubSubCaptureManagerMock("");
    subCaptureManagerMock->subCaptureMode = AubSubCaptureManager::SubCaptureMode::Toggle;
    subCaptureManagerMock->setSubCaptureIsActive(false);
    subCaptureManagerMock->setSubCaptureToggleActive(true);
    aubCsr->subCaptureManager.reset(subCaptureManagerMock);

    MockKernelWithInternals kernelInternals(*pDevice);
    Kernel *kernel = kernelInternals.mockKernel;
    MockMultiDispatchInfo multiDispatchInfo(kernel);
    aubCsr->activateAubSubCapture(multiDispatchInfo);

    EXPECT_TRUE(aubCsr->subCaptureManager->isSubCaptureEnabled());
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverInStandaloneAndSubCaptureModeWhenSubCaptureRemainsDeactivatedThenNeitherProgrammingFlagsAreInitializedNorCsrIsFlushed) {
    DebugManagerStateRestore stateRestore;
    std::unique_ptr<MockAubCsr<FamilyType>> aubCsr(new MockAubCsr<FamilyType>(*platformDevices[0], "", true, *pDevice->executionEnvironment));

    auto subCaptureManagerMock = new AubSubCaptureManagerMock("");
    subCaptureManagerMock->subCaptureMode = AubSubCaptureManager::SubCaptureMode::Toggle;
    subCaptureManagerMock->setSubCaptureIsActive(false);
    subCaptureManagerMock->setSubCaptureToggleActive(false);
    aubCsr->subCaptureManager.reset(subCaptureManagerMock);

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

    auto subCaptureManagerMock = new AubSubCaptureManagerMock("");
    subCaptureManagerMock->subCaptureMode = AubSubCaptureManager::SubCaptureMode::Toggle;
    subCaptureManagerMock->setSubCaptureIsActive(true);
    subCaptureManagerMock->setSubCaptureToggleActive(true);
    aubCsr->subCaptureManager.reset(subCaptureManagerMock);

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

    auto subCaptureManagerMock = new AubSubCaptureManagerMock("");
    subCaptureManagerMock->subCaptureMode = AubSubCaptureManager::SubCaptureMode::Toggle;
    subCaptureManagerMock->setSubCaptureIsActive(false);
    subCaptureManagerMock->setSubCaptureToggleActive(true);
    aubCsr->subCaptureManager.reset(subCaptureManagerMock);

    MockKernelWithInternals kernelInternals(*pDevice);
    Kernel *kernel = kernelInternals.mockKernel;
    MockMultiDispatchInfo multiDispatchInfo(kernel);
    aubCsr->activateAubSubCapture(multiDispatchInfo);

    EXPECT_FALSE(aubCsr->flushBatchedSubmissionsCalled);
    EXPECT_TRUE(aubCsr->initProgrammingFlagsCalled);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverInStandaloneAndSubCaptureModeWhenSubCaptureGetsDeactivatedThenCsrIsFlushed) {
    DebugManagerStateRestore stateRestore;
    std::unique_ptr<MockAubCsr<FamilyType>> aubCsr(new MockAubCsr<FamilyType>(*platformDevices[0], "", true, *pDevice->executionEnvironment));

    auto subCaptureManagerMock = new AubSubCaptureManagerMock("");
    subCaptureManagerMock->subCaptureMode = AubSubCaptureManager::SubCaptureMode::Toggle;
    subCaptureManagerMock->setSubCaptureIsActive(true);
    subCaptureManagerMock->setSubCaptureToggleActive(false);
    aubCsr->subCaptureManager.reset(subCaptureManagerMock);

    MockKernelWithInternals kernelInternals(*pDevice);
    Kernel *kernel = kernelInternals.mockKernel;
    MockMultiDispatchInfo multiDispatchInfo(kernel);
    aubCsr->activateAubSubCapture(multiDispatchInfo);

    EXPECT_TRUE(aubCsr->flushBatchedSubmissionsCalled);
    EXPECT_FALSE(aubCsr->initProgrammingFlagsCalled);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenForcedBatchBufferFlatteningInImmediateDispatchModeThenNewCombinedBatchBufferIsCreated) {
    std::unique_ptr<MemoryManager> memoryManager(nullptr);
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>(*platformDevices[0], "", true, *pDevice->executionEnvironment));
    memoryManager.reset(aubCsr->createMemoryManager(false, false));
    auto flatBatchBufferHelper = new FlatBatchBufferHelperHw<FamilyType>(*pDevice->executionEnvironment);
    aubCsr->overwriteFlatBatchBufferHelper(flatBatchBufferHelper);

    auto chainedBatchBuffer = memoryManager->allocateGraphicsMemory(128u, 64u, false, false);
    auto otherAllocation = memoryManager->allocateGraphicsMemory(128u, 64u, false, false);
    ASSERT_NE(nullptr, chainedBatchBuffer);

    GraphicsAllocation *commandBuffer = memoryManager->allocateGraphicsMemory(4096);
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 128u, chainedBatchBuffer, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};

    size_t sizeBatchBuffer = 0xffffu;

    std::unique_ptr<GraphicsAllocation, std::function<void(GraphicsAllocation *)>> flatBatchBuffer(
        aubCsr->getFlatBatchBufferHelper().flattenBatchBuffer(batchBuffer, sizeBatchBuffer, DispatchMode::ImmediateDispatch),
        [&](GraphicsAllocation *ptr) { memoryManager->freeGraphicsMemory(ptr); });
    EXPECT_NE(nullptr, flatBatchBuffer->getUnderlyingBuffer());
    EXPECT_EQ(alignUp(128u + 128u, 0x1000), sizeBatchBuffer);

    memoryManager->freeGraphicsMemory(commandBuffer);
    memoryManager->freeGraphicsMemory(chainedBatchBuffer);
    memoryManager->freeGraphicsMemory(otherAllocation);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenForcedBatchBufferInImmediateDispatchModeAndNoChainedBatchBufferThenCombinedBatchBufferIsNotCreated) {
    std::unique_ptr<MemoryManager> memoryManager(nullptr);
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>(*platformDevices[0], "", true, *pDevice->executionEnvironment));
    memoryManager.reset(aubCsr->createMemoryManager(false, false));
    auto flatBatchBufferHelper = new FlatBatchBufferHelperHw<FamilyType>(*pDevice->executionEnvironment);
    aubCsr->overwriteFlatBatchBufferHelper(flatBatchBufferHelper);

    GraphicsAllocation *commandBuffer = memoryManager->allocateGraphicsMemory(4096);
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 128u, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};

    size_t sizeBatchBuffer = 0xffffu;

    std::unique_ptr<GraphicsAllocation, std::function<void(GraphicsAllocation *)>> flatBatchBuffer(
        aubCsr->getFlatBatchBufferHelper().flattenBatchBuffer(batchBuffer, sizeBatchBuffer, DispatchMode::ImmediateDispatch),
        [&](GraphicsAllocation *ptr) { memoryManager->freeGraphicsMemory(ptr); });
    EXPECT_EQ(nullptr, flatBatchBuffer.get());
    EXPECT_EQ(0xffffu, sizeBatchBuffer);

    memoryManager->freeGraphicsMemory(commandBuffer);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenForcedBatchBufferAndNotImmediateOrBatchedDispatchModeThenCombinedBatchBufferIsNotCreated) {
    std::unique_ptr<MemoryManager> memoryManager(nullptr);
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>(*platformDevices[0], "", true, *pDevice->executionEnvironment));
    memoryManager.reset(aubCsr->createMemoryManager(false, false));
    auto flatBatchBufferHelper = new FlatBatchBufferHelperHw<FamilyType>(*pDevice->executionEnvironment);
    aubCsr->overwriteFlatBatchBufferHelper(flatBatchBufferHelper);

    auto chainedBatchBuffer = memoryManager->allocateGraphicsMemory(128u, 64u, false, false);
    auto otherAllocation = memoryManager->allocateGraphicsMemory(128u, 64u, false, false);
    ASSERT_NE(nullptr, chainedBatchBuffer);

    GraphicsAllocation *commandBuffer = memoryManager->allocateGraphicsMemory(4096);
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 128u, chainedBatchBuffer, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};

    size_t sizeBatchBuffer = 0xffffu;

    std::unique_ptr<GraphicsAllocation, std::function<void(GraphicsAllocation *)>> flatBatchBuffer(
        aubCsr->getFlatBatchBufferHelper().flattenBatchBuffer(batchBuffer, sizeBatchBuffer, DispatchMode::AdaptiveDispatch),
        [&](GraphicsAllocation *ptr) { memoryManager->freeGraphicsMemory(ptr); });
    EXPECT_EQ(nullptr, flatBatchBuffer.get());
    EXPECT_EQ(0xffffu, sizeBatchBuffer);

    memoryManager->freeGraphicsMemory(commandBuffer);
    memoryManager->freeGraphicsMemory(chainedBatchBuffer);
    memoryManager->freeGraphicsMemory(otherAllocation);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenRegisterCommandChunkIsCalledThenNewChunkIsAddedToTheList) {
    typedef typename FamilyType::MI_BATCH_BUFFER_START MI_BATCH_BUFFER_START;

    auto aubExecutionEnvironment = getEnvironment<AUBCommandStreamReceiverHw<FamilyType>>(false, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<AUBCommandStreamReceiverHw<FamilyType>>();
    LinearStream cs(aubExecutionEnvironment->commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 128u, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};

    aubCsr->getFlatBatchBufferHelper().registerCommandChunk(batchBuffer, sizeof(MI_BATCH_BUFFER_START));
    ASSERT_EQ(1u, aubCsr->getFlatBatchBufferHelper().getCommandChunkList().size());
    EXPECT_EQ(128u + sizeof(MI_BATCH_BUFFER_START), aubCsr->getFlatBatchBufferHelper().getCommandChunkList()[0].endOffset);

    CommandChunk chunk;
    chunk.endOffset = 0x123;
    aubCsr->getFlatBatchBufferHelper().registerCommandChunk(chunk);

    ASSERT_EQ(2u, aubCsr->getFlatBatchBufferHelper().getCommandChunkList().size());
    EXPECT_EQ(0x123u, aubCsr->getFlatBatchBufferHelper().getCommandChunkList()[1].endOffset);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenRemovePatchInfoDataIsCalledThenElementIsRemovedFromPatchInfoList) {
    auto aubExecutionEnvironment = getEnvironment<AUBCommandStreamReceiverHw<FamilyType>>(false, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<AUBCommandStreamReceiverHw<FamilyType>>();

    PatchInfoData patchInfoData(0xA000, 0x0, PatchInfoAllocationType::KernelArg, 0xB000, 0x0, PatchInfoAllocationType::Default);
    aubCsr->getFlatBatchBufferHelper().setPatchInfoData(patchInfoData);
    EXPECT_EQ(1u, aubCsr->getFlatBatchBufferHelper().getPatchInfoCollection().size());

    EXPECT_TRUE(aubCsr->getFlatBatchBufferHelper().removePatchInfoData(0xC000));
    EXPECT_EQ(1u, aubCsr->getFlatBatchBufferHelper().getPatchInfoCollection().size());

    EXPECT_TRUE(aubCsr->getFlatBatchBufferHelper().removePatchInfoData(0xB000));
    EXPECT_EQ(0u, aubCsr->getFlatBatchBufferHelper().getPatchInfoCollection().size());
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenAddGucStartMessageIsCalledThenBatchBufferAddressIsStoredInPatchInfoCollection) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.AddPatchInfoCommentsForAUBDump.set(true);

    auto aubExecutionEnvironment = getEnvironment<AUBCommandStreamReceiverHw<FamilyType>>(false, false, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<AUBCommandStreamReceiverHw<FamilyType>>();
    LinearStream cs(aubExecutionEnvironment->commandBuffer);

    std::unique_ptr<char> batchBuffer(new char[1024]);
    aubCsr->addGUCStartMessage(static_cast<uint64_t>(reinterpret_cast<std::uintptr_t>(batchBuffer.get())), EngineType::ENGINE_RCS);

    auto &patchInfoCollection = aubCsr->getFlatBatchBufferHelper().getPatchInfoCollection();
    ASSERT_EQ(1u, patchInfoCollection.size());
    EXPECT_EQ(patchInfoCollection[0].sourceAllocation, reinterpret_cast<uint64_t>(batchBuffer.get()));
    EXPECT_EQ(patchInfoCollection[0].targetType, PatchInfoAllocationType::GUCStartMessage);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenForcedBatchBufferFlatteningInBatchedDispatchModeThenNewCombinedBatchBufferIsCreated) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.FlattenBatchBufferForAUBDump.set(true);
    DebugManager.flags.AddPatchInfoCommentsForAUBDump.set(true);
    DebugManager.flags.CsrDispatchMode.set(static_cast<uint32_t>(DispatchMode::BatchedDispatch));

    auto aubExecutionEnvironment = getEnvironment<AUBCommandStreamReceiverHw<FamilyType>>(false, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<AUBCommandStreamReceiverHw<FamilyType>>();
    auto memoryManager = aubExecutionEnvironment->executionEnvironment->memoryManager.get();
    LinearStream cs(aubExecutionEnvironment->commandBuffer);

    CommandChunk chunk1;
    CommandChunk chunk2;
    CommandChunk chunk3;

    std::unique_ptr<char> commands1(new char[0x100u]);
    commands1.get()[0] = 0x1;
    chunk1.baseAddressCpu = chunk1.baseAddressGpu = reinterpret_cast<uint64_t>(commands1.get());
    chunk1.startOffset = 0u;
    chunk1.endOffset = 0x50u;

    std::unique_ptr<char> commands2(new char[0x100u]);
    commands2.get()[0] = 0x2;
    chunk2.baseAddressCpu = chunk2.baseAddressGpu = reinterpret_cast<uint64_t>(commands2.get());
    chunk2.startOffset = 0u;
    chunk2.endOffset = 0x50u;
    aubCsr->getFlatBatchBufferHelper().registerBatchBufferStartAddress(reinterpret_cast<uint64_t>(commands2.get() + 0x40), reinterpret_cast<uint64_t>(commands1.get()));

    std::unique_ptr<char> commands3(new char[0x100u]);
    commands3.get()[0] = 0x3;
    chunk3.baseAddressCpu = chunk3.baseAddressGpu = reinterpret_cast<uint64_t>(commands3.get());
    chunk3.startOffset = 0u;
    chunk3.endOffset = 0x50u;
    aubCsr->getFlatBatchBufferHelper().registerBatchBufferStartAddress(reinterpret_cast<uint64_t>(commands3.get() + 0x40), reinterpret_cast<uint64_t>(commands2.get()));

    aubCsr->getFlatBatchBufferHelper().registerCommandChunk(chunk1);
    aubCsr->getFlatBatchBufferHelper().registerCommandChunk(chunk2);
    aubCsr->getFlatBatchBufferHelper().registerCommandChunk(chunk3);

    ASSERT_EQ(3u, aubCsr->getFlatBatchBufferHelper().getCommandChunkList().size());

    PatchInfoData patchInfoData1(0xAAAu, 0xAu, PatchInfoAllocationType::IndirectObjectHeap, chunk1.baseAddressGpu, 0x10, PatchInfoAllocationType::Default);
    PatchInfoData patchInfoData2(0xBBBu, 0xAu, PatchInfoAllocationType::IndirectObjectHeap, chunk1.baseAddressGpu, 0x60, PatchInfoAllocationType::Default);
    PatchInfoData patchInfoData3(0xCCCu, 0xAu, PatchInfoAllocationType::IndirectObjectHeap, 0x0, 0x10, PatchInfoAllocationType::Default);

    aubCsr->getFlatBatchBufferHelper().setPatchInfoData(patchInfoData1);
    aubCsr->getFlatBatchBufferHelper().setPatchInfoData(patchInfoData2);
    aubCsr->getFlatBatchBufferHelper().setPatchInfoData(patchInfoData3);

    ASSERT_EQ(3u, aubCsr->getFlatBatchBufferHelper().getPatchInfoCollection().size());

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 128u, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};

    size_t sizeBatchBuffer = 0u;

    std::unique_ptr<GraphicsAllocation, std::function<void(GraphicsAllocation *)>> flatBatchBuffer(
        aubCsr->getFlatBatchBufferHelper().flattenBatchBuffer(batchBuffer, sizeBatchBuffer, DispatchMode::BatchedDispatch),
        [&](GraphicsAllocation *ptr) { memoryManager->freeGraphicsMemory(ptr); });

    EXPECT_NE(nullptr, flatBatchBuffer.get());
    EXPECT_EQ(alignUp(0x50u + 0x40u + 0x40u + CSRequirements::csOverfetchSize, 0x1000u), sizeBatchBuffer);

    ASSERT_EQ(1u, aubCsr->getFlatBatchBufferHelper().getPatchInfoCollection().size());
    EXPECT_EQ(0xAAAu, aubCsr->getFlatBatchBufferHelper().getPatchInfoCollection()[0].sourceAllocation);

    EXPECT_EQ(0u, aubCsr->getFlatBatchBufferHelper().getCommandChunkList().size());

    EXPECT_EQ(0x3, static_cast<char *>(flatBatchBuffer->getUnderlyingBuffer())[0]);
    EXPECT_EQ(0x2, static_cast<char *>(flatBatchBuffer->getUnderlyingBuffer())[0x40]);
    EXPECT_EQ(0x1, static_cast<char *>(flatBatchBuffer->getUnderlyingBuffer())[0x40 + 0x40]);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenDefaultDebugConfigThenExpectFlattenBatchBufferIsNotCalled) {
    auto aubExecutionEnvironment = getEnvironment<MockAubCsr<FamilyType>>(true, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<MockAubCsr<FamilyType>>();
    LinearStream cs(aubExecutionEnvironment->commandBuffer);

    auto mockHelper = new MockFlatBatchBufferHelper<FamilyType>(*aubExecutionEnvironment->executionEnvironment);
    aubCsr->overwriteFlatBatchBufferHelper(mockHelper);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    auto engineType = OCLRT::ENGINE_RCS;
    ResidencyContainer allocationsForResidency = {};

    EXPECT_CALL(*mockHelper, flattenBatchBuffer(::testing::_, ::testing::_, ::testing::_)).Times(0);
    aubCsr->flush(batchBuffer, engineType, allocationsForResidency, *pDevice->getOsContext());
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenForcedFlattenBatchBufferAndImmediateDispatchModeThenExpectFlattenBatchBufferIsCalled) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.FlattenBatchBufferForAUBDump.set(true);
    DebugManager.flags.CsrDispatchMode.set(static_cast<uint32_t>(DispatchMode::ImmediateDispatch));

    auto aubExecutionEnvironment = getEnvironment<MockAubCsr<FamilyType>>(true, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<MockAubCsr<FamilyType>>();
    auto allocationsForResidency = aubCsr->getResidencyAllocations();
    LinearStream cs(aubExecutionEnvironment->commandBuffer);

    auto mockHelper = new MockFlatBatchBufferHelper<FamilyType>(*aubExecutionEnvironment->executionEnvironment);
    aubCsr->overwriteFlatBatchBufferHelper(mockHelper);

    auto chainedBatchBuffer = aubExecutionEnvironment->executionEnvironment->memoryManager->allocateGraphicsMemory(128u, 64u, false, false);
    ASSERT_NE(nullptr, chainedBatchBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 128u, chainedBatchBuffer, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    auto engineType = OCLRT::ENGINE_RCS;

    aubCsr->makeResident(*chainedBatchBuffer);

    std::unique_ptr<GraphicsAllocation, std::function<void(GraphicsAllocation *)>> ptr(
        aubExecutionEnvironment->executionEnvironment->memoryManager->allocateGraphicsMemory(4096, 4096, false, false),
        [&](GraphicsAllocation *ptr) { aubExecutionEnvironment->executionEnvironment->memoryManager->freeGraphicsMemory(ptr); });

    auto expectedAllocation = ptr.get();
    EXPECT_CALL(*mockHelper, flattenBatchBuffer(::testing::_, ::testing::_, ::testing::_)).WillOnce(::testing::Return(ptr.release()));
    aubCsr->flush(batchBuffer, engineType, allocationsForResidency, *pDevice->getOsContext());

    EXPECT_EQ(batchBuffer.commandBufferAllocation, expectedAllocation);

    aubExecutionEnvironment->executionEnvironment->memoryManager->freeGraphicsMemory(chainedBatchBuffer);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenForcedFlattenBatchBufferAndImmediateDispatchModeAndThereIsNoChainedBatchBufferThenExpectFlattenBatchBufferIsCalledAnyway) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.FlattenBatchBufferForAUBDump.set(true);
    DebugManager.flags.CsrDispatchMode.set(static_cast<uint32_t>(DispatchMode::ImmediateDispatch));

    auto aubExecutionEnvironment = getEnvironment<MockAubCsr<FamilyType>>(true, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<MockAubCsr<FamilyType>>();
    auto allocationsForResidency = aubCsr->getResidencyAllocations();
    LinearStream cs(aubExecutionEnvironment->commandBuffer);

    auto mockHelper = new MockFlatBatchBufferHelper<FamilyType>(*aubExecutionEnvironment->executionEnvironment);
    aubCsr->overwriteFlatBatchBufferHelper(mockHelper);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 128u, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    auto engineType = OCLRT::ENGINE_RCS;

    EXPECT_CALL(*mockHelper, flattenBatchBuffer(::testing::_, ::testing::_, ::testing::_)).Times(1);
    aubCsr->flush(batchBuffer, engineType, allocationsForResidency, *pDevice->getOsContext());
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenForcedFlattenBatchBufferAndBatchedDispatchModeThenExpectFlattenBatchBufferIsCalledAnyway) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.FlattenBatchBufferForAUBDump.set(true);
    DebugManager.flags.CsrDispatchMode.set(static_cast<uint32_t>(DispatchMode::BatchedDispatch));

    auto aubExecutionEnvironment = getEnvironment<MockAubCsr<FamilyType>>(true, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<MockAubCsr<FamilyType>>();
    LinearStream cs(aubExecutionEnvironment->commandBuffer);

    auto mockHelper = new MockFlatBatchBufferHelper<FamilyType>(*aubExecutionEnvironment->executionEnvironment);
    aubCsr->overwriteFlatBatchBufferHelper(mockHelper);
    ResidencyContainer allocationsForResidency;

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 128u, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    auto engineType = OCLRT::ENGINE_RCS;

    EXPECT_CALL(*mockHelper, flattenBatchBuffer(::testing::_, ::testing::_, ::testing::_)).Times(1);
    aubCsr->flush(batchBuffer, engineType, allocationsForResidency, *pDevice->getOsContext());
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenAddPatchInfoCommentsForAUBDumpIsSetThenAddPatchInfoCommentsIsCalled) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.AddPatchInfoCommentsForAUBDump.set(true);

    auto aubExecutionEnvironment = getEnvironment<MockAubCsr<FamilyType>>(true, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<MockAubCsr<FamilyType>>();
    LinearStream cs(aubExecutionEnvironment->commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    auto engineType = OCLRT::ENGINE_RCS;
    ResidencyContainer allocationsForResidency;

    EXPECT_CALL(*aubCsr, addPatchInfoComments()).Times(1);
    aubCsr->flush(batchBuffer, engineType, allocationsForResidency, *pDevice->getOsContext());
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenAddPatchInfoCommentsForAUBDumpIsNotSetThenAddPatchInfoCommentsIsNotCalled) {
    auto aubExecutionEnvironment = getEnvironment<MockAubCsr<FamilyType>>(true, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<MockAubCsr<FamilyType>>();
    LinearStream cs(aubExecutionEnvironment->commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    auto engineType = OCLRT::ENGINE_RCS;
    ResidencyContainer allocationsForResidency;

    EXPECT_CALL(*aubCsr, addPatchInfoComments()).Times(0);

    aubCsr->flush(batchBuffer, engineType, allocationsForResidency, *pDevice->getOsContext());
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenGetIndirectPatchCommandsIsCalledForEmptyPatchInfoListThenIndirectPatchCommandBufferIsNotCreated) {
    auto aubExecutionEnvironment = getEnvironment<AUBCommandStreamReceiverHw<FamilyType>>(false, false, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<AUBCommandStreamReceiverHw<FamilyType>>();

    size_t indirectPatchCommandsSize = 0u;
    std::vector<PatchInfoData> indirectPatchInfo;

    std::unique_ptr<char> commandBuffer(aubCsr->getFlatBatchBufferHelper().getIndirectPatchCommands(indirectPatchCommandsSize, indirectPatchInfo));
    EXPECT_EQ(0u, indirectPatchCommandsSize);
    EXPECT_EQ(0u, indirectPatchInfo.size());
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenGetIndirectPatchCommandsIsCalledForNonEmptyPatchInfoListThenIndirectPatchCommandBufferIsCreated) {
    typedef typename FamilyType::MI_STORE_DATA_IMM MI_STORE_DATA_IMM;
    std::unique_ptr<MemoryManager> memoryManager(nullptr);
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>(*platformDevices[0], "", true, *pDevice->executionEnvironment));
    memoryManager.reset(aubCsr->createMemoryManager(false, false));

    PatchInfoData patchInfo1(0xA000, 0u, PatchInfoAllocationType::KernelArg, 0x6000, 0x100, PatchInfoAllocationType::IndirectObjectHeap);
    PatchInfoData patchInfo2(0xB000, 0u, PatchInfoAllocationType::KernelArg, 0x6000, 0x200, PatchInfoAllocationType::IndirectObjectHeap);
    PatchInfoData patchInfo3(0xC000, 0u, PatchInfoAllocationType::IndirectObjectHeap, 0x1000, 0x100, PatchInfoAllocationType::Default);
    PatchInfoData patchInfo4(0xC000, 0u, PatchInfoAllocationType::Default, 0x2000, 0x100, PatchInfoAllocationType::GUCStartMessage);

    aubCsr->getFlatBatchBufferHelper().setPatchInfoData(patchInfo1);
    aubCsr->getFlatBatchBufferHelper().setPatchInfoData(patchInfo2);
    aubCsr->getFlatBatchBufferHelper().setPatchInfoData(patchInfo3);
    aubCsr->getFlatBatchBufferHelper().setPatchInfoData(patchInfo4);

    size_t indirectPatchCommandsSize = 0u;
    std::vector<PatchInfoData> indirectPatchInfo;

    std::unique_ptr<char> commandBuffer(aubCsr->getFlatBatchBufferHelper().getIndirectPatchCommands(indirectPatchCommandsSize, indirectPatchInfo));
    EXPECT_EQ(4u, indirectPatchInfo.size());
    EXPECT_EQ(2u * sizeof(MI_STORE_DATA_IMM), indirectPatchCommandsSize);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenAddBatchBufferStartCalledAndBatchBUfferFlatteningEnabledThenBatchBufferStartAddressIsRegistered) {
    typedef typename FamilyType::MI_BATCH_BUFFER_START MI_BATCH_BUFFER_START;
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.FlattenBatchBufferForAUBDump.set(true);

    std::unique_ptr<MemoryManager> memoryManager(nullptr);
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>(*platformDevices[0], "", true, *pDevice->executionEnvironment));
    memoryManager.reset(aubCsr->createMemoryManager(false, false));

    MI_BATCH_BUFFER_START bbStart;

    aubCsr->addBatchBufferStart(&bbStart, 0xA000u, false);
    std::map<uint64_t, uint64_t> &batchBufferStartAddressSequence = aubCsr->getFlatBatchBufferHelper().getBatchBufferStartAddressSequence();

    ASSERT_EQ(1u, batchBufferStartAddressSequence.size());
    std::pair<uint64_t, uint64_t> addr = *batchBufferStartAddressSequence.begin();
    EXPECT_EQ(reinterpret_cast<uint64_t>(&bbStart), addr.first);
    EXPECT_EQ(0xA000u, addr.second);
}

class OsAgnosticMemoryManagerForImagesWithNoHostPtr : public OsAgnosticMemoryManager {
  public:
    OsAgnosticMemoryManagerForImagesWithNoHostPtr(ExecutionEnvironment &executionEnvironment) : OsAgnosticMemoryManager(false, false, executionEnvironment) {}

    GraphicsAllocation *allocateGraphicsMemoryForImage(ImageInfo &imgInfo, Gmm *gmm) override {
        auto imageAllocation = OsAgnosticMemoryManager::allocateGraphicsMemoryForImage(imgInfo, gmm);
        cpuPtr = imageAllocation->getUnderlyingBuffer();
        imageAllocation->setCpuPtrAndGpuAddress(nullptr, imageAllocation->getGpuAddress());
        return imageAllocation;
    };
    void freeGraphicsMemoryImpl(GraphicsAllocation *imageAllocation) override {
        imageAllocation->setCpuPtrAndGpuAddress(cpuPtr, imageAllocation->getGpuAddress());
        OsAgnosticMemoryManager::freeGraphicsMemoryImpl(imageAllocation);
    };
    void *lockResource(GraphicsAllocation *imageAllocation) override {
        lockResourceParam.wasCalled = true;
        lockResourceParam.inImageAllocation = imageAllocation;
        lockCpuPtr = alignedMalloc(imageAllocation->getUnderlyingBufferSize(), MemoryConstants::pageSize);
        lockResourceParam.retCpuPtr = lockCpuPtr;
        return lockResourceParam.retCpuPtr;
    };
    void unlockResource(GraphicsAllocation *imageAllocation) override {
        unlockResourceParam.wasCalled = true;
        unlockResourceParam.inImageAllocation = imageAllocation;
        alignedFree(lockCpuPtr);
    };

    struct LockResourceParam {
        bool wasCalled = false;
        GraphicsAllocation *inImageAllocation = nullptr;
        void *retCpuPtr = nullptr;
    } lockResourceParam;
    struct UnlockResourceParam {
        bool wasCalled = false;
        GraphicsAllocation *inImageAllocation = nullptr;
    } unlockResourceParam;

  protected:
    void *cpuPtr = nullptr;
    void *lockCpuPtr = nullptr;
};

using AubCommandStreamReceiverNoHostPtrTests = ::testing::Test;
HWTEST_F(AubCommandStreamReceiverNoHostPtrTests, givenAubCommandStreamReceiverWhenWriteMemoryIsCalledOnImageWithNoHostPtrThenResourceShouldBeLockedToGetCpuAddress) {
    ExecutionEnvironment executionEnvironment;
    auto memoryManager = new OsAgnosticMemoryManagerForImagesWithNoHostPtr(executionEnvironment);
    executionEnvironment.memoryManager.reset(memoryManager);

    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>(*platformDevices[0], "", true, executionEnvironment));

    cl_image_desc imgDesc = {};
    imgDesc.image_width = 512;
    imgDesc.image_height = 1;
    imgDesc.image_type = CL_MEM_OBJECT_IMAGE2D;

    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);
    auto queryGmm = MockGmm::queryImgParams(imgInfo);

    auto imageAllocation = memoryManager->allocateGraphicsMemoryForImage(imgInfo, queryGmm.get());
    ASSERT_NE(nullptr, imageAllocation);

    EXPECT_TRUE(aubCsr->writeMemory(*imageAllocation));

    EXPECT_TRUE(memoryManager->lockResourceParam.wasCalled);
    EXPECT_EQ(imageAllocation, memoryManager->lockResourceParam.inImageAllocation);
    EXPECT_NE(nullptr, memoryManager->lockResourceParam.retCpuPtr);

    EXPECT_TRUE(memoryManager->unlockResourceParam.wasCalled);
    EXPECT_EQ(imageAllocation, memoryManager->unlockResourceParam.inImageAllocation);

    queryGmm.release();
    memoryManager->freeGraphicsMemory(imageAllocation);
}

HWTEST_F(AubCommandStreamReceiverTests, givenNoDbgDeviceIdFlagWhenAubCsrIsCreatedThenUseDefaultDeviceId) {
    const HardwareInfo &hwInfoIn = *platformDevices[0];
    std::unique_ptr<MockAubCsr<FamilyType>> aubCsr(new MockAubCsr<FamilyType>(hwInfoIn, "", true, *pDevice->executionEnvironment));
    EXPECT_EQ(hwInfoIn.capabilityTable.aubDeviceId, aubCsr->aubDeviceId);
}

HWTEST_F(AubCommandStreamReceiverTests, givenDbgDeviceIdFlagIsSetWhenAubCsrIsCreatedThenUseDebugDeviceId) {
    DebugManagerStateRestore stateRestore;
    DebugManager.flags.OverrideAubDeviceId.set(9); //this is Hsw, not used
    const HardwareInfo &hwInfoIn = *platformDevices[0];
    std::unique_ptr<MockAubCsr<FamilyType>> aubCsr(new MockAubCsr<FamilyType>(hwInfoIn, "", true, *pDevice->executionEnvironment));
    EXPECT_EQ(9u, aubCsr->aubDeviceId);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenGetGTTDataIsCalledThenLocalMemoryIsSetAccordingToCsrFeature) {
    const HardwareInfo &hwInfoIn = *platformDevices[0];
    std::unique_ptr<MockAubCsr<FamilyType>> aubCsr(new MockAubCsr<FamilyType>(hwInfoIn, "", true, *pDevice->executionEnvironment));
    AubGTTData data = {};
    aubCsr->getGTTData(nullptr, data);
    EXPECT_TRUE(data.present);

    if (aubCsr->localMemoryEnabled) {
        EXPECT_TRUE(data.localMemory);
    } else {
        EXPECT_FALSE(data.localMemory);
    }
}

HWTEST_F(AubCommandStreamReceiverTests, givenPhysicalAddressWhenSetGttEntryIsCalledThenGttEntrysBitFieldsShouldBePopulated) {
    typedef typename AUBFamilyMapper<FamilyType>::AUB AUB;

    AubMemDump::MiGttEntry entry = {};
    uint64_t address = 0x0123456789;
    AubGTTData data = {true, false};
    AUB::setGttEntry(entry, address, data);

    EXPECT_EQ(entry.pageConfig.PhysicalAddress, address / 4096);
    EXPECT_TRUE(entry.pageConfig.Present);
    EXPECT_FALSE(entry.pageConfig.LocalMemory);
}

HWTEST_F(AubCommandStreamReceiverTests, whenGetMemoryBankForGttIsCalledThenCorrectBankIsReturned) {
    const HardwareInfo &hwInfoIn = *platformDevices[0];
    std::unique_ptr<MockAubCsr<FamilyType>> aubCsr(new MockAubCsr<FamilyType>(hwInfoIn, "", true, *pDevice->executionEnvironment));
    aubCsr->localMemoryEnabled = false;

    auto bank = aubCsr->getMemoryBankForGtt();
    EXPECT_EQ(MemoryBanks::MainBank, bank);
}

HWTEST_F(AubCommandStreamReceiverTests, givenEntryBitsPresentAndWritableWhenGetAddressSpaceFromPTEBitsIsCalledThenTraceNonLocalIsReturned) {
    const HardwareInfo &hwInfoIn = *platformDevices[0];
    std::unique_ptr<MockAubCsr<FamilyType>> aubCsr(new MockAubCsr<FamilyType>(hwInfoIn, "", true, *pDevice->executionEnvironment));

    auto space = aubCsr->getAddressSpaceFromPTEBits(PageTableEntry::presentBit | PageTableEntry::writableBit);
    EXPECT_EQ(AubMemDump::AddressSpaceValues::TraceNonlocal, space);
}

template <typename GfxFamily>
struct MockAubCsrToTestExternalAllocations : public AUBCommandStreamReceiverHw<GfxFamily> {
    using AUBCommandStreamReceiverHw<GfxFamily>::AUBCommandStreamReceiverHw;
    using AUBCommandStreamReceiverHw<GfxFamily>::externalAllocations;

    bool writeMemory(AllocationView &allocationView) override {
        writeMemoryParametrization.wasCalled = true;
        writeMemoryParametrization.receivedAllocationView = allocationView;
        writeMemoryParametrization.statusToReturn = (0 != allocationView.second) ? true : false;
        return writeMemoryParametrization.statusToReturn;
    }
    struct WriteMemoryParametrization {
        bool wasCalled = false;
        AllocationView receivedAllocationView = {};
        bool statusToReturn = false;
    } writeMemoryParametrization;
};

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenMakeResidentExternalIsCalledThenGivenAllocationViewShouldBeAddedToExternalAllocations) {
    auto aubCsr = std::make_unique<MockAubCsrToTestExternalAllocations<FamilyType>>(*platformDevices[0], "", true, *pDevice->executionEnvironment);
    size_t size = 100;
    auto ptr = std::make_unique<char[]>(size);
    auto addr = reinterpret_cast<uint64_t>(ptr.get());
    AllocationView externalAllocation(addr, size);

    ASSERT_EQ(0u, aubCsr->externalAllocations.size());
    aubCsr->makeResidentExternal(externalAllocation);
    EXPECT_EQ(1u, aubCsr->externalAllocations.size());
    EXPECT_EQ(addr, aubCsr->externalAllocations[0].first);
    EXPECT_EQ(size, aubCsr->externalAllocations[0].second);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenMakeNonResidentExternalIsCalledThenMatchingAllocationViewShouldBeRemovedFromExternalAllocations) {
    auto aubCsr = std::make_unique<MockAubCsrToTestExternalAllocations<FamilyType>>(*platformDevices[0], "", true, *pDevice->executionEnvironment);
    size_t size = 100;
    auto ptr = std::make_unique<char[]>(size);
    auto addr = reinterpret_cast<uint64_t>(ptr.get());
    AllocationView externalAllocation(addr, size);
    aubCsr->makeResidentExternal(externalAllocation);

    ASSERT_EQ(1u, aubCsr->externalAllocations.size());
    aubCsr->makeNonResidentExternal(addr);
    EXPECT_EQ(0u, aubCsr->externalAllocations.size());
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenMakeNonResidentExternalIsCalledThenNonMatchingAllocationViewShouldNotBeRemovedFromExternalAllocations) {
    auto aubCsr = std::make_unique<MockAubCsrToTestExternalAllocations<FamilyType>>(*platformDevices[0], "", true, *pDevice->executionEnvironment);
    size_t size = 100;
    auto ptr = std::make_unique<char[]>(size);
    auto addr = reinterpret_cast<uint64_t>(ptr.get());
    AllocationView externalAllocation(addr, size);
    aubCsr->makeResidentExternal(externalAllocation);

    ASSERT_EQ(1u, aubCsr->externalAllocations.size());
    aubCsr->makeNonResidentExternal(0);
    EXPECT_EQ(1u, aubCsr->externalAllocations.size());
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenProcessResidencyIsCalledThenExternalAllocationsShouldBeMadeResident) {
    auto aubCsr = std::make_unique<MockAubCsrToTestExternalAllocations<FamilyType>>(*platformDevices[0], "", true, *pDevice->executionEnvironment);
    size_t size = 100;
    auto ptr = std::make_unique<char[]>(size);
    auto addr = reinterpret_cast<uint64_t>(ptr.get());
    AllocationView externalAllocation(addr, size);
    aubCsr->makeResidentExternal(externalAllocation);

    ASSERT_EQ(1u, aubCsr->externalAllocations.size());
    ResidencyContainer allocationsForResidency;
    aubCsr->processResidency(allocationsForResidency, *pDevice->getOsContext());

    EXPECT_TRUE(aubCsr->writeMemoryParametrization.wasCalled);
    EXPECT_EQ(addr, aubCsr->writeMemoryParametrization.receivedAllocationView.first);
    EXPECT_EQ(size, aubCsr->writeMemoryParametrization.receivedAllocationView.second);
    EXPECT_TRUE(aubCsr->writeMemoryParametrization.statusToReturn);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenProcessResidencyIsCalledThenExternalAllocationWithZeroSizeShouldNotBeMadeResident) {
    auto aubCsr = std::make_unique<MockAubCsrToTestExternalAllocations<FamilyType>>(*platformDevices[0], "", true, *pDevice->executionEnvironment);
    AllocationView externalAllocation(0, 0);
    aubCsr->makeResidentExternal(externalAllocation);

    ASSERT_EQ(1u, aubCsr->externalAllocations.size());
    ResidencyContainer allocationsForResidency;
    aubCsr->processResidency(allocationsForResidency, *pDevice->getOsContext());

    EXPECT_TRUE(aubCsr->writeMemoryParametrization.wasCalled);
    EXPECT_EQ(0u, aubCsr->writeMemoryParametrization.receivedAllocationView.first);
    EXPECT_EQ(0u, aubCsr->writeMemoryParametrization.receivedAllocationView.second);
    EXPECT_FALSE(aubCsr->writeMemoryParametrization.statusToReturn);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenWriteMemoryIsCalledThenGraphicsAllocationSizeIsReadCorrectly) {
    std::unique_ptr<MemoryManager> memoryManager(nullptr);
    auto aubCsr = std::make_unique<AUBCommandStreamReceiverHw<FamilyType>>(*platformDevices[0], "", false, *pDevice->executionEnvironment);
    memoryManager.reset(aubCsr->createMemoryManager(false, false));

    PhysicalAddressAllocator allocator;
    struct PpgttMock : std::conditional<is64bit, PML4, PDPE>::type {
        PpgttMock(PhysicalAddressAllocator *allocator) : std::conditional<is64bit, PML4, PDPE>::type(allocator) {}

        void pageWalk(uintptr_t vm, size_t size, size_t offset, uint64_t entryBits, PageWalker &pageWalker, uint32_t memoryBank) override {
            receivedSize = size;
        }
        size_t receivedSize = 0;
    };
    auto ppgttMock = new PpgttMock(&allocator);

    aubCsr->ppgtt.reset(ppgttMock);

    auto gfxAllocation = memoryManager->allocateGraphicsMemory(sizeof(uint32_t), sizeof(uint32_t), false, false);
    gfxAllocation->setAubWritable(true);

    auto gmm = new Gmm(nullptr, 1, false);
    gfxAllocation->gmm = gmm;

    for (bool compressed : {false, true}) {
        gmm->isRenderCompressed = compressed;

        aubCsr->writeMemory(*gfxAllocation);

        if (compressed) {
            EXPECT_EQ(gfxAllocation->gmm->gmmResourceInfo->getSizeAllocation(), ppgttMock->receivedSize);
        } else {
            EXPECT_EQ(gfxAllocation->getUnderlyingBufferSize(), ppgttMock->receivedSize);
        }
    }

    memoryManager->freeGraphicsMemory(gfxAllocation);
}

HWTEST_F(AubCommandStreamReceiverTests, whenAubCommandStreamReceiverIsCreatedThenPPGTTAndGGTTCreatedHavePhysicalAddressAllocatorSet) {
    auto aubCsr = std::make_unique<AUBCommandStreamReceiverHw<FamilyType>>(*platformDevices[0], "", false, *pDevice->executionEnvironment);
    ASSERT_NE(nullptr, aubCsr->ppgtt.get());
    ASSERT_NE(nullptr, aubCsr->ggtt.get());

    uintptr_t address = 0x20000;
    auto physicalAddress = aubCsr->ppgtt->map(address, MemoryConstants::pageSize, 0, MemoryBanks::MainBank);
    EXPECT_NE(0u, physicalAddress);

    physicalAddress = aubCsr->ggtt->map(address, MemoryConstants::pageSize, 0, MemoryBanks::MainBank);
    EXPECT_NE(0u, physicalAddress);
}
