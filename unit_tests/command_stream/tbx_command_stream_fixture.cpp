/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "command_stream/tbx_command_stream_fixture.h"

#include "core/command_stream/command_stream_receiver.h"
#include "core/device/device.h"
#include "opencl/source/command_queue/command_queue.h"

#include "gen_common/gen_cmd_parse.h"
#include "gtest/gtest.h"
#include "mocks/mock_device.h"

namespace NEO {

void TbxCommandStreamFixture::SetUp(MockDevice *pDevice) {
    // Create our TBX command stream receiver based on HW type
    pCommandStreamReceiver = TbxCommandStreamReceiver::create("", false, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex());
    ASSERT_NE(nullptr, pCommandStreamReceiver);
    memoryManager = new OsAgnosticMemoryManager(*pDevice->executionEnvironment);
    pDevice->resetCommandStreamReceiver(pCommandStreamReceiver);
}

void TbxCommandStreamFixture::TearDown() {
    delete memoryManager;
    CommandStreamFixture::TearDown();
}
} // namespace NEO
