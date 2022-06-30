/*
 * Copyright (C) 2018-2022 Intel Corporation
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
class glFunctionHelper {
  public:
    glFunctionHelper::glFunctionHelper(OsLibrary *glLibrary, const std::string &functionName) {
        glFunctionPtr = (*glLibrary)[functionName];
    }

    ConvertibleProcAddr operator[](const char *name) {
        return ConvertibleProcAddr{glFunctionPtr(name)};
    }

  protected:
    // clang-format off
    PROC(__stdcall *glFunctionPtr)(LPCSTR Arg1) = nullptr;
    // clang-format on
};
}; // namespace NEO
