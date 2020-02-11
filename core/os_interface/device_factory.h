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
class HwDeviceId;

bool getDevices(size_t &numDevicesReturned, ExecutionEnvironment &executionEnvironment);
class DeviceFactory {
  public:
    using HwDeviceIds = std::vector<std::unique_ptr<HwDeviceId>>;
    static bool getDevices(size_t &numDevices, ExecutionEnvironment &executionEnvironment);
    static bool getDevicesForProductFamilyOverride(size_t &numDevices, ExecutionEnvironment &executionEnvironment);
    static void releaseDevices();
    static bool isHwModeSelected();

  protected:
    static size_t numDevices;
};

class DeviceFactoryCleaner {
  public:
    ~DeviceFactoryCleaner() { DeviceFactory::releaseDevices(); }
};

} // namespace NEO
