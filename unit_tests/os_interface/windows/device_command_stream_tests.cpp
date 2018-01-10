/*
 * Copyright (c) 2017, Intel Corporation
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

#include "gtest/gtest.h"
#include "gmock/gmock.h"
#pragma warning(push)
#pragma warning(disable : 4005)
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/command_stream/aub_command_stream_receiver.h"
#include "runtime/command_stream/device_command_stream.h"
#include "runtime/command_stream/linear_stream.h"
#include "runtime/helpers/options.h"
#include "runtime/gen_common/hw_cmds.h"
#include "runtime/memory_manager/memory_manager.h"
#include "runtime/mem_obj/buffer.h"
#include "runtime/os_interface/windows/wddm_device_command_stream.h"
#include "runtime/os_interface/windows/wddm_memory_manager.h"
#include "runtime/helpers/translationtable_callbacks.h"
#pragma warning(pop)

#include "test.h"
#include "unit_tests/fixtures/memory_management_fixture.h"
#include "unit_tests/mocks/mock_buffer.h"
#include "unit_tests/mocks/mock_device.h"
#include "unit_tests/mocks/mock_graphics_allocation.h"
#include "unit_tests/mocks/mock_submissions_aggregator.h"
#include "unit_tests/mocks/mock_gmm_page_table_mngr.h"
#include "unit_tests/os_interface/windows/mock_wddm_memory_manager.h"
#include "unit_tests/os_interface/windows/wddm_fixture.h"
#include "unit_tests/os_interface/windows/mock_gdi_interface.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"

using namespace OCLRT;
using namespace ::testing;

class WddmCommandStreamFixture : public WddmFixtureMock {
  public:
    DeviceCommandStreamReceiver<DEFAULT_TEST_FAMILY_NAME> *csr = nullptr;
    MemoryManager *mm = nullptr;
    MockDevice *device = nullptr;
    MockWddmMemoryManager *mockWddmMM = nullptr;
    DebugManagerStateRestore stateRestore;

    void SetUp() {
        WddmFixtureMock::SetUp();
        ASSERT_NE(wddm, nullptr);

        DebugManager.flags.CsrDispatchMode.set(CommandStreamReceiver::DispatchMode::ImmediateDispatch);

        csr = new WddmCommandStreamReceiver<DEFAULT_TEST_FAMILY_NAME>(*platformDevices[0], wddm);
        ASSERT_NE(nullptr, csr);

        mockWddmMM = new MockWddmMemoryManager(wddm);
        mm = mockWddmMM;
        csr->setMemoryManager(mm);

        device = MockDevice::create<MockDevice>(platformDevices[0]);
        ASSERT_NE(nullptr, device);
        mm->device = device;

        ASSERT_NE(nullptr, mm);
    }

    void TearDown() {
        mockWddmMM = nullptr;
        delete csr->getTagAddress();
        delete csr;
        delete mm;
        delete device;
        WddmFixtureMock::TearDown();
    }
};

class WddmCommandStreamWithMockGdiFixture : public WddmFixture {
  public:
    DeviceCommandStreamReceiver<DEFAULT_TEST_FAMILY_NAME> *csr = nullptr;
    MemoryManager *mm = nullptr;
    MockDevice *device = nullptr;
    MockGdi gdi;
    DebugManagerStateRestore stateRestore;
    GraphicsAllocation *tagAllocation;

    void SetUp() {
        WddmFixture::SetUp(&gdi);
        ASSERT_NE(wddm, nullptr);
        DebugManager.flags.CsrDispatchMode.set(CommandStreamReceiver::DispatchMode::ImmediateDispatch);
        csr = new WddmCommandStreamReceiver<DEFAULT_TEST_FAMILY_NAME>(*platformDevices[0], wddm);
        ASSERT_NE(nullptr, csr);

        mm = csr->createMemoryManager(false);
        ASSERT_NE(nullptr, mm);

        device = MockDevice::create<MockDevice>(platformDevices[0]);
        ASSERT_NE(nullptr, device);
        mm->device = device;

        tagAllocation = mm->allocateGraphicsMemory(1024, 4096);
    }

    void TearDown() {
        mm->freeGraphicsMemory(tagAllocation);
        delete csr->getTagAddress();
        delete csr;
        delete mm;
        wddm = nullptr;
        delete device;
        WddmFixture::TearDown();
    }
};

typedef ::Test<WddmCommandStreamFixture> WddmCommandStreamTest;
typedef ::Test<WddmCommandStreamWithMockGdiFixture> WddmCommandStreamMockGdiTest;
typedef ::Test<WddmCommandStreamFixture> WddmDefaultTest;

TEST(DeviceCommandStreamTest, Create) {
    auto ptr = DeviceCommandStreamReceiver<DEFAULT_TEST_FAMILY_NAME>::create(DEFAULT_TEST_PLATFORM::hwInfo);
    EXPECT_NE(nullptr, ptr);
    auto wddm = static_cast<WddmCommandStreamReceiver<DEFAULT_TEST_FAMILY_NAME> *>(ptr)->peekWddm();
    EXPECT_NE(nullptr, wddm);
    delete ptr;
    //wddm still valid
    delete wddm;
}

TEST_F(WddmCommandStreamTest, givenFlushStampWhenWaitCalledThenWaitForSpecifiedMonitoredFence) {
    uint64_t stampToWait = 123;
    csr->waitForFlushStamp(stampToWait);
    EXPECT_EQ(1u, wddm->waitFromCpuResult.called);
    EXPECT_TRUE(wddm->waitFromCpuResult.success);
    EXPECT_EQ(stampToWait, wddm->waitFromCpuResult.uint64ParamPassed);
}

TEST_F(WddmCommandStreamTest, Flush) {
    GraphicsAllocation *commandBuffer = mm->allocateGraphicsMemory(4096, 4096);
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, false, false, cs.getUsed(), &cs};
    auto flushStamp = csr->flush(batchBuffer, EngineType::ENGINE_RCS, nullptr);

    EXPECT_EQ(1u, wddm->submitResult.called);
    EXPECT_TRUE(wddm->submitResult.success);
    EXPECT_EQ(flushStamp, wddm->getMonitoredFence().lastSubmittedFence);

    mm->freeGraphicsMemory(commandBuffer);
}

TEST_F(WddmCommandStreamTest, FlushWithOffset) {
    auto offset = 128u;
    GraphicsAllocation *commandBuffer = mm->allocateGraphicsMemory(4096, 4096);
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), offset, false, false, cs.getUsed(), &cs};
    csr->flush(batchBuffer, EngineType::ENGINE_RCS, nullptr);
    EXPECT_EQ(1u, wddm->submitResult.called);
    EXPECT_TRUE(wddm->submitResult.success);
    EXPECT_EQ(wddm->submitResult.commandBufferSubmitted, (char *)commandBuffer->getUnderlyingBuffer() + offset);

    mm->freeGraphicsMemory(commandBuffer);
}

TEST_F(WddmCommandStreamTest, givenWdmmWhenSubmitIsCalledThenCoherencyRequiredFlagIsSetToFalse) {
    GraphicsAllocation *commandBuffer = mm->allocateGraphicsMemory(4096, 4096);
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, false, false, cs.getUsed(), &cs};
    csr->flush(batchBuffer, EngineType::ENGINE_RCS, nullptr);
    auto commandHeader = wddm->submitResult.commandHeaderSubmitted;

    COMMAND_BUFFER_HEADER *pHeader = reinterpret_cast<COMMAND_BUFFER_HEADER *>(commandHeader);

    EXPECT_FALSE(pHeader->RequiresCoherency);

    mm->freeGraphicsMemory(commandBuffer);
}

TEST_F(WddmCommandStreamTest, givenWdmmWhenSubmitIsCalledAndPreemptionIsDisabledThenSetHeaderFieldToFalse) {
    device->setPreemptionMode(PreemptionMode::Disabled);

    GraphicsAllocation *commandBuffer = mm->allocateGraphicsMemory(4096, 4096);
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, false, false, cs.getUsed(), &cs};
    csr->flush(batchBuffer, EngineType::ENGINE_RCS, nullptr);
    auto commandHeader = wddm->submitResult.commandHeaderSubmitted;

    COMMAND_BUFFER_HEADER *pHeader = reinterpret_cast<COMMAND_BUFFER_HEADER *>(commandHeader);

    EXPECT_FALSE(pHeader->NeedsMidBatchPreEmptionSupport);

    mm->freeGraphicsMemory(commandBuffer);
}

TEST_F(WddmCommandStreamTest, givenWdmmWhenSubmitIsCalledAndPreemptionIsEnabledThenSetHeaderFieldToTrue) {
    device->setPreemptionMode(PreemptionMode::ThreadGroup);

    GraphicsAllocation *commandBuffer = mm->allocateGraphicsMemory(4096, 4096);
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, false, false, cs.getUsed(), &cs};
    csr->flush(batchBuffer, EngineType::ENGINE_RCS, nullptr);
    auto commandHeader = wddm->submitResult.commandHeaderSubmitted;

    COMMAND_BUFFER_HEADER *pHeader = reinterpret_cast<COMMAND_BUFFER_HEADER *>(commandHeader);

    EXPECT_TRUE(pHeader->NeedsMidBatchPreEmptionSupport);

    mm->freeGraphicsMemory(commandBuffer);
}

TEST_F(WddmCommandStreamTest, makeResident) {
    WddmMemoryManager *wddmMM = reinterpret_cast<WddmMemoryManager *>(mm);

    GraphicsAllocation *commandBuffer = mm->allocateGraphicsMemory(4096, 4096);
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);

    csr->makeResident(*commandBuffer);

    EXPECT_EQ(0u, wddm->makeResidentResult.called);
    EXPECT_EQ(1u, mm->getResidencyAllocations().size());
    EXPECT_EQ(commandBuffer, mm->getResidencyAllocations()[0]);

    mm->freeGraphicsMemory(commandBuffer);
}

TEST_F(WddmCommandStreamTest, makeNonResidentPutsAllocationInEvictionAllocations) {
    WddmMemoryManager *wddmMM = reinterpret_cast<WddmMemoryManager *>(mm);

    GraphicsAllocation *commandBuffer = mm->allocateGraphicsMemory(4096, 4096);
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);

    csr->makeResident(*cs.getGraphicsAllocation());

    csr->makeNonResident(*commandBuffer);

    EXPECT_EQ(1u, mm->getEvictionAllocations().size());

    mm->freeGraphicsMemory(commandBuffer);
}

TEST_F(WddmCommandStreamTest, processEvictionPlacesAllAllocationsOnTrimCandidateList) {
    WddmMemoryManager *wddmMM = reinterpret_cast<WddmMemoryManager *>(mm);

    GraphicsAllocation *allocation = mm->allocateGraphicsMemory(4096, 4096);
    GraphicsAllocation *allocation2 = mm->allocateGraphicsMemory(4096, 4096);
    ASSERT_NE(nullptr, allocation);
    ASSERT_NE(nullptr, allocation2);

    mm->pushAllocationForEviction(allocation);
    mm->pushAllocationForEviction(allocation2);

    EXPECT_EQ(2u, mm->getEvictionAllocations().size());

    csr->processEviction();

    EXPECT_EQ(2u, mockWddmMM->trimCandidateList.size());

    mm->freeGraphicsMemory(allocation);
    mm->freeGraphicsMemory(allocation2);
}

TEST_F(WddmCommandStreamTest, processEvictionClearsEvictionAllocations) {
    WddmMemoryManager *wddmMM = reinterpret_cast<WddmMemoryManager *>(mm);

    GraphicsAllocation *allocation = mm->allocateGraphicsMemory(4096, 4096);
    ASSERT_NE(nullptr, allocation);

    mm->pushAllocationForEviction(allocation);

    EXPECT_EQ(1u, mm->getEvictionAllocations().size());

    csr->processEviction();

    EXPECT_EQ(0u, mm->getEvictionAllocations().size());

    mm->freeGraphicsMemory(allocation);
}

TEST_F(WddmCommandStreamTest, makeResidentNonResidentMemObj) {
    GraphicsAllocation *gfxAllocation = mm->allocateGraphicsMemory(256);
    Buffer *buffer = new AlignedBuffer(gfxAllocation);
    WddmMemoryManager *wddmMM = reinterpret_cast<WddmMemoryManager *>(mm);

    csr->makeResident(*buffer->getGraphicsAllocation());
    EXPECT_EQ(0u, wddm->makeResidentResult.called);
    EXPECT_EQ(1u, mm->getResidencyAllocations().size());
    EXPECT_EQ(gfxAllocation, mm->getResidencyAllocations()[0]);

    csr->makeNonResident(*buffer->getGraphicsAllocation());
    EXPECT_EQ(gfxAllocation, mm->getEvictionAllocations()[0]);

    delete buffer;
    mm->freeGraphicsMemory(gfxAllocation);
}

TEST_F(WddmCommandStreamTest, createAllocationAndMakeResident) {

    void *host_ptr = (void *)0x1234;
    auto size = 1234u;

    WddmAllocation *gfxAllocation = (WddmAllocation *)csr->createAllocationAndHandleResidency(host_ptr, size);

    ASSERT_NE(nullptr, gfxAllocation);

    EXPECT_EQ(1u, mm->getResidencyAllocations().size());

    EXPECT_EQ(host_ptr, gfxAllocation->getUnderlyingBuffer());
}

TEST_F(WddmCommandStreamTest, killAllTemporaryAllocation) {
    void *host_ptr = (void *)0x1212341;
    auto size = 17262u;

    GraphicsAllocation *graphicsAllocation = csr->createAllocationAndHandleResidency(host_ptr, size);
    ASSERT_NE(nullptr, graphicsAllocation);

    graphicsAllocation->taskCount = 1;
    csr->cleanAllocationList(-1, TEMPORARY_ALLOCATION);
    //no memory leaks reported makes this test pass.
}

TEST_F(WddmCommandStreamTest, killCompletedAllocations) {
    void *host_ptr = (void *)0x1212341;
    void *host_ptr2 = (void *)0x2212341;
    auto size = 17262u;

    GraphicsAllocation *graphicsAllocation = csr->createAllocationAndHandleResidency(host_ptr, size);
    ASSERT_NE(nullptr, graphicsAllocation);

    GraphicsAllocation *graphicsAllocation2 = csr->createAllocationAndHandleResidency(host_ptr2, size);

    graphicsAllocation->taskCount = 1;
    graphicsAllocation2->taskCount = 100;

    csr->cleanAllocationList(1, TEMPORARY_ALLOCATION);
    //graphicsAllocation2 still lives
    EXPECT_EQ(host_ptr2, graphicsAllocation2->getUnderlyingBuffer());

    auto *memoryManager = (WddmMemoryManager *)csr->getMemoryManager();
    auto &hostPtrManager = memoryManager->hostPtrManager;

    auto alignedPtr = alignDown(host_ptr, MemoryConstants::pageSize);
    auto alignedPtr2 = alignDown(host_ptr2, MemoryConstants::pageSize);

    auto fragment = hostPtrManager.getFragment(alignedPtr2);
    ASSERT_NE(nullptr, fragment);

    EXPECT_EQ(alignedPtr2, fragment->fragmentCpuPointer);

    auto fragment2 = hostPtrManager.getFragment(alignedPtr);
    EXPECT_EQ(nullptr, fragment2);
}

TEST_F(WddmCommandStreamMockGdiTest, FlushCallsWddmMakeResidentForResidencyAllocations) {
    GraphicsAllocation *commandBuffer = mm->allocateGraphicsMemory(4096, 4096);
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);

    csr->makeResident(*commandBuffer);

    EXPECT_EQ(1u, mm->getResidencyAllocations().size());

    gdi.getMakeResidentArg().NumAllocations = 0;

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, false, false, cs.getUsed(), &cs};
    csr->flush(batchBuffer, EngineType::ENGINE_RCS, nullptr);

    EXPECT_NE(0u, gdi.getMakeResidentArg().NumAllocations);

    mm->freeGraphicsMemory(commandBuffer);
}

TEST_F(WddmCommandStreamMockGdiTest, makeResidentClearsResidencyAllocations) {
    GraphicsAllocation *commandBuffer = mm->allocateGraphicsMemory(4096, 4096);
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);

    csr->makeResident(*commandBuffer);

    EXPECT_EQ(1u, mm->getResidencyAllocations().size());
    EXPECT_EQ(0u, mm->getEvictionAllocations().size());

    EXPECT_EQ(trimListUnusedPosition, ((WddmAllocation *)commandBuffer)->getTrimCandidateListPosition());

    csr->processResidency(nullptr);

    csr->makeSurfacePackNonResident(nullptr);

    EXPECT_EQ(0u, mm->getResidencyAllocations().size());
    EXPECT_EQ(0u, mm->getEvictionAllocations().size());

    EXPECT_EQ(0u, ((WddmAllocation *)commandBuffer)->getTrimCandidateListPosition());

    mm->freeGraphicsMemory(commandBuffer);
}

template <typename GfxFamily>
struct MockWddmCsr : public WddmCommandStreamReceiver<GfxFamily> {
    MockWddmCsr(const HardwareInfo &hwInfoIn, Wddm *wddm) : WddmCommandStreamReceiver(hwInfoIn, wddm){};
    using CommandStreamReceiver::commandStream;
    using CommandStreamReceiver::dispatchMode;
    using CommandStreamReceiver::getCS;

    void overrideDispatchPolicy(CommandStreamReceiver::DispatchMode overrideValue) {
        this->dispatchMode = overrideValue;
    }

    SubmissionAggregator *peekSubmissionAggregator() {
        return this->submissionAggregator.get();
    }

    void overrideSubmissionAggregator(SubmissionAggregator *newSubmissionsAggregator) {
        this->submissionAggregator.reset(newSubmissionsAggregator);
    }

    int flushCalledCount = 0;
    CommandBuffer recordedCommandBuffer;
};

HWTEST_F(WddmCommandStreamMockGdiTest, givenRecordedCommandBufferWhenItIsSubmittedThenFlushTaskIsProperlyCalled) {
    std::unique_ptr<MockWddmCsr<FamilyType>> mockCsr(new MockWddmCsr<FamilyType>(*platformDevices[0], this->wddm));
    mockCsr->setMemoryManager(mm);
    mockCsr->overrideDispatchPolicy(CommandStreamReceiver::DispatchMode::BatchedDispatch);

    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    auto commandBuffer = mm->allocateGraphicsMemory(1024, 4096);

    mockCsr->setTagAllocation(tagAllocation);

    LinearStream cs(commandBuffer);

    DispatchFlags dispatchFlags;
    dispatchFlags.requiresCoherency = true;
    mockCsr->flushTask(cs, 0u, cs, cs, cs, cs, 0u, dispatchFlags);

    auto &cmdBuffers = mockedSubmissionsAggregator->peekCommandBuffers();
    auto storedCommandBuffer = cmdBuffers.peekHead();

    ResidencyContainer copyOfResidency = storedCommandBuffer->surfaces;
    copyOfResidency.push_back(storedCommandBuffer->batchBuffer.commandBufferAllocation);

    mockCsr->flushBatchedSubmissions();

    EXPECT_TRUE(cmdBuffers.peekIsEmpty());

    EXPECT_EQ(1u, mockWddm->submitResult.called);
    auto csrCommandStream = mockCsr->commandStream.getGraphicsAllocation();
    EXPECT_EQ(csrCommandStream->getUnderlyingBuffer(), mockWddm->submitResult.commandBufferSubmitted);
    EXPECT_TRUE(((COMMAND_BUFFER_HEADER *)mockWddm->submitResult.commandHeaderSubmitted)->RequiresCoherency);
    EXPECT_EQ(3u, mockWddm->makeResidentResult.handleCount);

    std::vector<D3DKMT_HANDLE> expectedHandles;
    expectedHandles.push_back(((WddmAllocation *)tagAllocation)->handle);
    expectedHandles.push_back(((WddmAllocation *)commandBuffer)->handle);
    expectedHandles.push_back(((WddmAllocation *)csrCommandStream)->handle);

    for (auto i = 0u; i < mockWddm->makeResidentResult.handleCount; i++) {
        auto handle = mockWddm->makeResidentResult.handlePack[i];
        auto found = false;
        for (auto &expectedHandle : expectedHandles) {
            if (expectedHandle == handle) {
                found = true;
            }
        }
        EXPECT_TRUE(found);
    }

    EXPECT_NE(trimListUnusedPosition, ((WddmAllocation *)tagAllocation)->getTrimCandidateListPosition());
    EXPECT_NE(trimListUnusedPosition, ((WddmAllocation *)commandBuffer)->getTrimCandidateListPosition());
    EXPECT_NE(trimListUnusedPosition, ((WddmAllocation *)csrCommandStream)->getTrimCandidateListPosition());

    mm->freeGraphicsMemory(commandBuffer);
}

HWTEST_F(WddmDefaultTest, givenDefaultWddmCsrWhenItIsCreatedThenBatchingIsTurnedOn) {
    DebugManager.flags.CsrDispatchMode.set(0);
    std::unique_ptr<MockWddmCsr<FamilyType>> mockCsr(new MockWddmCsr<FamilyType>(*platformDevices[0], this->wddm));
    EXPECT_EQ(CommandStreamReceiver::DispatchMode::BatchedDispatch, mockCsr->dispatchMode);
}

struct WddmCsrCompressionTests : WddmCommandStreamMockGdiTest {
    void setCompressionEnabled(bool enabled) {
        RuntimeCapabilityTable capabilityTable = {platformDevices[0]->capabilityTable};
        capabilityTable.ftrCompression = enabled;
        hwInfo = {*platformDevices[0]};
        hwInfo.capabilityTable = capabilityTable;
    }

    void SetUp() override {
        WddmCommandStreamMockGdiTest::SetUp();
        setCompressionEnabled(true);
    }

    HardwareInfo hwInfo = {};
};

HWTEST_F(WddmCsrCompressionTests, givenEnabledCompressionWhenInitializedThenCreatePagetableMngr) {
    EXPECT_EQ(nullptr, mockWddm->getPageTableManager());
    MockWddmCsr<FamilyType> mockWddmCsr(hwInfo, mockWddm);
    ASSERT_NE(nullptr, mockWddm->getPageTableManager());

    auto mockMngr = reinterpret_cast<MockGmmPageTableMngr *>(mockWddm->getPageTableManager());

    GMM_DEVICE_CALLBACKS expectedDeviceCb = {};
    GMM_TRANSLATIONTABLE_CALLBACKS expectedTTCallbacks = {};
    unsigned int expectedFlags = (TT_TYPE::TRTT | TT_TYPE::AUXTT);
    // clang-format off
    expectedDeviceCb.Adapter         = mockWddm->getAdapter();
    expectedDeviceCb.hDevice         = mockWddm->getDevice();
    expectedDeviceCb.PagingQueue     = mockWddm->getPagingQueue();
    expectedDeviceCb.PagingFence     = mockWddm->getPagingQueueSyncObject();

    expectedDeviceCb.pfnAllocate     = gdi.createAllocation;
    expectedDeviceCb.pfnDeallocate   = gdi.destroyAllocation;
    expectedDeviceCb.pfnMapGPUVA     = gdi.mapGpuVirtualAddress;
    expectedDeviceCb.pfnMakeResident = gdi.makeResident;
    expectedDeviceCb.pfnEvict        = gdi.evict;
    expectedDeviceCb.pfnReserveGPUVA = gdi.reserveGpuVirtualAddress;
    expectedDeviceCb.pfnUpdateGPUVA  = gdi.updateGpuVirtualAddress;
    expectedDeviceCb.pfnWaitFromCpu  = gdi.waitForSynchronizationObjectFromCpu;
    expectedDeviceCb.pfnLock         = gdi.lock2;
    expectedDeviceCb.pfnUnLock       = gdi.unlock2;
    expectedDeviceCb.pfnEscape       = gdi.escape;

    expectedTTCallbacks.pfWriteL3Adr = TTCallbacks<FamilyType>::writeL3Address;
    // clang-format on

    EXPECT_TRUE(memcmp(&expectedDeviceCb, &mockMngr->deviceCb, sizeof(GMM_DEVICE_CALLBACKS)) == 0);
    EXPECT_TRUE(memcmp(&expectedTTCallbacks, &mockMngr->translationTableCb, sizeof(GMM_TRANSLATIONTABLE_CALLBACKS)) == 0);
    EXPECT_TRUE(memcmp(&expectedFlags, &mockMngr->translationTableFlags, sizeof(unsigned int)) == 0);
}

HWTEST_F(WddmCsrCompressionTests, givenDisabledCompressionWhenInitializedThenDontCreatePagetableMngr) {
    setCompressionEnabled(false);
    MockWddmCsr<FamilyType> mockWddmCsr(hwInfo, mockWddm);
    EXPECT_EQ(nullptr, mockWddm->getPageTableManager());
}

HWTEST_F(WddmCsrCompressionTests, givenEnabledCompressionWhenFlushingThenInitTranslationTableOnce) {
    MockWddmCsr<FamilyType> mockWddmCsr(hwInfo, mockWddm);
    mockWddmCsr.overrideDispatchPolicy(CommandStreamReceiver::DispatchMode::BatchedDispatch);

    auto mockMngr = reinterpret_cast<MockGmmPageTableMngr *>(mockWddm->getPageTableManager());
    mockWddmCsr.setMemoryManager(mm);
    mockWddmCsr.setTagAllocation(tagAllocation);

    auto &csrCS = mockWddmCsr.getCS();

    auto graphicsAllocation = mm->allocateGraphicsMemory(1024, 4096);
    LinearStream cs(graphicsAllocation);

    EXPECT_FALSE(mockWddm->peekIsPageTableManagerInitialized());

    EXPECT_CALL(*mockMngr, initContextAuxTableRegister(&csrCS, GMM_ENGINE_TYPE::ENGINE_TYPE_RCS)).Times(1).WillOnce(Return(GMM_SUCCESS));
    EXPECT_CALL(*mockMngr, initContextTRTableRegister(&csrCS, GMM_ENGINE_TYPE::ENGINE_TYPE_RCS)).Times(1).WillOnce(Return(GMM_SUCCESS));

    DispatchFlags dispatchFlags;
    mockWddmCsr.flushTask(cs, 0u, cs, cs, cs, cs, 0u, dispatchFlags);

    EXPECT_TRUE(mockWddm->peekIsPageTableManagerInitialized());

    // flush again to check if PT manager was initialized once
    mockWddmCsr.flushTask(cs, 0u, cs, cs, cs, cs, 0u, dispatchFlags);

    mm->freeGraphicsMemory(graphicsAllocation);
}

HWTEST_F(WddmCsrCompressionTests, givenDisabledCompressionWhenFlushingThenDontInitTranslationTable) {
    setCompressionEnabled(false);
    MockWddmCsr<FamilyType> mockWddmCsr(hwInfo, mockWddm);
    mockWddmCsr.overrideDispatchPolicy(CommandStreamReceiver::DispatchMode::BatchedDispatch);

    EXPECT_EQ(nullptr, mockWddm->getPageTableManager());

    mockWddmCsr.setMemoryManager(mm);
    mockWddmCsr.setTagAllocation(tagAllocation);

    auto graphicsAllocation = mm->allocateGraphicsMemory(1024, 4096);
    LinearStream cs(graphicsAllocation);

    EXPECT_FALSE(mockWddm->peekIsPageTableManagerInitialized());

    DispatchFlags dispatchFlags;
    mockWddmCsr.flushTask(cs, 0u, cs, cs, cs, cs, 0u, dispatchFlags);

    EXPECT_FALSE(mockWddm->peekIsPageTableManagerInitialized());

    mm->freeGraphicsMemory(graphicsAllocation);
}
