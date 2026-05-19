/*
 * Copyright (C) 2023-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/assert_handler/assert_handler.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/helpers/stream_capture.h"
#include "shared/test/common/mocks/mock_assert_handler.h"
#include "shared/test/common/mocks/mock_device.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(AssertHandlerTests, WhenAssertHandlerIsCreatedThenAssertBufferIsAllocated) {
    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));

    MockAssertHandler assertHandler(device.get());

    ASSERT_NE(nullptr, assertHandler.getAssertBuffer());
    EXPECT_EQ(assertHandler.assertBufferSize, assertHandler.getAssertBuffer()->getUnderlyingBufferSize());

    EXPECT_EQ(0u, reinterpret_cast<AssertBufferHeader *>(assertHandler.getAssertBuffer()->getUnderlyingBuffer())->flags);
    EXPECT_EQ(assertHandler.assertBufferSize, reinterpret_cast<AssertBufferHeader *>(assertHandler.getAssertBuffer()->getUnderlyingBuffer())->size);
    EXPECT_EQ(sizeof(AssertBufferHeader), reinterpret_cast<AssertBufferHeader *>(assertHandler.getAssertBuffer()->getUnderlyingBuffer())->begin);
}

TEST(AssertHandlerTests, GivenAssertHandlerWhenCheckingAssertThenReturnValuIsBasedOnFlags) {
    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));

    MockAssertHandler assertHandler(device.get());

    ASSERT_NE(nullptr, assertHandler.getAssertBuffer());

    EXPECT_FALSE(assertHandler.checkAssert());

    reinterpret_cast<AssertBufferHeader *>(assertHandler.getAssertBuffer()->getUnderlyingBuffer())->flags = 1;
    EXPECT_TRUE(assertHandler.checkAssert());
}

TEST(AssertHandlerTests, GivenNoFlagSetWhenPrintAssertAndAbortCalledThenMessageIsPrintedAndAbortIsNotCalled) {
    if (sizeof(void *) == 4) {
        GTEST_SKIP();
    }
    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));

    MockAssertHandler assertHandler(device.get());
    ASSERT_NE(nullptr, assertHandler.getAssertBuffer());

    reinterpret_cast<AssertBufferHeader *>(assertHandler.getAssertBuffer()->getUnderlyingBuffer())->flags = 0;

    char const *message = "info!";
    auto stringAddress = ptrOffset(reinterpret_cast<uint32_t *>(assertHandler.getAssertBuffer()->getUnderlyingBuffer()), offsetof(AssertBufferHeader, begin) + sizeof(AssertBufferHeader::begin));
    auto formatStringAddress = reinterpret_cast<uint64_t>(message);
    memcpy(stringAddress, &formatStringAddress, 8);

    auto garbageData = ptrOffset(reinterpret_cast<uint32_t *>(stringAddress), sizeof(stringAddress));
    memset(garbageData, 1, sizeof(uint64_t));

    reinterpret_cast<AssertBufferHeader *>(assertHandler.getAssertBuffer()->getUnderlyingBuffer())->begin += sizeof(uint64_t);

    StreamCapture capture;
    capture.captureStderr();
    assertHandler.printAssertAndAbort();

    std::string output = capture.getCapturedStderr();
    EXPECT_TRUE(hasSubstr(output, std::string("AssertHandler::printMessage\n")));
    EXPECT_TRUE(hasSubstr(output, std::string("info!")));
}

TEST(AssertHandlerTests, GivenFlagSetWhenPrintAssertAndAbortCalledThenMessageIsPrintedAndAbortCalled) {
    if (sizeof(void *) == 4) {
        GTEST_SKIP();
    }
    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));

    MockAssertHandler assertHandler(device.get());
    ASSERT_NE(nullptr, assertHandler.getAssertBuffer());

    reinterpret_cast<AssertBufferHeader *>(assertHandler.getAssertBuffer()->getUnderlyingBuffer())->flags = 1;

    char const *message = "assert!";
    auto stringAddress = ptrOffset(reinterpret_cast<uint32_t *>(assertHandler.getAssertBuffer()->getUnderlyingBuffer()), offsetof(AssertBufferHeader, begin) + sizeof(AssertBufferHeader::begin));
    static_assert(((offsetof(AssertBufferHeader, begin) + sizeof(AssertBufferHeader::begin)) & 0b11) == 0, "requirement for uint32_t * alignment not met");
    auto formatStringAddress = reinterpret_cast<uint64_t>(message);
    memcpy(stringAddress, &formatStringAddress, 8);

    auto garbageData = ptrOffset(reinterpret_cast<uint32_t *>(stringAddress), sizeof(stringAddress));
    memset(garbageData, 1, sizeof(uint64_t));

    reinterpret_cast<AssertBufferHeader *>(assertHandler.getAssertBuffer()->getUnderlyingBuffer())->begin += sizeof(uint64_t);

    StreamCapture capture;
    capture.captureStderr();
    EXPECT_THROW(assertHandler.printAssertAndAbort(), std::exception);

    std::string output = capture.getCapturedStderr();
    EXPECT_STREQ("AssertHandler::printMessage\nassert!", output.c_str());
}

TEST(AssertHandlerTests, GivenFlagSetWhenPrintAssertAndAbortCalledThenDirectSubmissionIsStoppedBeforeAbort) {
    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));

    MockAssertHandler assertHandler(device.get());
    ASSERT_NE(nullptr, assertHandler.getAssertBuffer());

    reinterpret_cast<AssertBufferHeader *>(assertHandler.getAssertBuffer()->getUnderlyingBuffer())->flags = 1;

    EXPECT_FALSE(device->stopDirectSubmissionCalled);

    StreamCapture capture;
    capture.captureStderr();
    EXPECT_THROW(assertHandler.printAssertAndAbort(), std::exception);
    capture.getCapturedStderr();

    EXPECT_TRUE(device->stopDirectSubmissionCalled);
}

TEST(AssertHandlerTests, GivenNoFlagSetWhenPrintAssertAndAbortCalledThenDirectSubmissionIsNotStopped) {
    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));

    MockAssertHandler assertHandler(device.get());
    ASSERT_NE(nullptr, assertHandler.getAssertBuffer());

    reinterpret_cast<AssertBufferHeader *>(assertHandler.getAssertBuffer()->getUnderlyingBuffer())->flags = 0;

    assertHandler.printAssertAndAbort();

    EXPECT_FALSE(device->stopDirectSubmissionCalled);
}

TEST(AssertHandlerTests, GivenMessagesInBufferWhenPrintAssertAndAbortCalledThenBufferIsResetAfterPrinting) {
    if (sizeof(void *) == 4) {
        GTEST_SKIP();
    }
    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));

    MockAssertHandler assertHandler(device.get());
    ASSERT_NE(nullptr, assertHandler.getAssertBuffer());

    auto header = reinterpret_cast<AssertBufferHeader *>(assertHandler.getAssertBuffer()->getUnderlyingBuffer());

    char const *message = "info!";
    auto stringAddress = ptrOffset(reinterpret_cast<uint32_t *>(assertHandler.getAssertBuffer()->getUnderlyingBuffer()), offsetof(AssertBufferHeader, begin) + sizeof(AssertBufferHeader::begin));
    auto formatStringAddress = reinterpret_cast<uint64_t>(message);
    memcpy(stringAddress, &formatStringAddress, 8);

    auto garbageData = ptrOffset(reinterpret_cast<uint32_t *>(stringAddress), sizeof(stringAddress));
    memset(garbageData, 1, sizeof(uint64_t));

    header->begin += sizeof(uint64_t);
    header->flags = 1;

    StreamCapture capture;
    capture.captureStderr();
    EXPECT_THROW(assertHandler.printAssertAndAbort(), std::exception);
    capture.getCapturedStderr();

    EXPECT_EQ(0u, header->flags);
    EXPECT_EQ(sizeof(AssertBufferHeader), header->begin);
}

TEST(AssertHandlerTests, GivenBufferResetAfterPrintWhenNewMessagesWrittenThenNewMessagesArePrinted) {
    if (sizeof(void *) == 4) {
        GTEST_SKIP();
    }
    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));

    MockAssertHandler assertHandler(device.get());
    ASSERT_NE(nullptr, assertHandler.getAssertBuffer());

    auto header = reinterpret_cast<AssertBufferHeader *>(assertHandler.getAssertBuffer()->getUnderlyingBuffer());

    char const *firstMessage = "first!";
    auto stringAddress = ptrOffset(reinterpret_cast<uint32_t *>(assertHandler.getAssertBuffer()->getUnderlyingBuffer()), offsetof(AssertBufferHeader, begin) + sizeof(AssertBufferHeader::begin));
    auto formatStringAddress = reinterpret_cast<uint64_t>(firstMessage);
    memcpy(stringAddress, &formatStringAddress, 8);

    auto garbageData = ptrOffset(reinterpret_cast<uint32_t *>(stringAddress), sizeof(stringAddress));
    memset(garbageData, 1, sizeof(uint64_t));

    header->begin += sizeof(uint64_t);

    StreamCapture capture;
    capture.captureStderr();
    assertHandler.printAssertAndAbort();
    std::string firstOutput = capture.getCapturedStderr();

    EXPECT_TRUE(hasSubstr(firstOutput, std::string("first!")));
    EXPECT_EQ(sizeof(AssertBufferHeader), header->begin);

    char const *secondMessage = "second!";
    stringAddress = ptrOffset(reinterpret_cast<uint32_t *>(assertHandler.getAssertBuffer()->getUnderlyingBuffer()), offsetof(AssertBufferHeader, begin) + sizeof(AssertBufferHeader::begin));
    formatStringAddress = reinterpret_cast<uint64_t>(secondMessage);
    memcpy(stringAddress, &formatStringAddress, 8);

    garbageData = ptrOffset(reinterpret_cast<uint32_t *>(stringAddress), sizeof(stringAddress));
    memset(garbageData, 1, sizeof(uint64_t));

    header->begin += sizeof(uint64_t);

    StreamCapture capture2;
    capture2.captureStderr();
    assertHandler.printAssertAndAbort();
    std::string secondOutput = capture2.getCapturedStderr();

    EXPECT_TRUE(hasSubstr(secondOutput, std::string("second!")));
}

TEST(AssertHandlerTests, GivenEmptyBufferWhenPrintAssertAndAbortCalledThenBufferRemainsReset) {
    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));

    MockAssertHandler assertHandler(device.get());
    ASSERT_NE(nullptr, assertHandler.getAssertBuffer());

    auto header = reinterpret_cast<AssertBufferHeader *>(assertHandler.getAssertBuffer()->getUnderlyingBuffer());

    EXPECT_EQ(sizeof(AssertBufferHeader), header->begin);
    EXPECT_EQ(0u, header->flags);

    assertHandler.printAssertAndAbort();

    EXPECT_EQ(sizeof(AssertBufferHeader), header->begin);
    EXPECT_EQ(0u, header->flags);
}
