/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/os_library.h"

#include "level_zero/sysman/source/shared/linux/tracefs_api/sysman_tracefs_api.h"

#include <fcntl.h>
#include <map>

struct tracefs_instance {}; // NOLINT(readability-identifier-naming)

struct tep_handle {}; // NOLINT(readability-identifier-naming)

namespace L0 {
namespace Sysman {
namespace ult {

class MockTraceFsOsLibrary : public NEO::OsLibrary {
  public:
    bool isLoaded() override { return false; }
    void *getProcAddress(const std::string &procName) override;
    std::string getFullPath() override {
        return std::string();
    }

    void deleteEntryPoint(const std::string &procName);

    MockTraceFsOsLibrary();

    static struct tracefs_instance mockTraceFsInstance;
    static struct tep_handle mockTepHandle;

    constexpr static const char *mockInstanceName = "test_instance";
    constexpr static const char *mockTraceDir = "/sys/kernel/tracing/instances/test";
    constexpr static const char *mockFileName = "trace";
    constexpr static const char *mockFileContent = "test trace data";
    constexpr static const char *mockSystemName = "i915";
    constexpr static const char *mockEventName = "test_event";
    constexpr static int mockFileMode = O_RDONLY;
    constexpr static int mockFileFd = 42;
    constexpr static int mockBufferPercent = 50;
    constexpr static long long mockBufferSize = 4096;
    constexpr static int mockCpu = 0;
    constexpr static size_t mockSize = 8192;

  private:
    std::map<std::string, void *> funcMap;
};

} // namespace ult
} // namespace Sysman
} // namespace L0
