/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "ocl_igc_interface/gt_system_info.h"
#include "ocl_igc_interface/igc_features_and_workarounds.h"
#include "ocl_igc_interface/platform.h"

namespace NEO {
using IgcFeaturesAndWorkaroundsTag = IGC::IgcFeaturesAndWorkarounds<2>;
using GTSystemInfoTag = IGC::GTSystemInfo<4>;
using PlatformTag = IGC::Platform<2>;

} // namespace NEO
