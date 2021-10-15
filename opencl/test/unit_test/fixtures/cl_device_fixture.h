/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/test/common/mocks/mock_device.h"

#include "opencl/test/unit_test/mocks/mock_cl_device.h"

namespace NEO {
struct HardwareInfo;

struct ClDeviceFixture {
    void SetUp();
    void SetUpImpl(const NEO::HardwareInfo *hardwareInfo);
    void TearDown();

    MockDevice *createWithUsDeviceId(unsigned short usDeviceId);

    MockDevice *pDevice = nullptr;
    MockClDevice *pClDevice = nullptr;
    volatile uint32_t *pTagMemory = nullptr;
    HardwareInfo hardwareInfo = {};
    PLATFORM platformHelper = {};
    OsContext *osContext = nullptr;
    const uint32_t rootDeviceIndex = 0u;
    MockClExecutionEnvironment *pClExecutionEnvironment = nullptr;
};
} // namespace NEO
