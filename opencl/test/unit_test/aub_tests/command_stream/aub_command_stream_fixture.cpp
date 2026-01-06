/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/aub_tests/command_stream/aub_command_stream_fixture.h"

#include "shared/source/command_stream/aub_command_stream_receiver.h"
#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/command_stream/tbx_command_stream_receiver.h"
#include "shared/source/device/device.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/test/common/mocks/mock_device.h"

#include "opencl/source/command_queue/command_queue.h"
#include "opencl/test/unit_test/aub_tests/fixtures/aub_fixture.h"

#include "gtest/gtest.h"

namespace NEO {

void AUBCommandStreamFixture::setUp(const HardwareInfo *hardwareInfo) {
    AUBFixture::setUp(hardwareInfo);

    CommandStreamFixture::setUp(pCmdQ);

    pClDevice = device;
    pDevice = &pClDevice->device;
    pCommandStreamReceiver = csr;
    pTagMemory = pCommandStreamReceiver->getTagAddress();
    this->commandQueue = pCmdQ;
}

void AUBCommandStreamFixture::setUp() {
    setUp(nullptr);
}

void AUBCommandStreamFixture::tearDown() {
    CommandStreamFixture::tearDown();
    AUBFixture::tearDown();
}
} // namespace NEO
