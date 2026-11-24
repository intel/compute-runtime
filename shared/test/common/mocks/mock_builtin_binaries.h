/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

namespace NEO {
struct HardwareInfo;

namespace MockBuiltinBinaries {

void registerForDevice(const HardwareInfo &hwInfo);
void registerIntermediateRepresentations();

} // namespace MockBuiltinBinaries
} // namespace NEO
