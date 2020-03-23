/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstdlib>
#include <memory>
#include <vector>

namespace NEO {

class ExecutionEnvironment;
class Device;
bool prepareDeviceEnvironments(ExecutionEnvironment &executionEnvironment);
class DeviceFactory {
  public:
    static bool prepareDeviceEnvironments(ExecutionEnvironment &executionEnvironment);
    static bool prepareDeviceEnvironmentsForProductFamilyOverride(ExecutionEnvironment &executionEnvironment);
    static std::vector<std::unique_ptr<Device>> createDevices(ExecutionEnvironment &executionEnvironment);
    static bool isHwModeSelected();

    static std::unique_ptr<Device> (*createRootDeviceFunc)(ExecutionEnvironment &executionEnvironment, uint32_t rootDeviceIndex);
};
} // namespace NEO
