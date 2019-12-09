/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "device_fixture.h"

#include "core/helpers/options.h"
#include "runtime/command_stream/command_stream_receiver.h"

#include "gtest/gtest.h"

using NEO::Device;
using NEO::HardwareInfo;
using NEO::platformDevices;

void DeviceFixture::SetUp() {
    pDevice = DeviceHelper<>::create();
    ASSERT_NE(nullptr, pDevice);

    auto &commandStreamReceiver = pDevice->getGpgpuCommandStreamReceiver();
    pTagMemory = commandStreamReceiver.getTagAddress();
    ASSERT_NE(nullptr, const_cast<uint32_t *>(pTagMemory));
}

void DeviceFixture::TearDown() {
    delete pDevice;
}
