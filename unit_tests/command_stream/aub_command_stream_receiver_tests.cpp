/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "runtime/aub_mem_dump/aub_mem_dump.h"
#include "runtime/command_stream/aub_command_stream_receiver_hw.h"
#include "runtime/helpers/dispatch_info.h"
#include "runtime/helpers/flat_batch_buffer_helper_hw.h"
#include "runtime/helpers/hw_info.h"
#include "runtime/memory_manager/memory_manager.h"
#include "runtime/os_interface/debug_settings_manager.h"
#include "test.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/mocks/mock_aub_subcapture_manager.h"
#include "unit_tests/mocks/mock_csr.h"
#include "unit_tests/mocks/mock_gmm.h"
#include "unit_tests/mocks/mock_kernel.h"
#include "unit_tests/mocks/mock_mdi.h"

#include <memory>

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winconsistent-missing-override"
#endif

using namespace OCLRT;

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

typedef Test<DeviceFixture> AubCommandStreamReceiverTests;

template <typename GfxFamily>
struct MockAubCsr : public AUBCommandStreamReceiverHw<GfxFamily> {
    MockAubCsr(const HardwareInfo &hwInfoIn, const std::string &fileName, bool standalone)
        : AUBCommandStreamReceiverHw<GfxFamily>(hwInfoIn, fileName, standalone){};

    DispatchMode peekDispatchMode() const {
        return this->dispatchMode;
    }

    GraphicsAllocation *getTagAllocation() const {
        return this->tagAllocation;
    }

    void setLatestSentTaskCount(uint32_t latestSentTaskCount) {
        this->latestSentTaskCount = latestSentTaskCount;
    }

    void flushBatchedSubmissions() override {
        flushBatchedSubmissionsCalled = true;
    }
    void initProgrammingFlags() override {
        initProgrammingFlagsCalled = true;
    }
    bool flushBatchedSubmissionsCalled = false;
    bool initProgrammingFlagsCalled = false;

    void initFile(const std::string &fileName) override {
        fileIsOpen = true;
        openFileName = fileName;
    }
    void closeFile() override {
        fileIsOpen = false;
        openFileName = "";
    }
    bool isFileOpen() override {
        return fileIsOpen;
    }
    const std::string &getFileName() override {
        return openFileName;
    }
    bool fileIsOpen = false;
    std::string openFileName = "";

    MOCK_METHOD0(addPatchInfoComments, bool(void));
};

template <typename GfxFamily>
struct MockAubCsrToTestDumpAubNonWritable : public AUBCommandStreamReceiverHw<GfxFamily> {
    using AUBCommandStreamReceiverHw<GfxFamily>::AUBCommandStreamReceiverHw;
    using AUBCommandStreamReceiverHw<GfxFamily>::dumpAubNonWritable;

    bool writeMemory(GraphicsAllocation &gfxAllocation) override {
        return true;
    }
};

struct MockAubFileStream : public AUBCommandStreamReceiver::AubFileStream {
    void flush() override {
        flushCalled = true;
    }
    bool flushCalled = false;
};

struct GmockAubFileStream : public AUBCommandStreamReceiver::AubFileStream {
    MOCK_METHOD1(addComment, bool(const char *message));
};

struct AubExecutionEnvironment {
    std::unique_ptr<ExecutionEnvironment> executionEnvironment;
    GraphicsAllocation *commandBuffer = nullptr;
    template <typename CsrType>
    CsrType *getCsr() {
        return static_cast<CsrType *>(executionEnvironment->commandStreamReceiver.get());
    }
    ~AubExecutionEnvironment() {
        if (commandBuffer) {
            executionEnvironment->memoryManager->freeGraphicsMemory(commandBuffer);
        }
    }
};

template <typename CsrType>
std::unique_ptr<AubExecutionEnvironment> getEnvironment(bool createTagAllocation, bool allocateCommandBuffer) {
    std::unique_ptr<ExecutionEnvironment> executionEnvironment(new ExecutionEnvironment);
    executionEnvironment->commandStreamReceiver.reset(new CsrType(*platformDevices[0], "", true));
    executionEnvironment->memoryManager.reset(executionEnvironment->commandStreamReceiver->createMemoryManager(false));
    executionEnvironment->commandStreamReceiver->setMemoryManager(executionEnvironment->memoryManager.get());
    if (createTagAllocation) {
        executionEnvironment->commandStreamReceiver->initializeTagAllocation();
    }

    std::unique_ptr<AubExecutionEnvironment> aubExecutionEnvironment(new AubExecutionEnvironment);
    if (allocateCommandBuffer) {
        aubExecutionEnvironment->commandBuffer = executionEnvironment->memoryManager->allocateGraphicsMemory(4096);
    }
    aubExecutionEnvironment->executionEnvironment = std::move(executionEnvironment);
    return aubExecutionEnvironment;
}

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

    CommandStreamReceiver *aubCsr = AUBCommandStreamReceiver::create(hwInfo, "", true);
    EXPECT_EQ(nullptr, aubCsr);

    const_cast<PLATFORM *>(hwInfo.pPlatform)->eRenderCoreFamily = family;
}

TEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenTypeIsCheckedThenAubCsrIsReturned) {
    HardwareInfo hwInfo = *platformDevices[0];
    std::unique_ptr<CommandStreamReceiver> aubCsr(AUBCommandStreamReceiver::create(hwInfo, "", true));
    EXPECT_NE(nullptr, aubCsr);
    EXPECT_EQ(CommandStreamReceiverType::CSR_AUB, aubCsr->getType());
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCsrWhenItIsCreatedWithDefaultSettingsThenItHasBatchedDispatchModeEnabled) {
    DebugManagerStateRestore stateRestore;
    DebugManager.flags.CsrDispatchMode.set(0);
    std::unique_ptr<MockAubCsr<FamilyType>> aubCsr(new MockAubCsr<FamilyType>(*platformDevices[0], "", true));
    EXPECT_EQ(DispatchMode::BatchedDispatch, aubCsr->peekDispatchMode());
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCsrWhenItIsCreatedWithDebugSettingsThenItHasProperDispatchModeEnabled) {
    DebugManagerStateRestore stateRestore;
    DebugManager.flags.CsrDispatchMode.set(static_cast<uint32_t>(DispatchMode::ImmediateDispatch));
    std::unique_ptr<MockAubCsr<FamilyType>> aubCsr(new MockAubCsr<FamilyType>(*platformDevices[0], "", true));
    EXPECT_EQ(DispatchMode::ImmediateDispatch, aubCsr->peekDispatchMode());
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenItIsCreatedThenMemoryManagerIsNotNull) {
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>(**platformDevices, "", true));
    std::unique_ptr<MemoryManager> memoryManager(aubCsr->createMemoryManager(false));
    EXPECT_NE(nullptr, memoryManager.get());
    aubCsr->setMemoryManager(nullptr);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverInSubCaptureModeWhenItIsCreatedThenFileIsNotCreated) {
    DebugManagerStateRestore stateRestore;
    DebugManager.flags.AUBDumpSubCaptureMode.set(static_cast<int32_t>(AubSubCaptureManager::SubCaptureMode::Filter));
    HardwareInfo hwInfo = *platformDevices[0];
    std::string fileName = "file_name.aub";
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(reinterpret_cast<AUBCommandStreamReceiverHw<FamilyType> *>(AUBCommandStreamReceiver::create(hwInfo, fileName, true)));
    EXPECT_NE(nullptr, aubCsr);
    EXPECT_FALSE(aubCsr->isFileOpen());
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCsrInSubCaptureModeWhenItIsCreatedWithoutDebugFilterSettingsThenItInitializesSubCaptureFiltersWithDefaults) {
    DebugManagerStateRestore stateRestore;
    DebugManager.flags.AUBDumpSubCaptureMode.set(static_cast<int32_t>(AubSubCaptureManager::SubCaptureMode::Filter));
    std::unique_ptr<MockAubCsr<FamilyType>> aubCsr(new MockAubCsr<FamilyType>(*platformDevices[0], "", true));
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
    std::unique_ptr<MockAubCsr<FamilyType>> aubCsr(new MockAubCsr<FamilyType>(*platformDevices[0], "", true));
    EXPECT_EQ(static_cast<uint32_t>(DebugManager.flags.AUBDumpFilterKernelStartIdx.get()), aubCsr->subCaptureManager->subCaptureFilter.dumpKernelStartIdx);
    EXPECT_EQ(static_cast<uint32_t>(DebugManager.flags.AUBDumpFilterKernelEndIdx.get()), aubCsr->subCaptureManager->subCaptureFilter.dumpKernelEndIdx);
    EXPECT_STREQ(DebugManager.flags.AUBDumpFilterKernelName.get().c_str(), aubCsr->subCaptureManager->subCaptureFilter.dumpKernelName.c_str());
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenInitFileIsCalledWithInvalidFileNameThenFileIsNotOpened) {
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>(**platformDevices, "", true));
    std::string invalidFileName = "";

    aubCsr->initFile(invalidFileName);
    EXPECT_FALSE(aubCsr->isFileOpen());
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenInitFileIsCalledThenFileIsOpenedAndFileNameIsStored) {
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>(**platformDevices, "", true));
    std::string fileName = "file_name.aub";

    aubCsr->initFile(fileName);
    EXPECT_TRUE(aubCsr->isFileOpen());
    EXPECT_STREQ(fileName.c_str(), aubCsr->getFileName().c_str());

    aubCsr->closeFile();
    EXPECT_FALSE(aubCsr->isFileOpen());
    EXPECT_TRUE(aubCsr->getFileName().empty());
}

HWTEST_F(AubCommandStreamReceiverTests, givenGraphicsAllocationWhenMakeResidentCalledMultipleTimesAffectsResidencyOnce) {
    std::unique_ptr<MemoryManager> memoryManager(nullptr);
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>(*platformDevices[0], "", true));
    memoryManager.reset(aubCsr->createMemoryManager(false));
    auto gfxAllocation = memoryManager->allocateGraphicsMemory(sizeof(uint32_t), sizeof(uint32_t), false, false);

    // First makeResident marks the allocation resident
    aubCsr->makeResident(*gfxAllocation);
    EXPECT_NE(ObjectNotResident, gfxAllocation->residencyTaskCount);
    EXPECT_EQ((int)aubCsr->peekTaskCount() + 1, gfxAllocation->residencyTaskCount);
    EXPECT_EQ(1u, memoryManager->getResidencyAllocations().size());

    // Second makeResident should have no impact
    aubCsr->makeResident(*gfxAllocation);
    EXPECT_NE(ObjectNotResident, gfxAllocation->residencyTaskCount);
    EXPECT_EQ((int)aubCsr->peekTaskCount() + 1, gfxAllocation->residencyTaskCount);
    EXPECT_EQ(1u, memoryManager->getResidencyAllocations().size());

    // First makeNonResident marks the allocation as nonresident
    aubCsr->makeNonResident(*gfxAllocation);
    EXPECT_EQ(ObjectNotResident, gfxAllocation->residencyTaskCount);
    EXPECT_EQ(1u, memoryManager->getEvictionAllocations().size());

    // Second makeNonResident should have no impact
    aubCsr->makeNonResident(*gfxAllocation);
    EXPECT_EQ(ObjectNotResident, gfxAllocation->residencyTaskCount);
    EXPECT_EQ(1u, memoryManager->getEvictionAllocations().size());

    memoryManager->freeGraphicsMemoryImpl(gfxAllocation);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenFlushIsCalledThenItShouldInitializeEngineInfoTable) {
    auto aubExecutionEnvironment = getEnvironment<AUBCommandStreamReceiverHw<FamilyType>>(true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<AUBCommandStreamReceiverHw<FamilyType>>();
    LinearStream cs(aubExecutionEnvironment->commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    auto engineType = OCLRT::ENGINE_RCS;
    ResidencyContainer allocationsForResidency = {};

    aubCsr->flush(batchBuffer, engineType, &allocationsForResidency);
    EXPECT_NE(nullptr, aubCsr->engineInfoTable[engineType].pLRCA);
    EXPECT_NE(nullptr, aubCsr->engineInfoTable[engineType].pGlobalHWStatusPage);
    EXPECT_NE(nullptr, aubCsr->engineInfoTable[engineType].pRingBuffer);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverInSubCaptureModeWhenFlushIsCalledButSubCaptureIsDisabledThenItShouldntInitializeEngineInfoTable) {
    auto aubExecutionEnvironment = getEnvironment<AUBCommandStreamReceiverHw<FamilyType>>(true, true);
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

    aubCsr->flush(batchBuffer, engineType, &allocationsForResidency);
    EXPECT_EQ(nullptr, aubCsr->engineInfoTable[engineType].pLRCA);
    EXPECT_EQ(nullptr, aubCsr->engineInfoTable[engineType].pGlobalHWStatusPage);
    EXPECT_EQ(nullptr, aubCsr->engineInfoTable[engineType].pRingBuffer);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenFlushIsCalledThenItShouldLeaveProperRingTailAlignment) {
    auto aubExecutionEnvironment = getEnvironment<AUBCommandStreamReceiverHw<FamilyType>>(true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<AUBCommandStreamReceiverHw<FamilyType>>();
    LinearStream cs(aubExecutionEnvironment->commandBuffer);

    auto engineType = OCLRT::ENGINE_RCS;
    auto ringTailAlignment = sizeof(uint64_t);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};

    // First flush typically includes a preamble and chain to command buffer
    aubCsr->overrideDispatchPolicy(DispatchMode::ImmediateDispatch);
    aubCsr->flush(batchBuffer, engineType, nullptr);
    EXPECT_EQ(0ull, aubCsr->engineInfoTable[engineType].tailRingBuffer % ringTailAlignment);

    // Second flush should just submit command buffer
    cs.getSpace(sizeof(uint64_t));
    aubCsr->flush(batchBuffer, engineType, nullptr);
    EXPECT_EQ(0ull, aubCsr->engineInfoTable[engineType].tailRingBuffer % ringTailAlignment);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverInNonStandaloneModeWhenFlushIsCalledThenItShouldNotUpdateHwTagWithLatestSentTaskCount) {
    std::unique_ptr<MemoryManager> memoryManager(nullptr);
    std::unique_ptr<MockAubCsr<FamilyType>> aubCsr(new MockAubCsr<FamilyType>(*platformDevices[0], "", false));
    memoryManager.reset(aubCsr->createMemoryManager(false));

    auto commandBuffer = memoryManager->allocateGraphicsMemory(4096);
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    auto engineType = OCLRT::ENGINE_RCS;
    ResidencyContainer allocationsForResidency = {};

    aubCsr->setTagAllocation(pDevice->disconnectCurrentTagAllocationAndReturnIt());
    ASSERT_NE(nullptr, aubCsr->getTagAllocation());
    EXPECT_EQ(initialHardwareTag, *aubCsr->getTagAddress());

    aubCsr->setLatestSentTaskCount(aubCsr->peekTaskCount() + 1);

    EXPECT_NE(aubCsr->peekLatestSentTaskCount(), *aubCsr->getTagAddress());

    aubCsr->flush(batchBuffer, engineType, &allocationsForResidency);

    EXPECT_NE(aubCsr->peekLatestSentTaskCount(), *aubCsr->getTagAddress());
    EXPECT_EQ(initialHardwareTag, *aubCsr->getTagAddress());

    memoryManager->freeGraphicsMemory(commandBuffer);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverInStandaloneModeWhenFlushIsCalledThenItShouldUpdateHwTagWithLatestSentTaskCount) {
    std::unique_ptr<MemoryManager> memoryManager(nullptr);
    std::unique_ptr<MockAubCsr<FamilyType>> aubCsr(new MockAubCsr<FamilyType>(*platformDevices[0], "", true));
    memoryManager.reset(aubCsr->createMemoryManager(false));

    auto commandBuffer = memoryManager->allocateGraphicsMemory(4096);
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    auto engineType = OCLRT::ENGINE_RCS;
    ResidencyContainer allocationsForResidency = {};

    aubCsr->setTagAllocation(pDevice->disconnectCurrentTagAllocationAndReturnIt());
    ASSERT_NE(nullptr, aubCsr->getTagAllocation());
    EXPECT_EQ(initialHardwareTag, *aubCsr->getTagAddress());

    aubCsr->setLatestSentTaskCount(aubCsr->peekTaskCount() + 1);

    EXPECT_NE(aubCsr->peekLatestSentTaskCount(), *aubCsr->getTagAddress());

    aubCsr->flush(batchBuffer, engineType, &allocationsForResidency);

    EXPECT_EQ(aubCsr->peekLatestSentTaskCount(), *aubCsr->getTagAddress());

    memoryManager->freeGraphicsMemory(commandBuffer);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverInStandaloneAndSubCaptureModeWhenFlushIsCalledButSubCaptureIsDisabledThenItShouldUpdateHwTagWithLatestSentTaskCount) {
    DebugManagerStateRestore stateRestore;
    std::unique_ptr<MemoryManager> memoryManager(nullptr);
    std::unique_ptr<MockAubCsr<FamilyType>> aubCsr(new MockAubCsr<FamilyType>(*platformDevices[0], "", true));
    memoryManager.reset(aubCsr->createMemoryManager(false));

    auto aubSubCaptureManagerMock = new AubSubCaptureManagerMock("");
    aubSubCaptureManagerMock->subCaptureMode = AubSubCaptureManager::SubCaptureMode::Toggle;
    aubSubCaptureManagerMock->disableSubCapture();
    aubCsr->subCaptureManager.reset(aubSubCaptureManagerMock);
    ASSERT_FALSE(aubCsr->subCaptureManager->isSubCaptureEnabled());

    auto commandBuffer = memoryManager->allocateGraphicsMemory(4096);
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    auto engineType = OCLRT::ENGINE_RCS;
    ResidencyContainer allocationsForResidency = {};

    aubCsr->setTagAllocation(pDevice->disconnectCurrentTagAllocationAndReturnIt());
    ASSERT_NE(nullptr, aubCsr->getTagAllocation());
    EXPECT_EQ(initialHardwareTag, *aubCsr->getTagAddress());

    aubCsr->setLatestSentTaskCount(aubCsr->peekTaskCount() + 1);

    EXPECT_NE(aubCsr->peekLatestSentTaskCount(), *aubCsr->getTagAddress());

    aubCsr->flush(batchBuffer, engineType, &allocationsForResidency);

    EXPECT_EQ(aubCsr->peekLatestSentTaskCount(), *aubCsr->getTagAddress());

    memoryManager->freeGraphicsMemory(commandBuffer);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverInNonStandaloneAndSubCaptureModeWhenFlushIsCalledButSubCaptureIsDisabledThenItShouldNotUpdateHwTagWithLatestSentTaskCount) {
    DebugManagerStateRestore stateRestore;
    std::unique_ptr<MemoryManager> memoryManager(nullptr);
    std::unique_ptr<MockAubCsr<FamilyType>> aubCsr(new MockAubCsr<FamilyType>(*platformDevices[0], "", false));
    memoryManager.reset(aubCsr->createMemoryManager(false));

    auto aubSubCaptureManagerMock = new AubSubCaptureManagerMock("");
    aubSubCaptureManagerMock->subCaptureMode = AubSubCaptureManager::SubCaptureMode::Toggle;
    aubSubCaptureManagerMock->disableSubCapture();
    aubCsr->subCaptureManager.reset(aubSubCaptureManagerMock);
    ASSERT_FALSE(aubCsr->subCaptureManager->isSubCaptureEnabled());

    auto commandBuffer = memoryManager->allocateGraphicsMemory(4096);
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    auto engineType = OCLRT::ENGINE_RCS;
    ResidencyContainer allocationsForResidency = {};

    aubCsr->setTagAllocation(pDevice->disconnectCurrentTagAllocationAndReturnIt());
    ASSERT_NE(nullptr, aubCsr->getTagAllocation());
    EXPECT_EQ(initialHardwareTag, *aubCsr->getTagAddress());

    aubCsr->setLatestSentTaskCount(aubCsr->peekTaskCount() + 1);

    EXPECT_NE(aubCsr->peekLatestSentTaskCount(), *aubCsr->getTagAddress());

    aubCsr->flush(batchBuffer, engineType, &allocationsForResidency);

    EXPECT_NE(aubCsr->peekLatestSentTaskCount(), *aubCsr->getTagAddress());
    EXPECT_EQ(initialHardwareTag, *aubCsr->getTagAddress());

    memoryManager->freeGraphicsMemory(commandBuffer);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverInSubCaptureModeWhenFlushIsCalledAndSubCaptureIsEnabledThenItShouldDeactivateSubCapture) {
    DebugManagerStateRestore stateRestore;
    std::unique_ptr<MemoryManager> memoryManager(nullptr);
    std::unique_ptr<MockAubCsr<FamilyType>> aubCsr(new MockAubCsr<FamilyType>(*platformDevices[0], "", false));
    memoryManager.reset(aubCsr->createMemoryManager(false));

    const DispatchInfo dispatchInfo;
    auto aubSubCaptureManagerMock = new AubSubCaptureManagerMock("");
    aubSubCaptureManagerMock->subCaptureMode = AubSubCaptureManager::SubCaptureMode::Toggle;
    aubSubCaptureManagerMock->setSubCaptureToggleActive(true);
    aubSubCaptureManagerMock->activateSubCapture(dispatchInfo);
    aubCsr->subCaptureManager.reset(aubSubCaptureManagerMock);
    ASSERT_TRUE(aubCsr->subCaptureManager->isSubCaptureEnabled());

    auto commandBuffer = memoryManager->allocateGraphicsMemory(4096);
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    auto engineType = OCLRT::ENGINE_RCS;
    ResidencyContainer allocationsForResidency = {};

    aubCsr->flush(batchBuffer, engineType, &allocationsForResidency);

    EXPECT_FALSE(aubCsr->subCaptureManager->isSubCaptureEnabled());

    memoryManager->freeGraphicsMemory(commandBuffer);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverInStandaloneModeWhenFlushIsCalledThenItShouldCallMakeResidentOnCommandBufferAllocation) {
    std::unique_ptr<MemoryManager> memoryManager(nullptr);
    std::unique_ptr<MockAubCsr<FamilyType>> aubCsr(new MockAubCsr<FamilyType>(*platformDevices[0], "", true));
    memoryManager.reset(aubCsr->createMemoryManager(false));

    aubCsr->setTagAllocation(pDevice->disconnectCurrentTagAllocationAndReturnIt());
    ASSERT_NE(nullptr, aubCsr->getTagAllocation());

    GraphicsAllocation *commandBuffer = memoryManager->allocateGraphicsMemory(4096);
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    auto engineType = OCLRT::ENGINE_RCS;

    EXPECT_EQ(ObjectNotResident, commandBuffer->residencyTaskCount);

    aubCsr->overrideDispatchPolicy(DispatchMode::ImmediateDispatch);
    aubCsr->flush(batchBuffer, engineType, nullptr);

    EXPECT_NE(ObjectNotResident, commandBuffer->residencyTaskCount);
    EXPECT_EQ((int)aubCsr->peekTaskCount() + 1, commandBuffer->residencyTaskCount);

    aubCsr->makeSurfacePackNonResident(nullptr, false);

    EXPECT_EQ(ObjectNotResident, commandBuffer->residencyTaskCount);

    memoryManager->freeGraphicsMemory(commandBuffer);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverInNoneStandaloneModeWhenFlushIsCalledThenItShouldNotCallMakeResidentOnCommandBufferAllocation) {
    std::unique_ptr<MemoryManager> memoryManager(nullptr);
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>(*platformDevices[0], "", false));
    memoryManager.reset(aubCsr->createMemoryManager(false));

    GraphicsAllocation *commandBuffer = memoryManager->allocateGraphicsMemory(4096);
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    auto engineType = OCLRT::ENGINE_RCS;

    EXPECT_EQ(ObjectNotResident, commandBuffer->residencyTaskCount);

    aubCsr->flush(batchBuffer, engineType, nullptr);

    EXPECT_EQ(ObjectNotResident, commandBuffer->residencyTaskCount);

    memoryManager->freeGraphicsMemory(commandBuffer);
    aubCsr->setMemoryManager(nullptr);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverInStandaloneModeWhenFlushIsCalledThenItShouldCallMakeResidentOnResidencyAllocations) {
    std::unique_ptr<MemoryManager> memoryManager(nullptr);
    std::unique_ptr<MockAubCsr<FamilyType>> aubCsr(new MockAubCsr<FamilyType>(*platformDevices[0], "", true));
    memoryManager.reset(aubCsr->createMemoryManager(false));

    aubCsr->setTagAllocation(pDevice->disconnectCurrentTagAllocationAndReturnIt());
    ASSERT_NE(nullptr, aubCsr->getTagAllocation());

    auto gfxAllocation = memoryManager->allocateGraphicsMemory(sizeof(uint32_t), sizeof(uint32_t), false, false);
    ASSERT_NE(nullptr, gfxAllocation);

    GraphicsAllocation *commandBuffer = memoryManager->allocateGraphicsMemory(4096);
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    auto engineType = OCLRT::ENGINE_RCS;
    ResidencyContainer allocationsForResidency = {gfxAllocation};

    EXPECT_EQ(ObjectNotResident, gfxAllocation->residencyTaskCount);
    EXPECT_EQ(ObjectNotResident, commandBuffer->residencyTaskCount);

    aubCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);
    aubCsr->flush(batchBuffer, engineType, &allocationsForResidency);

    EXPECT_NE(ObjectNotResident, gfxAllocation->residencyTaskCount);
    EXPECT_EQ((int)aubCsr->peekTaskCount() + 1, gfxAllocation->residencyTaskCount);

    EXPECT_NE(ObjectNotResident, commandBuffer->residencyTaskCount);
    EXPECT_EQ((int)aubCsr->peekTaskCount() + 1, commandBuffer->residencyTaskCount);

    aubCsr->makeSurfacePackNonResident(&allocationsForResidency, false);

    EXPECT_EQ(ObjectNotResident, gfxAllocation->residencyTaskCount);
    EXPECT_EQ(ObjectNotResident, commandBuffer->residencyTaskCount);

    memoryManager->freeGraphicsMemory(commandBuffer);
    memoryManager->freeGraphicsMemory(gfxAllocation);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverInNoneStandaloneModeWhenFlushIsCalledThenItShouldNotCallMakeResidentOnResidencyAllocations) {
    std::unique_ptr<MemoryManager> memoryManager(nullptr);
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>(*platformDevices[0], "", false));
    memoryManager.reset(aubCsr->createMemoryManager(false));
    auto gfxAllocation = memoryManager->allocateGraphicsMemory(sizeof(uint32_t), sizeof(uint32_t), false, false);

    GraphicsAllocation *commandBuffer = memoryManager->allocateGraphicsMemory(4096);
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    auto engineType = OCLRT::ENGINE_RCS;
    ResidencyContainer allocationsForResidency = {gfxAllocation};

    EXPECT_EQ(ObjectNotResident, gfxAllocation->residencyTaskCount);
    EXPECT_EQ(ObjectNotResident, commandBuffer->residencyTaskCount);

    aubCsr->flush(batchBuffer, engineType, &allocationsForResidency);

    EXPECT_EQ(ObjectNotResident, gfxAllocation->residencyTaskCount);
    EXPECT_EQ(ObjectNotResident, commandBuffer->residencyTaskCount);

    memoryManager->freeGraphicsMemoryImpl(commandBuffer);
    memoryManager->freeGraphicsMemoryImpl(gfxAllocation);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverInStandaloneAndSubCaptureModeWhenFlushIsCalledAndSubCaptureIsEnabledThenItShouldCallMakeResidentOnCommandBufferAndResidencyAllocations) {
    DebugManagerStateRestore stateRestore;
    std::unique_ptr<MemoryManager> memoryManager(nullptr);
    std::unique_ptr<MockAubCsr<FamilyType>> aubCsr(new MockAubCsr<FamilyType>(*platformDevices[0], "", true));
    memoryManager.reset(aubCsr->createMemoryManager(false));

    const DispatchInfo dispatchInfo;
    auto aubSubCaptureManagerMock = new AubSubCaptureManagerMock("");
    aubSubCaptureManagerMock->subCaptureMode = AubSubCaptureManager::SubCaptureMode::Toggle;
    aubSubCaptureManagerMock->setSubCaptureToggleActive(true);
    aubSubCaptureManagerMock->activateSubCapture(dispatchInfo);
    aubCsr->subCaptureManager.reset(aubSubCaptureManagerMock);
    ASSERT_TRUE(aubCsr->subCaptureManager->isSubCaptureEnabled());

    aubCsr->setTagAllocation(pDevice->disconnectCurrentTagAllocationAndReturnIt());
    ASSERT_NE(nullptr, aubCsr->getTagAllocation());

    auto gfxAllocation = memoryManager->allocateGraphicsMemory(sizeof(uint32_t), sizeof(uint32_t), false, false);
    ASSERT_NE(nullptr, gfxAllocation);

    GraphicsAllocation *commandBuffer = memoryManager->allocateGraphicsMemory(4096);
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    auto engineType = OCLRT::ENGINE_RCS;
    ResidencyContainer allocationsForResidency = {gfxAllocation};

    EXPECT_EQ(ObjectNotResident, gfxAllocation->residencyTaskCount);
    EXPECT_EQ(ObjectNotResident, commandBuffer->residencyTaskCount);

    aubCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);
    aubCsr->flush(batchBuffer, engineType, &allocationsForResidency);

    EXPECT_NE(ObjectNotResident, gfxAllocation->residencyTaskCount);
    EXPECT_EQ((int)aubCsr->peekTaskCount() + 1, gfxAllocation->residencyTaskCount);

    EXPECT_NE(ObjectNotResident, commandBuffer->residencyTaskCount);
    EXPECT_EQ((int)aubCsr->peekTaskCount() + 1, commandBuffer->residencyTaskCount);

    aubCsr->makeSurfacePackNonResident(&allocationsForResidency, false);

    EXPECT_EQ(ObjectNotResident, gfxAllocation->residencyTaskCount);
    EXPECT_EQ(ObjectNotResident, commandBuffer->residencyTaskCount);

    memoryManager->freeGraphicsMemory(commandBuffer);
    memoryManager->freeGraphicsMemory(gfxAllocation);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenGraphicsAllocationIsCreatedThenItDoesntHaveTypeNonAubWritable) {
    std::unique_ptr<MemoryManager> memoryManager(nullptr);
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>(*platformDevices[0], "", true));
    memoryManager.reset(aubCsr->createMemoryManager(false));

    auto gfxAllocation = memoryManager->allocateGraphicsMemory(sizeof(uint32_t), sizeof(uint32_t), false, false);

    EXPECT_TRUE(gfxAllocation->isAubWritable());

    memoryManager->freeGraphicsMemory(gfxAllocation);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenProcessResidencyIsCalledOnDefaultAllocationThenAllocationTypeShouldNotBeMadeNonAubWritable) {
    std::unique_ptr<MemoryManager> memoryManager(nullptr);
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>(*platformDevices[0], "", true));
    memoryManager.reset(aubCsr->createMemoryManager(false));

    auto gfxDefaultAllocation = memoryManager->allocateGraphicsMemory(sizeof(uint32_t), sizeof(uint32_t), false, false);

    ResidencyContainer allocationsForResidency = {gfxDefaultAllocation};
    aubCsr->processResidency(&allocationsForResidency);

    EXPECT_TRUE(gfxDefaultAllocation->isAubWritable());

    memoryManager->freeGraphicsMemory(gfxDefaultAllocation);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenProcessResidencyIsCalledOnBufferAndImageAllocationsThenAllocationsTypesShouldBeMadeNonAubWritable) {
    std::unique_ptr<MemoryManager> memoryManager(nullptr);
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>(*platformDevices[0], "", true));
    memoryManager.reset(aubCsr->createMemoryManager(false));

    auto gfxBufferAllocation = memoryManager->allocateGraphicsMemory(sizeof(uint32_t), sizeof(uint32_t), false, false);
    gfxBufferAllocation->setAllocationType(GraphicsAllocation::AllocationType::BUFFER);

    auto gfxImageAllocation = memoryManager->allocateGraphicsMemory(sizeof(uint32_t), sizeof(uint32_t), false, false);
    gfxImageAllocation->setAllocationType(GraphicsAllocation::AllocationType::IMAGE);

    ResidencyContainer allocationsForResidency = {gfxBufferAllocation, gfxImageAllocation};
    aubCsr->processResidency(&allocationsForResidency);

    EXPECT_FALSE(gfxBufferAllocation->isAubWritable());
    EXPECT_FALSE(gfxImageAllocation->isAubWritable());

    memoryManager->freeGraphicsMemory(gfxBufferAllocation);
    memoryManager->freeGraphicsMemory(gfxImageAllocation);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverInSubCaptureModWhenProcessResidencyIsCalledWithDumpAubNonWritableFlagThenAllocationsTypesShouldBeMadeAubWritable) {
    DebugManagerStateRestore stateRestore;
    std::unique_ptr<MemoryManager> memoryManager(nullptr);
    std::unique_ptr<MockAubCsrToTestDumpAubNonWritable<FamilyType>> aubCsr(new MockAubCsrToTestDumpAubNonWritable<FamilyType>(*platformDevices[0], "", true));
    memoryManager.reset(aubCsr->createMemoryManager(false));

    auto gfxBufferAllocation = memoryManager->allocateGraphicsMemory(sizeof(uint32_t), sizeof(uint32_t), false, false);
    gfxBufferAllocation->setAllocationType(GraphicsAllocation::AllocationType::BUFFER);
    gfxBufferAllocation->setAubWritable(false);

    auto gfxImageAllocation = memoryManager->allocateGraphicsMemory(sizeof(uint32_t), sizeof(uint32_t), false, false);
    gfxImageAllocation->setAllocationType(GraphicsAllocation::AllocationType::IMAGE);
    gfxImageAllocation->setAubWritable(false);

    aubCsr->dumpAubNonWritable = true;

    ResidencyContainer allocationsForResidency = {gfxBufferAllocation, gfxImageAllocation};
    aubCsr->processResidency(&allocationsForResidency);

    EXPECT_TRUE(gfxBufferAllocation->isAubWritable());
    EXPECT_TRUE(gfxImageAllocation->isAubWritable());

    memoryManager->freeGraphicsMemory(gfxBufferAllocation);
    memoryManager->freeGraphicsMemory(gfxImageAllocation);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenProcessResidencyIsCalledWithoutDumpAubWritableFlagThenAllocationsTypesShouldBeKeptNonAubWritable) {
    DebugManagerStateRestore stateRestore;
    std::unique_ptr<MemoryManager> memoryManager(nullptr);
    std::unique_ptr<MockAubCsrToTestDumpAubNonWritable<FamilyType>> aubCsr(new MockAubCsrToTestDumpAubNonWritable<FamilyType>(*platformDevices[0], "", true));
    memoryManager.reset(aubCsr->createMemoryManager(false));

    auto gfxBufferAllocation = memoryManager->allocateGraphicsMemory(sizeof(uint32_t), sizeof(uint32_t), false, false);
    gfxBufferAllocation->setAllocationType(GraphicsAllocation::AllocationType::BUFFER);
    gfxBufferAllocation->setAubWritable(false);

    auto gfxImageAllocation = memoryManager->allocateGraphicsMemory(sizeof(uint32_t), sizeof(uint32_t), false, false);
    gfxImageAllocation->setAllocationType(GraphicsAllocation::AllocationType::IMAGE);
    gfxImageAllocation->setAubWritable(false);

    aubCsr->dumpAubNonWritable = false;

    ResidencyContainer allocationsForResidency = {gfxBufferAllocation, gfxImageAllocation};
    aubCsr->processResidency(&allocationsForResidency);

    EXPECT_FALSE(gfxBufferAllocation->isAubWritable());
    EXPECT_FALSE(gfxImageAllocation->isAubWritable());

    memoryManager->freeGraphicsMemory(gfxBufferAllocation);
    memoryManager->freeGraphicsMemory(gfxImageAllocation);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenGraphicsAllocationTypeIsntNonAubWritableThenWriteMemoryIsAllowed) {
    std::unique_ptr<MemoryManager> memoryManager(nullptr);
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>(*platformDevices[0], "", true));
    memoryManager.reset(aubCsr->createMemoryManager(false));

    auto gfxAllocation = memoryManager->allocateGraphicsMemory(sizeof(uint32_t), sizeof(uint32_t), false, false);

    EXPECT_TRUE(aubCsr->writeMemory(*gfxAllocation));

    memoryManager->freeGraphicsMemory(gfxAllocation);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenGraphicsAllocationTypeIsNonAubWritableThenWriteMemoryIsNotAllowed) {
    std::unique_ptr<MemoryManager> memoryManager(nullptr);
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>(*platformDevices[0], "", true));
    memoryManager.reset(aubCsr->createMemoryManager(false));

    auto gfxAllocation = memoryManager->allocateGraphicsMemory(sizeof(uint32_t), sizeof(uint32_t), false, false);

    gfxAllocation->setAubWritable(false);
    EXPECT_FALSE(aubCsr->writeMemory(*gfxAllocation));

    memoryManager->freeGraphicsMemory(gfxAllocation);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenGraphicsAllocationSizeIsZeroThenWriteMemoryIsNotAllowed) {
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>(*platformDevices[0], "", true));
    GraphicsAllocation gfxAllocation((void *)0x1234, 0);

    EXPECT_FALSE(aubCsr->writeMemory(gfxAllocation));
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverInSubCaptureModeWhenAubSubCaptureIsActivatedThenFileIsOpened) {
    DebugManagerStateRestore stateRestore;
    std::unique_ptr<MockAubCsr<FamilyType>> aubCsr(new MockAubCsr<FamilyType>(*platformDevices[0], "", false));

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
    std::unique_ptr<MockAubCsr<FamilyType>> aubCsr(new MockAubCsr<FamilyType>(*platformDevices[0], "", false));

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
    std::unique_ptr<MockAubCsr<FamilyType>> aubCsr(new MockAubCsr<FamilyType>(*platformDevices[0], "", false));
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
    std::unique_ptr<MockAubCsr<FamilyType>> aubCsr(new MockAubCsr<FamilyType>(*platformDevices[0], "", false));
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
    std::unique_ptr<MockAubCsrToTestDumpAubNonWritable<FamilyType>> aubCsr(new MockAubCsrToTestDumpAubNonWritable<FamilyType>(*platformDevices[0], "", false));

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
    std::unique_ptr<MockAubCsrToTestDumpAubNonWritable<FamilyType>> aubCsr(new MockAubCsrToTestDumpAubNonWritable<FamilyType>(*platformDevices[0], "", false));

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
    std::unique_ptr<MockAubCsr<FamilyType>> aubCsr(new MockAubCsr<FamilyType>(*platformDevices[0], "", false));

    auto subCaptureManagerMock = new AubSubCaptureManagerMock("");
    subCaptureManagerMock->subCaptureMode = AubSubCaptureManager::SubCaptureMode::Toggle;
    subCaptureManagerMock->setSubCaptureIsActive(false);
    subCaptureManagerMock->setSubCaptureToggleActive(false);
    aubCsr->subCaptureManager.reset(subCaptureManagerMock);

    const DispatchInfo dispatchInfo;
    aubCsr->activateAubSubCapture(dispatchInfo);

    EXPECT_FALSE(aubCsr->subCaptureManager->isSubCaptureEnabled());
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverInSubCaptureModeWhenSubCaptureIsToggledOnThenSubCaptureGetsEnabled) {
    DebugManagerStateRestore stateRestore;
    std::unique_ptr<MockAubCsr<FamilyType>> aubCsr(new MockAubCsr<FamilyType>(*platformDevices[0], "", false));

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
    std::unique_ptr<MockAubCsr<FamilyType>> aubCsr(new MockAubCsr<FamilyType>(*platformDevices[0], "", true));

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
    std::unique_ptr<MockAubCsr<FamilyType>> aubCsr(new MockAubCsr<FamilyType>(*platformDevices[0], "", true));

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
    std::unique_ptr<MockAubCsr<FamilyType>> aubCsr(new MockAubCsr<FamilyType>(*platformDevices[0], "", true));

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
    std::unique_ptr<MockAubCsr<FamilyType>> aubCsr(new MockAubCsr<FamilyType>(*platformDevices[0], "", true));

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
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>(*platformDevices[0], "", true));
    memoryManager.reset(aubCsr->createMemoryManager(false));
    auto flatBatchBufferHelper = new FlatBatchBufferHelperHw<FamilyType>(memoryManager.get());
    aubCsr->overwriteFlatBatchBufferHelper(flatBatchBufferHelper);

    auto chainedBatchBuffer = memoryManager->allocateGraphicsMemory(128u, 64u, false, false);
    auto otherAllocation = memoryManager->allocateGraphicsMemory(128u, 64u, false, false);
    ASSERT_NE(nullptr, chainedBatchBuffer);

    GraphicsAllocation *commandBuffer = memoryManager->allocateGraphicsMemory(4096);
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 128u, chainedBatchBuffer, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};

    size_t sizeBatchBuffer = 0xffffu;

    std::unique_ptr<void, std::function<void(void *)>> flatBatchBuffer(flatBatchBufferHelper->flattenBatchBuffer(batchBuffer, sizeBatchBuffer, DispatchMode::ImmediateDispatch), [&](void *ptr) { memoryManager->alignedFreeWrapper(ptr); });
    EXPECT_NE(nullptr, flatBatchBuffer.get());
    EXPECT_EQ(alignUp(128u + 128u, 0x1000), sizeBatchBuffer);

    memoryManager->freeGraphicsMemory(commandBuffer);
    memoryManager->freeGraphicsMemory(chainedBatchBuffer);
    memoryManager->freeGraphicsMemory(otherAllocation);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenForcedBatchBufferInImmediateDispatchModeAndNoChainedBatchBufferThenCombinedBatchBufferIsNotCreated) {
    std::unique_ptr<MemoryManager> memoryManager(nullptr);
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>(*platformDevices[0], "", true));
    memoryManager.reset(aubCsr->createMemoryManager(false));
    auto flatBatchBufferHelper = new FlatBatchBufferHelperHw<FamilyType>(memoryManager.get());
    aubCsr->overwriteFlatBatchBufferHelper(flatBatchBufferHelper);

    GraphicsAllocation *commandBuffer = memoryManager->allocateGraphicsMemory(4096);
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 128u, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};

    size_t sizeBatchBuffer = 0xffffu;

    std::unique_ptr<void, std::function<void(void *)>> flatBatchBuffer(flatBatchBufferHelper->flattenBatchBuffer(batchBuffer, sizeBatchBuffer, DispatchMode::ImmediateDispatch), [&](void *ptr) { memoryManager->alignedFreeWrapper(ptr); });
    EXPECT_EQ(nullptr, flatBatchBuffer.get());
    EXPECT_EQ(0xffffu, sizeBatchBuffer);

    memoryManager->freeGraphicsMemory(commandBuffer);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenForcedBatchBufferAndNotImmediateOrBatchedDispatchModeThenCombinedBatchBufferIsNotCreated) {
    std::unique_ptr<MemoryManager> memoryManager(nullptr);
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>(*platformDevices[0], "", true));
    memoryManager.reset(aubCsr->createMemoryManager(false));
    auto flatBatchBufferHelper = new FlatBatchBufferHelperHw<FamilyType>(memoryManager.get());
    aubCsr->overwriteFlatBatchBufferHelper(flatBatchBufferHelper);

    auto chainedBatchBuffer = memoryManager->allocateGraphicsMemory(128u, 64u, false, false);
    auto otherAllocation = memoryManager->allocateGraphicsMemory(128u, 64u, false, false);
    ASSERT_NE(nullptr, chainedBatchBuffer);

    GraphicsAllocation *commandBuffer = memoryManager->allocateGraphicsMemory(4096);
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 128u, chainedBatchBuffer, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};

    size_t sizeBatchBuffer = 0xffffu;

    std::unique_ptr<void, std::function<void(void *)>> flatBatchBuffer(flatBatchBufferHelper->flattenBatchBuffer(batchBuffer, sizeBatchBuffer, DispatchMode::AdaptiveDispatch), [&](void *ptr) { memoryManager->alignedFreeWrapper(ptr); });
    EXPECT_EQ(nullptr, flatBatchBuffer.get());
    EXPECT_EQ(0xffffu, sizeBatchBuffer);

    memoryManager->freeGraphicsMemory(commandBuffer);
    memoryManager->freeGraphicsMemory(chainedBatchBuffer);
    memoryManager->freeGraphicsMemory(otherAllocation);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenRegisterCommandChunkIsCalledThenNewChunkIsAddedToTheList) {
    typedef typename FamilyType::MI_BATCH_BUFFER_START MI_BATCH_BUFFER_START;

    auto aubExecutionEnvironment = getEnvironment<AUBCommandStreamReceiverHw<FamilyType>>(false, true);
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
    auto aubExecutionEnvironment = getEnvironment<AUBCommandStreamReceiverHw<FamilyType>>(false, true);
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

    auto aubExecutionEnvironment = getEnvironment<AUBCommandStreamReceiverHw<FamilyType>>(false, false);
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

    auto aubExecutionEnvironment = getEnvironment<AUBCommandStreamReceiverHw<FamilyType>>(false, true);
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

    std::unique_ptr<void, std::function<void(void *)>> flatBatchBuffer(aubCsr->getFlatBatchBufferHelper().flattenBatchBuffer(batchBuffer, sizeBatchBuffer, DispatchMode::BatchedDispatch), [&](void *ptr) { memoryManager->alignedFreeWrapper(ptr); });

    EXPECT_NE(nullptr, flatBatchBuffer.get());
    EXPECT_EQ(alignUp(0x50u + 0x40u + 0x40u + CSRequirements::csOverfetchSize, 0x1000u), sizeBatchBuffer);

    ASSERT_EQ(1u, aubCsr->getFlatBatchBufferHelper().getPatchInfoCollection().size());
    EXPECT_EQ(0xAAAu, aubCsr->getFlatBatchBufferHelper().getPatchInfoCollection()[0].sourceAllocation);

    EXPECT_EQ(0u, aubCsr->getFlatBatchBufferHelper().getCommandChunkList().size());

    EXPECT_EQ(0x3, static_cast<char *>(flatBatchBuffer.get())[0]);
    EXPECT_EQ(0x2, static_cast<char *>(flatBatchBuffer.get())[0x40]);
    EXPECT_EQ(0x1, static_cast<char *>(flatBatchBuffer.get())[0x40 + 0x40]);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenDefaultDebugConfigThenExpectFlattenBatchBufferIsNotCalled) {
    auto aubExecutionEnvironment = getEnvironment<MockAubCsr<FamilyType>>(true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<MockAubCsr<FamilyType>>();
    LinearStream cs(aubExecutionEnvironment->commandBuffer);

    auto mockHelper = new MockFlatBatchBufferHelper<FamilyType>(aubCsr->getMemoryManager());
    aubCsr->overwriteFlatBatchBufferHelper(mockHelper);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    auto engineType = OCLRT::ENGINE_RCS;
    ResidencyContainer allocationsForResidency = {};

    EXPECT_CALL(*mockHelper, flattenBatchBuffer(::testing::_, ::testing::_, ::testing::_)).Times(0);
    aubCsr->flush(batchBuffer, engineType, &allocationsForResidency);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenForcedFlattenBatchBufferAndImmediateDispatchModeThenExpectFlattenBatchBufferIsCalled) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.FlattenBatchBufferForAUBDump.set(true);
    DebugManager.flags.CsrDispatchMode.set(static_cast<uint32_t>(DispatchMode::ImmediateDispatch));

    auto aubExecutionEnvironment = getEnvironment<MockAubCsr<FamilyType>>(true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<MockAubCsr<FamilyType>>();
    LinearStream cs(aubExecutionEnvironment->commandBuffer);

    auto mockHelper = new MockFlatBatchBufferHelper<FamilyType>(aubCsr->getMemoryManager());
    aubCsr->overwriteFlatBatchBufferHelper(mockHelper);

    auto chainedBatchBuffer = aubExecutionEnvironment->executionEnvironment->memoryManager->allocateGraphicsMemory(128u, 64u, false, false);
    ASSERT_NE(nullptr, chainedBatchBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 128u, chainedBatchBuffer, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    auto engineType = OCLRT::ENGINE_RCS;

    aubCsr->makeResident(*chainedBatchBuffer);

    std::unique_ptr<void, decltype(alignedFree) *> ptr(alignedMalloc(4096, 4096), alignedFree);

    EXPECT_CALL(*mockHelper, flattenBatchBuffer(::testing::_, ::testing::_, ::testing::_)).WillOnce(::testing::Return(ptr.release()));
    aubCsr->flush(batchBuffer, engineType, nullptr);

    aubExecutionEnvironment->executionEnvironment->memoryManager->freeGraphicsMemory(chainedBatchBuffer);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenForcedFlattenBatchBufferAndImmediateDispatchModeAndThereIsNoChainedBatchBufferThenExpectFlattenBatchBufferIsCalledAnyway) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.FlattenBatchBufferForAUBDump.set(true);
    DebugManager.flags.CsrDispatchMode.set(static_cast<uint32_t>(DispatchMode::ImmediateDispatch));

    auto aubExecutionEnvironment = getEnvironment<MockAubCsr<FamilyType>>(true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<MockAubCsr<FamilyType>>();
    LinearStream cs(aubExecutionEnvironment->commandBuffer);

    auto mockHelper = new MockFlatBatchBufferHelper<FamilyType>(aubCsr->getMemoryManager());
    aubCsr->overwriteFlatBatchBufferHelper(mockHelper);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 128u, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    auto engineType = OCLRT::ENGINE_RCS;

    EXPECT_CALL(*mockHelper, flattenBatchBuffer(::testing::_, ::testing::_, ::testing::_)).Times(1);
    aubCsr->flush(batchBuffer, engineType, nullptr);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenForcedFlattenBatchBufferAndBatchedDispatchModeThenExpectFlattenBatchBufferIsCalledAnyway) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.FlattenBatchBufferForAUBDump.set(true);
    DebugManager.flags.CsrDispatchMode.set(static_cast<uint32_t>(DispatchMode::BatchedDispatch));

    auto aubExecutionEnvironment = getEnvironment<MockAubCsr<FamilyType>>(true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<MockAubCsr<FamilyType>>();
    LinearStream cs(aubExecutionEnvironment->commandBuffer);

    auto mockHelper = new MockFlatBatchBufferHelper<FamilyType>(aubCsr->getMemoryManager());
    aubCsr->overwriteFlatBatchBufferHelper(mockHelper);
    ResidencyContainer allocationsForResidency;

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 128u, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    auto engineType = OCLRT::ENGINE_RCS;

    EXPECT_CALL(*mockHelper, flattenBatchBuffer(::testing::_, ::testing::_, ::testing::_)).Times(1);
    aubCsr->flush(batchBuffer, engineType, &allocationsForResidency);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenAddPatchInfoCommentsForAUBDumpIsSetThenAddPatchInfoCommentsIsCalled) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.AddPatchInfoCommentsForAUBDump.set(true);

    auto aubExecutionEnvironment = getEnvironment<MockAubCsr<FamilyType>>(true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<MockAubCsr<FamilyType>>();
    LinearStream cs(aubExecutionEnvironment->commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    auto engineType = OCLRT::ENGINE_RCS;
    ResidencyContainer allocationsForResidency;

    EXPECT_CALL(*aubCsr, addPatchInfoComments()).Times(1);
    aubCsr->flush(batchBuffer, engineType, &allocationsForResidency);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenAddPatchInfoCommentsForAUBDumpIsNotSetThenAddPatchInfoCommentsIsNotCalled) {
    auto aubExecutionEnvironment = getEnvironment<MockAubCsr<FamilyType>>(false, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<MockAubCsr<FamilyType>>();
    LinearStream cs(aubExecutionEnvironment->commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    auto engineType = OCLRT::ENGINE_RCS;
    ResidencyContainer allocationsForResidency;
    aubCsr->setTagAllocation(pDevice->disconnectCurrentTagAllocationAndReturnIt());

    EXPECT_CALL(*aubCsr, addPatchInfoComments()).Times(0);
    aubCsr->flush(batchBuffer, engineType, &allocationsForResidency);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAddPatchInfoCommentsCalledWhenNoPatchInfoDataObjectsThenCommentsAreEmpty) {
    auto aubExecutionEnvironment = getEnvironment<AUBCommandStreamReceiverHw<FamilyType>>(false, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<AUBCommandStreamReceiverHw<FamilyType>>();
    LinearStream cs(aubExecutionEnvironment->commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 128u, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};

    std::unique_ptr<AUBCommandStreamReceiver::AubFileStream> mockAubFileStream(new GmockAubFileStream());
    GmockAubFileStream *mockAubFileStreamPtr = static_cast<GmockAubFileStream *>(mockAubFileStream.get());
    ASSERT_NE(nullptr, mockAubFileStreamPtr);
    mockAubFileStream.swap(aubCsr->stream);

    std::vector<std::string> comments;

    EXPECT_CALL(*mockAubFileStreamPtr, addComment(_)).Times(2).WillRepeatedly(::testing::Invoke([&](const char *str) -> bool {
        comments.push_back(std::string(str));
        return true;
    }));
    bool result = aubCsr->addPatchInfoComments();
    EXPECT_TRUE(result);

    ASSERT_EQ(2u, comments.size());

    EXPECT_EQ("PatchInfoData\n", comments[0]);
    EXPECT_EQ("AllocationsList\n", comments[1]);

    mockAubFileStream.swap(aubCsr->stream);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenFlushIsCalledThenFileStreamShouldBeFlushed) {
    auto aubExecutionEnvironment = getEnvironment<AUBCommandStreamReceiverHw<FamilyType>>(true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<AUBCommandStreamReceiverHw<FamilyType>>();
    LinearStream cs(aubExecutionEnvironment->commandBuffer);

    std::unique_ptr<AUBCommandStreamReceiver::AubFileStream> mockAubFileStream(new MockAubFileStream());
    MockAubFileStream *mockAubFileStreamPtr = static_cast<MockAubFileStream *>(mockAubFileStream.get());
    ASSERT_NE(nullptr, mockAubFileStreamPtr);
    mockAubFileStream.swap(aubCsr->stream);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    auto engineType = OCLRT::ENGINE_RCS;
    ResidencyContainer allocationsForResidency = {};

    aubCsr->flush(batchBuffer, engineType, &allocationsForResidency);
    EXPECT_TRUE(mockAubFileStreamPtr->flushCalled);

    mockAubFileStream.swap(aubCsr->stream);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAddPatchInfoCommentsCalledWhenFirstAddCommentsFailsThenFunctionReturnsFalse) {
    auto aubExecutionEnvironment = getEnvironment<AUBCommandStreamReceiverHw<FamilyType>>(false, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<AUBCommandStreamReceiverHw<FamilyType>>();
    LinearStream cs(aubExecutionEnvironment->commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 128u, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};

    std::unique_ptr<AUBCommandStreamReceiver::AubFileStream> mockAubFileStream(new GmockAubFileStream());
    GmockAubFileStream *mockAubFileStreamPtr = static_cast<GmockAubFileStream *>(mockAubFileStream.get());
    ASSERT_NE(nullptr, mockAubFileStreamPtr);
    mockAubFileStream.swap(aubCsr->stream);

    EXPECT_CALL(*mockAubFileStreamPtr, addComment(_)).Times(1).WillOnce(Return(false));
    bool result = aubCsr->addPatchInfoComments();
    EXPECT_FALSE(result);

    mockAubFileStream.swap(aubCsr->stream);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAddPatchInfoCommentsCalledWhenSecondAddCommentsFailsThenFunctionReturnsFalse) {
    auto aubExecutionEnvironment = getEnvironment<AUBCommandStreamReceiverHw<FamilyType>>(false, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<AUBCommandStreamReceiverHw<FamilyType>>();
    LinearStream cs(aubExecutionEnvironment->commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 128u, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};

    std::unique_ptr<AUBCommandStreamReceiver::AubFileStream> mockAubFileStream(new GmockAubFileStream());
    GmockAubFileStream *mockAubFileStreamPtr = static_cast<GmockAubFileStream *>(mockAubFileStream.get());
    ASSERT_NE(nullptr, mockAubFileStreamPtr);
    mockAubFileStream.swap(aubCsr->stream);

    EXPECT_CALL(*mockAubFileStreamPtr, addComment(_)).Times(2).WillOnce(Return(true)).WillOnce(Return(false));
    bool result = aubCsr->addPatchInfoComments();
    EXPECT_FALSE(result);

    mockAubFileStream.swap(aubCsr->stream);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAddPatchInfoCommentsCalledWhenPatchInfoDataObjectsAddedThenCommentsAreNotEmpty) {
    auto aubExecutionEnvironment = getEnvironment<AUBCommandStreamReceiverHw<FamilyType>>(false, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<AUBCommandStreamReceiverHw<FamilyType>>();
    LinearStream cs(aubExecutionEnvironment->commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 128u, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};

    std::unique_ptr<AUBCommandStreamReceiver::AubFileStream> mockAubFileStream(new GmockAubFileStream());
    GmockAubFileStream *mockAubFileStreamPtr = static_cast<GmockAubFileStream *>(mockAubFileStream.get());
    ASSERT_NE(nullptr, mockAubFileStreamPtr);
    mockAubFileStream.swap(aubCsr->stream);

    PatchInfoData patchInfoData[2] = {{0xAAAAAAAA, 128u, PatchInfoAllocationType::Default, 0xBBBBBBBB, 256u, PatchInfoAllocationType::Default},
                                      {0xBBBBBBBB, 128u, PatchInfoAllocationType::Default, 0xDDDDDDDD, 256u, PatchInfoAllocationType::Default}};

    EXPECT_TRUE(aubCsr->getFlatBatchBufferHelper().setPatchInfoData(patchInfoData[0]));
    EXPECT_TRUE(aubCsr->getFlatBatchBufferHelper().setPatchInfoData(patchInfoData[1]));

    std::vector<std::string> comments;

    EXPECT_CALL(*mockAubFileStreamPtr, addComment(_)).Times(2).WillRepeatedly(::testing::Invoke([&](const char *str) -> bool {
        comments.push_back(std::string(str));
        return true;
    }));
    bool result = aubCsr->addPatchInfoComments();
    EXPECT_TRUE(result);

    ASSERT_EQ(2u, comments.size());

    EXPECT_EQ("PatchInfoData", comments[0].substr(0, 13));
    EXPECT_EQ("AllocationsList", comments[1].substr(0, 15));

    std::string line;
    std::istringstream input1;
    input1.str(comments[0]);

    uint32_t lineNo = 0;
    while (std::getline(input1, line)) {
        if (line.substr(0, 13) == "PatchInfoData") {
            continue;
        }
        std::ostringstream ss;
        ss << std::hex << patchInfoData[lineNo].sourceAllocation << ";" << patchInfoData[lineNo].sourceAllocationOffset << ";" << patchInfoData[lineNo].sourceType << ";";
        ss << patchInfoData[lineNo].targetAllocation << ";" << patchInfoData[lineNo].targetAllocationOffset << ";" << patchInfoData[lineNo].targetType << ";";

        EXPECT_EQ(ss.str(), line);
        lineNo++;
    }

    std::vector<std::string> expectedAddresses = {"aaaaaaaa", "bbbbbbbb", "cccccccc", "dddddddd"};
    lineNo = 0;

    std::istringstream input2;
    input2.str(comments[1]);
    while (std::getline(input2, line)) {
        if (line.substr(0, 15) == "AllocationsList") {
            continue;
        }

        bool foundAddr = false;
        for (auto &addr : expectedAddresses) {
            if (line.substr(0, 8) == addr) {
                foundAddr = true;
                break;
            }
        }
        EXPECT_TRUE(foundAddr);
        EXPECT_TRUE(line.size() > 9);
        lineNo++;
    }

    mockAubFileStream.swap(aubCsr->stream);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAddPatchInfoCommentsCalledWhenSourceAllocationIsNullThenDoNotAddToAllocationsList) {
    auto aubExecutionEnvironment = getEnvironment<AUBCommandStreamReceiverHw<FamilyType>>(false, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<AUBCommandStreamReceiverHw<FamilyType>>();
    LinearStream cs(aubExecutionEnvironment->commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 128u, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};

    std::unique_ptr<AUBCommandStreamReceiver::AubFileStream> mockAubFileStream(new GmockAubFileStream());
    GmockAubFileStream *mockAubFileStreamPtr = static_cast<GmockAubFileStream *>(mockAubFileStream.get());
    ASSERT_NE(nullptr, mockAubFileStreamPtr);
    mockAubFileStream.swap(aubCsr->stream);

    PatchInfoData patchInfoData = {0x0, 0u, PatchInfoAllocationType::Default, 0xBBBBBBBB, 0u, PatchInfoAllocationType::Default};
    EXPECT_TRUE(aubCsr->getFlatBatchBufferHelper().setPatchInfoData(patchInfoData));

    std::vector<std::string> comments;

    EXPECT_CALL(*mockAubFileStreamPtr, addComment(_)).Times(2).WillRepeatedly(::testing::Invoke([&](const char *str) -> bool {
        comments.push_back(std::string(str));
        return true;
    }));
    bool result = aubCsr->addPatchInfoComments();
    EXPECT_TRUE(result);

    ASSERT_EQ(2u, comments.size());

    ASSERT_EQ("PatchInfoData", comments[0].substr(0, 13));
    ASSERT_EQ("AllocationsList", comments[1].substr(0, 15));

    std::string line;
    std::istringstream input;
    input.str(comments[1]);

    uint32_t lineNo = 0;

    std::vector<std::string> expectedAddresses = {"bbbbbbbb"};
    while (std::getline(input, line)) {
        if (line.substr(0, 15) == "AllocationsList") {
            continue;
        }

        bool foundAddr = false;
        for (auto &addr : expectedAddresses) {
            if (line.substr(0, 8) == addr) {
                foundAddr = true;
                break;
            }
        }
        EXPECT_TRUE(foundAddr);
        EXPECT_TRUE(line.size() > 9);
        lineNo++;
    }

    mockAubFileStream.swap(aubCsr->stream);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAddPatchInfoCommentsCalledWhenTargetAllocationIsNullThenDoNotAddToAllocationsList) {
    auto aubExecutionEnvironment = getEnvironment<AUBCommandStreamReceiverHw<FamilyType>>(false, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<AUBCommandStreamReceiverHw<FamilyType>>();
    LinearStream cs(aubExecutionEnvironment->commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 128u, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};

    std::unique_ptr<AUBCommandStreamReceiver::AubFileStream> mockAubFileStream(new GmockAubFileStream());
    GmockAubFileStream *mockAubFileStreamPtr = static_cast<GmockAubFileStream *>(mockAubFileStream.get());
    ASSERT_NE(nullptr, mockAubFileStreamPtr);
    mockAubFileStream.swap(aubCsr->stream);

    PatchInfoData patchInfoData = {0xAAAAAAAA, 0u, PatchInfoAllocationType::Default, 0x0, 0u, PatchInfoAllocationType::Default};
    EXPECT_TRUE(aubCsr->getFlatBatchBufferHelper().setPatchInfoData(patchInfoData));

    std::vector<std::string> comments;

    EXPECT_CALL(*mockAubFileStreamPtr, addComment(_)).Times(2).WillRepeatedly(::testing::Invoke([&](const char *str) -> bool {
        comments.push_back(std::string(str));
        return true;
    }));
    bool result = aubCsr->addPatchInfoComments();
    EXPECT_TRUE(result);

    ASSERT_EQ(2u, comments.size());

    ASSERT_EQ("PatchInfoData", comments[0].substr(0, 13));
    ASSERT_EQ("AllocationsList", comments[1].substr(0, 15));

    std::string line;
    std::istringstream input;
    input.str(comments[1]);

    uint32_t lineNo = 0;

    std::vector<std::string> expectedAddresses = {"aaaaaaaa"};
    while (std::getline(input, line)) {
        if (line.substr(0, 15) == "AllocationsList") {
            continue;
        }

        bool foundAddr = false;
        for (auto &addr : expectedAddresses) {
            if (line.substr(0, 8) == addr) {
                foundAddr = true;
                break;
            }
        }
        EXPECT_TRUE(foundAddr);
        EXPECT_TRUE(line.size() > 9);
        lineNo++;
    }

    mockAubFileStream.swap(aubCsr->stream);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenGetIndirectPatchCommandsIsCalledForEmptyPatchInfoListThenIndirectPatchCommandBufferIsNotCreated) {
    auto aubExecutionEnvironment = getEnvironment<AUBCommandStreamReceiverHw<FamilyType>>(false, false);
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
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>(*platformDevices[0], "", true));
    memoryManager.reset(aubCsr->createMemoryManager(false));

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
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>(*platformDevices[0], "", true));
    memoryManager.reset(aubCsr->createMemoryManager(false));

    MI_BATCH_BUFFER_START bbStart;

    aubCsr->addBatchBufferStart(&bbStart, 0xA000u, false);
    std::map<uint64_t, uint64_t> &batchBufferStartAddressSequence = aubCsr->getFlatBatchBufferHelper().getBatchBufferStartAddressSequence();

    ASSERT_EQ(1u, batchBufferStartAddressSequence.size());
    std::pair<uint64_t, uint64_t> addr = *batchBufferStartAddressSequence.begin();
    EXPECT_EQ(reinterpret_cast<uint64_t>(&bbStart), addr.first);
    EXPECT_EQ(0xA000u, addr.second);
}

HWTEST_F(AubCommandStreamReceiverTests, givenFlatBatchBufferHelperWhenSettingSroreQwordOnSDICommandThenAppropriateBitIsSet) {
    typedef typename FamilyType::MI_STORE_DATA_IMM MI_STORE_DATA_IMM;

    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>(*platformDevices[0], "", true));

    MI_STORE_DATA_IMM cmd;
    cmd.init();
    FlatBatchBufferHelperHw<FamilyType>::sdiSetStoreQword(&cmd, false);
    EXPECT_EQ(0u, static_cast<uint32_t>(cmd.getStoreQword()));
    FlatBatchBufferHelperHw<FamilyType>::sdiSetStoreQword(&cmd, true);
    EXPECT_EQ(1u, static_cast<uint32_t>(cmd.getStoreQword()));
}

class OsAgnosticMemoryManagerForImagesWithNoHostPtr : public OsAgnosticMemoryManager {
  public:
    GraphicsAllocation *allocateGraphicsMemoryForImage(ImageInfo &imgInfo, Gmm *gmm) override {
        auto imageAllocation = OsAgnosticMemoryManager::allocateGraphicsMemoryForImage(imgInfo, gmm);
        cpuPtr = imageAllocation->getUnderlyingBuffer();
        imageAllocation->setCpuPtrAndGpuAddress(nullptr, imageAllocation->getGpuAddress());
        return imageAllocation;
    };
    void freeGraphicsMemoryImpl(GraphicsAllocation *imageAllocation) override {
        imageAllocation->setCpuPtrAndGpuAddress(lockResourceParam.retCpuPtr, imageAllocation->getGpuAddress());
        OsAgnosticMemoryManager::freeGraphicsMemoryImpl(imageAllocation);
    };
    void *lockResource(GraphicsAllocation *imageAllocation) override {
        lockResourceParam.wasCalled = true;
        lockResourceParam.inImageAllocation = imageAllocation;
        lockResourceParam.retCpuPtr = cpuPtr;
        return lockResourceParam.retCpuPtr;
    };
    void unlockResource(GraphicsAllocation *imageAllocation) override {
        unlockResourceParam.wasCalled = true;
        unlockResourceParam.inImageAllocation = imageAllocation;
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
};

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenWriteMemoryIsCalledOnImageWithNoHostPtrThenResourceShouldBeLockedToGetCpuAddress) {
    std::unique_ptr<OsAgnosticMemoryManagerForImagesWithNoHostPtr> memoryManager(nullptr);
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>(*platformDevices[0], "", true));
    memoryManager.reset(new OsAgnosticMemoryManagerForImagesWithNoHostPtr);
    aubCsr->setMemoryManager(memoryManager.get());

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
    std::unique_ptr<MockAubCsr<FamilyType>> aubCsr(new MockAubCsr<FamilyType>(hwInfoIn, "", true));
    EXPECT_EQ(hwInfoIn.capabilityTable.aubDeviceId, aubCsr->aubDeviceId);
}

HWTEST_F(AubCommandStreamReceiverTests, givenDbgDeviceIdFlagIsSetWhenAubCsrIsCreatedThenUseDebugDeviceId) {
    DebugManagerStateRestore stateRestore;
    DebugManager.flags.OverrideAubDeviceId.set(9); //this is Hsw, not used
    const HardwareInfo &hwInfoIn = *platformDevices[0];
    std::unique_ptr<MockAubCsr<FamilyType>> aubCsr(new MockAubCsr<FamilyType>(hwInfoIn, "", true));
    EXPECT_EQ(9u, aubCsr->aubDeviceId);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenGetGTTDataIsCalledThenLocalMemoryShouldBeDisabled) {
    const HardwareInfo &hwInfoIn = *platformDevices[0];
    std::unique_ptr<MockAubCsr<FamilyType>> aubCsr(new MockAubCsr<FamilyType>(hwInfoIn, "", true));
    AubGTTData data = {};
    aubCsr->getGTTData(nullptr, data);
    EXPECT_TRUE(data.present);
    EXPECT_FALSE(data.localMemory);
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

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
