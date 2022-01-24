/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hp_core/hw_cmds_base.h"

#include "opencl/source/context/context.h"
#include "opencl/source/mem_obj/image.inl"

namespace NEO {

using Family = XeHpFamily;
static auto gfxCore = IGFX_XE_HP_CORE;

template <>
void ImageHw<Family>::appendSurfaceStateParams(Family::RENDER_SURFACE_STATE *surfaceState, uint32_t rootDeviceIndex, bool useGlobalAtomics) {
    EncodeSurfaceStateArgs args{};
    args.outMemory = surfaceState;
    args.useGlobalAtomics = useGlobalAtomics;
    args.areMultipleSubDevicesInContext = context->containsMultipleSubDevices(rootDeviceIndex);
    args.implicitScaling = args.areMultipleSubDevicesInContext;
    EncodeSurfaceState<Family>::encodeImplicitScalingParams(args);
}
} // namespace NEO
#include "opencl/source/mem_obj/image_tgllp_and_later.inl"

// factory initializer
#include "opencl/source/mem_obj/image_factory_init.inl"
