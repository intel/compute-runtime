/*
 * Copyright (C) 2023-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/ze_intel_gpu.h"

#include "zello_common.h"

int main(int argc, char *argv[]) {
    const std::string blackBoxName = "Zello DP Support";
    LevelZeroBlackBoxTests::verbose = LevelZeroBlackBoxTests::isVerbose(argc, argv);
    bool aubMode = LevelZeroBlackBoxTests::isAubMode(argc, argv);

    ze_context_handle_t context = {};
    ze_driver_handle_t driverHandle = {};
    auto devices = LevelZeroBlackBoxTests::zelloInitContextAndGetDevices(context, driverHandle);
    auto device = devices[0];

    bool outputValidationSuccessful = true;

    ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    SUCCESS_OR_TERMINATE(zeDeviceGetProperties(device, &deviceProperties));
    LevelZeroBlackBoxTests::printDeviceProperties(deviceProperties);

    // Gather Dot Product (DP) support from driver
    ze_device_module_properties_t deviceModProps = {ZE_STRUCTURE_TYPE_DEVICE_MODULE_PROPERTIES};
    ze_intel_device_module_dp_exp_properties_t moduleDpProps = {ZE_STRUCTURE_INTEL_DEVICE_MODULE_DP_EXP_PROPERTIES};
    deviceModProps.pNext = &moduleDpProps;

    SUCCESS_OR_TERMINATE(zeDeviceGetModuleProperties(device, &deviceModProps));

    if (moduleDpProps.flags & ZE_INTEL_DEVICE_MODULE_EXP_FLAG_DP4A) {
        std::cout << "DP4A supported" << std::endl;
    }

    if (moduleDpProps.flags & ZE_INTEL_DEVICE_MODULE_EXP_FLAG_DPAS) {
        std::cout << "DPAS supported" << std::endl;
    }

    ze_device_compute_dotproduct_ext_properties_t dotProductProps = {ZE_STRUCTURE_TYPE_DEVICE_COMPUTE_DOTPRODUCT_EXT_PROPERTIES};
    ze_device_compute_properties_t computeProps = {ZE_STRUCTURE_TYPE_DEVICE_COMPUTE_PROPERTIES};
    computeProps.pNext = &dotProductProps;

    SUCCESS_OR_TERMINATE(zeDeviceGetComputeProperties(device, &computeProps));

    std::cout << std::hex
              << "dp_caps 0x" << dotProductProps.dp_caps << std::endl
              << "dpv4_input_types 0x" << dotProductProps.dpv4_input_types << std::endl
              << "dpv4_output_types 0x" << dotProductProps.dpv4_output_types << std::endl
              << "dpas_input_types 0x" << dotProductProps.dpas_input_types << std::endl
              << "dpas_output_types 0x" << dotProductProps.dpas_output_types << std::endl
              << "bdpas_input_types 0x" << dotProductProps.bdpas_input_types << std::endl
              << "bdpas_output_types 0x" << dotProductProps.bdpas_output_types << std::endl
              << std::dec;

    const bool moduleDp4a = (moduleDpProps.flags & ZE_INTEL_DEVICE_MODULE_EXP_FLAG_DP4A) != 0;
    const bool moduleDpas = (moduleDpProps.flags & ZE_INTEL_DEVICE_MODULE_EXP_FLAG_DPAS) != 0;
    const bool computeDp4a = (dotProductProps.dp_caps & ZE_DEVICE_DP_CAPABILITY_FLAG_DEVICE_NON_SYSTOLIC_DPA4_SIMD_ALL) != 0;
    const bool computeDpas = (dotProductProps.dp_caps & ZE_DEVICE_DP_CAPABILITY_FLAG_DEVICE_SYSTOLIC_DPAS_DEPTH8) != 0;
    if (moduleDp4a != computeDp4a || moduleDpas != computeDpas) {
        std::cerr << "DP4A or DPAS support differs between module and compute dot product properties" << std::endl;
        outputValidationSuccessful = false;
    }

    LevelZeroBlackBoxTests::printResult(aubMode, outputValidationSuccessful, blackBoxName);
    outputValidationSuccessful = aubMode ? true : outputValidationSuccessful;
    return (outputValidationSuccessful ? 0 : 1);
}
