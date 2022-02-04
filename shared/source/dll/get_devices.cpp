/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/create_command_stream_impl.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"

namespace NEO {

bool prepareDeviceEnvironments(ExecutionEnvironment &executionEnvironment, std::string &osPciPath, const uint32_t rootDeviceIndex) {
    bool returnValue = false;
    if (osPciPath.empty()) {
        returnValue = prepareDeviceEnvironmentsImpl(executionEnvironment);
    } else {
        returnValue = prepareDeviceEnvironmentImpl(executionEnvironment, osPciPath, rootDeviceIndex);
    }

    if (DebugManager.flags.Force32BitDriverSupport.get() != -1) {
        return returnValue;
    }

    if (returnValue) {
        auto i = 0u;
        while (i < executionEnvironment.rootDeviceEnvironments.size()) {
            bool unsupportedDeviceDetected = false;

            auto &featureTable = executionEnvironment.rootDeviceEnvironments[i]->getHardwareInfo()->featureTable;
            if (!featureTable.flags.ftrRcsNode && !featureTable.flags.ftrCCSNode) {
                unsupportedDeviceDetected = true;
            }

            if (unsupportedDeviceDetected) {
                executionEnvironment.rootDeviceEnvironments.erase(executionEnvironment.rootDeviceEnvironments.begin() + i);
            } else {
                i++;
            }
        }
    }

    return returnValue && executionEnvironment.rootDeviceEnvironments.size() > 0;
}

bool prepareDeviceEnvironments(ExecutionEnvironment &executionEnvironment) {
    std::string path = "";
    return prepareDeviceEnvironments(executionEnvironment, path, 0u);
}

bool prepareDeviceEnvironment(ExecutionEnvironment &executionEnvironment, std::string &osPciPath, const uint32_t rootDeviceIndex) {
    return prepareDeviceEnvironments(executionEnvironment, osPciPath, rootDeviceIndex);
}
const HardwareInfo *getDefaultHwInfo() {
    return &DEFAULT_PLATFORM::hwInfo;
}
} // namespace NEO
