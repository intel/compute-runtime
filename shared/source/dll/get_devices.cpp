/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/create_command_stream_impl.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"

namespace NEO {

bool prepareDeviceEnvironments(ExecutionEnvironment &executionEnvironment) {
    auto returnValue = prepareDeviceEnvironmentsImpl(executionEnvironment);

    if (DebugManager.flags.Force32BitDriverSupport.get() != -1) {
        return returnValue;
    }

    if (returnValue) {
        auto i = 0u;
        while (i < executionEnvironment.rootDeviceEnvironments.size()) {
            bool unsupportedDeviceDetected = false;

            auto &featureTable = executionEnvironment.rootDeviceEnvironments[i]->getHardwareInfo()->featureTable;
            if (!featureTable.ftrRcsNode && !featureTable.ftrCCSNode) {
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

} // namespace NEO
