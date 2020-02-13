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
bool getDevices(size_t &numDevicesReturned, ExecutionEnvironment &executionEnvironment);
class DeviceFactory {
  public:
    static bool getDevices(size_t &numDevices, ExecutionEnvironment &executionEnvironment);
    static bool getDevicesForProductFamilyOverride(size_t &numDevices, ExecutionEnvironment &executionEnvironment);
    static bool isHwModeSelected();
};
} // namespace NEO
