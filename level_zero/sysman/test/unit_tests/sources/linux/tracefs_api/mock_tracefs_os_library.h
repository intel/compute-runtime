/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/os_library.h"

#include "level_zero/sysman/source/shared/linux/tracefs_api/sysman_tracefs_api.h"

#include <cstdint>
#include <fcntl.h>
#include <map>

struct tracefs_instance {}; // NOLINT(readability-identifier-naming)

struct tep_handle {}; // NOLINT(readability-identifier-naming)

namespace L0 {
namespace Sysman {
namespace ult {

class MockTraceFsOsLibrary : public NEO::OsLibrary {
  public:
    bool isLoaded() override { return true; }
    void *getProcAddress(const std::string &procName) override;
    std::string getFullPath() override {
        return std::string();
    }

    void deleteEntryPoint(const std::string &procName);

    MockTraceFsOsLibrary();
    ~MockTraceFsOsLibrary() override = default;

    static struct tracefs_instance mockTraceFsInstance;
    static struct tep_handle mockTepHandle;

    static const char *mockInstanceName;
    static const char *mockTraceDir;
    static const char *mockFileName;
    static const char *mockFileContent;
    static const char *mockSystemName;
    static const char *mockEventName;
    static int mockFileMode;
    static int mockFileFd;
    static int mockBufferPercent;
    static long long mockBufferSize;
    static int mockCpu;
    static size_t mockSize;

    static uint32_t instanceDestroyCallCount;
    static uint32_t instanceFreeCallCount;
    static uint32_t putTracingFileCallCount;

  private:
    std::map<std::string, void *> funcMap;
};

} // namespace ult
} // namespace Sysman
} // namespace L0
