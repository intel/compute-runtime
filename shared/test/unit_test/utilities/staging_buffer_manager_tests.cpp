/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/staging_buffer_manager.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_svm_manager.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/common/test_macros/test_checks_shared.h"

#include "gtest/gtest.h"

using namespace NEO;

class StagingBufferManagerFixture : public DeviceFixture {
  public:
    void setUp() {
        DeviceFixture::setUp();
        REQUIRE_SVM_OR_SKIP(&hardwareInfo);
        this->svmAllocsManager = std::make_unique<MockSVMAllocsManager>(pDevice->getMemoryManager(), false);
        debugManager.flags.EnableCopyWithStagingBuffers.set(1);
        RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
        std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};
        this->stagingBufferManager = std::make_unique<StagingBufferManager>(svmAllocsManager.get(), rootDeviceIndices, deviceBitfields);
        this->csr = pDevice->commandStreamReceivers[0].get();
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

    void copyThroughStagingBuffers(size_t copySize, size_t expectedChunks, size_t expectedAllocations) {
        auto usmBuffer = allocateDeviceBuffer(copySize);
        auto nonUsmBuffer = new unsigned char[copySize];

        size_t chunkCounter = 0;
        memset(usmBuffer, 0, copySize);
        memset(nonUsmBuffer, 0xFF, copySize);

        auto chunkCopy = [&](void *chunkDst, void *stagingBuffer, const void *chunkSrc, size_t chunkSize) {
            chunkCounter++;
            memcpy(stagingBuffer, chunkSrc, chunkSize);
            memcpy(chunkDst, stagingBuffer, chunkSize);
            return 0;
        };
        auto initialNumOfUsmAllocations = svmAllocsManager->svmAllocs.getNumAllocs();
        auto ret = stagingBufferManager->performCopy(usmBuffer, nonUsmBuffer, copySize, chunkCopy, csr);
        auto newUsmAllocations = svmAllocsManager->svmAllocs.getNumAllocs() - initialNumOfUsmAllocations;

        EXPECT_EQ(0, ret);
        EXPECT_EQ(0, memcmp(usmBuffer, nonUsmBuffer, copySize));
        EXPECT_EQ(expectedChunks, chunkCounter);
        EXPECT_EQ(expectedAllocations, newUsmAllocations);
        svmAllocsManager->freeSVMAlloc(usmBuffer);
        delete[] nonUsmBuffer;
    }

    constexpr static size_t stagingBufferSize = MemoryConstants::megaByte * 2;
    DebugManagerStateRestore restorer;
    std::unique_ptr<MockSVMAllocsManager> svmAllocsManager;
    std::unique_ptr<StagingBufferManager> stagingBufferManager;
    CommandStreamReceiver *csr;
};

using StagingBufferManagerTest = Test<StagingBufferManagerFixture>;

TEST_F(StagingBufferManagerTest, givenStagingBufferEnabledWhenValidForCopyThenReturnTrue) {
    constexpr size_t bufferSize = 1024;
    auto usmBuffer = allocateDeviceBuffer(bufferSize);
    unsigned char nonUsmBuffer[bufferSize];

    struct {
        void *dstPtr;
        void *srcPtr;
        bool hasDependencies;
        bool expectValid;
    } copyParamsStruct[5]{
        {usmBuffer, nonUsmBuffer, false, true},     // nonUsm -> usm without dependencies
        {usmBuffer, nonUsmBuffer, true, false},     // nonUsm -> usm with dependencies
        {nonUsmBuffer, nonUsmBuffer, false, false}, // nonUsm -> nonUsm without dependencies
        {usmBuffer, usmBuffer, false, false},       // usm -> usm without dependencies
        {nonUsmBuffer, usmBuffer, false, false}     // usm -> nonUsm without dependencies
    };
    for (auto i = 0; i < 5; i++) {
        auto actualValid = stagingBufferManager->isValidForCopy(*pDevice, copyParamsStruct[i].dstPtr, copyParamsStruct[i].srcPtr, copyParamsStruct[i].hasDependencies);
        EXPECT_EQ(actualValid, copyParamsStruct[i].expectValid);
    }

    debugManager.flags.EnableCopyWithStagingBuffers.set(0);
    EXPECT_FALSE(stagingBufferManager->isValidForCopy(*pDevice, usmBuffer, nonUsmBuffer, false));

    debugManager.flags.EnableCopyWithStagingBuffers.set(-1);
    auto isStaingBuffersEnabled = pDevice->getProductHelper().isStagingBuffersEnabled();
    EXPECT_EQ(isStaingBuffersEnabled, stagingBufferManager->isValidForCopy(*pDevice, usmBuffer, nonUsmBuffer, false));
    svmAllocsManager->freeSVMAlloc(usmBuffer);
}

TEST_F(StagingBufferManagerTest, givenStagingBufferWhenPerformCopyThenCopyData) {
    constexpr size_t numOfChunkCopies = 8;
    constexpr size_t remainder = 1024;
    constexpr size_t totalCopySize = stagingBufferSize * numOfChunkCopies + remainder;
    copyThroughStagingBuffers(totalCopySize, numOfChunkCopies + 1, 1);
}

TEST_F(StagingBufferManagerTest, givenStagingBufferWhenPerformCopyWithoutRemainderThenNoRemainderCalled) {
    constexpr size_t numOfChunkCopies = 8;
    constexpr size_t totalCopySize = stagingBufferSize * numOfChunkCopies;
    copyThroughStagingBuffers(totalCopySize, numOfChunkCopies, 1);
}

TEST_F(StagingBufferManagerTest, givenStagingBufferWhenTaskCountNotReadyThenDontReuseBuffers) {
    constexpr size_t numOfChunkCopies = 8;
    constexpr size_t totalCopySize = stagingBufferSize * numOfChunkCopies;

    *csr->getTagAddress() = csr->peekTaskCount();
    copyThroughStagingBuffers(totalCopySize, numOfChunkCopies, 8);
}

TEST_F(StagingBufferManagerTest, givenStagingBufferWhenUpdatedTaskCountThenReuseBuffers) {
    constexpr size_t numOfChunkCopies = 8;
    constexpr size_t totalCopySize = stagingBufferSize * numOfChunkCopies;

    *csr->getTagAddress() = csr->peekTaskCount();
    copyThroughStagingBuffers(totalCopySize, numOfChunkCopies, 8);

    *csr->getTagAddress() = csr->peekTaskCount() + numOfChunkCopies;
    copyThroughStagingBuffers(totalCopySize, numOfChunkCopies, 0);
    EXPECT_EQ(numOfChunkCopies, svmAllocsManager->svmAllocs.getNumAllocs());
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

    auto chunkCopy = [&](void *chunkDst, void *stagingBuffer, const void *chunkSrc, size_t chunkSize) {
        chunkCounter++;
        memcpy(stagingBuffer, chunkSrc, chunkSize);
        memcpy(chunkDst, stagingBuffer, chunkSize);
        return expectedErrorCode;
    };
    auto initialNumOfUsmAllocations = svmAllocsManager->svmAllocs.getNumAllocs();
    auto ret = stagingBufferManager->performCopy(usmBuffer, nonUsmBuffer, totalCopySize, chunkCopy, csr);
    auto newUsmAllocations = svmAllocsManager->svmAllocs.getNumAllocs() - initialNumOfUsmAllocations;

    EXPECT_EQ(expectedErrorCode, ret);
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

    auto chunkCopy = [&](void *chunkDst, void *stagingBuffer, const void *chunkSrc, size_t chunkSize) {
        chunkCounter++;
        memcpy(stagingBuffer, chunkSrc, chunkSize);
        memcpy(chunkDst, stagingBuffer, chunkSize);
        if (chunkCounter <= numOfChunkCopies) {
            return 0;
        } else {
            return expectedErrorCode;
        }
    };
    auto initialNumOfUsmAllocations = svmAllocsManager->svmAllocs.getNumAllocs();
    auto ret = stagingBufferManager->performCopy(usmBuffer, nonUsmBuffer, totalCopySize, chunkCopy, csr);
    auto newUsmAllocations = svmAllocsManager->svmAllocs.getNumAllocs() - initialNumOfUsmAllocations;

    EXPECT_EQ(expectedErrorCode, ret);
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
    stagingBufferManager = std::make_unique<StagingBufferManager>(svmAllocsManager.get(), rootDeviceIndices, deviceBitfields);
    copyThroughStagingBuffers(totalCopySize, numOfChunkCopies + 1, 1);
}