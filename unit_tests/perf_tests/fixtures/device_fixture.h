/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/device/device.h"

#include <cassert>

namespace OCLRT {
struct HardwareInfo;
extern const HardwareInfo **platformDevices;
} // namespace OCLRT

// Even though there aren't any defaults, this pattern is used
// throughout testing.  Included here for consistency.
struct DeviceDefaults {
};

template <typename DeviceTraits = DeviceDefaults>
struct DeviceHelper {
    static OCLRT::Device *create(const OCLRT::HardwareInfo *hardwareInfo = nullptr) {
        auto device = OCLRT::Device::create(hardwareInfo);
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

    OCLRT::Device *pDevice;
    volatile uint32_t *pTagMemory;
};
