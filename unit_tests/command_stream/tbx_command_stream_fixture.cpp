/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/command_stream/tbx_command_stream_fixture.h"

#include "runtime/command_queue/command_queue.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/device/device.h"
#include "unit_tests/gen_common/gen_cmd_parse.h"
#include "unit_tests/mocks/mock_device.h"

#include "gtest/gtest.h"

namespace OCLRT {

void TbxCommandStreamFixture::SetUp(MockDevice *pDevice) {
    // Create our TBX command stream receiver based on HW type
    const auto &hwInfo = pDevice->getHardwareInfo();
    pCommandStreamReceiver = TbxCommandStreamReceiver::create(hwInfo, "", false, *pDevice->executionEnvironment);
    ASSERT_NE(nullptr, pCommandStreamReceiver);
    mmTbx = new TbxMemoryManager(false, false, *pDevice->executionEnvironment);
    pDevice->resetCommandStreamReceiver(pCommandStreamReceiver);
}

void TbxCommandStreamFixture::TearDown() {
    delete mmTbx;
    CommandStreamFixture::TearDown();
}
} // namespace OCLRT
