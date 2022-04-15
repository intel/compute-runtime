/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/offline_compiler/source/ocloc_dll_options.h"

#include <utility>

namespace NEO {

class MockIgaDllGuard {
  public:
    MockIgaDllGuard(const char *newPath = "") : newDllPath{newPath} {
    }

    ~MockIgaDllGuard() {
        disable();
    }

    void enable() {
        oldDllPath = std::exchange(Os::igaDllName, newDllPath);
    }

    void disable() {
        if (oldDllPath) {
            Os::igaDllName = std::exchange(oldDllPath, nullptr);
        }
    }

  private:
    const char *oldDllPath{};
    const char *newDllPath{};
};

} // namespace NEO