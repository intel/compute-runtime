/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/aub_mem_dump/page_table_entry_bits.h"
#include "runtime/command_stream/aub_command_stream_receiver_hw.h"
#include "test.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/mocks/mock_aub_csr.h"
#include "unit_tests/mocks/mock_aub_file_stream.h"
#include "driver_version.h"

#include <fstream>
#include <memory>

using namespace OCLRT;

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

typedef Test<DeviceFixture> AubFileStreamTests;

HWTEST_F(AubFileStreamTests, givenAubCommandStreamReceiverWhenInitFileIsCalledWithInvalidFileNameThenFileIsNotOpened) {
    auto aubCsr = std::make_unique<AUBCommandStreamReceiverHw<FamilyType>>(**platformDevices, "", true, *pDevice->executionEnvironment);
    std::string invalidFileName = "";

    aubCsr->initFile(invalidFileName);
    EXPECT_FALSE(aubCsr->isFileOpen());
}

HWTEST_F(AubFileStreamTests, givenAubCommandStreamReceiverWhenInitFileIsCalledThenFileIsOpenedAndFileNameIsStored) {
    auto aubCsr = std::make_unique<AUBCommandStreamReceiverHw<FamilyType>>(**platformDevices, "", true, *pDevice->executionEnvironment);
    std::string fileName = "file_name.aub";

    aubCsr->initFile(fileName);
    EXPECT_TRUE(aubCsr->isFileOpen());
    EXPECT_STREQ(fileName.c_str(), aubCsr->getFileName().c_str());

    aubCsr->closeFile();
    EXPECT_FALSE(aubCsr->isFileOpen());
    EXPECT_TRUE(aubCsr->getFileName().empty());
}

HWTEST_F(AubFileStreamTests, givenAubCommandStreamReceiverWhenReopenFileIsCalledThenFileWithSpecifiedNameIsReopened) {
    auto aubCsr = std::make_unique<AUBCommandStreamReceiverHw<FamilyType>>(**platformDevices, "", true, *pDevice->executionEnvironment);
    std::string fileName = "file_name.aub";
    std::string newFileName = "new_file_name.aub";

    aubCsr->reopenFile(fileName);
    EXPECT_TRUE(aubCsr->isFileOpen());
    EXPECT_STREQ(fileName.c_str(), aubCsr->getFileName().c_str());

    aubCsr->reopenFile(newFileName);
    EXPECT_TRUE(aubCsr->isFileOpen());
    EXPECT_STREQ(newFileName.c_str(), aubCsr->getFileName().c_str());
}

HWTEST_F(AubFileStreamTests, givenAubCommandStreamReceiverWhenInitFileIsCalledThenFileShouldBeInitializedWithHeaderOnce) {
    auto aubCsr = std::make_unique<AUBCommandStreamReceiverHw<FamilyType>>(**platformDevices, "", true, *pDevice->executionEnvironment);
    std::string fileName = "file_name.aub";

    std::unique_ptr<AUBCommandStreamReceiver::AubFileStream> mockAubFileStream(new MockAubFileStream());
    MockAubFileStream *mockAubFileStreamPtr = static_cast<MockAubFileStream *>(mockAubFileStream.get());
    ASSERT_NE(nullptr, mockAubFileStreamPtr);
    aubCsr->stream = mockAubFileStreamPtr;

    aubCsr->initFile(fileName);
    aubCsr->initFile(fileName);

    EXPECT_EQ(1u, mockAubFileStreamPtr->initCalledCnt);
}

HWTEST_F(AubFileStreamTests, givenAubCommandStreamReceiverWhenOpenFileIsCalledThenFileStreamShouldBeLocked) {
    auto aubCsr = std::make_unique<AUBCommandStreamReceiverHw<FamilyType>>(**platformDevices, "", true, *pDevice->executionEnvironment);
    std::string fileName = "file_name.aub";

    std::unique_ptr<AUBCommandStreamReceiver::AubFileStream> mockAubFileStream(new MockAubFileStream());
    MockAubFileStream *mockAubFileStreamPtr = static_cast<MockAubFileStream *>(mockAubFileStream.get());
    ASSERT_NE(nullptr, mockAubFileStreamPtr);
    aubCsr->stream = mockAubFileStreamPtr;

    aubCsr->openFile(fileName);
    EXPECT_TRUE(mockAubFileStreamPtr->lockStreamCalled);
}

HWTEST_F(AubFileStreamTests, givenAubCommandStreamReceiverWhenReopenFileIsCalledThenFileStreamShouldBeLocked) {
    auto aubCsr = std::make_unique<AUBCommandStreamReceiverHw<FamilyType>>(**platformDevices, "", true, *pDevice->executionEnvironment);
    std::string fileName = "file_name.aub";

    std::unique_ptr<AUBCommandStreamReceiver::AubFileStream> mockAubFileStream(new MockAubFileStream());
    MockAubFileStream *mockAubFileStreamPtr = static_cast<MockAubFileStream *>(mockAubFileStream.get());
    ASSERT_NE(nullptr, mockAubFileStreamPtr);
    aubCsr->stream = mockAubFileStreamPtr;

    aubCsr->reopenFile(fileName);
    EXPECT_TRUE(mockAubFileStreamPtr->lockStreamCalled);
}

HWTEST_F(AubFileStreamTests, givenAubCommandStreamReceiverWhenFlushIsCalledThenFileStreamShouldBeLocked) {
    auto aubExecutionEnvironment = getEnvironment<AUBCommandStreamReceiverHw<FamilyType>>(true, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<AUBCommandStreamReceiverHw<FamilyType>>();
    LinearStream cs(aubExecutionEnvironment->commandBuffer);

    std::unique_ptr<AUBCommandStreamReceiver::AubFileStream> mockAubFileStream(new MockAubFileStream());
    MockAubFileStream *mockAubFileStreamPtr = static_cast<MockAubFileStream *>(mockAubFileStream.get());
    ASSERT_NE(nullptr, mockAubFileStreamPtr);
    aubCsr->stream = mockAubFileStreamPtr;

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    auto engineType = OCLRT::ENGINE_RCS;
    ResidencyContainer allocationsForResidency = {};

    aubCsr->flush(batchBuffer, engineType, allocationsForResidency, *pDevice->getOsContext());
    EXPECT_TRUE(mockAubFileStreamPtr->lockStreamCalled);
}

HWTEST_F(AubFileStreamTests, givenAubCommandStreamReceiverWhenFlushIsCalledThenItShouldCallPollForCompletion) {
    auto aubExecutionEnvironment = getEnvironment<MockAubCsr<FamilyType>>(false, true, false);
    auto aubCsr = aubExecutionEnvironment->template getCsr<MockAubCsr<FamilyType>>();
    LinearStream cs(aubExecutionEnvironment->commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    auto engineType = OCLRT::ENGINE_RCS;
    ResidencyContainer allocationsForResidency = {};

    aubCsr->flush(batchBuffer, engineType, allocationsForResidency, *pDevice->getOsContext());
    EXPECT_TRUE(aubCsr->pollForCompletionCalled);
}

HWTEST_F(AubFileStreamTests, givenAubCommandStreamReceiverWhenFlushIsCalledThenFileStreamShouldBeFlushed) {
    auto aubExecutionEnvironment = getEnvironment<AUBCommandStreamReceiverHw<FamilyType>>(true, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<AUBCommandStreamReceiverHw<FamilyType>>();
    LinearStream cs(aubExecutionEnvironment->commandBuffer);

    std::unique_ptr<AUBCommandStreamReceiver::AubFileStream> mockAubFileStream(new MockAubFileStream());
    MockAubFileStream *mockAubFileStreamPtr = static_cast<MockAubFileStream *>(mockAubFileStream.get());
    ASSERT_NE(nullptr, mockAubFileStreamPtr);
    aubCsr->stream = mockAubFileStreamPtr;

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    auto engineType = OCLRT::ENGINE_RCS;
    ResidencyContainer allocationsForResidency = {};

    aubCsr->flush(batchBuffer, engineType, allocationsForResidency, *pDevice->getOsContext());
    EXPECT_TRUE(mockAubFileStreamPtr->flushCalled);
}

HWTEST_F(AubFileStreamTests, givenAubCommandStreamReceiverWhenExpectMemoryIsCalledThenPageWalkIsCallingStreamsExpectMemory) {
    auto aubCsr = std::make_unique<AUBCommandStreamReceiverHw<FamilyType>>(**platformDevices, "", true, *pDevice->executionEnvironment);

    std::unique_ptr<AUBCommandStreamReceiver::AubFileStream> mockAubFileStream(std::make_unique<MockAubFileStream>());
    MockAubFileStream *mockAubFileStreamPtr = static_cast<MockAubFileStream *>(mockAubFileStream.get());
    ASSERT_NE(nullptr, mockAubFileStreamPtr);
    aubCsr->stream = mockAubFileStreamPtr;

    uintptr_t gpuAddress = 0x30000;
    void *sourceAddress = reinterpret_cast<void *>(0x50000);
    auto physicalAddress = aubCsr->ppgtt->map(gpuAddress, MemoryConstants::pageSize, PageTableEntry::presentBit, MemoryBanks::MainBank);

    aubCsr->expectMemory(reinterpret_cast<void *>(gpuAddress), sourceAddress, MemoryConstants::pageSize);

    EXPECT_EQ(AubMemDump::AddressSpaceValues::TraceNonlocal, mockAubFileStreamPtr->addressSpaceCapturedFromExpectMemory);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(sourceAddress), mockAubFileStreamPtr->memoryCapturedFromExpectMemory);
    EXPECT_EQ(physicalAddress, mockAubFileStreamPtr->physAddressCapturedFromExpectMemory);
    EXPECT_EQ(MemoryConstants::pageSize, mockAubFileStreamPtr->sizeCapturedFromExpectMemory);
}

HWTEST_F(AubFileStreamTests, givenAubCommandStreamReceiverWhenExpectMMIOIsCalledThenHeaderIsWrittenToFile) {
    std::string fileName = "file_name.aub";
    auto aubCsr = std::make_unique<AUBCommandStreamReceiverHw<FamilyType>>(**platformDevices, fileName.c_str(), true, *pDevice->executionEnvironment);

    std::remove(fileName.c_str());

    std::unique_ptr<AUBCommandStreamReceiver::AubFileStream> mockAubFileStream(std::make_unique<MockAubFileStream>());
    MockAubFileStream *mockAubFileStreamPtr = static_cast<MockAubFileStream *>(mockAubFileStream.get());
    ASSERT_NE(nullptr, mockAubFileStreamPtr);
    aubCsr->stream = mockAubFileStreamPtr;
    aubCsr->initFile(fileName);

    aubCsr->expectMMIO(5, 10);

    aubCsr->getAubStream()->fileHandle.flush();

    std::ifstream aubFile(fileName);
    EXPECT_TRUE(aubFile.is_open());

    if (aubFile.is_open()) {
        AubMemDump::CmdServicesMemTraceRegisterCompare header;
        aubFile.read(reinterpret_cast<char *>(&header), sizeof(AubMemDump::CmdServicesMemTraceRegisterCompare));
        EXPECT_EQ(5u, header.registerOffset);
        EXPECT_EQ(10u, header.data[0]);
        aubFile.close();
    }
}

HWTEST_F(AubFileStreamTests, givenAubCommandStreamReceiverWhenInitializeEngineIsCalledThenMemTraceCommentWithDriverVersionIsPutIntoAubStream) {
    auto aubExecutionEnvironment = getEnvironment<AUBCommandStreamReceiverHw<FamilyType>>(false, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<AUBCommandStreamReceiverHw<FamilyType>>();

    std::unique_ptr<AUBCommandStreamReceiver::AubFileStream> mockAubFileStream(new GmockAubFileStream());
    GmockAubFileStream *mockAubFileStreamPtr = static_cast<GmockAubFileStream *>(mockAubFileStream.get());
    ASSERT_NE(nullptr, mockAubFileStreamPtr);
    aubCsr->stream = mockAubFileStreamPtr;

    std::vector<std::string> comments;

    EXPECT_CALL(*mockAubFileStreamPtr, addComment(_)).WillRepeatedly(::testing::Invoke([&](const char *str) -> bool {
        comments.push_back(std::string(str));
        return true;
    }));
    auto engineIndex = aubCsr->getEngineIndex(OCLRT::ENGINE_RCS);
    aubCsr->initializeEngine(engineIndex);

#define QTR(a) #a
#define TOSTR(b) QTR(b)
    const std::string expectedVersion = TOSTR(NEO_DRIVER_VERSION);
#undef QTR
#undef TOSTR

    std::string commentWithDriverVersion = "driver version: " + expectedVersion;
    EXPECT_EQ(commentWithDriverVersion, comments[0]);
}

HWTEST_F(AubFileStreamTests, givenAddPatchInfoCommentsCalledWhenNoPatchInfoDataObjectsThenCommentsAreEmpty) {
    auto aubExecutionEnvironment = getEnvironment<AUBCommandStreamReceiverHw<FamilyType>>(false, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<AUBCommandStreamReceiverHw<FamilyType>>();
    LinearStream cs(aubExecutionEnvironment->commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 128u, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};

    std::unique_ptr<AUBCommandStreamReceiver::AubFileStream> mockAubFileStream(new GmockAubFileStream());
    GmockAubFileStream *mockAubFileStreamPtr = static_cast<GmockAubFileStream *>(mockAubFileStream.get());
    ASSERT_NE(nullptr, mockAubFileStreamPtr);
    aubCsr->stream = mockAubFileStreamPtr;

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
}

HWTEST_F(AubFileStreamTests, givenAddPatchInfoCommentsCalledWhenFirstAddCommentsFailsThenFunctionReturnsFalse) {
    auto aubExecutionEnvironment = getEnvironment<AUBCommandStreamReceiverHw<FamilyType>>(false, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<AUBCommandStreamReceiverHw<FamilyType>>();
    LinearStream cs(aubExecutionEnvironment->commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 128u, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};

    std::unique_ptr<AUBCommandStreamReceiver::AubFileStream> mockAubFileStream(new GmockAubFileStream());
    GmockAubFileStream *mockAubFileStreamPtr = static_cast<GmockAubFileStream *>(mockAubFileStream.get());
    ASSERT_NE(nullptr, mockAubFileStreamPtr);
    aubCsr->stream = mockAubFileStreamPtr;

    EXPECT_CALL(*mockAubFileStreamPtr, addComment(_)).Times(1).WillOnce(Return(false));
    bool result = aubCsr->addPatchInfoComments();
    EXPECT_FALSE(result);
}

HWTEST_F(AubFileStreamTests, givenAddPatchInfoCommentsCalledWhenSecondAddCommentsFailsThenFunctionReturnsFalse) {
    auto aubExecutionEnvironment = getEnvironment<AUBCommandStreamReceiverHw<FamilyType>>(false, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<AUBCommandStreamReceiverHw<FamilyType>>();
    LinearStream cs(aubExecutionEnvironment->commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 128u, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};

    std::unique_ptr<AUBCommandStreamReceiver::AubFileStream> mockAubFileStream(new GmockAubFileStream());
    GmockAubFileStream *mockAubFileStreamPtr = static_cast<GmockAubFileStream *>(mockAubFileStream.get());
    ASSERT_NE(nullptr, mockAubFileStreamPtr);
    aubCsr->stream = mockAubFileStreamPtr;

    EXPECT_CALL(*mockAubFileStreamPtr, addComment(_)).Times(2).WillOnce(Return(true)).WillOnce(Return(false));
    bool result = aubCsr->addPatchInfoComments();
    EXPECT_FALSE(result);
}

HWTEST_F(AubFileStreamTests, givenAddPatchInfoCommentsCalledWhenPatchInfoDataObjectsAddedThenCommentsAreNotEmpty) {
    auto aubExecutionEnvironment = getEnvironment<AUBCommandStreamReceiverHw<FamilyType>>(false, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<AUBCommandStreamReceiverHw<FamilyType>>();
    LinearStream cs(aubExecutionEnvironment->commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 128u, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};

    std::unique_ptr<AUBCommandStreamReceiver::AubFileStream> mockAubFileStream(new GmockAubFileStream());
    GmockAubFileStream *mockAubFileStreamPtr = static_cast<GmockAubFileStream *>(mockAubFileStream.get());
    ASSERT_NE(nullptr, mockAubFileStreamPtr);
    aubCsr->stream = mockAubFileStreamPtr;

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
}

HWTEST_F(AubFileStreamTests, givenAddPatchInfoCommentsCalledWhenSourceAllocationIsNullThenDoNotAddToAllocationsList) {
    auto aubExecutionEnvironment = getEnvironment<AUBCommandStreamReceiverHw<FamilyType>>(false, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<AUBCommandStreamReceiverHw<FamilyType>>();
    LinearStream cs(aubExecutionEnvironment->commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 128u, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};

    std::unique_ptr<AUBCommandStreamReceiver::AubFileStream> mockAubFileStream(new GmockAubFileStream());
    GmockAubFileStream *mockAubFileStreamPtr = static_cast<GmockAubFileStream *>(mockAubFileStream.get());
    ASSERT_NE(nullptr, mockAubFileStreamPtr);
    aubCsr->stream = mockAubFileStreamPtr;

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
}

HWTEST_F(AubFileStreamTests, givenAddPatchInfoCommentsCalledWhenTargetAllocationIsNullThenDoNotAddToAllocationsList) {
    auto aubExecutionEnvironment = getEnvironment<AUBCommandStreamReceiverHw<FamilyType>>(false, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<AUBCommandStreamReceiverHw<FamilyType>>();
    LinearStream cs(aubExecutionEnvironment->commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 128u, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};

    std::unique_ptr<AUBCommandStreamReceiver::AubFileStream> mockAubFileStream(new GmockAubFileStream());
    GmockAubFileStream *mockAubFileStreamPtr = static_cast<GmockAubFileStream *>(mockAubFileStream.get());
    ASSERT_NE(nullptr, mockAubFileStreamPtr);
    aubCsr->stream = mockAubFileStreamPtr;

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
}
