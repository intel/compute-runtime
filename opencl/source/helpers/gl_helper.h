/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/os_library.h"

#include "GL/gl.h"
#include "gl_types.h"

namespace NEO {
class GlFunctionHelper {
  public:
    GlFunctionHelper(OsLibrary *glLibrary, const std::string &functionName) {
        glFunctionPtr = (*glLibrary)[functionName];
    }

    ConvertibleProcAddr operator[](const char *name) {
        return ConvertibleProcAddr{reinterpret_cast<void *>(glFunctionPtr(name))};
    }

  protected:
    GLFunctionType glFunctionPtr = nullptr;
};
}; // namespace NEO
