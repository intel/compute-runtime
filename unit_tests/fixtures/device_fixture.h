/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "unit_tests/mocks/mock_device.h"

namespace OCLRT {
struct HardwareInfo;

struct DeviceFixture {
    void SetUp();
    void SetUpImpl(const OCLRT::HardwareInfo *hardwareInfo);
    void TearDown();

    MockDevice *createWithUsDeviceId(unsigned short usDeviceId);

    MockDevice *pDevice = nullptr;
    volatile uint32_t *pTagMemory = nullptr;
    HardwareInfo hwInfoHelper = {};
    PLATFORM platformHelper = {};
};
} // namespace OCLRT
