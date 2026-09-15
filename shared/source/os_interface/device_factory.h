/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace NEO {

class ExecutionEnvironment;
class Device;
class ProductHelper;
struct HardwareInfo;
struct RootDeviceEnvironment;
const HardwareInfo *getDefaultHwInfo();
bool prepareDeviceEnvironments(ExecutionEnvironment &executionEnvironment);
bool prepareDeviceEnvironment(ExecutionEnvironment &executionEnvironment, std::string &osPciPath, const uint32_t rootDeviceIndex);
bool isLeoDetectionEnabled();
void quitOclInitIfLeoEnabled(RootDeviceEnvironment &rootDeviceEnvironment);
class DeviceFactory {
  public:
    static bool prepareDeviceEnvironments(ExecutionEnvironment &executionEnvironment);
    static bool prepareDeviceEnvironment(ExecutionEnvironment &executionEnvironment, std::string &osPciPath, const uint32_t rootDeviceIndex);
    static bool prepareDeviceEnvironmentsForProductFamilyOverride(ExecutionEnvironment &executionEnvironment);
    static bool validateDeviceFlags(const ProductHelper &productHelper);
    static std::vector<std::unique_ptr<Device>> createDevices(ExecutionEnvironment &executionEnvironment);
    static std::unique_ptr<Device> createDevice(ExecutionEnvironment &executionEnvironment, std::string &osPciPath, const uint32_t rootDeviceIndex);
    static bool isHwModeSelected();
    static bool isTbxModeSelected();

    static std::unique_ptr<Device> (*createRootDeviceFunc)(ExecutionEnvironment &executionEnvironment, uint32_t rootDeviceIndex);
    static bool (*createMemoryManagerFunc)(ExecutionEnvironment &executionEnvironment);
    static bool isAllowedDeviceId(uint32_t deviceId, const std::string &deviceIdString);
};
} // namespace NEO
