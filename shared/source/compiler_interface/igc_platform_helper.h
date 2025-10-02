/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstdint>

namespace NEO {
struct HardwareInfo;

constexpr inline uint32_t ocl30ApiVersion = 300;

template <typename IgcPlatformType>
void populateIgcPlatform(IgcPlatformType &igcPlatform, const HardwareInfo &hwInfo);
} // namespace NEO
