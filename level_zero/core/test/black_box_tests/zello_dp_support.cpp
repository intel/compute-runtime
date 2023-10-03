/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "../../../include/ze_intel_gpu.h"
#include "zello_common.h"

#include <cstring>
#include <iomanip>

int main(int argc, char *argv[]) {
    const std::string blackBoxName = "Zello DP Support";
    verbose = isVerbose(argc, argv);
    bool aubMode = isAubMode(argc, argv);

    ze_context_handle_t context = {};
    ze_driver_handle_t driverHandle = {};
    auto devices = zelloInitContextAndGetDevices(context, driverHandle);
    auto device = devices[0];

    bool outputValidationSuccessful = true;

    ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    SUCCESS_OR_TERMINATE(zeDeviceGetProperties(device, &deviceProperties));
    printDeviceProperties(deviceProperties);

    // Gather Dot Product (DP) support from driver
    ze_device_module_properties_t deviceModProps = {ZE_STRUCTURE_TYPE_DEVICE_MODULE_PROPERTIES};
    ze_intel_device_module_dp_exp_properties_t moduleDpProps = {ZE_STRUCTURE_INTEL_DEVICE_MODULE_DP_EXP_PROPERTIES};
    deviceModProps.pNext = &moduleDpProps;

    SUCCESS_OR_TERMINATE(zeDeviceGetModuleProperties(device, &deviceModProps));

    if (moduleDpProps.flags & ZE_INTEL_DEVICE_MODULE_EXP_FLAG_DP4A) {
        printf("DP4A supported\n");
    }

    if (moduleDpProps.flags & ZE_INTEL_DEVICE_MODULE_EXP_FLAG_DPAS) {
        printf("DPAS supported\n");
    }

    SUCCESS_OR_TERMINATE(zeContextDestroy(context));

    printResult(aubMode, outputValidationSuccessful, blackBoxName);
    outputValidationSuccessful = aubMode ? true : outputValidationSuccessful;
    return (outputValidationSuccessful ? 0 : 1);
}