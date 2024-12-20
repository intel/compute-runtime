/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

namespace NEO {
struct HardwareInfo;

bool applyOpenVinoCompatibilityWaIfNeeded(HardwareInfo &hwInfo);

} // namespace NEO
