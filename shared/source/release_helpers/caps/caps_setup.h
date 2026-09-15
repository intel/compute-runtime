/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/release_helpers/caps/caps.h"

#include <optional>

namespace NEO {

struct HardwareInfo;
struct HardwareIpVersion;

std::optional<Caps> resolveCaps(const HardwareIpVersion &ipVersion);
void setupCaps(HardwareInfo &hwInfo);

} // namespace NEO
