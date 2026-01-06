/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/core/source/device/device.h"

#include <string>
#include <vector>

namespace L0 {
struct ExtensionInjectorHelper {
    static void addAdditionalExtensions(std::vector<std::pair<std::string, uint32_t>> &extensions, L0::Device *device);
};
} // namespace L0