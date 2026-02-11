/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

namespace NEO {
struct HardwareInfo;

bool applyOpenVinoCompatibilityWaIfNeeded(HardwareInfo &hwInfo) {
    return false;
}

bool isOpenVinoDetected() {
    return false;
}

} // namespace NEO
