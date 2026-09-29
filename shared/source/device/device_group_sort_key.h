/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "igfxfmid.h"

namespace NEO {

struct DeviceGroupSortKey {
    PRODUCT_FAMILY productFamily{};
    bool isIntegratedDevice{};
};

inline bool compareDeviceGroups(const DeviceGroupSortKey &lhs, const DeviceGroupSortKey &rhs) {
    if (lhs.isIntegratedDevice != rhs.isIntegratedDevice) {
        return rhs.isIntegratedDevice;
    }
    return lhs.productFamily > rhs.productFamily;
}

} // namespace NEO
