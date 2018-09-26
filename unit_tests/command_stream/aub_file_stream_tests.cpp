/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_stream/aub_command_stream_receiver_hw.h"
#include "test.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/mocks/mock_aub_file_stream.h"

#include <fstream>
#include <memory>

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winconsistent-missing-override"
#endif

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

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
