/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/fixtures/tbx_command_stream_fixture.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/command_stream/tbx_command_stream_receiver_hw.h"
#include "shared/test/common/mocks/mock_device.h"

#include "gtest/gtest.h"

namespace NEO {

void TbxCommandStreamFixture::SetUp(MockDevice *pDevice) {
    // Create our TBX command stream receiver based on HW type
    pCommandStreamReceiver = TbxCommandStreamReceiver::create("", false, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    ASSERT_NE(nullptr, pCommandStreamReceiver);
    memoryManager = new OsAgnosticMemoryManager(*pDevice->executionEnvironment);
    pDevice->resetCommandStreamReceiver(pCommandStreamReceiver);
}

void TbxCommandStreamFixture::TearDown() {
    delete memoryManager;
}
} // namespace NEO
