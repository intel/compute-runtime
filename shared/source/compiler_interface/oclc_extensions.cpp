/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/oclc_extensions.h"

#include "shared/source/compiler_interface/oclc_extensions_extra.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/string.h"

#include <sstream>
#include <string>

namespace NEO {

void getOpenclCFeaturesList(const HardwareInfo &hwInfo, OpenClCFeaturesContainer &openclCFeatures, const CompilerProductHelper &compilerProductHelper, const ReleaseHelper *releaseHelper) {
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

        if (hwInfo.capabilityTable.supportsFloatAtomics) {
            strcpy_s(openClCFeature.name, CL_NAME_VERSION_MAX_NAME_SIZE, "__opencl_c_ext_fp32_global_atomic_add");
            openclCFeatures.push_back(openClCFeature);

            strcpy_s(openClCFeature.name, CL_NAME_VERSION_MAX_NAME_SIZE, "__opencl_c_ext_fp32_local_atomic_add");
            openclCFeatures.push_back(openClCFeature);

            strcpy_s(openClCFeature.name, CL_NAME_VERSION_MAX_NAME_SIZE, "__opencl_c_ext_fp32_global_atomic_min_max");
            openclCFeatures.push_back(openClCFeature);

            strcpy_s(openClCFeature.name, CL_NAME_VERSION_MAX_NAME_SIZE, "__opencl_c_ext_fp32_local_atomic_min_max");
            openclCFeatures.push_back(openClCFeature);

            strcpy_s(openClCFeature.name, CL_NAME_VERSION_MAX_NAME_SIZE, "__opencl_c_ext_fp16_global_atomic_load_store");
            openclCFeatures.push_back(openClCFeature);

            strcpy_s(openClCFeature.name, CL_NAME_VERSION_MAX_NAME_SIZE, "__opencl_c_ext_fp16_local_atomic_load_store");
            openclCFeatures.push_back(openClCFeature);

            strcpy_s(openClCFeature.name, CL_NAME_VERSION_MAX_NAME_SIZE, "__opencl_c_ext_fp16_global_atomic_min_max");
            openclCFeatures.push_back(openClCFeature);

            strcpy_s(openClCFeature.name, CL_NAME_VERSION_MAX_NAME_SIZE, "__opencl_c_ext_fp16_local_atomic_min_max");
            openclCFeatures.push_back(openClCFeature);
        }
    }

    auto forceFp64Support = debugManager.flags.OverrideDefaultFP64Settings.get();
    if ((hwInfo.capabilityTable.ftrSupportsFP64 && (forceFp64Support == -1)) ||
        (forceFp64Support == 1)) {
        strcpy_s(openClCFeature.name, CL_NAME_VERSION_MAX_NAME_SIZE, "__opencl_c_fp64");
        openclCFeatures.push_back(openClCFeature);

        if (hwInfo.capabilityTable.supportsOcl21Features && hwInfo.capabilityTable.supportsFloatAtomics) {
            strcpy_s(openClCFeature.name, CL_NAME_VERSION_MAX_NAME_SIZE, "__opencl_c_ext_fp64_global_atomic_add");
            openclCFeatures.push_back(openClCFeature);

            strcpy_s(openClCFeature.name, CL_NAME_VERSION_MAX_NAME_SIZE, "__opencl_c_ext_fp64_local_atomic_add");
            openclCFeatures.push_back(openClCFeature);

            strcpy_s(openClCFeature.name, CL_NAME_VERSION_MAX_NAME_SIZE, "__opencl_c_ext_fp64_global_atomic_min_max");
            openclCFeatures.push_back(openClCFeature);

            strcpy_s(openClCFeature.name, CL_NAME_VERSION_MAX_NAME_SIZE, "__opencl_c_ext_fp64_local_atomic_min_max");
            openclCFeatures.push_back(openClCFeature);
        }
    }
    if (compilerProductHelper.isDotIntegerProductExtensionSupported()) {
        strcpy_s(openClCFeature.name, CL_NAME_VERSION_MAX_NAME_SIZE, "__opencl_c_integer_dot_product_input_4x8bit");
        openclCFeatures.push_back(openClCFeature);

        strcpy_s(openClCFeature.name, CL_NAME_VERSION_MAX_NAME_SIZE, "__opencl_c_integer_dot_product_input_4x8bit_packed");
        openclCFeatures.push_back(openClCFeature);
    }
    getOpenclCFeaturesListExtra(releaseHelper, openclCFeatures);
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

cl_version getOclCExtensionVersion(std::string name, cl_version defaultVer) {
    if (name.compare("cl_khr_integer_dot_product") == 0) {
        return CL_MAKE_VERSION(2u, 0, 0);
    } else if (name.compare("cl_intel_unified_shared_memory") == 0) {
        return CL_MAKE_VERSION(1u, 2u, 0);
    } else if (name.compare("cl_khr_external_memory") == 0) {
        return CL_MAKE_VERSION(0, 9u, 1u);
    } else {
        return defaultVer;
    }
}

} // namespace NEO
