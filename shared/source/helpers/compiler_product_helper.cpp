/*
 * Copyright (C) 2021-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/compiler_product_helper.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/kernel/kernel_properties.h"
#include "shared/source/release_helpers/compiler_release_helper/compiler_release_helper.h"

namespace NEO {

CompilerProductHelperCreateFunctionType compilerProductHelperFactory[NEO::maxProductEnumValue] = {};

uint32_t CompilerProductHelper::getHwIpVersion(const HardwareInfo &hwInfo) const {
    if (debugManager.flags.OverrideHwIpVersion.get() != -1) {
        return debugManager.flags.OverrideHwIpVersion.get();
    }
    return getProductConfigFromHwInfo(hwInfo);
}

std::string CompilerProductHelper::getDeviceExtensions(const HardwareInfo &hwInfo, const CompilerReleaseHelper &compilerReleaseHelper) const {
    std::string extensions = "cl_khr_byte_addressable_store "
                             "cl_khr_device_uuid "
                             "cl_khr_fp16 "
                             "cl_khr_global_int32_base_atomics "
                             "cl_khr_global_int32_extended_atomics "
                             "cl_khr_icd "
                             "cl_khr_icd_unloadable "
                             "cl_khr_local_int32_base_atomics "
                             "cl_khr_local_int32_extended_atomics "
                             "cl_intel_command_queue_families "
                             "cl_intel_subgroups "
                             "cl_intel_required_subgroup_size "
                             "cl_intel_subgroups_short "
                             "cl_intel_accelerator "
                             "cl_intel_driver_diagnostics "
                             "cl_khr_priority_hints "
                             "cl_khr_throttle_hints "
                             "cl_khr_create_command_queue "
                             "cl_intel_subgroups_char "
                             "cl_intel_subgroups_long "
                             "cl_khr_il_program "
                             "cl_intel_mem_force_host_memory "
                             "cl_khr_subgroup_extended_types "
                             "cl_khr_subgroup_non_uniform_vote "
                             "cl_khr_subgroup_ballot "
                             "cl_khr_subgroup_non_uniform_arithmetic "
                             "cl_khr_subgroup_shuffle "
                             "cl_khr_subgroup_shuffle_relative "
                             "cl_khr_subgroup_clustered_reduce "
                             "cl_intel_device_attribute_query "
                             "cl_khr_expect_assume "
                             "cl_khr_extended_bit_ops "
                             "cl_khr_suggested_local_work_size "
                             "cl_intel_split_work_group_barrier "
                             "cl_khr_int64_base_atomics "
                             "cl_khr_int64_extended_atomics "
                             "cl_khr_integer_dot_product "
                             "cl_intel_spirv_subgroups "
                             "cl_khr_spirv_linkonce_odr "
                             "cl_khr_spirv_no_integer_wrap_decoration "
                             "cl_khr_spirv_queries "
                             "cl_intel_unified_shared_memory "
                             "cl_ext_float_atomics "
                             "cl_intel_kernel_allocations_info ";

    auto supportsFp64 = hwInfo.capabilityTable.ftrSupportsFP64;
    if (debugManager.flags.OverrideDefaultFP64Settings.get() != -1) {
        supportsFp64 = debugManager.flags.OverrideDefaultFP64Settings.get();
    }
    if (supportsFp64) {
        extensions += "cl_khr_fp64 ";
    }

    if (hwInfo.capabilityTable.supportsIndependentForwardProgress) {
        extensions += "cl_khr_subgroups ";
    }

    if (hwInfo.capabilityTable.supportsMediaBlock) {
        extensions += "cl_intel_spirv_media_block_io ";
    }

    if (hwInfo.capabilityTable.supportsImages) {
        extensions += "cl_khr_mipmap_image cl_khr_mipmap_image_writes ";
    }

    if (debugManager.flags.ClKhrExternalMemoryExtension.get()) {
        extensions += "cl_khr_external_memory ";
    }

    if (debugManager.flags.EnableNV12.get() && hwInfo.capabilityTable.supportsImages) {
        extensions += "cl_intel_planar_yuv ";
    }
    if (debugManager.flags.EnablePackedYuv.get() && hwInfo.capabilityTable.supportsImages) {
        extensions += "cl_intel_packed_yuv ";
    }

    if (hwInfo.capabilityTable.supportsImages) {
        extensions += "cl_khr_image2d_from_buffer ";
        extensions += "cl_khr_depth_images ";
        extensions += "cl_khr_3d_image_writes ";
    }

    if (hwInfo.capabilityTable.supportsMediaBlock) {
        extensions += "cl_intel_media_block_io ";
    }

    if (hwInfo.caps.bFloat16ConversionSupported) {
        extensions += "cl_intel_bfloat16_conversions ";
    }

    if (this->isCreateBufferWithPropertiesSupported()) {
        extensions += "cl_intel_create_buffer_with_properties ";
    }

    if (this->isSubgroupLocalBlockIoSupported()) {
        extensions += "cl_intel_subgroup_local_block_io ";
    }

    if (compilerReleaseHelper.isMatrixMultiplyAccumulateSupported()) {
        extensions += "cl_intel_subgroup_matrix_multiply_accumulate ";
    }

    if (this->isMatrixMultiplyAccumulateTF32Supported(hwInfo)) {
        extensions += "cl_intel_subgroup_matrix_multiply_accumulate_tf32 ";
    }

    if (hwInfo.caps.splitMatrixMultiplyAccumulateSupported) {
        extensions += "cl_intel_subgroup_split_matrix_multiply_accumulate ";
    }

    if (this->isSubgroupNamedBarrierSupported()) {
        extensions += "cl_khr_subgroup_named_barrier ";
    }

    if (this->isSubgroupExtendedBlockReadSupported()) {
        extensions += "cl_intel_subgroup_extended_block_read ";
    }
    if (this->isSubgroup2DBlockIOSupported()) {
        extensions += "cl_intel_subgroup_2d_block_io ";
    }
    if (this->isSubgroupBufferPrefetchSupported()) {
        extensions += "cl_intel_subgroup_buffer_prefetch ";
    }

    return extensions;
}

StackVec<OclCVersion, 5> CompilerProductHelper::getDeviceOpenCLCVersions(OclCVersion max) const {
    if ((max.major == 0) && (max.minor != 0)) {
        max.major = 1;
        max.minor = 2;
    }

    struct {
        OclCVersion num;
        bool supported;
    } supportedVersionsMatrix[] = {
        {OclCVersion{1, 0}, true},
        {OclCVersion{1, 1}, true},
        {OclCVersion{1, 2}, true},
        {OclCVersion{3, 0}, true}};

    StackVec<OclCVersion, 5> ret;
    for (const auto &version : supportedVersionsMatrix) {
        if (version.supported && ((0 == max.major) || (max >= version.num))) {
            ret.push_back(version.num);
        }
    }

    return ret;
}

void CompilerProductHelper::getKernelFp32AtomicCapabilities(uint32_t &fp32Caps) const {
    fp32Caps = (0u | FpAtomicExtFlags::minMaxAtomicCaps | FpAtomicExtFlags::loadStoreAtomicCaps | FpAtomicExtFlags::addAtomicCaps);
}

void CompilerProductHelper::getKernelFp64AtomicCapabilities(uint32_t &fp64Caps) const {
    fp64Caps = (0u | FpAtomicExtFlags::minMaxAtomicCaps | FpAtomicExtFlags::loadStoreAtomicCaps | FpAtomicExtFlags::addAtomicCaps);
}

BuiltIn::AddressingMode CompilerProductHelper::getDefaultBuiltInAddressingMode(bool bindlessImages) const {
    return BuiltIn::AddressingMode::getDefaultMode(bindlessImages, this->isForceToStatelessRequired());
}

} // namespace NEO
