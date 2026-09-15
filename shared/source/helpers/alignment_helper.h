/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

namespace NEO {
struct HardwareInfo;

struct AlignmentHelper {
    static bool isReducedAlignmentAllowed(const HardwareInfo &hwInfo);
};

} // namespace NEO
