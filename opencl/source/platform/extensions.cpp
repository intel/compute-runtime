/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/platform/extensions.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/string.h"

#include <string>

namespace NEO {
const char *deviceExtensionsList = "cl_khr_byte_addressable_store "
                                   "cl_khr_fp16 "
                                   "cl_khr_global_int32_base_atomics "
                                   "cl_khr_global_int32_extended_atomics "
                                   "cl_khr_icd "
                                   "cl_khr_local_int32_base_atomics "
                                   "cl_khr_local_int32_extended_atomics "
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
                                   "cl_intel_mem_force_host_memory ";

std::string getExtensionsList(const HardwareInfo &hwInfo) {
    std::string allExtensionsList;
    allExtensionsList.reserve(1000);

    allExtensionsList.append(deviceExtensionsList);

    if (hwInfo.capabilityTable.supportsOcl21Features) {
        allExtensionsList += "cl_khr_subgroups ";
        if (hwInfo.capabilityTable.supportsVme) {
            allExtensionsList += "cl_intel_spirv_device_side_avc_motion_estimation ";
        }
        if (hwInfo.capabilityTable.supportsImages) {
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

void getOpenclCFeaturesList(const HardwareInfo &hwInfo, StackVec<cl_name_version, 15> &openclCFeatures) {
    cl_name_version openClCFeature;
    openClCFeature.version = CL_MAKE_VERSION(3, 0, 0);

    strcpy_s(openClCFeature.name, CL_NAME_VERSION_MAX_NAME_SIZE, "__opencl_c_atomic_order_acq_rel");
    openclCFeatures.push_back(openClCFeature);

    strcpy_s(openClCFeature.name, CL_NAME_VERSION_MAX_NAME_SIZE, "__opencl_c_int64");
    openclCFeatures.push_back(openClCFeature);

    if (hwInfo.capabilityTable.supportsImages) {
        strcpy_s(openClCFeature.name, CL_NAME_VERSION_MAX_NAME_SIZE, "__opencl_c_3d_image_writes");
        openclCFeatures.push_back(openClCFeature);

        strcpy_s(openClCFeature.name, CL_NAME_VERSION_MAX_NAME_SIZE, "__opencl_c_images");
        openclCFeatures.push_back(openClCFeature);
    }

    if (hwInfo.capabilityTable.supportsOcl21Features) {
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

        strcpy_s(openClCFeature.name, CL_NAME_VERSION_MAX_NAME_SIZE, "__opencl_c_read_write_images");
        openclCFeatures.push_back(openClCFeature);

        strcpy_s(openClCFeature.name, CL_NAME_VERSION_MAX_NAME_SIZE, "__opencl_c_work_group_collective_functions");
        openclCFeatures.push_back(openClCFeature);

        strcpy_s(openClCFeature.name, CL_NAME_VERSION_MAX_NAME_SIZE, "__opencl_c_subgroups");
        openclCFeatures.push_back(openClCFeature);
    }

    auto forceDeviceEnqueueSupport = DebugManager.flags.ForceDeviceEnqueueSupport.get();
    if ((hwInfo.capabilityTable.supportsDeviceEnqueue && (forceDeviceEnqueueSupport == -1)) ||
        (forceDeviceEnqueueSupport == 1)) {
        strcpy_s(openClCFeature.name, CL_NAME_VERSION_MAX_NAME_SIZE, "__opencl_c_device_enqueue");
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

std::string removeLastSpace(std::string &processedString) {
    if (processedString.size() > 0) {
        if (*processedString.rbegin() == ' ') {
            processedString.pop_back();
        }
    }
    return processedString;
}

std::string convertEnabledExtensionsToCompilerInternalOptions(const char *enabledExtensions) {
    std::string extensionsList = enabledExtensions;
    extensionsList.reserve(1000);
    removeLastSpace(extensionsList);
    std::string::size_type pos = 0;
    while ((pos = extensionsList.find(" ", pos)) != std::string::npos) {
        extensionsList.replace(pos, 1, ",+");
    }
    extensionsList = " -cl-ext=-all,+" + extensionsList + ",+cl_khr_3d_image_writes ";

    return extensionsList;
}

std::string convertEnabledOclCFeaturesToCompilerInternalOptions(StackVec<cl_name_version, 15> &openclCFeatures) {
    UNRECOVERABLE_IF(openclCFeatures.empty());
    std::string featuresList;
    featuresList.reserve(500);
    featuresList = " -cl-feature=";
    for (auto &feature : openclCFeatures) {
        featuresList.append("+");
        featuresList.append(feature.name);
        featuresList.append(",");
    }
    featuresList[featuresList.size() - 1] = ' ';
    return featuresList;
}

} // namespace NEO
