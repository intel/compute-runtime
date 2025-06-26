/*
 * Copyright (C) 2023-2025 Intel Corporation
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

    SUCCESS_OR_TERMINATE(zeContextDestroy(context));

    LevelZeroBlackBoxTests::printResult(aubMode, outputValidationSuccessful, blackBoxName);
    outputValidationSuccessful = aubMode ? true : outputValidationSuccessful;
    return (outputValidationSuccessful ? 0 : 1);
}
