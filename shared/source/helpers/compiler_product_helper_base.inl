/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/cache_policy.h"
#include "shared/source/helpers/compiler_product_helper.h"

namespace NEO {

template <PRODUCT_FAMILY gfxProduct>
bool CompilerProductHelperHw<gfxProduct>::isForceEmuInt32DivRemSPRequired() const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool CompilerProductHelperHw<gfxProduct>::isStatelessToStatefulBufferOffsetSupported() const {
    return true;
}

template <PRODUCT_FAMILY gfxProduct>
const char *CompilerProductHelperHw<gfxProduct>::getCachingPolicyOptions(bool isDebuggerActive) const {
    return L1CachePolicyHelper<gfxProduct>::getCachingPolicyOptions(isDebuggerActive);
}

template <PRODUCT_FAMILY gfxProduct>
bool CompilerProductHelperHw<gfxProduct>::failBuildProgramWithStatefulAccessPreference() const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
std::string CompilerProductHelperHw<gfxProduct>::getExtensions(const HardwareInfo &hwInfo) const {
    std::string extensions = "";
    auto enabledClVersion = hwInfo.capabilityTable.clVersionSupport;
    auto ocl21FeaturesEnabled = hwInfo.capabilityTable.supportsOcl21Features;
    if (DebugManager.flags.ForceOCLVersion.get() != 0) {
        enabledClVersion = DebugManager.flags.ForceOCLVersion.get();
        ocl21FeaturesEnabled = (enabledClVersion == 21);
    }
    if (DebugManager.flags.ForceOCL21FeaturesSupport.get() != -1) {
        ocl21FeaturesEnabled = DebugManager.flags.ForceOCL21FeaturesSupport.get();
    }
    auto supportsVme = hwInfo.capabilityTable.supportsVme;
    auto supportsAdvancedVme = hwInfo.capabilityTable.supportsVme;

    if (ocl21FeaturesEnabled) {

        if (supportsVme) {
            extensions += "cl_intel_spirv_device_side_avc_motion_estimation ";
        }
        if (hwInfo.capabilityTable.supportsMediaBlock) {
            extensions += "cl_intel_spirv_media_block_io ";
        }
        extensions += "cl_intel_spirv_subgroups ";
        extensions += "cl_khr_spirv_no_integer_wrap_decoration ";

        extensions += "cl_intel_unified_shared_memory ";
        if (hwInfo.capabilityTable.supportsImages) {
            extensions += "cl_khr_mipmap_image cl_khr_mipmap_image_writes ";
        }
    }

    if (enabledClVersion >= 20) {
        extensions += "cl_ext_float_atomics ";
    }

    if (DebugManager.flags.EnableNV12.get() && hwInfo.capabilityTable.supportsImages) {
        extensions += "cl_intel_planar_yuv ";
    }
    if (DebugManager.flags.EnablePackedYuv.get() && hwInfo.capabilityTable.supportsImages) {
        extensions += "cl_intel_packed_yuv ";
    }
    if (DebugManager.flags.EnableIntelVme.get() != -1) {
        supportsVme = !!DebugManager.flags.EnableIntelVme.get();
    }

    if (supportsVme) {
        extensions += "cl_intel_motion_estimation cl_intel_device_side_avc_motion_estimation ";
    }

    if (DebugManager.flags.EnableIntelAdvancedVme.get() != -1) {
        supportsAdvancedVme = !!DebugManager.flags.EnableIntelAdvancedVme.get();
    }
    if (supportsAdvancedVme) {
        extensions += "cl_intel_advanced_motion_estimation ";
    }

    if (hwInfo.capabilityTable.ftrSupportsInteger64BitAtomics) {
        extensions += "cl_khr_int64_base_atomics ";
        extensions += "cl_khr_int64_extended_atomics ";
    }

    if (hwInfo.capabilityTable.supportsImages) {
        extensions += "cl_khr_image2d_from_buffer ";
        extensions += "cl_khr_depth_images ";
        extensions += "cl_khr_3d_image_writes ";
    }

    if (hwInfo.capabilityTable.supportsMediaBlock) {
        extensions += "cl_intel_media_block_io ";
    }

    if (isBFloat16ConversionSupported(hwInfo)) {
        extensions += "cl_intel_bfloat16_conversions ";
    }

    if (isCreateBufferWithPropertiesSupported()) {
        extensions += "cl_intel_create_buffer_with_properties ";
    }

    if (isDotAccumulateSupported()) {
        extensions += "cl_intel_dot_accumulate ";
    }

    if (isSubgroupLocalBlockIoSupported()) {
        extensions += "cl_intel_subgroup_local_block_io ";
    }

    if (isMatrixMultiplyAccumulateSupported(hwInfo)) {
        extensions += "cl_intel_subgroup_matrix_multiply_accumulate ";
    }

    if (isSplitMatrixMultiplyAccumulateSupported(hwInfo)) {
        extensions += "cl_intel_subgroup_split_matrix_multiply_accumulate ";
    }

    if (isSubgroupNamedBarrierSupported()) {
        extensions += "cl_khr_subgroup_named_barrier ";
    }

    if (isSubgroupExtendedBlockReadSupported()) {
        extensions += "cl_intel_subgroup_extended_block_read ";
    }
    return extensions;
}

} // namespace NEO
