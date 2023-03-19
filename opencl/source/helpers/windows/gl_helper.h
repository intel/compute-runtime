/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/os_library.h"

#include "GL/gl.h"

namespace Os {
extern const char *openglDllName;
}

namespace NEO {
class GlFunctionHelper {
  public:
    GlFunctionHelper::GlFunctionHelper(OsLibrary *glLibrary, const std::string &functionName) {
        glFunctionPtr = (*glLibrary)[functionName];
    }

    ConvertibleProcAddr operator[](const char *name) {
        return ConvertibleProcAddr{glFunctionPtr(name)};
    }

  protected:
    // clang-format off
    PROC(__stdcall *glFunctionPtr)(LPCSTR arg1) = nullptr;
    // clang-format on
};
}; // namespace NEO
