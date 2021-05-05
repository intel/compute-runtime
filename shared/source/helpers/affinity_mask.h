/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/debug_helpers.h"

#include <array>
#include <bitset>
#include <string>
#include <vector>

namespace NEO {

class AffinityMaskHelper {
  public:
    using AffinityMaskContainer = std::vector<std::bitset<32>>;

    AffinityMaskHelper(bool allSubdevicesActive) {
        if (!allSubdevicesActive) {
            return;
        }

        constexpr size_t maxInitialSubdeviceCount = 4;

        enableAllGenericSubDevices(maxInitialSubdeviceCount);
    }

    AffinityMaskHelper() : AffinityMaskHelper(false) {}

    void enableGenericSubDevice(uint32_t subDeviceIndex) {
        subDevicesWithEnginesMasks.resize(subDeviceIndex + 1);

        genericSubDevicesMask.set(subDeviceIndex);
        subDevicesWithEnginesMasks[subDeviceIndex] = std::numeric_limits<uint32_t>::max();
    }

    void enableAllGenericSubDevices(uint32_t subDeviceCount) {
        for (uint32_t i = 0; i < subDeviceCount; i++) {
            enableGenericSubDevice(i);
        }
    }

    DeviceBitfield getGenericSubDevicesMask() const {
        return genericSubDevicesMask;
    }

    bool isDeviceEnabled() const {
        return genericSubDevicesMask.any();
    }

  protected:
    AffinityMaskContainer subDevicesWithEnginesMasks;
    DeviceBitfield genericSubDevicesMask = 0;
};
} // namespace NEO
