/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/test/common/mocks/mock_device.h"

namespace NEO {
struct HardwareInfo;

struct DeviceFixture {
    void SetUp(); // NOLINT(readability-identifier-naming)
    void setUpImpl(const NEO::HardwareInfo *hardwareInfo);
    void TearDown(); // NOLINT(readability-identifier-naming)

    MockDevice *createWithUsDeviceIdRevId(unsigned short usDeviceId, unsigned short usRevId);

    MockDevice *pDevice = nullptr;
    volatile uint32_t *pTagMemory = nullptr;
    HardwareInfo hardwareInfo = {};
    PLATFORM platformHelper = {};
    const uint32_t rootDeviceIndex = 0u;
};
} // namespace NEO
