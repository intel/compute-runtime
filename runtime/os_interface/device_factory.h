/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstdlib>

namespace NEO {

class ExecutionEnvironment;

class DeviceFactory {
  public:
    static bool getDevices(size_t &numDevices, ExecutionEnvironment &executionEnvironment);
    static bool getDevicesForProductFamilyOverride(size_t &numDevices, ExecutionEnvironment &executionEnvironment);
    static void releaseDevices();

  protected:
    static size_t numDevices;
};

class DeviceFactoryCleaner {
  public:
    ~DeviceFactoryCleaner() { DeviceFactory::releaseDevices(); }
};

} // namespace NEO
