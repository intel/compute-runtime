/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

namespace NEO {
struct HardwareInfo;

struct HwInfoHelper {
    static bool checkIfOcl21FeaturesEnabledOrEnforced(const HardwareInfo &hwInfo);
};
} // namespace NEO