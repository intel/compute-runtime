/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/test/unit_test/mocks/mock_device.h"

namespace NEO {
struct HardwareInfo;

struct DeviceFixture {
    void SetUp();
    void SetUpImpl(const NEO::HardwareInfo *hardwareInfo);
    void TearDown();

    MockDevice *createWithUsDeviceId(unsigned short usDeviceId);

    MockDevice *pDevice = nullptr;
    volatile uint32_t *pTagMemory = nullptr;
    HardwareInfo hardwareInfo = {};
    PLATFORM platformHelper = {};
};
} // namespace NEO
