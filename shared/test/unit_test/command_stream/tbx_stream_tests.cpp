/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/tbx_command_stream_receiver_hw.h"
#include "shared/source/tbx/tbx_proto.h"
#include "shared/test/common/mocks/mock_tbx_sockets.h"
#include "shared/test/unit_test/mocks/mock_tbx_stream.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(TbxStreamTests, givenTbxStreamWhenWriteMemoryIsCalledThenMemTypeIsSetCorrectly) {
    std::unique_ptr<TbxCommandStreamReceiver::TbxStream> mockTbxStream(new MockTbxStream());
    MockTbxStream *mockTbxStreamPtr = static_cast<MockTbxStream *>(mockTbxStream.get());

    MockTbxSockets *mockTbxSocket = new MockTbxSockets();
    mockTbxStreamPtr->socket = mockTbxSocket;

    uint32_t addressSpace = AubMemDump::AddressSpaceValues::TraceLocal;
    mockTbxStream->writeMemory(0, nullptr, 0, addressSpace, 0);
    EXPECT_EQ(mem_types::MEM_TYPE_LOCALMEM, mockTbxSocket->typeCapturedFromWriteMemory);

    addressSpace = AubMemDump::AddressSpaceValues::TraceNonlocal;
    mockTbxStream->writeMemory(0, nullptr, 0, addressSpace, 0);
    EXPECT_EQ(mem_types::MEM_TYPE_SYSTEM, mockTbxSocket->typeCapturedFromWriteMemory);

    addressSpace = AubMemDump::AddressSpaceValues::TraceLocal;
    mockTbxStream->writePTE(0, 0, addressSpace);
    EXPECT_EQ(mem_types::MEM_TYPE_LOCALMEM, mockTbxSocket->typeCapturedFromWriteMemory);

    addressSpace = AubMemDump::AddressSpaceValues::TraceNonlocal;
    mockTbxStream->writePTE(0, 0, addressSpace);
    EXPECT_EQ(mem_types::MEM_TYPE_SYSTEM, mockTbxSocket->typeCapturedFromWriteMemory);
}
