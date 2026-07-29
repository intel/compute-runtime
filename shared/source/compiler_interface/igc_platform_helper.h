/*
 * Copyright (C) 2022-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "igc_interface.h"

#include <cstdint>

namespace NEO {
struct HardwareInfo;

constexpr inline uint32_t ocl30ApiVersion = 300;

void populateIgcPlatform(PlatformTag &igcPlatform, const HardwareInfo &hwInfo);

bool initializeIgcDeviceContext(NEO::IgcOclDeviceCtxTag *igcDeviceCtx, const HardwareInfo &hwInfo);
} // namespace NEO
