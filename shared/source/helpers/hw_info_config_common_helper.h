/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

namespace NEO {
struct HardwareInfo;

namespace HwInfoConfigCommonHelper {
void enableBlitterOperationsSupport(HardwareInfo &hardwareInfo);
}
} // namespace NEO
