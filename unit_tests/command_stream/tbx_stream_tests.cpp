/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_stream/tbx_command_stream_receiver_hw.h"
#include "unit_tests/mocks/mock_tbx_sockets.h"
#include "unit_tests/mocks/mock_tbx_stream.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(TbxStreamTests, givenTbxStreamWhenWriteMemoryIsCalledThenTypeIsZero) {
    std::unique_ptr<TbxCommandStreamReceiver::TbxStream> mockTbxStream(new MockTbxStream());
    MockTbxStream *mockTbxStreamPtr = static_cast<MockTbxStream *>(mockTbxStream.get());

    MockTbxSockets *mockTbxSocket = new MockTbxSockets();
    mockTbxStreamPtr->socket = mockTbxSocket;

    mockTbxStream->writeMemory(0, nullptr, 0, 0, 0);
    EXPECT_EQ(0u, mockTbxSocket->typeCapturedFromWriteMemory);

    mockTbxStream->writePTE(0, 0, 0);
    EXPECT_EQ(0u, mockTbxSocket->typeCapturedFromWriteMemory);
}
