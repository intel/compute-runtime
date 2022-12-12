/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

namespace NEO {
struct HardwareInfo;

template <typename IgcPlatformType>
void populateIgcPlatform(IgcPlatformType &igcPlatform, const HardwareInfo &hwInfo);
} // namespace NEO
