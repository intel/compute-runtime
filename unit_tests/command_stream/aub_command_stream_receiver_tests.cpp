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

#include "runtime/command_stream/aub_command_stream_receiver_hw.h"
#include "runtime/helpers/hw_info.h"
#include "runtime/memory_manager/memory_manager.h"
#include "runtime/os_interface/debug_settings_manager.h"
#include "test.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/mocks/mock_gmm.h"

using OCLRT::AUBCommandStreamReceiver;
using OCLRT::AUBCommandStreamReceiverHw;
using OCLRT::BatchBuffer;
using OCLRT::CommandStreamReceiver;
using OCLRT::DebugManager;
using OCLRT::GraphicsAllocation;
using OCLRT::HardwareInfo;
using OCLRT::LinearStream;
using OCLRT::MemoryManager;
using OCLRT::ObjectNotResident;
using OCLRT::platformDevices;
using OCLRT::ResidencyContainer;

typedef Test<DeviceFixture> AubCommandStreamReceiverTests;

template <typename GfxFamily>
struct MockAubCsr : public AUBCommandStreamReceiverHw<GfxFamily> {
    MockAubCsr(const HardwareInfo &hwInfoIn, bool standalone) : AUBCommandStreamReceiverHw<GfxFamily>(hwInfoIn, standalone){};

    CommandStreamReceiver::DispatchMode peekDispatchMode() const {
        return this->dispatchMode;
    }
};

TEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenItIsCreatedWithWrongGfxCoreFamilyThenNullPointerShouldBeReturned) {
    HardwareInfo hwInfo = *platformDevices[0];
    GFXCORE_FAMILY family = hwInfo.pPlatform->eRenderCoreFamily;

    const_cast<PLATFORM *>(hwInfo.pPlatform)->eRenderCoreFamily = GFXCORE_FAMILY_FORCE_ULONG; // wrong gfx core family

    CommandStreamReceiver *aubCsr = AUBCommandStreamReceiver::create(hwInfo, "", true);
    EXPECT_EQ(nullptr, aubCsr);

    const_cast<PLATFORM *>(hwInfo.pPlatform)->eRenderCoreFamily = family;
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCsrWhenItIsCreatedWithDefaultSettingsThenItHasBatchedDispatchModeEnabled) {
    DebugManager.flags.CsrDispatchMode.set(0);
    std::unique_ptr<MockAubCsr<FamilyType>> aubCsr(new MockAubCsr<FamilyType>(*platformDevices[0], true));
    EXPECT_EQ(CommandStreamReceiver::DispatchMode::BatchedDispatch, aubCsr->peekDispatchMode());
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCsrWhenItIsCreatedWithDebugSettingsThenItHasProperDispatchModeEnabled) {
    DebugManagerStateRestore stateRestore;
    DebugManager.flags.CsrDispatchMode.set(CommandStreamReceiver::DispatchMode::ImmediateDispatch);
    std::unique_ptr<MockAubCsr<FamilyType>> aubCsr(new MockAubCsr<FamilyType>(*platformDevices[0], true));
    EXPECT_EQ(CommandStreamReceiver::DispatchMode::ImmediateDispatch, aubCsr->peekDispatchMode());
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenItIsCreatedThenMemoryManagerIsNotNull) {
    HardwareInfo hwInfo;

    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>(hwInfo, true));
    std::unique_ptr<MemoryManager> memoryManager(aubCsr->createMemoryManager(false));
    EXPECT_NE(nullptr, memoryManager.get());
    aubCsr->setMemoryManager(nullptr);
}

HWTEST_F(AubCommandStreamReceiverTests, givenGraphicsAllocationWhenMakeResidentCalledMultipleTimesAffectsResidencyOnce) {
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>(*platformDevices[0], true));
    std::unique_ptr<MemoryManager> memoryManager(aubCsr->createMemoryManager(false));
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
    aubCsr->setMemoryManager(nullptr);
}

HWTEST_F(AubCommandStreamReceiverTests, flushShouldLeaveProperRingTailAlignment) {
    auto aubCsr = new AUBCommandStreamReceiverHw<FamilyType>(*platformDevices[0], true);
    auto memoryManager = aubCsr->createMemoryManager(false);

    GraphicsAllocation *commandBuffer = memoryManager->allocateGraphicsMemory(4096, 4096);
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);

    auto engineType = OCLRT::ENGINE_RCS;
    auto ringTailAlignment = sizeof(uint64_t);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};

    // First flush typically includes a preamble and chain to command buffer
    aubCsr->overrideDispatchPolicy(CommandStreamReceiver::DispatchMode::ImmediateDispatch);
    aubCsr->flush(batchBuffer, engineType, nullptr);
    EXPECT_EQ(0ull, aubCsr->engineInfoTable[engineType].tailRingBuffer % ringTailAlignment);

    // Second flush should just submit command buffer
    cs.getSpace(sizeof(uint64_t));
    aubCsr->flush(batchBuffer, engineType, nullptr);
    EXPECT_EQ(0ull, aubCsr->engineInfoTable[engineType].tailRingBuffer % ringTailAlignment);

    memoryManager->freeGraphicsMemory(commandBuffer);
    delete aubCsr;
    delete memoryManager;
}

HWTEST_F(AubCommandStreamReceiverTests, flushShouldCallMakeResidentOnCommandBufferAllocation) {
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>(*platformDevices[0], true));
    std::unique_ptr<MemoryManager> memoryManager(aubCsr->createMemoryManager(false));

    GraphicsAllocation *commandBuffer = memoryManager->allocateGraphicsMemory(4096, 4096);
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    auto engineType = OCLRT::ENGINE_RCS;

    EXPECT_EQ(ObjectNotResident, commandBuffer->residencyTaskCount);

    aubCsr->overrideDispatchPolicy(CommandStreamReceiver::DispatchMode::ImmediateDispatch);
    aubCsr->flush(batchBuffer, engineType, nullptr);

    EXPECT_NE(ObjectNotResident, commandBuffer->residencyTaskCount);
    EXPECT_EQ((int)aubCsr->peekTaskCount() + 1, commandBuffer->residencyTaskCount);

    aubCsr->makeSurfacePackNonResident(nullptr);

    EXPECT_EQ(ObjectNotResident, commandBuffer->residencyTaskCount);

    memoryManager->freeGraphicsMemoryImpl(commandBuffer);
    aubCsr->setMemoryManager(nullptr);
}

HWTEST_F(AubCommandStreamReceiverTests, flushShouldCallMakeResidentOnResidencyAllocations) {
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>(*platformDevices[0], true));
    std::unique_ptr<MemoryManager> memoryManager(aubCsr->createMemoryManager(false));
    auto gfxAllocation = memoryManager->allocateGraphicsMemory(sizeof(uint32_t), sizeof(uint32_t), false, false);

    GraphicsAllocation *commandBuffer = memoryManager->allocateGraphicsMemory(4096, 4096);
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    auto engineType = OCLRT::ENGINE_RCS;
    ResidencyContainer allocationsForResidency = {gfxAllocation};

    EXPECT_EQ(ObjectNotResident, gfxAllocation->residencyTaskCount);
    EXPECT_EQ(ObjectNotResident, commandBuffer->residencyTaskCount);

    aubCsr->overrideDispatchPolicy(CommandStreamReceiver::DispatchMode::BatchedDispatch);
    aubCsr->flush(batchBuffer, engineType, &allocationsForResidency);

    EXPECT_NE(ObjectNotResident, gfxAllocation->residencyTaskCount);
    EXPECT_EQ((int)aubCsr->peekTaskCount() + 1, gfxAllocation->residencyTaskCount);

    EXPECT_NE(ObjectNotResident, commandBuffer->residencyTaskCount);
    EXPECT_EQ((int)aubCsr->peekTaskCount() + 1, commandBuffer->residencyTaskCount);

    aubCsr->makeSurfacePackNonResident(&allocationsForResidency);

    EXPECT_EQ(ObjectNotResident, gfxAllocation->residencyTaskCount);
    EXPECT_EQ(ObjectNotResident, commandBuffer->residencyTaskCount);

    memoryManager->freeGraphicsMemory(commandBuffer);
    memoryManager->freeGraphicsMemoryImpl(gfxAllocation);
    aubCsr->setMemoryManager(nullptr);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenGraphicsAllocationIsCreatedThenItDoesntHaveTypeNonAubWritable) {
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>(*platformDevices[0], true));
    std::unique_ptr<MemoryManager> memoryManager(aubCsr->createMemoryManager(false));
    auto gfxAllocation = memoryManager->allocateGraphicsMemory(sizeof(uint32_t), sizeof(uint32_t), false, false);

    EXPECT_FALSE(!!(gfxAllocation->getAllocationType() & GraphicsAllocation::ALLOCATION_TYPE_NON_AUB_WRITABLE));

    memoryManager->freeGraphicsMemoryImpl(gfxAllocation);
    aubCsr->setMemoryManager(nullptr);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenProcessResidencyIsCalledOnDefaultAllocationThenAllocationTypeShouldNotBeMadeNonAubWritable) {
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>(*platformDevices[0], true));
    std::unique_ptr<MemoryManager> memoryManager(aubCsr->createMemoryManager(false));
    auto gfxDefaultAllocation = memoryManager->allocateGraphicsMemory(sizeof(uint32_t), sizeof(uint32_t), false, false);

    ResidencyContainer allocationsForResidency = {gfxDefaultAllocation};
    aubCsr->processResidency(&allocationsForResidency);

    EXPECT_FALSE(!!(gfxDefaultAllocation->getAllocationType() & GraphicsAllocation::ALLOCATION_TYPE_NON_AUB_WRITABLE));

    memoryManager->freeGraphicsMemoryImpl(gfxDefaultAllocation);
    aubCsr->setMemoryManager(nullptr);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenProcessResidencyIsCalledOnBufferAndImageAllocationsThenAllocationsTypesShouldBeMadeNonAubWritable) {
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>(*platformDevices[0], true));
    std::unique_ptr<MemoryManager> memoryManager(aubCsr->createMemoryManager(false));

    auto gfxBufferAllocation = memoryManager->allocateGraphicsMemory(sizeof(uint32_t), sizeof(uint32_t), false, false);
    gfxBufferAllocation->setAllocationType(GraphicsAllocation::ALLOCATION_TYPE_BUFFER);

    auto gfxImageAllocation = memoryManager->allocateGraphicsMemory(sizeof(uint32_t), sizeof(uint32_t), false, false);
    gfxImageAllocation->setAllocationType(GraphicsAllocation::ALLOCATION_TYPE_IMAGE);

    ResidencyContainer allocationsForResidency = {gfxBufferAllocation, gfxImageAllocation};
    aubCsr->processResidency(&allocationsForResidency);

    EXPECT_TRUE(!!(gfxBufferAllocation->getAllocationType() & GraphicsAllocation::ALLOCATION_TYPE_NON_AUB_WRITABLE));
    EXPECT_TRUE(!!(gfxImageAllocation->getAllocationType() & GraphicsAllocation::ALLOCATION_TYPE_NON_AUB_WRITABLE));

    memoryManager->freeGraphicsMemoryImpl(gfxBufferAllocation);
    memoryManager->freeGraphicsMemoryImpl(gfxImageAllocation);
    aubCsr->setMemoryManager(nullptr);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenGraphicsAllocationTypeIsntNonAubWritableThenWriteMemoryIsAllowed) {
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>(*platformDevices[0], true));
    std::unique_ptr<MemoryManager> memoryManager(aubCsr->createMemoryManager(false));
    auto gfxAllocation = memoryManager->allocateGraphicsMemory(sizeof(uint32_t), sizeof(uint32_t), false, false);

    EXPECT_TRUE(aubCsr->writeMemory(*gfxAllocation));

    memoryManager->freeGraphicsMemoryImpl(gfxAllocation);
    aubCsr->setMemoryManager(nullptr);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenGraphicsAllocationTypeIsNonAubWritableThenWriteMemoryIsNotAllowed) {
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>(*platformDevices[0], true));
    std::unique_ptr<MemoryManager> memoryManager(aubCsr->createMemoryManager(false));
    auto gfxAllocation = memoryManager->allocateGraphicsMemory(sizeof(uint32_t), sizeof(uint32_t), false, false);

    gfxAllocation->setAllocationType(GraphicsAllocation::ALLOCATION_TYPE_NON_AUB_WRITABLE);
    EXPECT_FALSE(aubCsr->writeMemory(*gfxAllocation));

    memoryManager->freeGraphicsMemoryImpl(gfxAllocation);
    aubCsr->setMemoryManager(nullptr);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenGraphicsAllocationSizeIsZeroThenWriteMemoryIsNotAllowed) {
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>(*platformDevices[0], true));
    auto gfxAllocation = GraphicsAllocation((void *)0x1234, 0);

    EXPECT_FALSE(aubCsr->writeMemory(gfxAllocation));
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverInNoneStandaloneModeWhenFlushIsCalledThenItShouldNotCallMakeResidentOnCommandBufferAllocation) {
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>(*platformDevices[0], false));
    std::unique_ptr<MemoryManager> memoryManager(aubCsr->createMemoryManager(false));

    GraphicsAllocation *commandBuffer = memoryManager->allocateGraphicsMemory(4096, 4096);
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    auto engineType = OCLRT::ENGINE_RCS;

    EXPECT_EQ(ObjectNotResident, commandBuffer->residencyTaskCount);

    aubCsr->flush(batchBuffer, engineType, nullptr);

    EXPECT_EQ(ObjectNotResident, commandBuffer->residencyTaskCount);

    memoryManager->freeGraphicsMemoryImpl(commandBuffer);
    aubCsr->setMemoryManager(nullptr);
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
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>(*platformDevices[0], true));
    std::unique_ptr<OsAgnosticMemoryManagerForImagesWithNoHostPtr> memoryManager(new OsAgnosticMemoryManagerForImagesWithNoHostPtr);
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
    memoryManager->freeGraphicsMemoryImpl(imageAllocation);
    aubCsr->setMemoryManager(nullptr);
}
