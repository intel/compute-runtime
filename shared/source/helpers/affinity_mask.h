/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/common_types.h"

#include <bitset>
#include <limits>
#include <vector>

namespace NEO {

class AffinityMaskHelper {
  public:
    using AffinityMaskContainer = std::vector<std::bitset<4>>;

    AffinityMaskHelper(bool allSubdevicesActive) {
        if (!allSubdevicesActive) {
            return;
        }

        constexpr size_t maxInitialSubdeviceCount = 4;

        enableAllGenericSubDevices(maxInitialSubdeviceCount);
    }

    AffinityMaskHelper() : AffinityMaskHelper(false) {}

    void enableGenericSubDevice(uint32_t subDeviceIndex) {
        enableGenericSubDevice(subDeviceIndex, std::numeric_limits<uint32_t>::max());
    }

    void enableEngineInstancedSubDevice(uint32_t subDeviceIndex, uint32_t engineIndex) {
        enableGenericSubDevice(subDeviceIndex, (1u << engineIndex));
    }

    void enableAllGenericSubDevices(uint32_t subDeviceCount) {
        for (uint32_t i = 0; i < subDeviceCount; i++) {
            enableGenericSubDevice(i);
        }
    }

    DeviceBitfield getGenericSubDevicesMask() const {
        return genericSubDevicesMask;
    }

    DeviceBitfield getEnginesMask(uint32_t subDeviceIndex) const {
        return subDevicesWithEnginesMasks[subDeviceIndex];
    }

    bool isDeviceEnabled() const {
        return genericSubDevicesMask.any();
    }

  protected:
    void enableGenericSubDevice(uint32_t subDeviceIndex, uint32_t enginesMask) {
        if ((subDeviceIndex + 1) > subDevicesWithEnginesMasks.size()) {
            subDevicesWithEnginesMasks.resize(subDeviceIndex + 1);
            subDevicesWithEnginesMasks[subDeviceIndex] = 0;
        }

        genericSubDevicesMask.set(subDeviceIndex);
        subDevicesWithEnginesMasks[subDeviceIndex] |= enginesMask;
    }

    AffinityMaskContainer subDevicesWithEnginesMasks;
    DeviceBitfield genericSubDevicesMask = 0;
};
} // namespace NEO
