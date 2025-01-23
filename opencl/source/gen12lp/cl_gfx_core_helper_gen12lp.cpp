/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds.h"
#include "shared/source/helpers/populate_factory.h"

#include "opencl/source/helpers/cl_gfx_core_helper.h"
#include "opencl/source/helpers/cl_gfx_core_helper_base.inl"
#include "opencl/source/helpers/surface_formats.h"

namespace NEO {

using Family = Gen12LpFamily;
static auto gfxCore = IGFX_GEN12LP_CORE;

template <typename GfxFamily>
inline cl_command_queue_capabilities_intel ClGfxCoreHelperHw<GfxFamily>::getAdditionalDisabledQueueFamilyCapabilities(EngineGroupType type) const {
    if (type == EngineGroupType::copy) {
        return CL_QUEUE_CAPABILITY_CREATE_CROSS_QUEUE_EVENTS_INTEL;
    }
    return 0;
}

static const std::vector<cl_image_format> redescribeFormats = {
    {CL_R, CL_UNSIGNED_INT8},
    {CL_R, CL_UNSIGNED_INT16},
    {CL_R, CL_UNSIGNED_INT32},
    {CL_RG, CL_UNSIGNED_INT32},
    {CL_RGBA, CL_UNSIGNED_INT32}};

template <typename GfxFamily>
bool ClGfxCoreHelperHw<GfxFamily>::isFormatRedescribable(cl_image_format format) const {
    for (const auto &referenceFormat : redescribeFormats) {
        if (referenceFormat.image_channel_data_type == format.image_channel_data_type &&
            referenceFormat.image_channel_order == format.image_channel_order) {
            return false;
        }
    }

    return true;
}

#include "opencl/source/helpers/cl_gfx_core_helper_factory_init.inl"

template <>
cl_device_feature_capabilities_intel ClGfxCoreHelperHw<Family>::getSupportedDeviceFeatureCapabilities(const RootDeviceEnvironment &rootDeviceEnvironment) const {
    return CL_DEVICE_FEATURE_FLAG_DP4A_INTEL;
}

template class ClGfxCoreHelperHw<Family>;

} // namespace NEO
