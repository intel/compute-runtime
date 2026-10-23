/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/os_library.h"

#include "level_zero/tools/source/sysman/linux/udev/udev_lib_imp.h"

#include <map>
#include <string>
#include <vector>

namespace L0 {
namespace ult {

class MockUdevOsLibrary : public NEO::OsLibrary {
  public:
    bool isLoaded() override { return true; }
    void *getProcAddress(const std::string &procName) override;
    std::string getFullPath() override {
        return std::string();
    }

    void deleteEntryPoint(const std::string &procName);

    static NEO::OsLibrary *load(const NEO::OsLibraryCreateProperties &properties);

    MockUdevOsLibrary();
    ~MockUdevOsLibrary() override = default;

    static struct udev *const mockUdev;
    static struct udev_monitor *const mockUdevMonitor;
    static struct udev_device *const mockUdevDevice;

    static bool mockMonitorCreationFails;
    static int mockEnableReceivingResult;
    static int mockMonitorFd;
    static dev_t mockDevnum;
    static const char *mockAction;
    static const char *mockPropertyKey;
    static const char *mockPropertyValue;
    static std::string loadedLibraryName;
    static const std::vector<std::string> mockSubsystems;
    static uint32_t filterAddMatchSubsystemCallCount;
    static uint32_t deviceUnrefCallCount;

  private:
    std::map<std::string, void *> funcMap;
};

} // namespace ult
} // namespace L0
