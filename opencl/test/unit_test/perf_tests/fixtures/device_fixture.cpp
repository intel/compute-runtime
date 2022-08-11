/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "device_fixture.h"

#include "shared/source/command_stream/command_stream_receiver.h"

#include "gtest/gtest.h"

using NEO::Device;
using NEO::HardwareInfo;
using NEO::platformDevices;

void DeviceFixture::setUp() {
    pDevice = ClDeviceHelper<>::create();
    ASSERT_NE(nullptr, pDevice);

    auto &commandStreamReceiver = pDevice->getGpgpuCommandStreamReceiver();
    pTagMemory = commandStreamReceiver.getTagAddress();
    ASSERT_NE(nullptr, const_cast<uint32_t *>(pTagMemory));
}

void DeviceFixture::tearDown() {
    delete pDevice;
}
