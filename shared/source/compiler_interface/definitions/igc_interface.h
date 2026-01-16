/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "ocl_igc_interface/fcl_ocl_device_ctx.h"
#include "ocl_igc_interface/igc_ocl_device_ctx.h"

namespace NEO {
using IgcFeaturesAndWorkaroundsTag = IGC::IgcFeaturesAndWorkarounds<4>;
using GTSystemInfoTag = IGC::GTSystemInfo<4>;
using PlatformTag = IGC::Platform<2>;
using OclTranslationOutputTag = IGC::OclTranslationOutput<1>;
using FclOclTranslationCtxTag = IGC::FclOclTranslationCtx<2>;
using IgcOclTranslationCtxTag = IGC::IgcOclTranslationCtx<3>;
using IgcOclDeviceCtxTag = IGC::IgcOclDeviceCtx<3>;
using FclOclDeviceCtxTag = IGC::FclOclDeviceCtx<4>;
} // namespace NEO
