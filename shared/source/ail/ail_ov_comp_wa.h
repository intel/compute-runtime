/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

namespace NEO {
struct HardwareInfo;

bool applyOpenVinoCompatibilityWaIfNeeded(HardwareInfo &hwInfo);

bool isOpenVinoDetected();

} // namespace NEO
