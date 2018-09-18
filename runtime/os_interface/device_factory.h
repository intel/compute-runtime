/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstdlib>

namespace OCLRT {

struct HardwareInfo;
class ExecutionEnvironment;

class DeviceFactory {
  public:
    static bool getDevices(HardwareInfo **pHWInfos, size_t &numDevices, ExecutionEnvironment &executionEnvironment);
    static bool getDevicesForProductFamilyOverride(HardwareInfo **pHWInfos, size_t &numDevices, ExecutionEnvironment &executionEnvironment);
    static void releaseDevices();

  protected:
    static size_t numDevices;
    static HardwareInfo *hwInfos;
};

class DeviceFactoryCleaner {
  public:
    ~DeviceFactoryCleaner() { DeviceFactory::releaseDevices(); }
};

} // namespace OCLRT
