/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/device/device.h"

#include <cassert>

namespace NEO {
struct HardwareInfo;
extern const HardwareInfo **platformDevices;
} // namespace NEO

// Even though there aren't any defaults, this pattern is used
// throughout testing.  Included here for consistency.
struct DeviceDefaults {
};

template <typename DeviceTraits = DeviceDefaults>
struct ClDeviceHelper {
    static NEO::Device *create(const NEO::HardwareInfo *hardwareInfo = nullptr) {
        auto device = NEO::Device::create(hardwareInfo);
        assert(device != nullptr);
        return device;
    }
};

struct DeviceFixture {
    DeviceFixture()
        : pDevice(nullptr),
          pTagMemory(nullptr) {
    }

    void SetUp();
    void TearDown();

    NEO::Device *pDevice;
    volatile uint32_t *pTagMemory;
};
