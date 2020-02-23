/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/command_stream/tbx_command_stream_fixture.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/device/device.h"
#include "opencl/source/command_queue/command_queue.h"
#include "opencl/test/unit_test/gen_common/gen_cmd_parse.h"
#include "opencl/test/unit_test/mocks/mock_device.h"

#include "gtest/gtest.h"

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
