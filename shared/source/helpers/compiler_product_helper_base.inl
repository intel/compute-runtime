/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/cache_policy.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/hw_info_helper.h"
#include "shared/source/kernel/kernel_properties.h"
#include "shared/source/os_interface/os_inc_base.h"
#include "shared/source/release_helper/release_helper.h"

namespace NEO {

template <PRODUCT_FAMILY gfxProduct>
CompilerProductHelperHw<gfxProduct>::CompilerProductHelperHw() = default;

template <PRODUCT_FAMILY gfxProduct>
bool CompilerProductHelperHw<gfxProduct>::isMidThreadPreemptionSupported(const HardwareInfo &hwInfo) const {
    return hwInfo.featureTable.flags.ftrWalkerMTP;
}

template <PRODUCT_FAMILY gfxProduct>
bool CompilerProductHelperHw<gfxProduct>::isForceEmuInt32DivRemSPRequired() const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
uint64_t CompilerProductHelperHw<gfxProduct>::getHwInfoConfig(const HardwareInfo &hwInfo) const {
    return 0x0;
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
bool CompilerProductHelperHw<gfxProduct>::oclocEnforceZebinFormat() const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
std::string CompilerProductHelperHw<gfxProduct>::getDeviceExtensions(const HardwareInfo &hwInfo, const ReleaseHelper *releaseHelper) const {
    std::string extensions = "cl_khr_byte_addressable_store "
                             "cl_khr_device_uuid "
                             "cl_khr_fp16 "
                             "cl_khr_global_int32_base_atomics "
                             "cl_khr_global_int32_extended_atomics "
                             "cl_khr_icd "
                             "cl_khr_local_int32_base_atomics "
                             "cl_khr_local_int32_extended_atomics "
                             "cl_intel_command_queue_families "
                             "cl_intel_subgroups "
                             "cl_intel_required_subgroup_size "
                             "cl_intel_subgroups_short "
                             "cl_khr_spir "
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
                             "cl_intel_split_work_group_barrier ";

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

    auto enabledClVersion = hwInfo.capabilityTable.clVersionSupport;
    if (debugManager.flags.ForceOCLVersion.get() != 0) {
        enabledClVersion = debugManager.flags.ForceOCLVersion.get();
    }
    auto ocl21FeaturesEnabled = HwInfoHelper::checkIfOcl21FeaturesEnabledOrEnforced(hwInfo);

    if (ocl21FeaturesEnabled) {

        if (hwInfo.capabilityTable.supportsMediaBlock) {
            extensions += "cl_intel_spirv_media_block_io ";
        }
        extensions += "cl_intel_spirv_subgroups ";
        extensions += "cl_khr_spirv_linkonce_odr ";
        extensions += "cl_khr_spirv_no_integer_wrap_decoration ";

        extensions += "cl_intel_unified_shared_memory ";
        if (hwInfo.capabilityTable.supportsImages) {
            extensions += "cl_khr_mipmap_image cl_khr_mipmap_image_writes ";
        }
    }

    if (enabledClVersion >= 20) {
        extensions += "cl_ext_float_atomics ";
    }

    if (enabledClVersion >= 30 && debugManager.flags.ClKhrExternalMemoryExtension.get()) {
        extensions += "cl_khr_external_memory ";
    }

    if (debugManager.flags.EnableNV12.get() && hwInfo.capabilityTable.supportsImages) {
        extensions += "cl_intel_planar_yuv ";
    }
    if (debugManager.flags.EnablePackedYuv.get() && hwInfo.capabilityTable.supportsImages) {
        extensions += "cl_intel_packed_yuv ";
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

    if (isBFloat16ConversionSupported(releaseHelper)) {
        extensions += "cl_intel_bfloat16_conversions ";
    }

    if (isCreateBufferWithPropertiesSupported()) {
        extensions += "cl_intel_create_buffer_with_properties ";
    }

    if (isSubgroupLocalBlockIoSupported()) {
        extensions += "cl_intel_subgroup_local_block_io ";
    }

    if (isMatrixMultiplyAccumulateSupported(releaseHelper)) {
        extensions += "cl_intel_subgroup_matrix_multiply_accumulate ";
    }

    if (isMatrixMultiplyAccumulateTF32Supported(hwInfo)) {
        extensions += "cl_intel_subgroup_matrix_multiply_accumulate_tf32 ";
    }

    if (isSplitMatrixMultiplyAccumulateSupported(releaseHelper)) {
        extensions += "cl_intel_subgroup_split_matrix_multiply_accumulate ";
    }

    if (isSubgroupNamedBarrierSupported()) {
        extensions += "cl_khr_subgroup_named_barrier ";
    }

    if (isSubgroupExtendedBlockReadSupported()) {
        extensions += "cl_intel_subgroup_extended_block_read ";
    }
    if (isSubgroup2DBlockIOSupported()) {
        extensions += "cl_intel_subgroup_2d_block_io ";
    }
    if (isSubgroupBufferPrefetchSupported()) {
        extensions += "cl_intel_subgroup_buffer_prefetch ";
    }
    if (isDotIntegerProductExtensionSupported()) {
        extensions += "cl_khr_integer_dot_product ";
    }
    return extensions;
}

template <PRODUCT_FAMILY gfxProduct>
StackVec<OclCVersion, 5> CompilerProductHelperHw<gfxProduct>::getDeviceOpenCLCVersions(const HardwareInfo &hwInfo, OclCVersion max) const {
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
        {OclCVersion{3, 0}, hwInfo.capabilityTable.clVersionSupport == 30}};

    StackVec<OclCVersion, 5> ret;
    for (const auto &version : supportedVersionsMatrix) {
        if (version.supported && ((0 == max.major) || (max >= version.num))) {
            ret.push_back(version.num);
        }
    }

    return ret;
}

template <PRODUCT_FAMILY gfxProduct>
bool CompilerProductHelperHw<gfxProduct>::isHeaplessModeEnabled(const HardwareInfo &hwInfo) const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool CompilerProductHelperHw<gfxProduct>::isHeaplessStateInitEnabled([[maybe_unused]] bool heaplessModeEnabled) const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
uint32_t CompilerProductHelperHw<gfxProduct>::matchRevisionIdWithProductConfig(HardwareIpVersion ipVersion, uint32_t revisionID) const {
    return ipVersion.value;
}

template <PRODUCT_FAMILY gfxProduct>
bool CompilerProductHelperHw<gfxProduct>::isMatrixMultiplyAccumulateSupported(const ReleaseHelper *releaseHelper) const {
    if (releaseHelper) {
        return releaseHelper->isMatrixMultiplyAccumulateSupported();
    }

    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool CompilerProductHelperHw<gfxProduct>::isDotProductAccumulateSystolicSupported(const ReleaseHelper *releaseHelper) const {
    if (releaseHelper) {
        return releaseHelper->isDotProductAccumulateSystolicSupported();
    }

    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool CompilerProductHelperHw<gfxProduct>::isSplitMatrixMultiplyAccumulateSupported(const ReleaseHelper *releaseHelper) const {
    if (releaseHelper) {
        return releaseHelper->isSplitMatrixMultiplyAccumulateSupported();
    }
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool CompilerProductHelperHw<gfxProduct>::isBFloat16ConversionSupported(const ReleaseHelper *releaseHelper) const {
    if (releaseHelper) {
        return releaseHelper->isBFloat16ConversionSupported();
    }
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
void CompilerProductHelperHw<gfxProduct>::adjustHwInfoForIgc(HardwareInfo &hwInfo) const {
}

template <PRODUCT_FAMILY gfxProduct>
void CompilerProductHelperHw<gfxProduct>::getKernelFp16AtomicCapabilities(const ReleaseHelper *releaseHelper, uint32_t &fp16Caps) const {
    fp16Caps = (0u | FpAtomicExtFlags::minMaxAtomicCaps | FpAtomicExtFlags::loadStoreAtomicCaps);
    if (releaseHelper) {
        fp16Caps |= releaseHelper->getAdditionalFp16Caps();
    }
}

template <PRODUCT_FAMILY gfxProduct>
void CompilerProductHelperHw<gfxProduct>::getKernelFp32AtomicCapabilities(uint32_t &fp32Caps) const {
    fp32Caps = (0u | FpAtomicExtFlags::minMaxAtomicCaps | FpAtomicExtFlags::loadStoreAtomicCaps | FpAtomicExtFlags::addAtomicCaps);
}

template <PRODUCT_FAMILY gfxProduct>
void CompilerProductHelperHw<gfxProduct>::getKernelFp64AtomicCapabilities(uint32_t &fp64Caps) const {
    fp64Caps = (0u | FpAtomicExtFlags::minMaxAtomicCaps | FpAtomicExtFlags::loadStoreAtomicCaps | FpAtomicExtFlags::addAtomicCaps);
}

template <PRODUCT_FAMILY gfxProduct>
void CompilerProductHelperHw<gfxProduct>::getKernelCapabilitiesExtra(const ReleaseHelper *releaseHelper, uint32_t &extraCaps) const {
    if (releaseHelper) {
        extraCaps |= releaseHelper->getAdditionalExtraCaps();
    }
}

template <PRODUCT_FAMILY gfxProduct>
bool CompilerProductHelperHw<gfxProduct>::isBindlessAddressingDisabled(const ReleaseHelper *releaseHelper) const {
    if (releaseHelper) {
        return releaseHelper->isBindlessAddressingDisabled();
    }
    return true;
}

template <PRODUCT_FAMILY gfxProduct>
bool CompilerProductHelperHw<gfxProduct>::isForceBindlessRequired(const HardwareInfo &hwInfo) const {
    return this->isHeaplessModeEnabled(hwInfo);
}

template <PRODUCT_FAMILY gfxProduct>
const char *CompilerProductHelperHw<gfxProduct>::getCustomIgcLibraryName() const {
    if (debugManager.flags.IgcLibraryName.get() != "unk") {
        return debugManager.flags.IgcLibraryName.getRef().c_str();
    }
    return nullptr;
}

template <PRODUCT_FAMILY gfxProduct>
bool CompilerProductHelperHw<gfxProduct>::useIgcAsFcl() const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
const char *CompilerProductHelperHw<gfxProduct>::getFinalizerLibraryName() const {
    return nullptr;
}

template <PRODUCT_FAMILY gfxProduct>
IGC::CodeType::CodeType_t CompilerProductHelperHw<gfxProduct>::getPreferredIntermediateRepresentation() const {
    return IGC::CodeType::spirV;
}

} // namespace NEO
