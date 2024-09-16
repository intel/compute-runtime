/*
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/helpers/cl_gfx_core_helper.h"
#include "opencl/source/helpers/surface_formats.h"

namespace NEO {

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
} // namespace NEO
