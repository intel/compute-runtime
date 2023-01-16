/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/task_count_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/test/common/mocks/mock_device.h"

namespace NEO {
class MockDevice;

struct DeviceFixture {
    void setUp();
    void setUpImpl(const NEO::HardwareInfo *hardwareInfo);
    void tearDown();

    MockDevice *createWithUsDeviceIdRevId(unsigned short usDeviceId, unsigned short usRevId);

    MockDevice *pDevice = nullptr;
    volatile TagAddressType *pTagMemory = nullptr;
    HardwareInfo hardwareInfo = {};
    PLATFORM platformHelper = {};
    const uint32_t rootDeviceIndex = 0u;

    template <typename HelperType>
    HelperType &getHelper() const;
};

} // namespace NEO
