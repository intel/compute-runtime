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

#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/command_stream/command_stream_receiver_with_aub_dump.h"
#include "runtime/command_stream/aub_command_stream_receiver.h"
#include "runtime/command_stream/device_command_stream.h"
#include "runtime/command_stream/linear_stream.h"
#include "runtime/command_stream/preemption.h"
#include "runtime/helpers/built_ins_helper.h"
#include "runtime/gen_common/hw_cmds.h"
#include "runtime/helpers/options.h"
#include "runtime/helpers/translationtable_callbacks.h"
#include "runtime/memory_manager/memory_manager.h"
#include "runtime/mem_obj/buffer.h"
#include "runtime/os_interface/windows/wddm_device_command_stream.h"
#include "runtime/os_interface/windows/wddm_memory_manager.h"

#include "unit_tests/fixtures/memory_management_fixture.h"
#include "unit_tests/mocks/mock_builtins.h"
#include "unit_tests/mocks/mock_buffer.h"
#include "unit_tests/mocks/mock_device.h"
#include "unit_tests/mocks/mock_graphics_allocation.h"
#include "unit_tests/mocks/mock_submissions_aggregator.h"
#include "unit_tests/mocks/mock_gmm_page_table_mngr.h"
#include "unit_tests/os_interface/windows/mock_wddm_memory_manager.h"
#include "unit_tests/os_interface/windows/wddm_fixture.h"
#include "unit_tests/os_interface/windows/mock_gdi_interface.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"

#include "test.h"

using namespace OCLRT;
using namespace ::testing;

class WddmCommandStreamFixture {
  public:
    DeviceCommandStreamReceiver<DEFAULT_TEST_FAMILY_NAME> *csr = nullptr;
    MemoryManager *memManager = nullptr;
    MockDevice *device = nullptr;
    MockWddmMemoryManager *mockWddmMM = nullptr;
    WddmMock *wddm = nullptr;
    DebugManagerStateRestore stateRestore;

    virtual void SetUp() {
        wddm = static_cast<WddmMock *>(Wddm::createWddm(WddmInterfaceVersion::Wddm20));
        ASSERT_NE(wddm, nullptr);

        DebugManager.flags.CsrDispatchMode.set(static_cast<uint32_t>(DispatchMode::ImmediateDispatch));

        csr = new WddmCommandStreamReceiver<DEFAULT_TEST_FAMILY_NAME>(*platformDevices[0], wddm);
        ASSERT_NE(nullptr, csr);

        mockWddmMM = new MockWddmMemoryManager(wddm);
        memManager = mockWddmMM;
        csr->setMemoryManager(memManager);

        device = MockDevice::create<MockDevice>(platformDevices[0]);
        ASSERT_NE(nullptr, device);
        memManager->device = device;

        ASSERT_NE(nullptr, memManager);
    }

    virtual void TearDown() {
        mockWddmMM = nullptr;
        delete csr->getTagAddress();
        delete csr;
        delete memManager;
        delete device;
    }
};

class WddmCommandStreamWithMockGdiFixture {
  public:
    DeviceCommandStreamReceiver<DEFAULT_TEST_FAMILY_NAME> *csr = nullptr;
    MemoryManager *memManager = nullptr;
    MockDevice *device = nullptr;
    WddmMock *wddm = nullptr;
    MockGdi *gdi = nullptr;
    DebugManagerStateRestore stateRestore;
    GraphicsAllocation *tagAllocation;
    GraphicsAllocation *preemptionAllocation = nullptr;

    virtual void SetUp() {
        wddm = static_cast<WddmMock *>(Wddm::createWddm(WddmInterfaceVersion::Wddm20));
        gdi = new MockGdi();
        wddm->gdi.reset(gdi);
        ASSERT_NE(wddm, nullptr);
        DebugManager.flags.CsrDispatchMode.set(static_cast<uint32_t>(DispatchMode::ImmediateDispatch));
        csr = new WddmCommandStreamReceiver<DEFAULT_TEST_FAMILY_NAME>(*platformDevices[0], wddm);
        ASSERT_NE(nullptr, csr);

        memManager = csr->createMemoryManager(false);
        ASSERT_NE(nullptr, memManager);

        device = MockDevice::createWithMemoryManager<MockDevice>(platformDevices[0], memManager);
        ASSERT_NE(nullptr, device);
        memManager->device = device;
        tagAllocation = memManager->allocateGraphicsMemory(1024, 4096);
        if (device->getPreemptionMode() == PreemptionMode::MidThread) {
            preemptionAllocation = memManager->allocateGraphicsMemory(1024, 4096);
        }
        auto tagBuffer = (uint32_t *)tagAllocation->getUnderlyingBuffer();
        tagBuffer[0] = initialHardwareTag;
    }

    virtual void TearDown() {
        memManager->freeGraphicsMemory(tagAllocation);
        if (preemptionAllocation) {
            memManager->freeGraphicsMemory(preemptionAllocation);
        }
        delete csr->getTagAddress();
        delete csr;
        wddm = nullptr;
        delete device;
    }
};

typedef ::Test<WddmCommandStreamFixture> WddmCommandStreamTest;
typedef ::Test<WddmCommandStreamWithMockGdiFixture> WddmCommandStreamMockGdiTest;
typedef ::Test<WddmCommandStreamFixture> WddmDefaultTest;

TEST(DeviceCommandStreamTest, CreateWddmCSR) {
    std::unique_ptr<DeviceCommandStreamReceiver<DEFAULT_TEST_FAMILY_NAME>> csr(static_cast<DeviceCommandStreamReceiver<DEFAULT_TEST_FAMILY_NAME> *>(DeviceCommandStreamReceiver<DEFAULT_TEST_FAMILY_NAME>::create(DEFAULT_TEST_PLATFORM::hwInfo, false)));
    EXPECT_NE(nullptr, csr);
    std::unique_ptr<Wddm> wddm(static_cast<WddmCommandStreamReceiver<DEFAULT_TEST_FAMILY_NAME> *>(csr.get())->peekWddm());
    EXPECT_NE(nullptr, wddm);
}

TEST(DeviceCommandStreamTest, CreateWddmCSRWithAubDump) {
    std::unique_ptr<DeviceCommandStreamReceiver<DEFAULT_TEST_FAMILY_NAME>> csr(static_cast<DeviceCommandStreamReceiver<DEFAULT_TEST_FAMILY_NAME> *>(DeviceCommandStreamReceiver<DEFAULT_TEST_FAMILY_NAME>::create(DEFAULT_TEST_PLATFORM::hwInfo, true)));
    EXPECT_NE(nullptr, csr);
    std::unique_ptr<Wddm> wddm(static_cast<WddmCommandStreamReceiver<DEFAULT_TEST_FAMILY_NAME> *>(csr.get())->peekWddm());
    EXPECT_NE(nullptr, wddm);
    auto aubCSR = static_cast<CommandStreamReceiverWithAUBDump<WddmCommandStreamReceiver<DEFAULT_TEST_FAMILY_NAME>> *>(csr.get())->aubCSR;
    EXPECT_NE(nullptr, aubCSR);
}

TEST_F(WddmCommandStreamTest, givenFlushStampWhenWaitCalledThenWaitForSpecifiedMonitoredFence) {
    uint64_t stampToWait = 123;
    csr->waitForFlushStamp(stampToWait);
    EXPECT_EQ(1u, wddm->waitFromCpuResult.called);
    EXPECT_TRUE(wddm->waitFromCpuResult.success);
    EXPECT_EQ(stampToWait, wddm->waitFromCpuResult.uint64ParamPassed);
}

TEST_F(WddmCommandStreamTest, Flush) {
    GraphicsAllocation *commandBuffer = memManager->allocateGraphicsMemory(4096, 4096);
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    auto flushStamp = csr->flush(batchBuffer, EngineType::ENGINE_RCS, nullptr);

    EXPECT_EQ(1u, wddm->submitResult.called);
    EXPECT_TRUE(wddm->submitResult.success);
    EXPECT_EQ(flushStamp, wddm->getMonitoredFence().lastSubmittedFence);

    memManager->freeGraphicsMemory(commandBuffer);
}

TEST_F(WddmCommandStreamTest, givenGraphicsAllocationWithDifferentGpuAddressThenCpuAddressWhenSubmitIsCalledThenGpuAddressIsUsed) {
    GraphicsAllocation *commandBuffer = memManager->allocateGraphicsMemory(4096, 4096);

    auto cpuAddress = commandBuffer->getUnderlyingBuffer();
    uint64_t mockGpuAddres = 1337;
    commandBuffer->setCpuPtrAndGpuAddress(cpuAddress, mockGpuAddres);

    LinearStream cs(commandBuffer);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    auto flushStamp = csr->flush(batchBuffer, EngineType::ENGINE_RCS, nullptr);
    EXPECT_EQ(mockGpuAddres, wddm->submitResult.commandBufferSubmitted);
    memManager->freeGraphicsMemory(commandBuffer);
}
TEST_F(WddmCommandStreamTest, FlushWithOffset) {
    auto offset = 128u;
    GraphicsAllocation *commandBuffer = memManager->allocateGraphicsMemory(4096, 4096);
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), offset, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    csr->flush(batchBuffer, EngineType::ENGINE_RCS, nullptr);
    EXPECT_EQ(1u, wddm->submitResult.called);
    EXPECT_TRUE(wddm->submitResult.success);
    EXPECT_EQ(wddm->submitResult.commandBufferSubmitted, reinterpret_cast<uint64_t>(commandBuffer->getUnderlyingBuffer()) + offset);

    memManager->freeGraphicsMemory(commandBuffer);
}

TEST_F(WddmCommandStreamTest, givenWdmmWhenSubmitIsCalledThenCoherencyRequiredFlagIsSetToFalse) {
    GraphicsAllocation *commandBuffer = memManager->allocateGraphicsMemory(4096, 4096);
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    csr->flush(batchBuffer, EngineType::ENGINE_RCS, nullptr);
    auto commandHeader = wddm->submitResult.commandHeaderSubmitted;

    COMMAND_BUFFER_HEADER *pHeader = reinterpret_cast<COMMAND_BUFFER_HEADER *>(commandHeader);

    EXPECT_FALSE(pHeader->RequiresCoherency);

    memManager->freeGraphicsMemory(commandBuffer);
}

TEST_F(WddmCommandStreamTest, givenWdmmWhenSubmitIsCalledAndPreemptionIsDisabledThenSetHeaderFieldToFalse) {
    device->setPreemptionMode(PreemptionMode::Disabled);

    GraphicsAllocation *commandBuffer = memManager->allocateGraphicsMemory(4096, 4096);
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    csr->flush(batchBuffer, EngineType::ENGINE_RCS, nullptr);
    auto commandHeader = wddm->submitResult.commandHeaderSubmitted;

    COMMAND_BUFFER_HEADER *pHeader = reinterpret_cast<COMMAND_BUFFER_HEADER *>(commandHeader);

    EXPECT_FALSE(pHeader->NeedsMidBatchPreEmptionSupport);

    memManager->freeGraphicsMemory(commandBuffer);
}

TEST_F(WddmCommandStreamTest, givenWdmmWhenSubmitIsCalledAndPreemptionIsEnabledThenSetHeaderFieldToTrue) {
    device->setPreemptionMode(PreemptionMode::ThreadGroup);

    GraphicsAllocation *commandBuffer = memManager->allocateGraphicsMemory(4096, 4096);
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    csr->flush(batchBuffer, EngineType::ENGINE_RCS, nullptr);
    auto commandHeader = wddm->submitResult.commandHeaderSubmitted;

    COMMAND_BUFFER_HEADER *pHeader = reinterpret_cast<COMMAND_BUFFER_HEADER *>(commandHeader);

    EXPECT_TRUE(pHeader->NeedsMidBatchPreEmptionSupport);

    memManager->freeGraphicsMemory(commandBuffer);
}

TEST_F(WddmCommandStreamTest, givenWdmmWhenSubmitIsCalledAndThrottleIsToLowThenSetHeaderFieldsProperly) {
    GraphicsAllocation *commandBuffer = memManager->allocateGraphicsMemory(4096, 4096);
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::LOW, cs.getUsed(), &cs};
    csr->flush(batchBuffer, EngineType::ENGINE_RCS, nullptr);
    auto commandHeader = wddm->submitResult.commandHeaderSubmitted;

    COMMAND_BUFFER_HEADER *pHeader = reinterpret_cast<COMMAND_BUFFER_HEADER *>(commandHeader);

    EXPECT_EQ(0, pHeader->UmdRequestedSliceState);
    EXPECT_EQ(1, pHeader->UmdRequestedSubsliceCount);
    EXPECT_EQ(wddm->getGtSysInfo()->EUCount / wddm->getGtSysInfo()->SubSliceCount, pHeader->UmdRequestedEUCount);

    memManager->freeGraphicsMemory(commandBuffer);
}

TEST_F(WddmCommandStreamTest, givenWdmmWhenSubmitIsCalledAndThrottleIsToMediumThenSetHeaderFieldsProperly) {
    GraphicsAllocation *commandBuffer = memManager->allocateGraphicsMemory(4096, 4096);
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    csr->flush(batchBuffer, EngineType::ENGINE_RCS, nullptr);
    auto commandHeader = wddm->submitResult.commandHeaderSubmitted;

    COMMAND_BUFFER_HEADER *pHeader = reinterpret_cast<COMMAND_BUFFER_HEADER *>(commandHeader);

    EXPECT_EQ(0, pHeader->UmdRequestedSliceState);
    EXPECT_EQ(0, pHeader->UmdRequestedSubsliceCount);
    EXPECT_EQ(wddm->getGtSysInfo()->EUCount / wddm->getGtSysInfo()->SubSliceCount, pHeader->UmdRequestedEUCount);

    memManager->freeGraphicsMemory(commandBuffer);
}

TEST_F(WddmCommandStreamTest, givenWdmmWhenSubmitIsCalledAndThrottleIsToHighThenSetHeaderFieldsProperly) {
    GraphicsAllocation *commandBuffer = memManager->allocateGraphicsMemory(4096, 4096);
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::HIGH, cs.getUsed(), &cs};
    csr->flush(batchBuffer, EngineType::ENGINE_RCS, nullptr);
    auto commandHeader = wddm->submitResult.commandHeaderSubmitted;

    COMMAND_BUFFER_HEADER *pHeader = reinterpret_cast<COMMAND_BUFFER_HEADER *>(commandHeader);
    const uint32_t maxRequestedSubsliceCount = 7;
    EXPECT_EQ(0, pHeader->UmdRequestedSliceState);
    EXPECT_EQ((wddm->getGtSysInfo()->SubSliceCount <= maxRequestedSubsliceCount) ? wddm->getGtSysInfo()->SubSliceCount : 0, pHeader->UmdRequestedSubsliceCount);
    EXPECT_EQ(wddm->getGtSysInfo()->EUCount / wddm->getGtSysInfo()->SubSliceCount, pHeader->UmdRequestedEUCount);

    memManager->freeGraphicsMemory(commandBuffer);
}

TEST_F(WddmCommandStreamTest, givenWddmWithKmDafDisabledWhenFlushIsCalledWithAllocationsForResidencyThenNoneAllocationShouldBeKmDafLocked) {
    GraphicsAllocation *commandBuffer = memManager->allocateGraphicsMemory(4096, 4096);
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};

    auto linearStreamAllocation = memManager->allocateGraphicsMemory(sizeof(uint32_t), sizeof(uint32_t), false, false);
    ASSERT_NE(nullptr, linearStreamAllocation);
    linearStreamAllocation->setAllocationType(GraphicsAllocation::ALLOCATION_TYPE_LINEAR_STREAM);
    ResidencyContainer allocationsForResidency = {linearStreamAllocation};

    EXPECT_FALSE(wddm->isKmDafEnabled());
    auto flushStamp = csr->flush(batchBuffer, EngineType::ENGINE_RCS, &allocationsForResidency);

    EXPECT_EQ(0u, wddm->kmDafLockResult.called);
    EXPECT_EQ(0u, wddm->kmDafLockResult.lockedAllocations.size());

    memManager->freeGraphicsMemory(commandBuffer);
    memManager->freeGraphicsMemory(linearStreamAllocation);
}

TEST_F(WddmCommandStreamTest, givenWddmWithKmDafEnabledWhenFlushIsCalledWithoutAllocationsForResidencyThenNoneAllocationShouldBeKmDafLocked) {
    GraphicsAllocation *commandBuffer = memManager->allocateGraphicsMemory(4096, 4096);
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};

    wddm->setKmDafEnabled(true);
    auto flushStamp = csr->flush(batchBuffer, EngineType::ENGINE_RCS, nullptr);

    EXPECT_EQ(0u, wddm->kmDafLockResult.called);
    EXPECT_EQ(0u, wddm->kmDafLockResult.lockedAllocations.size());

    memManager->freeGraphicsMemory(commandBuffer);
}

TEST_F(WddmCommandStreamTest, givenWddmWithKmDafEnabledWhenFlushIsCalledWithResidencyAllocationsInMemoryManagerThenLinearStreamAllocationsShouldBeKmDafLocked) {
    GraphicsAllocation *commandBuffer = memManager->allocateGraphicsMemory(4096, 4096);
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};

    auto linearStreamAllocation = memManager->allocateGraphicsMemory(sizeof(uint32_t), sizeof(uint32_t), false, false);
    ASSERT_NE(nullptr, linearStreamAllocation);
    linearStreamAllocation->setAllocationType(GraphicsAllocation::ALLOCATION_TYPE_LINEAR_STREAM);

    csr->makeResident(*linearStreamAllocation);
    EXPECT_EQ(1u, memManager->getResidencyAllocations().size());
    EXPECT_EQ(linearStreamAllocation, memManager->getResidencyAllocations()[0]);

    wddm->setKmDafEnabled(true);
    auto flushStamp = csr->flush(batchBuffer, EngineType::ENGINE_RCS, nullptr);

    EXPECT_EQ(1u, wddm->kmDafLockResult.called);
    EXPECT_EQ(1u, wddm->kmDafLockResult.lockedAllocations.size());
    EXPECT_EQ(linearStreamAllocation, wddm->kmDafLockResult.lockedAllocations[0]);

    memManager->freeGraphicsMemory(commandBuffer);
    memManager->freeGraphicsMemory(linearStreamAllocation);
}

TEST_F(WddmCommandStreamTest, givenWddmWithKmDafEnabledWhenFlushIsCalledWithAllocationsForResidencyThenLinearStreamAllocationsShouldBeKmDafLocked) {
    GraphicsAllocation *commandBuffer = memManager->allocateGraphicsMemory(4096, 4096);
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};

    auto linearStreamAllocation = memManager->allocateGraphicsMemory(sizeof(uint32_t), sizeof(uint32_t), false, false);
    ASSERT_NE(nullptr, linearStreamAllocation);
    linearStreamAllocation->setAllocationType(GraphicsAllocation::ALLOCATION_TYPE_LINEAR_STREAM);
    ResidencyContainer allocationsForResidency = {linearStreamAllocation};

    wddm->setKmDafEnabled(true);
    auto flushStamp = csr->flush(batchBuffer, EngineType::ENGINE_RCS, &allocationsForResidency);

    EXPECT_EQ(1u, wddm->kmDafLockResult.called);
    EXPECT_EQ(1u, wddm->kmDafLockResult.lockedAllocations.size());
    EXPECT_EQ(linearStreamAllocation, wddm->kmDafLockResult.lockedAllocations[0]);

    memManager->freeGraphicsMemory(commandBuffer);
    memManager->freeGraphicsMemory(linearStreamAllocation);
}

TEST_F(WddmCommandStreamTest, givenWddmWithKmDafEnabledWhenFlushIsCalledWithAllocationsForResidencyThenFillPatternAllocationsShouldBeKmDafLocked) {
    GraphicsAllocation *commandBuffer = memManager->allocateGraphicsMemory(4096, 4096);
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};

    auto fillPatternAllocation = memManager->allocateGraphicsMemory(sizeof(uint32_t), sizeof(uint32_t), false, false);
    ASSERT_NE(nullptr, fillPatternAllocation);
    fillPatternAllocation->setAllocationType(GraphicsAllocation::ALLOCATION_TYPE_FILL_PATTERN);
    ResidencyContainer allocationsForResidency = {fillPatternAllocation};

    wddm->setKmDafEnabled(true);
    auto flushStamp = csr->flush(batchBuffer, EngineType::ENGINE_RCS, &allocationsForResidency);

    EXPECT_EQ(1u, wddm->kmDafLockResult.called);
    EXPECT_EQ(1u, wddm->kmDafLockResult.lockedAllocations.size());
    EXPECT_EQ(fillPatternAllocation, wddm->kmDafLockResult.lockedAllocations[0]);

    memManager->freeGraphicsMemory(commandBuffer);
    memManager->freeGraphicsMemory(fillPatternAllocation);
}

TEST_F(WddmCommandStreamTest, givenWddmWithKmDafEnabledWhenFlushIsCalledWithAllocationsForResidencyThenNonLinearStreamAllocationShouldNotBeKmDafLocked) {
    GraphicsAllocation *commandBuffer = memManager->allocateGraphicsMemory(4096, 4096);
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};

    auto nonLinearStreamAllocation = memManager->allocateGraphicsMemory(sizeof(uint32_t), sizeof(uint32_t), false, false);
    ASSERT_NE(nullptr, nonLinearStreamAllocation);
    ResidencyContainer allocationsForResidency = {nonLinearStreamAllocation};

    wddm->setKmDafEnabled(true);
    auto flushStamp = csr->flush(batchBuffer, EngineType::ENGINE_RCS, &allocationsForResidency);

    EXPECT_EQ(0u, wddm->kmDafLockResult.called);
    EXPECT_EQ(0u, wddm->kmDafLockResult.lockedAllocations.size());

    memManager->freeGraphicsMemory(commandBuffer);
    memManager->freeGraphicsMemory(nonLinearStreamAllocation);
}

TEST_F(WddmCommandStreamTest, makeResident) {
    WddmMemoryManager *wddmMM = reinterpret_cast<WddmMemoryManager *>(memManager);

    GraphicsAllocation *commandBuffer = memManager->allocateGraphicsMemory(4096, 4096);
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);

    csr->makeResident(*commandBuffer);

    EXPECT_EQ(0u, wddm->makeResidentResult.called);
    EXPECT_EQ(1u, memManager->getResidencyAllocations().size());
    EXPECT_EQ(commandBuffer, memManager->getResidencyAllocations()[0]);

    memManager->freeGraphicsMemory(commandBuffer);
}

TEST_F(WddmCommandStreamTest, makeNonResidentPutsAllocationInEvictionAllocations) {
    WddmMemoryManager *wddmMM = reinterpret_cast<WddmMemoryManager *>(memManager);

    GraphicsAllocation *commandBuffer = memManager->allocateGraphicsMemory(4096, 4096);
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);

    csr->makeResident(*cs.getGraphicsAllocation());

    csr->makeNonResident(*commandBuffer);

    EXPECT_EQ(1u, memManager->getEvictionAllocations().size());

    memManager->freeGraphicsMemory(commandBuffer);
}

TEST_F(WddmCommandStreamTest, processEvictionPlacesAllAllocationsOnTrimCandidateList) {
    WddmMemoryManager *wddmMM = reinterpret_cast<WddmMemoryManager *>(memManager);

    GraphicsAllocation *allocation = memManager->allocateGraphicsMemory(4096, 4096);
    GraphicsAllocation *allocation2 = memManager->allocateGraphicsMemory(4096, 4096);
    ASSERT_NE(nullptr, allocation);
    ASSERT_NE(nullptr, allocation2);

    memManager->pushAllocationForEviction(allocation);
    memManager->pushAllocationForEviction(allocation2);

    EXPECT_EQ(2u, memManager->getEvictionAllocations().size());

    csr->processEviction();

    EXPECT_EQ(2u, mockWddmMM->trimCandidateList.size());

    memManager->freeGraphicsMemory(allocation);
    memManager->freeGraphicsMemory(allocation2);
}

TEST_F(WddmCommandStreamTest, processEvictionClearsEvictionAllocations) {
    WddmMemoryManager *wddmMM = reinterpret_cast<WddmMemoryManager *>(memManager);

    GraphicsAllocation *allocation = memManager->allocateGraphicsMemory(4096, 4096);
    ASSERT_NE(nullptr, allocation);

    memManager->pushAllocationForEviction(allocation);

    EXPECT_EQ(1u, memManager->getEvictionAllocations().size());

    csr->processEviction();

    EXPECT_EQ(0u, memManager->getEvictionAllocations().size());

    memManager->freeGraphicsMemory(allocation);
}

TEST_F(WddmCommandStreamTest, makeResidentNonResidentMemObj) {
    GraphicsAllocation *gfxAllocation = memManager->allocateGraphicsMemory(256);
    Buffer *buffer = new AlignedBuffer(gfxAllocation);
    WddmMemoryManager *wddmMM = reinterpret_cast<WddmMemoryManager *>(memManager);

    csr->makeResident(*buffer->getGraphicsAllocation());
    EXPECT_EQ(0u, wddm->makeResidentResult.called);
    EXPECT_EQ(1u, memManager->getResidencyAllocations().size());
    EXPECT_EQ(gfxAllocation, memManager->getResidencyAllocations()[0]);

    csr->makeNonResident(*buffer->getGraphicsAllocation());
    EXPECT_EQ(gfxAllocation, memManager->getEvictionAllocations()[0]);

    delete buffer;
    memManager->freeGraphicsMemory(gfxAllocation);
}

TEST_F(WddmCommandStreamTest, createAllocationAndMakeResident) {
    void *hostPtr = reinterpret_cast<void *>(wddm->virtualAllocAddress + 0x1234);
    auto size = 1234u;

    WddmAllocation *gfxAllocation = static_cast<WddmAllocation *>(csr->createAllocationAndHandleResidency(hostPtr, size));

    ASSERT_NE(nullptr, gfxAllocation);

    EXPECT_EQ(1u, memManager->getResidencyAllocations().size());

    EXPECT_EQ(hostPtr, gfxAllocation->getUnderlyingBuffer());
}

TEST_F(WddmCommandStreamTest, givenHostPtrWhenPtrBelowRestrictionThenCreateAllocationAndMakeResident) {
    void *hostPtr = reinterpret_cast<void *>(memManager->getAlignedMallocRestrictions()->minAddress - 0x1000);
    auto size = 0x2000u;

    WddmAllocation *gfxAllocation = static_cast<WddmAllocation *>(csr->createAllocationAndHandleResidency(hostPtr, size));

    void *expectedReserve = reinterpret_cast<void *>(wddm->virtualAllocAddress);

    ASSERT_NE(nullptr, gfxAllocation);

    EXPECT_EQ(1u, memManager->getResidencyAllocations().size());

    EXPECT_EQ(hostPtr, gfxAllocation->getUnderlyingBuffer());
    EXPECT_EQ(expectedReserve, gfxAllocation->getReservedAddress());
    EXPECT_EQ(reinterpret_cast<uint64_t>(expectedReserve), gfxAllocation->getGpuAddress());
}

TEST_F(WddmCommandStreamTest, killAllTemporaryAllocation) {
    void *host_ptr = (void *)0x1212341;
    auto size = 17262u;

    GraphicsAllocation *graphicsAllocation = csr->createAllocationAndHandleResidency(host_ptr, size);
    ASSERT_NE(nullptr, graphicsAllocation);

    graphicsAllocation->taskCount = 1;
    csr->waitForTaskCountAndCleanAllocationList(-1, TEMPORARY_ALLOCATION);
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

    csr->waitForTaskCountAndCleanAllocationList(1, TEMPORARY_ALLOCATION);
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
    GraphicsAllocation *commandBuffer = memManager->allocateGraphicsMemory(4096, 4096);
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);

    csr->makeResident(*commandBuffer);

    EXPECT_EQ(1u, memManager->getResidencyAllocations().size());

    gdi->getMakeResidentArg().NumAllocations = 0;

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    csr->flush(batchBuffer, EngineType::ENGINE_RCS, nullptr);

    EXPECT_NE(0u, gdi->getMakeResidentArg().NumAllocations);

    memManager->freeGraphicsMemory(commandBuffer);
}

TEST_F(WddmCommandStreamMockGdiTest, makeResidentClearsResidencyAllocations) {
    GraphicsAllocation *commandBuffer = memManager->allocateGraphicsMemory(4096, 4096);
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);

    csr->makeResident(*commandBuffer);

    EXPECT_EQ(1u, memManager->getResidencyAllocations().size());
    EXPECT_EQ(0u, memManager->getEvictionAllocations().size());

    EXPECT_EQ(trimListUnusedPosition, ((WddmAllocation *)commandBuffer)->getTrimCandidateListPosition());

    csr->processResidency(nullptr);

    csr->makeSurfacePackNonResident(nullptr);

    EXPECT_EQ(0u, memManager->getResidencyAllocations().size());
    EXPECT_EQ(0u, memManager->getEvictionAllocations().size());

    EXPECT_EQ(0u, ((WddmAllocation *)commandBuffer)->getTrimCandidateListPosition());

    memManager->freeGraphicsMemory(commandBuffer);
}

template <typename GfxFamily>
struct MockWddmCsr : public WddmCommandStreamReceiver<GfxFamily> {
    MockWddmCsr(const HardwareInfo &hwInfoIn, Wddm *wddm) : WddmCommandStreamReceiver(hwInfoIn, wddm){};
    using CommandStreamReceiver::commandStream;
    using CommandStreamReceiver::dispatchMode;
    using CommandStreamReceiver::getCS;
    using WddmCommandStreamReceiver<GfxFamily>::pageTableManagerInitialized;

    void overrideDispatchPolicy(DispatchMode overrideValue) {
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
    mockCsr->setMemoryManager(memManager);
    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);

    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    auto commandBuffer = memManager->allocateGraphicsMemory(1024, 4096);
    auto dshAlloc = memManager->allocateGraphicsMemory(1024, 4096);
    auto iohAlloc = memManager->allocateGraphicsMemory(1024, 4096);
    auto sshAlloc = memManager->allocateGraphicsMemory(1024, 4096);

    mockCsr->setTagAllocation(tagAllocation);
    mockCsr->setPreemptionCsrAllocation(preemptionAllocation);

    LinearStream cs(commandBuffer);
    IndirectHeap dsh(dshAlloc);
    IndirectHeap ioh(iohAlloc);
    IndirectHeap ssh(sshAlloc);

    DispatchFlags dispatchFlags;
    dispatchFlags.guardCommandBufferWithPipeControl = true;
    dispatchFlags.requiresCoherency = true;
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(device->getHardwareInfo());
    mockCsr->flushTask(cs, 0u, dsh, ioh, ssh, 0u, dispatchFlags);

    auto &cmdBuffers = mockedSubmissionsAggregator->peekCommandBuffers();
    auto storedCommandBuffer = cmdBuffers.peekHead();

    ResidencyContainer copyOfResidency = storedCommandBuffer->surfaces;
    copyOfResidency.push_back(storedCommandBuffer->batchBuffer.commandBufferAllocation);

    mockCsr->flushBatchedSubmissions();

    EXPECT_TRUE(cmdBuffers.peekIsEmpty());

    //preemption allocation + sip Kernel
    size_t csrSurfaceCount = (device->getPreemptionMode() == PreemptionMode::MidThread) ? 2 : 0;

    EXPECT_EQ(1u, wddm->submitResult.called);
    auto csrCommandStream = mockCsr->commandStream.getGraphicsAllocation();
    EXPECT_EQ(reinterpret_cast<uint64_t>(csrCommandStream->getUnderlyingBuffer()), wddm->submitResult.commandBufferSubmitted);
    EXPECT_TRUE(((COMMAND_BUFFER_HEADER *)wddm->submitResult.commandHeaderSubmitted)->RequiresCoherency);
    EXPECT_EQ(6u + csrSurfaceCount, wddm->makeResidentResult.handleCount);

    std::vector<D3DKMT_HANDLE> expectedHandles;
    expectedHandles.push_back(((WddmAllocation *)tagAllocation)->handle);
    expectedHandles.push_back(((WddmAllocation *)commandBuffer)->handle);
    expectedHandles.push_back(((WddmAllocation *)dshAlloc)->handle);
    expectedHandles.push_back(((WddmAllocation *)iohAlloc)->handle);
    expectedHandles.push_back(((WddmAllocation *)sshAlloc)->handle);
    expectedHandles.push_back(((WddmAllocation *)csrCommandStream)->handle);

    for (auto i = 0u; i < wddm->makeResidentResult.handleCount; i++) {
        auto handle = wddm->makeResidentResult.handlePack[i];
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
    EXPECT_EQ(trimListUnusedPosition, ((WddmAllocation *)dshAlloc)->getTrimCandidateListPosition());
    EXPECT_EQ(trimListUnusedPosition, ((WddmAllocation *)iohAlloc)->getTrimCandidateListPosition());
    EXPECT_NE(trimListUnusedPosition, ((WddmAllocation *)sshAlloc)->getTrimCandidateListPosition());
    EXPECT_NE(trimListUnusedPosition, ((WddmAllocation *)csrCommandStream)->getTrimCandidateListPosition());

    memManager->freeGraphicsMemory(dshAlloc);
    memManager->freeGraphicsMemory(iohAlloc);
    memManager->freeGraphicsMemory(sshAlloc);
    memManager->freeGraphicsMemory(commandBuffer);
}

HWTEST_F(WddmDefaultTest, givenDefaultWddmCsrWhenItIsCreatedThenBatchingIsTurnedOn) {
    DebugManager.flags.CsrDispatchMode.set(0);
    std::unique_ptr<MockWddmCsr<FamilyType>> mockCsr(new MockWddmCsr<FamilyType>(*platformDevices[0], this->wddm));
    EXPECT_EQ(DispatchMode::BatchedDispatch, mockCsr->dispatchMode);
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

    void createMockWddm() {
        myMockWddm.reset(static_cast<WddmMock *>(Wddm::createWddm(WddmInterfaceVersion::Wddm20)));
    }

    HardwareInfo hwInfo = {};
    std::unique_ptr<WddmMock> myMockWddm;
};

HWTEST_F(WddmCsrCompressionTests, givenEnabledCompressionWhenInitializedThenCreatePagetableMngr) {
    createMockWddm();
    EXPECT_EQ(nullptr, myMockWddm->getPageTableManager());
    MockWddmCsr<FamilyType> mockWddmCsr(hwInfo, myMockWddm.get());
    ASSERT_NE(nullptr, myMockWddm->getPageTableManager());

    auto mockMngr = reinterpret_cast<MockGmmPageTableMngr *>(myMockWddm->getPageTableManager());

    GMM_DEVICE_CALLBACKS expectedDeviceCb = {};
    GMM_TRANSLATIONTABLE_CALLBACKS expectedTTCallbacks = {};
    unsigned int expectedFlags = (TT_TYPE::TRTT | TT_TYPE::AUXTT);
    auto myGdi = myMockWddm->getGdi();
    // clang-format off
    expectedDeviceCb.Adapter         = myMockWddm->getAdapter();
    expectedDeviceCb.hDevice         = myMockWddm->getDevice();
    expectedDeviceCb.PagingQueue     = myMockWddm->getPagingQueue();
    expectedDeviceCb.PagingFence     = myMockWddm->getPagingQueueSyncObject();

    expectedDeviceCb.pfnAllocate     = myGdi->createAllocation;
    expectedDeviceCb.pfnDeallocate   = myGdi->destroyAllocation;
    expectedDeviceCb.pfnMapGPUVA     = myGdi->mapGpuVirtualAddress;
    expectedDeviceCb.pfnMakeResident = myGdi->makeResident;
    expectedDeviceCb.pfnEvict        = myGdi->evict;
    expectedDeviceCb.pfnReserveGPUVA = myGdi->reserveGpuVirtualAddress;
    expectedDeviceCb.pfnUpdateGPUVA  = myGdi->updateGpuVirtualAddress;
    expectedDeviceCb.pfnWaitFromCpu  = myGdi->waitForSynchronizationObjectFromCpu;
    expectedDeviceCb.pfnLock         = myGdi->lock2;
    expectedDeviceCb.pfnUnLock       = myGdi->unlock2;
    expectedDeviceCb.pfnEscape       = myGdi->escape;

    expectedTTCallbacks.pfWriteL3Adr = TTCallbacks<FamilyType>::writeL3Address;
    // clang-format on

    EXPECT_TRUE(memcmp(&expectedDeviceCb, &mockMngr->deviceCb, sizeof(GMM_DEVICE_CALLBACKS)) == 0);
    EXPECT_TRUE(memcmp(&expectedTTCallbacks, &mockMngr->translationTableCb, sizeof(GMM_TRANSLATIONTABLE_CALLBACKS)) == 0);
    EXPECT_TRUE(memcmp(&expectedFlags, &mockMngr->translationTableFlags, sizeof(unsigned int)) == 0);
}

HWTEST_F(WddmCsrCompressionTests, givenDisabledCompressionWhenInitializedThenDontCreatePagetableMngr) {
    setCompressionEnabled(false);
    createMockWddm();
    MockWddmCsr<FamilyType> mockWddmCsr(hwInfo, myMockWddm.get());
    EXPECT_EQ(nullptr, myMockWddm->getPageTableManager());
}

HWTEST_F(WddmCsrCompressionTests, givenEnabledCompressionWhenFlushingThenInitTranslationTableOnce) {
    createMockWddm();
    MockWddmCsr<FamilyType> mockWddmCsr(hwInfo, myMockWddm.get());
    mockWddmCsr.overrideDispatchPolicy(DispatchMode::BatchedDispatch);

    auto mockMngr = reinterpret_cast<MockGmmPageTableMngr *>(myMockWddm->getPageTableManager());
    mockWddmCsr.setMemoryManager(memManager);
    mockWddmCsr.setTagAllocation(tagAllocation);
    mockWddmCsr.setPreemptionCsrAllocation(preemptionAllocation);
    auto &csrCS = mockWddmCsr.getCS();

    auto graphicsAllocation = memManager->allocateGraphicsMemory(1024, 4096);
    IndirectHeap cs(graphicsAllocation);

    EXPECT_FALSE(mockWddmCsr.pageTableManagerInitialized);

    EXPECT_CALL(*mockMngr, initContextAuxTableRegister(&mockWddmCsr, GMM_ENGINE_TYPE::ENGINE_TYPE_RCS))
        .Times(1)
        .WillOnce(Return(GMM_SUCCESS));
    EXPECT_CALL(*mockMngr, initContextTRTableRegister(&mockWddmCsr, GMM_ENGINE_TYPE::ENGINE_TYPE_RCS))
        .Times(1)
        .WillOnce(Return(GMM_SUCCESS));

    DispatchFlags dispatchFlags;
    mockWddmCsr.flushTask(cs, 0u, cs, cs, cs, 0u, dispatchFlags);

    EXPECT_TRUE(mockWddmCsr.pageTableManagerInitialized);

    // flush again to check if PT manager was initialized once
    mockWddmCsr.flushTask(cs, 0u, cs, cs, cs, 0u, dispatchFlags);

    memManager->freeGraphicsMemory(graphicsAllocation);
}

HWTEST_F(WddmCsrCompressionTests, givenDisabledCompressionWhenFlushingThenDontInitTranslationTable) {
    setCompressionEnabled(false);
    createMockWddm();
    MockWddmCsr<FamilyType> mockWddmCsr(hwInfo, myMockWddm.get());
    mockWddmCsr.overrideDispatchPolicy(DispatchMode::BatchedDispatch);

    EXPECT_EQ(nullptr, myMockWddm->getPageTableManager());

    mockWddmCsr.setMemoryManager(memManager);
    mockWddmCsr.setTagAllocation(tagAllocation);
    mockWddmCsr.setPreemptionCsrAllocation(preemptionAllocation);

    auto graphicsAllocation = memManager->allocateGraphicsMemory(1024, 4096);
    IndirectHeap cs(graphicsAllocation);

    EXPECT_FALSE(mockWddmCsr.pageTableManagerInitialized);

    DispatchFlags dispatchFlags;
    mockWddmCsr.flushTask(cs, 0u, cs, cs, cs, 0u, dispatchFlags);

    EXPECT_FALSE(mockWddmCsr.pageTableManagerInitialized);

    memManager->freeGraphicsMemory(graphicsAllocation);
}
