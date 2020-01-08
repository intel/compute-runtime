/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/platform/extensions.h"

#include "core/helpers/hw_info.h"

#include <string>

namespace NEO {

const char *deviceExtensionsList = "cl_khr_3d_image_writes "
                                   "cl_khr_byte_addressable_store "
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
                                   "cl_khr_create_command_queue ";

std::string getExtensionsList(const HardwareInfo &hwInfo) {
    std::string allExtensionsList;
    allExtensionsList.reserve(1000);

    allExtensionsList.append(deviceExtensionsList);

    if (hwInfo.capabilityTable.clVersionSupport >= 21) {
        allExtensionsList += "cl_khr_subgroups ";
        allExtensionsList += "cl_khr_il_program ";
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

    if (hwInfo.capabilityTable.supportsVme) {
        allExtensionsList += "cl_intel_motion_estimation cl_intel_device_side_avc_motion_estimation ";
    }

    return allExtensionsList;
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
    extensionsList = " -cl-ext=-all,+" + extensionsList + " ";
    return extensionsList;
}

} // namespace NEO
