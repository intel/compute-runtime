/*
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/populate_factory.h"
#include "shared/source/memory_manager/compression_selector.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/xe_hpg_core/hw_cmds_xe_hpg_core_base.h"

#include "opencl/source/helpers/cl_gfx_core_helper_base.inl"
#include "opencl/source/helpers/cl_gfx_core_helper_xehp_and_later.inl"
#include "opencl/source/helpers/surface_formats.h"

namespace NEO {

using Family = XeHpgCoreFamily;
static auto gfxCore = IGFX_XE_HPG_CORE;

#include "opencl/source/helpers/cl_gfx_core_helper_factory_init.inl"

template <>
bool ClGfxCoreHelperHw<Family>::requiresNonAuxMode(const ArgDescPointer &argAsPtr) const {

    if (CompressionSelector::allowStatelessCompression()) {
        return false;
    } else {
        return !argAsPtr.isPureStateful();
    }
}

template <>
bool ClGfxCoreHelperHw<Family>::requiresAuxResolves(const KernelInfo &kernelInfo) const {

    if (CompressionSelector::allowStatelessCompression()) {
        return false;
    } else {
        return hasStatelessAccessToBuffer(kernelInfo);
    }
}

template <>
std::vector<uint32_t> ClGfxCoreHelperHw<Family>::getSupportedThreadArbitrationPolicies() const {
    return {};
}

template <>
bool ClGfxCoreHelperHw<Family>::isSupportedKernelThreadArbitrationPolicy() const { return false; }

static const std::vector<cl_image_format> incompressibleFormats = {
    {CL_LUMINANCE, CL_UNORM_INT8},
    {CL_LUMINANCE, CL_UNORM_INT16},
    {CL_LUMINANCE, CL_HALF_FLOAT},
    {CL_LUMINANCE, CL_FLOAT},
    {CL_INTENSITY, CL_UNORM_INT8},
    {CL_INTENSITY, CL_UNORM_INT16},
    {CL_INTENSITY, CL_HALF_FLOAT},
    {CL_INTENSITY, CL_FLOAT},
    {CL_A, CL_UNORM_INT16},
    {CL_A, CL_HALF_FLOAT},
    {CL_A, CL_FLOAT}};

template <>
bool ClGfxCoreHelperHw<Family>::allowImageCompression(cl_image_format format) const {
    for (auto &referenceFormat : incompressibleFormats) {
        if (format.image_channel_data_type == referenceFormat.image_channel_data_type &&
            format.image_channel_order == referenceFormat.image_channel_order) {
            return false;
        }
    }

    return true;
}

template class ClGfxCoreHelperHw<Family>;

} // namespace NEO
