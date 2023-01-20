/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/oclc_extensions.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/string.h"

#include <sstream>
#include <string>

namespace NEO {
const char *deviceExtensionsList = "cl_khr_byte_addressable_store "
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
                                   "cl_khr_suggested_local_work_size "
                                   "cl_intel_split_work_group_barrier ";

std::string getExtensionsList(const HardwareInfo &hwInfo) {
    std::string allExtensionsList;
    allExtensionsList.reserve(1000);

    allExtensionsList.append(deviceExtensionsList);

    if (hwInfo.capabilityTable.supportsOcl21Features) {
        allExtensionsList += "cl_khr_subgroups ";
        if (hwInfo.capabilityTable.supportsVme) {
            allExtensionsList += "cl_intel_spirv_device_side_avc_motion_estimation ";
        }
        if (hwInfo.capabilityTable.supportsMediaBlock) {
            allExtensionsList += "cl_intel_spirv_media_block_io ";
        }
        allExtensionsList += "cl_intel_spirv_subgroups ";
        allExtensionsList += "cl_khr_spirv_no_integer_wrap_decoration ";
    }

    if (hwInfo.capabilityTable.ftrSupportsFP64) {
        allExtensionsList += "cl_khr_fp64 ";
    }

    if (hwInfo.capabilityTable.ftrSupportsInteger64BitAtomics) {
        allExtensionsList += "cl_khr_int64_base_atomics ";
        allExtensionsList += "cl_khr_int64_extended_atomics ";
    }

    if (hwInfo.capabilityTable.supportsImages) {
        allExtensionsList += "cl_khr_3d_image_writes ";
    }

    if (hwInfo.capabilityTable.supportsVme) {
        allExtensionsList += "cl_intel_motion_estimation cl_intel_device_side_avc_motion_estimation ";
    }

    return allExtensionsList;
}

void getOpenclCFeaturesList(const HardwareInfo &hwInfo, OpenClCFeaturesContainer &openclCFeatures) {
    cl_name_version openClCFeature;
    openClCFeature.version = CL_MAKE_VERSION(3, 0, 0);

    strcpy_s(openClCFeature.name, CL_NAME_VERSION_MAX_NAME_SIZE, "__opencl_c_int64");
    openclCFeatures.push_back(openClCFeature);

    if (hwInfo.capabilityTable.supportsImages) {
        strcpy_s(openClCFeature.name, CL_NAME_VERSION_MAX_NAME_SIZE, "__opencl_c_3d_image_writes");
        openclCFeatures.push_back(openClCFeature);

        strcpy_s(openClCFeature.name, CL_NAME_VERSION_MAX_NAME_SIZE, "__opencl_c_images");
        openclCFeatures.push_back(openClCFeature);

        strcpy_s(openClCFeature.name, CL_NAME_VERSION_MAX_NAME_SIZE, "__opencl_c_read_write_images");
        openclCFeatures.push_back(openClCFeature);
    }

    if (hwInfo.capabilityTable.supportsOcl21Features) {
        strcpy_s(openClCFeature.name, CL_NAME_VERSION_MAX_NAME_SIZE, "__opencl_c_atomic_order_acq_rel");
        openclCFeatures.push_back(openClCFeature);

        strcpy_s(openClCFeature.name, CL_NAME_VERSION_MAX_NAME_SIZE, "__opencl_c_atomic_order_seq_cst");
        openclCFeatures.push_back(openClCFeature);

        strcpy_s(openClCFeature.name, CL_NAME_VERSION_MAX_NAME_SIZE, "__opencl_c_atomic_scope_all_devices");
        openclCFeatures.push_back(openClCFeature);

        strcpy_s(openClCFeature.name, CL_NAME_VERSION_MAX_NAME_SIZE, "__opencl_c_atomic_scope_device");
        openclCFeatures.push_back(openClCFeature);

        strcpy_s(openClCFeature.name, CL_NAME_VERSION_MAX_NAME_SIZE, "__opencl_c_generic_address_space");
        openclCFeatures.push_back(openClCFeature);

        strcpy_s(openClCFeature.name, CL_NAME_VERSION_MAX_NAME_SIZE, "__opencl_c_program_scope_global_variables");
        openclCFeatures.push_back(openClCFeature);

        strcpy_s(openClCFeature.name, CL_NAME_VERSION_MAX_NAME_SIZE, "__opencl_c_work_group_collective_functions");
        openclCFeatures.push_back(openClCFeature);

        strcpy_s(openClCFeature.name, CL_NAME_VERSION_MAX_NAME_SIZE, "__opencl_c_subgroups");
        openclCFeatures.push_back(openClCFeature);
    }

    auto forcePipeSupport = DebugManager.flags.ForcePipeSupport.get();
    if ((hwInfo.capabilityTable.supportsPipes && (forcePipeSupport == -1)) ||
        (forcePipeSupport == 1)) {
        strcpy_s(openClCFeature.name, CL_NAME_VERSION_MAX_NAME_SIZE, "__opencl_c_pipes");
        openclCFeatures.push_back(openClCFeature);
    }

    auto forceFp64Support = DebugManager.flags.OverrideDefaultFP64Settings.get();
    if ((hwInfo.capabilityTable.ftrSupportsFP64 && (forceFp64Support == -1)) ||
        (forceFp64Support == 1)) {
        strcpy_s(openClCFeature.name, CL_NAME_VERSION_MAX_NAME_SIZE, "__opencl_c_fp64");
        openclCFeatures.push_back(openClCFeature);
    }
}

std::string convertEnabledExtensionsToCompilerInternalOptions(const char *enabledExtensions,
                                                              OpenClCFeaturesContainer &openclCFeatures) {

    std::string extensionsList = enabledExtensions;
    extensionsList.reserve(1500);
    extensionsList = " -cl-ext=-all,";
    std::istringstream extensionsStringStream(enabledExtensions);
    std::string extension;
    while (extensionsStringStream >> extension) {
        extensionsList.append("+");
        extensionsList.append(extension);
        extensionsList.append(",");
    }
    for (auto &feature : openclCFeatures) {
        extensionsList.append("+");
        extensionsList.append(feature.name);
        extensionsList.append(",");
    }
    extensionsList[extensionsList.size() - 1] = ' ';

    return extensionsList;
}

std::string getOclVersionCompilerInternalOption(unsigned int oclVersion) {
    switch (oclVersion) {
    case 30:
        return "-ocl-version=300 ";
    case 21:
        return "-ocl-version=210 ";
    default:
        return "-ocl-version=120 ";
    }
}

} // namespace NEO
