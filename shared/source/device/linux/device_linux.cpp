/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/device.h"

namespace NEO {

void Device::getAdapterLuid(std::array<uint8_t, HwInfoConfig::luidSize> &luid) {}

bool Device::verifyAdapterLuid() { return false; }
} // namespace NEO
