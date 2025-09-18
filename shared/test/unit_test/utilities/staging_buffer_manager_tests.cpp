/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/os_interface.h"
#include "shared/source/utilities/staging_buffer_manager.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_svm_manager.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/common/test_macros/test.h"

#include "gtest/gtest.h"

using namespace NEO;

struct MockOSIface : OSInterface {
    bool isSizeWithinThresholdForStaging(const void *ptr, size_t size) const override {
        return isSizeWithinThresholdValue;
    }
    bool isSizeWithinThresholdValue = true;
};

class StagingBufferManagerFixture : public DeviceFixture {
  public:
    void setUp() {
        DeviceFixture::setUp();
        this->svmAllocsManager = std::make_unique<MockSVMAllocsManager>(pDevice->getMemoryManager());
        debugManager.flags.EnableCopyWithStagingBuffers.set(1);
        RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
        std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};
        this->stagingBufferManager = std::make_unique<StagingBufferManager>(svmAllocsManager.get(), rootDeviceIndices, deviceBitfields, false);
        this->csr = pDevice->commandStreamReceivers[0].get();
        this->osInterface = new MockOSIface{};
        this->pDevice->getRootDeviceEnvironmentRef().osInterface.reset(this->osInterface);
    }

    void tearDown() {
        stagingBufferManager.reset();
        svmAllocsManager.reset();
        DeviceFixture::tearDown();
    }

    void *allocateDeviceBuffer(size_t size) {
        RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
        std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};
        SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::deviceUnifiedMemory, 0u, rootDeviceIndices, deviceBitfields);
        unifiedMemoryProperties.device = pDevice;
        return svmAllocsManager->createHostUnifiedMemoryAllocation(size, unifiedMemoryProperties);
    }

    void copyThroughStagingBuffers(size_t copySize, size_t expectedChunks, size_t expectedAllocations, CommandStreamReceiver *csr) {
        auto usmBuffer = allocateDeviceBuffer(copySize);
        auto nonUsmBuffer = new unsigned char[copySize];

        size_t chunkCounter = 0;
        memset(usmBuffer, 0, copySize);
        memset(nonUsmBuffer, 0xFF, copySize);

        ChunkCopyFunction chunkCopy = [&](void *chunkSrc, void *chunkDst, size_t chunkSize) {
            chunkCounter++;
            memcpy(chunkDst, chunkSrc, chunkSize);
            reinterpret_cast<MockCommandStreamReceiver *>(csr)->taskCount++;
            return 0;
        };
        auto initialNumOfUsmAllocations = svmAllocsManager->svmAllocs.getNumAllocs();
        auto ret = stagingBufferManager->performCopy(usmBuffer, nonUsmBuffer, copySize, chunkCopy, csr);
        auto newUsmAllocations = svmAllocsManager->svmAllocs.getNumAllocs() - initialNumOfUsmAllocations;

        EXPECT_EQ(0, ret.chunkCopyStatus);
        EXPECT_EQ(WaitStatus::ready, ret.waitStatus);
        EXPECT_EQ(0, memcmp(usmBuffer, nonUsmBuffer, copySize));
        EXPECT_EQ(expectedChunks, chunkCounter);
        EXPECT_EQ(expectedAllocations, newUsmAllocations);
        svmAllocsManager->freeSVMAlloc(usmBuffer);
        delete[] nonUsmBuffer;
    }

    void imageTransferThroughStagingBuffers(bool isRead, size_t rowPitch, const size_t *globalOrigin, const size_t *globalRegion, size_t expectedChunks) {
        auto hostPtr = new unsigned char[stagingBufferSize * (expectedChunks + globalOrigin[1])];
        auto imageData = new unsigned char[stagingBufferSize * (expectedChunks + globalOrigin[1])];
        if (isRead) {
            memset(hostPtr, 0, stagingBufferSize * (expectedChunks + globalOrigin[1]));
            memset(imageData, 0xFF, stagingBufferSize * (expectedChunks + globalOrigin[1]));
        } else {
            memset(hostPtr, 0xFF, stagingBufferSize * (expectedChunks + globalOrigin[1]));
            memset(imageData, 0, stagingBufferSize * (expectedChunks + globalOrigin[1]));
        }
        size_t chunkCounter = 0;
        size_t expectedOrigin = globalOrigin[1];
        auto rowSize = globalRegion[0] * pixelElemSize;
        auto expectedRowsPerChunk = std::min<size_t>(std::max<size_t>(1ul, stagingBufferSize / rowPitch), globalRegion[1]);
        auto numOfChunks = globalRegion[1] / expectedRowsPerChunk;
        auto remainder = globalRegion[1] % (expectedRowsPerChunk * numOfChunks);

        // This lambda function simulates chunk read/write on image.
        // Iterates over rows in given image chunk, copies each row with offset origin[0]
        // For writes, data is transferred in staging buffer -> image direction.
        // For reads, it's image -> staging buffer.
        ChunkTransferImageFunc chunkTransfer = [&](void *stagingBuffer, const size_t *origin, const size_t *region) -> int32_t {
            EXPECT_NE(nullptr, stagingBuffer);
            EXPECT_NE(nullptr, origin);
            EXPECT_NE(nullptr, region);

            EXPECT_EQ(globalOrigin[0], origin[0]);
            EXPECT_EQ(globalRegion[0], region[0]);

            EXPECT_EQ(expectedOrigin, origin[1]);
            if (chunkCounter + 1 == expectedChunks && remainder != 0) {
                EXPECT_EQ(remainder, region[1]);
            } else {
                EXPECT_EQ(expectedRowsPerChunk, region[1]);
            }

            for (auto rowId = 0u; rowId < region[1]; rowId++) {
                void *dst = ptrOffset(imageData, (origin[1] + rowId) * rowPitch + origin[0] * pixelElemSize);
                void *src = ptrOffset(stagingBuffer, rowId * rowPitch);
                if (isRead) {
                    std::swap(dst, src);
                }
                memcpy(dst, src, rowSize);
            }
            expectedOrigin += region[1];
            chunkCounter++;
            reinterpret_cast<MockCommandStreamReceiver *>(csr)->taskCount++;
            return 0;
        };
        auto initialNumOfUsmAllocations = svmAllocsManager->svmAllocs.getNumAllocs();
        auto ret = stagingBufferManager->performImageTransfer(hostPtr, globalOrigin, globalRegion, rowPitch, rowPitch * globalRegion[1], pixelElemSize, false, chunkTransfer, csr, isRead);
        auto newUsmAllocations = svmAllocsManager->svmAllocs.getNumAllocs() - initialNumOfUsmAllocations;

        for (auto rowId = 0u; rowId < globalRegion[1]; rowId++) {
            auto offset = (globalOrigin[1] + rowId) * rowPitch;
            EXPECT_EQ(0, memcmp(ptrOffset(hostPtr, rowId * rowPitch), ptrOffset(imageData, offset + globalOrigin[0] * pixelElemSize), rowSize));
        }

        EXPECT_EQ(0, ret.chunkCopyStatus);
        EXPECT_EQ(WaitStatus::ready, ret.waitStatus);
        EXPECT_EQ(expectedChunks, chunkCounter);

        auto expectedNewUsmAllocations = 1u;
        if (isRead && rowPitch * globalRegion[1] > stagingBufferSize) {
            expectedNewUsmAllocations = 2u;
        }
        EXPECT_EQ(expectedNewUsmAllocations, newUsmAllocations);
        delete[] hostPtr;
        delete[] imageData;
    }

    void bufferTransferThroughStagingBuffers(size_t copySize, size_t expectedChunks, size_t expectedAllocations, CommandStreamReceiver *csr) {
        auto buffer = new unsigned char[copySize];
        auto nonUsmBuffer = new unsigned char[copySize];

        size_t chunkCounter = 0;
        memset(buffer, 0, copySize);
        memset(nonUsmBuffer, 0xFF, copySize);

        ChunkTransferBufferFunc chunkCopy = [&](void *stagingBuffer, size_t offset, size_t size) {
            chunkCounter++;
            memcpy(buffer + offset, stagingBuffer, size);
            reinterpret_cast<MockCommandStreamReceiver *>(csr)->taskCount++;
            return 0;
        };
        auto initialNumOfUsmAllocations = svmAllocsManager->svmAllocs.getNumAllocs();
        auto ret = stagingBufferManager->performBufferTransfer(nonUsmBuffer, 0, copySize, chunkCopy, csr, false);
        auto newUsmAllocations = svmAllocsManager->svmAllocs.getNumAllocs() - initialNumOfUsmAllocations;

        EXPECT_EQ(0, ret.chunkCopyStatus);
        EXPECT_EQ(WaitStatus::ready, ret.waitStatus);
        EXPECT_EQ(0, memcmp(buffer, nonUsmBuffer, copySize));
        EXPECT_EQ(expectedChunks, chunkCounter);
        EXPECT_EQ(expectedAllocations, newUsmAllocations);

        delete[] buffer;
        delete[] nonUsmBuffer;
    }

    void fillUserData(unsigned int *userData, size_t size) {
        for (auto i = 0u; i < size; i++) {
            userData[i] = i;
        }
    }

    constexpr static size_t stagingBufferSize = MemoryConstants::pageSize;
    constexpr static size_t pitchSize = stagingBufferSize / 2;
    constexpr static size_t pixelElemSize = 1u;
    DebugManagerStateRestore restorer;
    std::unique_ptr<MockSVMAllocsManager> svmAllocsManager;
    std::unique_ptr<StagingBufferManager> stagingBufferManager;
    CommandStreamReceiver *csr;
    MockOSIface *osInterface;
};

using StagingBufferManagerTest = Test<StagingBufferManagerFixture>;

TEST_F(StagingBufferManagerTest, givenStagingBufferEnabledWhenValidForCopyThenReturnTrue) {
    constexpr size_t bufferSize = 1024;
    auto usmBuffer = allocateDeviceBuffer(bufferSize);
    unsigned char nonUsmBuffer[bufferSize];
    auto svmData = svmAllocsManager->getSVMAlloc(usmBuffer);
    auto alloc = svmData->gpuAllocations.getDefaultGraphicsAllocation();
    struct {
        void *dstPtr;
        void *srcPtr;
        size_t size;
        bool hasDependencies;
        bool usedByOsContext;
        bool expectValid;
    } copyParamsStruct[7]{
        {usmBuffer, nonUsmBuffer, bufferSize, false, true, true},             // nonUsm -> usm without dependencies
        {usmBuffer, nonUsmBuffer, bufferSize, true, true, false},             // nonUsm -> usm with dependencies
        {nonUsmBuffer, nonUsmBuffer, bufferSize, false, true, false},         // nonUsm -> nonUsm without dependencies
        {usmBuffer, usmBuffer, bufferSize, false, true, false},               // usm -> usm without dependencies
        {nonUsmBuffer, usmBuffer, bufferSize, false, true, false},            // usm -> nonUsm without dependencies
        {usmBuffer, nonUsmBuffer, bufferSize, false, false, true},            // nonUsm -> usm unused by os context, small transfer
        {usmBuffer, nonUsmBuffer, stagingBufferSize * 2, false, false, false} // nonUsm -> usm unused by os context, large transfer
    };
    for (auto i = 0; i < 7; i++) {
        if (copyParamsStruct[i].usedByOsContext) {
            alloc->updateTaskCount(1, 0);
        } else {
            alloc->releaseUsageInOsContext(0);
        }
        auto actualValid = stagingBufferManager->isValidForCopy(*pDevice, copyParamsStruct[i].dstPtr, copyParamsStruct[i].srcPtr, copyParamsStruct[i].size, copyParamsStruct[i].hasDependencies, 0u);
        EXPECT_EQ(actualValid, copyParamsStruct[i].expectValid);
        stagingBufferManager->resetDetectedPtrs();
    }

    debugManager.flags.EnableCopyWithStagingBuffers.set(0);
    EXPECT_FALSE(stagingBufferManager->isValidForCopy(*pDevice, usmBuffer, nonUsmBuffer, bufferSize, false, 0u));
    stagingBufferManager->resetDetectedPtrs();

    debugManager.flags.EnableCopyWithStagingBuffers.set(-1);
    auto isStagingBuffersEnabled = pDevice->getProductHelper().isStagingBuffersEnabled();
    EXPECT_EQ(isStagingBuffersEnabled, stagingBufferManager->isValidForCopy(*pDevice, usmBuffer, nonUsmBuffer, bufferSize, false, 0u));

    stagingBufferManager->registerHostPtr(nonUsmBuffer);
    EXPECT_FALSE(stagingBufferManager->isValidForCopy(*pDevice, usmBuffer, nonUsmBuffer, bufferSize, false, 0u));
    stagingBufferManager->resetDetectedPtrs();

    this->osInterface->isSizeWithinThresholdValue = false;
    EXPECT_FALSE(stagingBufferManager->isValidForCopy(*pDevice, usmBuffer, nonUsmBuffer, bufferSize, false, 0u));
    stagingBufferManager->resetDetectedPtrs();

    this->pDevice->getRootDeviceEnvironmentRef().osInterface.reset(nullptr);
    EXPECT_EQ(isStagingBuffersEnabled, stagingBufferManager->isValidForCopy(*pDevice, usmBuffer, nonUsmBuffer, bufferSize, false, 0u));
    svmAllocsManager->freeSVMAlloc(usmBuffer);
}

TEST_F(StagingBufferManagerTest, givenStagingBufferEnabledWhenValidForStagingTransferThenReturnCorrectValue) {
    constexpr size_t bufferSize = 1024;
    auto usmBuffer = allocateDeviceBuffer(bufferSize);
    unsigned char nonUsmBuffer[bufferSize];

    struct {
        void *ptr;
        bool hasDependencies;
        bool expectValid;
    } copyParamsStruct[4]{
        {usmBuffer, false, false},
        {usmBuffer, true, false},
        {nonUsmBuffer, false, true},
        {nonUsmBuffer, true, false},
    };
    for (auto i = 0; i < 4; i++) {
        auto actualValid = stagingBufferManager->isValidForStagingTransfer(*pDevice, copyParamsStruct[i].ptr, bufferSize, copyParamsStruct[i].hasDependencies);
        EXPECT_EQ(actualValid, copyParamsStruct[i].expectValid);
    }

    debugManager.flags.EnableCopyWithStagingBuffers.set(0);
    EXPECT_FALSE(stagingBufferManager->isValidForStagingTransfer(*pDevice, nonUsmBuffer, bufferSize, false));

    debugManager.flags.EnableCopyWithStagingBuffers.set(-1);
    auto isStaingBuffersEnabled = pDevice->getProductHelper().isStagingBuffersEnabled();
    stagingBufferManager->resetDetectedPtrs();
    EXPECT_EQ(isStaingBuffersEnabled, stagingBufferManager->isValidForStagingTransfer(*pDevice, nonUsmBuffer, bufferSize, false));

    EXPECT_FALSE(stagingBufferManager->isValidForStagingTransfer(*pDevice, usmBuffer, bufferSize, false));

    stagingBufferManager->registerHostPtr(nonUsmBuffer);
    EXPECT_FALSE(stagingBufferManager->isValidForStagingTransfer(*pDevice, nonUsmBuffer, bufferSize, false));

    svmAllocsManager->freeSVMAlloc(usmBuffer);
}

TEST_F(StagingBufferManagerTest, givenStagingBufferWhenPerformCopyOnHwThenDontSetWritable) {
    constexpr size_t numOfChunkCopies = 8;
    constexpr size_t remainder = 1024;
    constexpr size_t totalCopySize = stagingBufferSize * numOfChunkCopies + remainder;
    copyThroughStagingBuffers(totalCopySize, numOfChunkCopies + 1, 1, csr);
    auto svmData = svmAllocsManager->svmAllocs.allocations[0].second.get();
    auto alloc = svmData->gpuAllocations.getDefaultGraphicsAllocation();
    alloc->setAubWritable(false, std::numeric_limits<uint32_t>::max());
    alloc->setTbxWritable(false, std::numeric_limits<uint32_t>::max());
    copyThroughStagingBuffers(totalCopySize, numOfChunkCopies + 1, 0, csr);

    EXPECT_FALSE(alloc->isAubWritable(std::numeric_limits<uint32_t>::max()));
    EXPECT_FALSE(alloc->isTbxWritable(std::numeric_limits<uint32_t>::max()));
}

TEST_F(StagingBufferManagerTest, givenStagingBufferWhenPerformCopyOnSimulationThenSetWritable) {
    constexpr size_t numOfChunkCopies = 8;
    constexpr size_t remainder = 1024;
    constexpr size_t totalCopySize = stagingBufferSize * numOfChunkCopies + remainder;

    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};
    stagingBufferManager = std::make_unique<StagingBufferManager>(svmAllocsManager.get(), rootDeviceIndices, deviceBitfields, true);

    copyThroughStagingBuffers(totalCopySize, numOfChunkCopies + 1, 1, csr);
    auto svmData = svmAllocsManager->svmAllocs.allocations[0].second.get();
    auto alloc = svmData->gpuAllocations.getDefaultGraphicsAllocation();
    alloc->setAubWritable(false, std::numeric_limits<uint32_t>::max());
    alloc->setTbxWritable(false, std::numeric_limits<uint32_t>::max());
    copyThroughStagingBuffers(totalCopySize, numOfChunkCopies + 1, 0, csr);

    EXPECT_TRUE(alloc->isAubWritable(std::numeric_limits<uint32_t>::max()));
    EXPECT_TRUE(alloc->isTbxWritable(std::numeric_limits<uint32_t>::max()));
}

TEST_F(StagingBufferManagerTest, givenStagingBufferWhenPerformCopyThenCopyData) {
    constexpr size_t numOfChunkCopies = 8;
    constexpr size_t remainder = 1024;
    constexpr size_t totalCopySize = stagingBufferSize * numOfChunkCopies + remainder;
    copyThroughStagingBuffers(totalCopySize, numOfChunkCopies + 1, 1, csr);
}

TEST_F(StagingBufferManagerTest, givenStagingBufferWhenPerformCopyWithoutRemainderThenNoRemainderCalled) {
    constexpr size_t numOfChunkCopies = 8;
    constexpr size_t totalCopySize = stagingBufferSize * numOfChunkCopies;
    copyThroughStagingBuffers(totalCopySize, numOfChunkCopies, 1, csr);
}

TEST_F(StagingBufferManagerTest, givenStagingBufferWhenTaskCountNotReadyThenDontReuseBuffers) {
    constexpr size_t numOfChunkCopies = 8;
    constexpr size_t totalCopySize = stagingBufferSize * numOfChunkCopies;

    *csr->getTagAddress() = csr->peekTaskCount();
    copyThroughStagingBuffers(totalCopySize, numOfChunkCopies, 8, csr);
}

TEST_F(StagingBufferManagerTest, givenStagingBufferWhenTaskCountNotReadyButSmallTransfersThenReuseBuffer) {
    constexpr size_t numOfChunkCopies = 1;
    constexpr size_t totalCopySize = MemoryConstants::pageSize;
    constexpr size_t availableTransfersWithinBuffer = stagingBufferSize / totalCopySize;
    *csr->getTagAddress() = csr->peekTaskCount();
    copyThroughStagingBuffers(totalCopySize, numOfChunkCopies, 1, csr);
    for (auto i = 1u; i < availableTransfersWithinBuffer; i++) {
        copyThroughStagingBuffers(totalCopySize, numOfChunkCopies, 0, csr);
    }
    copyThroughStagingBuffers(totalCopySize, numOfChunkCopies, 1, csr);
}

TEST_F(StagingBufferManagerTest, givenStagingBufferWhenUpdatedTaskCountThenReuseBuffers) {
    constexpr size_t numOfChunkCopies = 8;
    constexpr size_t totalCopySize = stagingBufferSize * numOfChunkCopies;

    *csr->getTagAddress() = csr->peekTaskCount();
    copyThroughStagingBuffers(totalCopySize, numOfChunkCopies, 8, csr);

    *csr->getTagAddress() = csr->peekTaskCount() + numOfChunkCopies;
    copyThroughStagingBuffers(totalCopySize, numOfChunkCopies, 0, csr);
    EXPECT_EQ(numOfChunkCopies, svmAllocsManager->svmAllocs.getNumAllocs());
}

TEST_F(StagingBufferManagerTest, givenStagingBufferWhenPerformCopyOnDifferentCsrThenReuseStagingBuffers) {
    constexpr size_t numOfChunkCopies = 8;
    constexpr size_t remainder = 1024;
    constexpr size_t totalCopySize = stagingBufferSize * numOfChunkCopies + remainder;
    copyThroughStagingBuffers(totalCopySize, numOfChunkCopies + 1, 1, csr);
    copyThroughStagingBuffers(totalCopySize, numOfChunkCopies + 1, 0, pDevice->commandStreamReceivers[1].get());
}

TEST_F(StagingBufferManagerTest, givenStagingBufferWhenFailedChunkCopyThenEarlyReturnWithFailure) {
    constexpr size_t numOfChunkCopies = 8;
    constexpr size_t remainder = 1024;
    constexpr size_t totalCopySize = stagingBufferSize * numOfChunkCopies + remainder;
    constexpr int expectedErrorCode = 1;
    auto usmBuffer = allocateDeviceBuffer(totalCopySize);
    auto nonUsmBuffer = new unsigned char[totalCopySize];

    size_t chunkCounter = 0;
    memset(usmBuffer, 0, totalCopySize);
    memset(nonUsmBuffer, 0xFF, totalCopySize);

    ChunkCopyFunction chunkCopy = [&](void *chunkSrc, void *chunkDst, size_t chunkSize) {
        chunkCounter++;
        memcpy(chunkDst, chunkSrc, chunkSize);
        return expectedErrorCode;
    };
    auto initialNumOfUsmAllocations = svmAllocsManager->svmAllocs.getNumAllocs();
    auto ret = stagingBufferManager->performCopy(usmBuffer, nonUsmBuffer, totalCopySize, chunkCopy, csr);
    auto newUsmAllocations = svmAllocsManager->svmAllocs.getNumAllocs() - initialNumOfUsmAllocations;

    EXPECT_EQ(expectedErrorCode, ret.chunkCopyStatus);
    EXPECT_EQ(WaitStatus::ready, ret.waitStatus);
    EXPECT_NE(0, memcmp(usmBuffer, nonUsmBuffer, totalCopySize));
    EXPECT_EQ(1u, chunkCounter);
    EXPECT_EQ(1u, newUsmAllocations);
    svmAllocsManager->freeSVMAlloc(usmBuffer);
    delete[] nonUsmBuffer;
}

TEST_F(StagingBufferManagerTest, givenStagingBufferWhenFailedRemainderCopyThenReturnWithFailure) {
    constexpr size_t numOfChunkCopies = 8;
    constexpr size_t remainder = 1024;
    constexpr size_t totalCopySize = stagingBufferSize * numOfChunkCopies + remainder;
    constexpr int expectedErrorCode = 1;
    auto usmBuffer = allocateDeviceBuffer(totalCopySize);
    auto nonUsmBuffer = new unsigned char[totalCopySize];

    size_t chunkCounter = 0;
    memset(usmBuffer, 0, totalCopySize);
    memset(nonUsmBuffer, 0xFF, totalCopySize);

    ChunkCopyFunction chunkCopy = [&](void *chunkSrc, void *chunkDst, size_t chunkSize) {
        chunkCounter++;
        memcpy(chunkDst, chunkSrc, chunkSize);
        if (chunkCounter <= numOfChunkCopies) {
            return 0;
        } else {
            return expectedErrorCode;
        }
    };
    auto initialNumOfUsmAllocations = svmAllocsManager->svmAllocs.getNumAllocs();
    auto ret = stagingBufferManager->performCopy(usmBuffer, nonUsmBuffer, totalCopySize, chunkCopy, csr);
    auto newUsmAllocations = svmAllocsManager->svmAllocs.getNumAllocs() - initialNumOfUsmAllocations;

    EXPECT_EQ(expectedErrorCode, ret.chunkCopyStatus);
    EXPECT_EQ(WaitStatus::ready, ret.waitStatus);
    EXPECT_EQ(numOfChunkCopies + 1, chunkCounter);
    EXPECT_EQ(1u, newUsmAllocations);
    svmAllocsManager->freeSVMAlloc(usmBuffer);
    delete[] nonUsmBuffer;
}

TEST_F(StagingBufferManagerTest, givenStagingBufferWhenChangedBufferSizeThenPerformCopyWithCorrectNumberOfChunks) {
    constexpr size_t stagingBufferSize = 512;
    constexpr size_t numOfChunkCopies = 8;
    constexpr size_t remainder = 1024;
    constexpr size_t totalCopySize = MemoryConstants::kiloByte * stagingBufferSize * numOfChunkCopies + remainder;
    debugManager.flags.StagingBufferSize.set(stagingBufferSize); // 512KB

    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};
    stagingBufferManager = std::make_unique<StagingBufferManager>(svmAllocsManager.get(), rootDeviceIndices, deviceBitfields, false);
    copyThroughStagingBuffers(totalCopySize, numOfChunkCopies + 1, 1, csr);
}

HWTEST_F(StagingBufferManagerTest, givenStagingBufferWhenDirectSubmissionEnabledThenFlushTagCalled) {
    constexpr size_t numOfChunkCopies = 8;
    constexpr size_t totalCopySize = stagingBufferSize * numOfChunkCopies;
    auto ultCsr = reinterpret_cast<UltCommandStreamReceiver<FamilyType> *>(csr);
    VariableBackup<bool> directSubmissionAvailable{&ultCsr->directSubmissionAvailable, true};
    ultCsr->callFlushTagUpdate = false;

    auto usmBuffer = allocateDeviceBuffer(totalCopySize);
    auto nonUsmBuffer = new unsigned char[totalCopySize];

    size_t flushTagsCalled = 0;
    ChunkCopyFunction chunkCopy = [&](void *chunkSrc, void *chunkDst, size_t chunkSize) {
        if (ultCsr->flushTagUpdateCalled) {
            flushTagsCalled++;
            ultCsr->flushTagUpdateCalled = false;
        }
        reinterpret_cast<MockCommandStreamReceiver *>(csr)->taskCount++;
        return 0;
    };
    stagingBufferManager->performCopy(usmBuffer, nonUsmBuffer, totalCopySize, chunkCopy, csr);
    if (ultCsr->flushTagUpdateCalled) {
        flushTagsCalled++;
    }

    EXPECT_EQ(flushTagsCalled, numOfChunkCopies);
    svmAllocsManager->freeSVMAlloc(usmBuffer);
    delete[] nonUsmBuffer;
}

HWTEST_F(StagingBufferManagerTest, givenFailedAllocationWhenRequestStagingBufferCalledThenReturnNullptr) {
    size_t size = MemoryConstants::pageSize2M;
    auto memoryManager = static_cast<MockMemoryManager *>(pDevice->getMemoryManager());
    memoryManager->isMockHostMemoryManager = true;
    memoryManager->forceFailureInPrimaryAllocation = true;
    auto [heapAllocator, stagingBuffer] = stagingBufferManager->requestStagingBuffer(size);
    EXPECT_EQ(stagingBuffer, 0u);
}

TEST_F(StagingBufferManagerTest, givenStagingBufferWhenPerformImageWriteThenWholeRegionCovered) {
    size_t expectedChunks = 8;
    const size_t globalOrigin[3] = {0, 0, 0};
    const size_t globalRegion[3] = {4, expectedChunks, 1};
    imageTransferThroughStagingBuffers(false, stagingBufferSize, globalOrigin, globalRegion, expectedChunks);
}

TEST_F(StagingBufferManagerTest, givenStagingBufferWhenPerformImageWriteWithOriginThenWholeRegionCovered) {
    size_t expectedChunks = 8;
    const size_t globalOrigin[3] = {4, 4, 0};
    const size_t globalRegion[3] = {4, expectedChunks, 1};
    imageTransferThroughStagingBuffers(false, stagingBufferSize, globalOrigin, globalRegion, expectedChunks);
}

TEST_F(StagingBufferManagerTest, givenStagingBufferWhenPerformImageWriteWithMultipleRowsPerChunkThenWholeRegionCovered) {
    size_t expectedChunks = 4;
    const size_t globalOrigin[3] = {0, 0, 0};
    const size_t globalRegion[3] = {4, 8, 1};
    imageTransferThroughStagingBuffers(false, pitchSize, globalOrigin, globalRegion, expectedChunks);
}

TEST_F(StagingBufferManagerTest, givenStagingBufferWhenPerformImageWriteWithRemainderThenWholeRegionCovered) {
    size_t expectedChunks = 4;
    const size_t globalOrigin[3] = {0, 0, 0};
    const size_t globalRegion[3] = {4, 7, 1};
    imageTransferThroughStagingBuffers(false, pitchSize, globalOrigin, globalRegion, expectedChunks);
}

TEST_F(StagingBufferManagerTest, givenStagingBufferWhenPerformImageReadThenRegionCovered) {
    const size_t globalOrigin[3] = {0, 0, 0};
    const size_t globalRegion[3] = {4, 1, 1};
    imageTransferThroughStagingBuffers(true, 4, globalOrigin, globalRegion, 1);
}

HWTEST_F(StagingBufferManagerTest, givenStagingBufferWhenPerformImageReadThenDownloadAllocationsCalledForAllReadChunks) {
    size_t expectedChunks = 8;
    const size_t globalOrigin[3] = {0, 0, 0};
    const size_t globalRegion[3] = {4, expectedChunks, 1};
    imageTransferThroughStagingBuffers(true, stagingBufferSize, globalOrigin, globalRegion, expectedChunks);
    auto ultCsr = reinterpret_cast<UltCommandStreamReceiver<FamilyType> *>(csr);
    EXPECT_EQ(expectedChunks, ultCsr->downloadAllocationsCalledCount);
    EXPECT_TRUE(ultCsr->latestDownloadAllocationsBlocking);
}

TEST_F(StagingBufferManagerTest, givenStagingBufferWhenPerformImageReadThenWholeRegionCovered) {
    size_t expectedChunks = 8;
    const size_t globalOrigin[3] = {0, 0, 0};
    const size_t globalRegion[3] = {stagingBufferSize, expectedChunks, 1};
    imageTransferThroughStagingBuffers(true, stagingBufferSize, globalOrigin, globalRegion, expectedChunks);
}

TEST_F(StagingBufferManagerTest, givenStagingBufferWhenPerformImageReadWithOriginThenWholeRegionCovered) {
    size_t expectedChunks = 8;
    const size_t globalOrigin[3] = {4, 4, 0};
    const size_t globalRegion[3] = {4, expectedChunks, 1};
    imageTransferThroughStagingBuffers(true, stagingBufferSize, globalOrigin, globalRegion, expectedChunks);
}

TEST_F(StagingBufferManagerTest, givenStagingBufferWhenPerformImageReadWithMultipleRowsPerChunkThenWholeRegionCovered) {
    size_t expectedChunks = 4;
    const size_t globalOrigin[3] = {0, 0, 0};
    const size_t globalRegion[3] = {pitchSize, 8, 1};
    imageTransferThroughStagingBuffers(true, pixelElemSize * globalRegion[0], globalOrigin, globalRegion, expectedChunks);
}

TEST_F(StagingBufferManagerTest, givenStagingBufferWhenPerformImageReadWithRemainderThenWholeRegionCovered) {
    size_t expectedChunks = 4;
    const size_t globalOrigin[3] = {0, 0, 0};
    const size_t globalRegion[3] = {pitchSize, 7, 1};
    imageTransferThroughStagingBuffers(true, pixelElemSize * globalRegion[0], globalOrigin, globalRegion, expectedChunks);
}

TEST_F(StagingBufferManagerTest, givenStagingBufferWhenPerformImageReadWithRemainderAndTransfersWithinLimitThenWholeRegionCovered) {
    size_t expectedChunks = 2;
    const size_t globalOrigin[3] = {0, 0, 0};
    const size_t globalRegion[3] = {pitchSize, 3, 1};
    imageTransferThroughStagingBuffers(true, pixelElemSize * globalRegion[0], globalOrigin, globalRegion, expectedChunks);
}

HWTEST_F(StagingBufferManagerTest, givenStagingBufferWhenGpuHangDuringChunkReadFromImageThenReturnImmediatelyWithFailure) {
    size_t expectedChunks = 4;
    const size_t globalOrigin[3] = {0, 0, 0};
    const size_t globalRegion[3] = {4, 8, 1};
    auto ptr = new unsigned char[stagingBufferSize * expectedChunks];

    size_t chunkCounter = 0;
    ChunkTransferImageFunc chunkWrite = [&](void *stagingBuffer, const size_t *origin, const size_t *region) -> int32_t {
        ++chunkCounter;
        return 0;
    };
    auto ultCsr = reinterpret_cast<UltCommandStreamReceiver<FamilyType> *>(csr);
    ultCsr->waitForTaskCountReturnValue = WaitStatus::gpuHang;
    auto ret = stagingBufferManager->performImageTransfer(ptr, globalOrigin, globalRegion, pitchSize, pitchSize, pixelElemSize, false, chunkWrite, csr, true);
    EXPECT_EQ(0, ret.chunkCopyStatus);
    EXPECT_EQ(WaitStatus::gpuHang, ret.waitStatus);
    EXPECT_EQ(2u, chunkCounter);
    delete[] ptr;
}

HWTEST_F(StagingBufferManagerTest, givenStagingBufferWhenGpuHangAfterChunkReadFromImageThenReturnWithFailure) {
    size_t expectedChunks = 4;
    const size_t globalOrigin[3] = {0, 0, 0};
    const size_t globalRegion[3] = {4, 8, 1};
    auto ptr = new unsigned char[stagingBufferSize * expectedChunks];

    auto ultCsr = reinterpret_cast<UltCommandStreamReceiver<FamilyType> *>(csr);
    size_t chunkCounter = 0;
    ChunkTransferImageFunc chunkWrite = [&](void *stagingBuffer, const size_t *origin, const size_t *region) -> int32_t {
        ++chunkCounter;
        if (chunkCounter == expectedChunks) {
            ultCsr->waitForTaskCountReturnValue = WaitStatus::gpuHang;
        }
        return 0;
    };
    auto ret = stagingBufferManager->performImageTransfer(ptr, globalOrigin, globalRegion, pitchSize, pitchSize, pixelElemSize, false, chunkWrite, csr, true);
    EXPECT_EQ(0, ret.chunkCopyStatus);
    EXPECT_EQ(WaitStatus::gpuHang, ret.waitStatus);
    EXPECT_EQ(4u, chunkCounter);
    delete[] ptr;
}

HWTEST_F(StagingBufferManagerTest, givenStagingBufferWhenGpuHangDuringRemainderChunkReadFromImageThenReturnImmediatelyWithFailure) {
    size_t expectedChunks = 4;
    const size_t globalOrigin[3] = {0, 0, 0};
    const size_t globalRegion[3] = {4, 7, 1};
    auto ptr = new unsigned char[stagingBufferSize * expectedChunks];

    auto ultCsr = reinterpret_cast<UltCommandStreamReceiver<FamilyType> *>(csr);
    size_t chunkCounter = 0;
    size_t remainderCounter = 4;
    ChunkTransferImageFunc chunkWrite = [&](void *stagingBuffer, const size_t *origin, const size_t *region) -> int32_t {
        ++chunkCounter;
        if (chunkCounter == remainderCounter - 1) {
            ultCsr->waitForTaskCountReturnValue = WaitStatus::gpuHang;
        }
        return 0;
    };
    auto ret = stagingBufferManager->performImageTransfer(ptr, globalOrigin, globalRegion, pitchSize, pitchSize, pixelElemSize, false, chunkWrite, csr, true);
    EXPECT_EQ(0, ret.chunkCopyStatus);
    EXPECT_EQ(WaitStatus::gpuHang, ret.waitStatus);
    EXPECT_EQ(remainderCounter - 1, chunkCounter);
    delete[] ptr;
}

TEST_F(StagingBufferManagerTest, givenStagingBufferWhenFailedChunkImageWriteThenEarlyReturnWithFailure) {
    size_t expectedChunks = 4;
    const size_t globalOrigin[3] = {0, 0, 0};
    const size_t globalRegion[3] = {4, 8, 1};
    constexpr int expectedErrorCode = 1;
    auto ptr = new unsigned char[stagingBufferSize * expectedChunks];

    size_t chunkCounter = 0;
    ChunkTransferImageFunc chunkWrite = [&](void *stagingBuffer, const size_t *origin, const size_t *region) -> int32_t {
        ++chunkCounter;
        return expectedErrorCode;
    };
    auto ret = stagingBufferManager->performImageTransfer(ptr, globalOrigin, globalRegion, pitchSize, pitchSize, pixelElemSize, false, chunkWrite, csr, false);
    EXPECT_EQ(expectedErrorCode, ret.chunkCopyStatus);
    EXPECT_EQ(WaitStatus::ready, ret.waitStatus);
    EXPECT_EQ(1u, chunkCounter);
    delete[] ptr;
}

TEST_F(StagingBufferManagerTest, givenStagingBufferWhenFailedChunkImageWriteWithRemainderThenReturnWithFailure) {
    size_t expectedChunks = 4;
    const size_t globalOrigin[3] = {0, 0, 0};
    const size_t globalRegion[3] = {4, 7, 1};
    constexpr int expectedErrorCode = 1;
    auto ptr = new unsigned char[stagingBufferSize * expectedChunks];

    size_t chunkCounter = 0;
    size_t remainderCounter = 4;
    ChunkTransferImageFunc chunkWrite = [&](void *stagingBuffer, const size_t *origin, const size_t *region) -> int32_t {
        ++chunkCounter;
        if (chunkCounter == remainderCounter) {
            return expectedErrorCode;
        }
        return 0;
    };
    auto ret = stagingBufferManager->performImageTransfer(ptr, globalOrigin, globalRegion, pitchSize, pitchSize, pixelElemSize, false, chunkWrite, csr, false);
    EXPECT_EQ(expectedErrorCode, ret.chunkCopyStatus);
    EXPECT_EQ(WaitStatus::ready, ret.waitStatus);
    EXPECT_EQ(remainderCounter, chunkCounter);
    delete[] ptr;
}

struct Image3DTestInfo {
    size_t expectedChunks;
    size_t slicePitch;
    size_t slices;
};
class StagingBufferManager3DImageTest : public StagingBufferManagerTest,
                                        public ::testing::WithParamInterface<Image3DTestInfo> {};

HWTEST_P(StagingBufferManager3DImageTest, givenStagingBufferWhenPerformImageTransferCalledWith3DImageThenSplitCorrectly) {
    size_t expectedChunks = GetParam().expectedChunks;
    auto rowPitch = 4u;
    auto rowsNum = 4u;
    auto slicePitch = GetParam().slicePitch;
    const size_t globalOrigin[3] = {0, 0, 0};
    const size_t globalRegion[3] = {rowPitch, rowsNum, GetParam().slices};
    auto size = stagingBufferSize * expectedChunks / sizeof(unsigned int);
    auto ptr = new unsigned int[size];
    fillUserData(ptr, size);

    size_t chunkCounter = 0;
    ChunkTransferImageFunc chunkWrite = [&](void *stagingBuffer, const size_t *origin, const size_t *region) -> int32_t {
        // Verify that staging buffer contains correct data based on origin offset.
        auto offset = origin[0] + origin[1] * rowPitch + origin[2] * slicePitch;
        auto userPtr = ptr + (offset / sizeof(uint32_t));
        EXPECT_EQ(0, memcmp(userPtr, stagingBuffer, region[0] * region[1] * region[2]));

        ++chunkCounter;
        return 0;
    };
    auto ret = stagingBufferManager->performImageTransfer(ptr, globalOrigin, globalRegion, rowPitch, slicePitch, pixelElemSize, false, chunkWrite, csr, false);
    EXPECT_EQ(0, ret.chunkCopyStatus);
    EXPECT_EQ(WaitStatus::ready, ret.waitStatus);
    EXPECT_EQ(expectedChunks, chunkCounter);
    delete[] ptr;
}

Image3DTestInfo imageTestsInfo[] = {
    {8u, StagingBufferManagerFixture::stagingBufferSize, 8},     // (4, 4, 8) split into (4, 4, 1) * 8
    {4u, StagingBufferManagerFixture::stagingBufferSize / 2, 8}, // (4, 4, 8) split into (4, 4, 2) * 4
    {5u, StagingBufferManagerFixture::stagingBufferSize / 2, 9}, // (4, 4, 9) split into (4, 4, 2) * 4 + (4, 4, 1)
};

INSTANTIATE_TEST_SUITE_P(
    StagingBufferManagerTest_,
    StagingBufferManager3DImageTest,
    testing::ValuesIn(imageTestsInfo));

TEST_F(StagingBufferManager3DImageTest, givenStagingBufferWhenPerformImageTransferCalledWith3DImageAndSlicePitchThenCopyDataCorrectly) {
    auto rowPitch = 16u;
    auto slicePitch = 256u;
    const size_t globalOrigin[3] = {0, 0, 0};
    const size_t globalRegion[3] = {16u, 4u, 4u};
    auto size = stagingBufferSize;
    auto userDst = new char[size];
    auto imageData = new char[size];
    memset(userDst, 0, size);
    memset(imageData, 0xFF, size);

    ChunkTransferImageFunc chunkWrite = [&](void *stagingBuffer, const size_t *origin, const size_t *region) -> int32_t {
        for (auto sliceId = 0u; sliceId < 4u; sliceId++) {
            auto offset = (sliceId * slicePitch);
            memcpy(ptrOffset(stagingBuffer, offset), imageData + offset, region[0] * region[1]);
        }
        return 0;
    };
    auto ret = stagingBufferManager->performImageTransfer(userDst, globalOrigin, globalRegion, rowPitch, slicePitch, pixelElemSize, false, chunkWrite, csr, true);
    EXPECT_EQ(0, ret.chunkCopyStatus);
    EXPECT_EQ(WaitStatus::ready, ret.waitStatus);

    for (auto sliceId = 0u; sliceId < 4u; sliceId++) {
        auto sliceOffset = (sliceId * slicePitch);
        EXPECT_EQ(0, memcmp(userDst + sliceOffset, imageData + sliceOffset, rowPitch * globalRegion[1]));
    }
    EXPECT_NE(0, memcmp(userDst, imageData, stagingBufferSize));

    delete[] userDst;
    delete[] imageData;
}

HWTEST_F(StagingBufferManagerTest, givenStagingBufferWhenGpuHangDuringSliceRemainderChunkReadFromImageThenReturnImmediatelyWithFailure) {
    auto expectedChunks = 5u;
    size_t rowPitch = 4u;
    auto rowsNum = 4u;
    size_t slicePitch = pitchSize;
    const size_t globalOrigin[3] = {0, 0, 0};
    const size_t globalRegion[3] = {rowPitch, rowsNum, 9};
    auto size = stagingBufferSize * expectedChunks / sizeof(unsigned int);
    auto ptr = new unsigned int[size];

    auto ultCsr = reinterpret_cast<UltCommandStreamReceiver<FamilyType> *>(csr);
    size_t chunkCounter = 0;
    ChunkTransferImageFunc chunkWrite = [&](void *stagingBuffer, const size_t *origin, const size_t *region) -> int32_t {
        ++chunkCounter;
        if (chunkCounter == expectedChunks - 1) {
            ultCsr->waitForTaskCountReturnValue = WaitStatus::gpuHang;
        }
        return 0;
    };
    auto ret = stagingBufferManager->performImageTransfer(ptr, globalOrigin, globalRegion, rowPitch, slicePitch, pixelElemSize, false, chunkWrite, csr, true);
    EXPECT_EQ(0, ret.chunkCopyStatus);
    EXPECT_EQ(WaitStatus::gpuHang, ret.waitStatus);
    EXPECT_EQ(expectedChunks - 1, chunkCounter);
    delete[] ptr;
}

TEST_F(StagingBufferManagerTest, givenStagingBufferWhenFailedChunkImageWriteWithSliceRemainderThenReturnWithFailure) {
    auto expectedChunks = 5u;
    size_t rowPitch = 4u;
    auto rowsNum = 4u;
    size_t slicePitch = pitchSize;
    const size_t globalOrigin[3] = {0, 0, 0};
    const size_t globalRegion[3] = {rowPitch, rowsNum, 9};
    auto size = stagingBufferSize * expectedChunks / sizeof(unsigned int);
    auto ptr = new unsigned int[size];

    size_t chunkCounter = 0;
    constexpr int expectedErrorCode = 1;
    ChunkTransferImageFunc chunkWrite = [&](void *stagingBuffer, const size_t *origin, const size_t *region) -> int32_t {
        ++chunkCounter;
        if (chunkCounter == expectedChunks) {
            return expectedErrorCode;
        }
        return 0;
    };
    auto ret = stagingBufferManager->performImageTransfer(ptr, globalOrigin, globalRegion, rowPitch, slicePitch, pixelElemSize, false, chunkWrite, csr, false);
    EXPECT_EQ(expectedErrorCode, ret.chunkCopyStatus);
    EXPECT_EQ(WaitStatus::ready, ret.waitStatus);
    EXPECT_EQ(expectedChunks, chunkCounter);
    delete[] ptr;
}

struct ImageMipMapTestInfo {
    size_t region[3];
    size_t expectedMipMapIndex;
};
class StagingBufferManagerImageMipMapTest : public StagingBufferManagerTest,
                                            public ::testing::WithParamInterface<ImageMipMapTestInfo> {};

TEST_P(StagingBufferManagerImageMipMapTest, givenStagingBufferWhenPerformImageTransferWithMipMappedImageThenOriginsSetCorrectly) {
    constexpr auto mipMapLevel = 10u;
    size_t globalOrigin[4] = {};
    auto region = GetParam().region;
    auto expectedMipMapIndex = GetParam().expectedMipMapIndex;
    globalOrigin[expectedMipMapIndex] = mipMapLevel;
    unsigned int ptr[256];

    ChunkTransferImageFunc chunkWrite = [&](void *stagingBuffer, const size_t *origin, const size_t *region) -> int32_t {
        // Verify that mip map level was forwarded to passed lambda function at correct origin index.
        EXPECT_EQ(mipMapLevel, origin[expectedMipMapIndex]);
        return 0;
    };
    auto ret = stagingBufferManager->performImageTransfer(ptr, globalOrigin, region, region[0], region[0] * region[1], pixelElemSize, true, chunkWrite, csr, false);
    EXPECT_EQ(0, ret.chunkCopyStatus);
    EXPECT_EQ(WaitStatus::ready, ret.waitStatus);
}

ImageMipMapTestInfo imageMipMapTestsInfo[] = {
    {{4u, 1u, 1u}, 1},
    {{4u, 4u, 1u}, 2},
    {{4u, 4u, 4u}, 3},
    {{4u, 1u, 1u}, 2},  // 2D image with (4, 1, 1) region
    {{4u, 1u, 1u}, 3}}; // 3D image with (4, 1, 1) region

INSTANTIATE_TEST_SUITE_P(
    StagingBufferManagerTest_,
    StagingBufferManagerImageMipMapTest,
    testing::ValuesIn(imageMipMapTestsInfo));

TEST_F(StagingBufferManagerTest, givenStagingBufferWhenPerformBufferTransferThenCopyData) {
    constexpr size_t numOfChunkCopies = 8;
    constexpr size_t remainder = 1024;
    constexpr size_t totalCopySize = stagingBufferSize * numOfChunkCopies + remainder;
    bufferTransferThroughStagingBuffers(totalCopySize, numOfChunkCopies + 1, 1, csr);
}

TEST_F(StagingBufferManagerTest, givenStagingBufferWhenPerformBufferTransferWithoutRemainderThenNoRemainderCalled) {
    constexpr size_t numOfChunkCopies = 8;
    constexpr size_t totalCopySize = stagingBufferSize * numOfChunkCopies;
    bufferTransferThroughStagingBuffers(totalCopySize, numOfChunkCopies, 1, csr);
}

TEST_F(StagingBufferManagerTest, givenStagingBufferWhenFailedChunkBufferWriteThenEarlyReturnWithFailure) {
    size_t expectedChunks = 4;
    constexpr int expectedErrorCode = 1;
    auto ptr = new unsigned char[stagingBufferSize * expectedChunks];

    size_t chunkCounter = 0;
    ChunkTransferBufferFunc chunkWrite = [&](void *stagingBuffer, size_t offset, size_t size) -> int32_t {
        ++chunkCounter;
        return expectedErrorCode;
    };
    auto ret = stagingBufferManager->performBufferTransfer(ptr, 0, stagingBufferSize * expectedChunks, chunkWrite, csr, false);
    EXPECT_EQ(expectedErrorCode, ret.chunkCopyStatus);
    EXPECT_EQ(WaitStatus::ready, ret.waitStatus);
    EXPECT_EQ(1u, chunkCounter);
    delete[] ptr;
}

TEST_F(StagingBufferManagerTest, givenStagingBufferWhenReadBufferThenDataCopiedCorrectly) {
    size_t expectedChunks = 4;
    auto ptr = new unsigned char[stagingBufferSize * expectedChunks];
    auto bufferData = new unsigned char[stagingBufferSize * expectedChunks];
    memset(ptr, 0, stagingBufferSize * expectedChunks);
    memset(bufferData, 0xFF, stagingBufferSize * expectedChunks);

    ChunkTransferBufferFunc chunkWrite = [&](void *stagingBuffer, size_t offset, size_t size) -> int32_t {
        memcpy(stagingBuffer, bufferData + offset, size);
        return 0;
    };
    auto ret = stagingBufferManager->performBufferTransfer(ptr, 0, stagingBufferSize * expectedChunks, chunkWrite, csr, true);
    EXPECT_EQ(0, ret.chunkCopyStatus);
    EXPECT_EQ(WaitStatus::ready, ret.waitStatus);
    EXPECT_EQ(0, memcmp(ptr, bufferData, stagingBufferSize * expectedChunks));

    delete[] ptr;
    delete[] bufferData;
}

TEST_F(StagingBufferManagerTest, givenStagingBufferWhenFailedChunkBufferWriteWithRemainderThenReturnWithFailure) {
    size_t expectedChunks = 2;
    constexpr int expectedErrorCode = 1;
    auto ptr = new unsigned char[stagingBufferSize * expectedChunks + 512];

    size_t chunkCounter = 0;
    size_t remainderCounter = expectedChunks + 1;
    ChunkTransferBufferFunc chunkWrite = [&](void *stagingBuffer, size_t offset, size_t size) -> int32_t {
        ++chunkCounter;
        if (chunkCounter == remainderCounter) {
            return expectedErrorCode;
        }
        return 0;
    };
    auto ret = stagingBufferManager->performBufferTransfer(ptr, 0, stagingBufferSize * expectedChunks + 512, chunkWrite, csr, false);
    EXPECT_EQ(expectedErrorCode, ret.chunkCopyStatus);
    EXPECT_EQ(WaitStatus::ready, ret.waitStatus);
    EXPECT_EQ(remainderCounter, chunkCounter);
    delete[] ptr;
}