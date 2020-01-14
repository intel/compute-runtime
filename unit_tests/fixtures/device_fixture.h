/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "unit_tests/mocks/mock_device.h"

namespace NEO {
struct HardwareInfo;

struct DeviceFixture {
    void SetUp();
    void SetUpImpl(const NEO::HardwareInfo *hardwareInfo);
    void TearDown();

    MockDevice *createWithUsDeviceId(unsigned short usDeviceId);

    MockDevice *pDevice = nullptr;
    MockClDevice *pClDevice = nullptr;
    volatile uint32_t *pTagMemory = nullptr;
    HardwareInfo hardwareInfo = {};
    PLATFORM platformHelper = {};
};
} // namespace NEO
