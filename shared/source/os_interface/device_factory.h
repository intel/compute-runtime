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
bool getDevices(size_t &numDevicesReturned, ExecutionEnvironment &executionEnvironment);
class DeviceFactory {
  public:
    static bool getDevices(size_t &numDevices, ExecutionEnvironment &executionEnvironment);
    static bool getDevicesForProductFamilyOverride(size_t &numDevices, ExecutionEnvironment &executionEnvironment);
    static std::vector<std::unique_ptr<Device>> createDevices(ExecutionEnvironment &executionEnvironment);
    static bool isHwModeSelected();

    static std::unique_ptr<Device> (*createRootDeviceFunc)(ExecutionEnvironment &executionEnvironment, uint32_t rootDeviceIndex);
};
} // namespace NEO
