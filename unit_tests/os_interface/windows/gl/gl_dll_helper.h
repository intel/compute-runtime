/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/os_interface/os_library.h"
#include "Gl/gl.h"

using namespace OCLRT;
namespace Os {
extern const char *openglDllName;
}

typedef void (*GLString)(const char *, unsigned int);
struct glDllHelper {
  public:
    glDllHelper() {
        glDllLoad.reset(OsLibrary::load(Os::openglDllName));
        if (glDllLoad) {
            glSetString = (*glDllLoad)["glSetString"];
            UNRECOVERABLE_IF(glSetString == nullptr);
        }
    }
    ~glDllHelper() {
        if (glDllLoad) {
            glSetString("Intel", GL_VENDOR);
            glSetString("4.0", GL_VERSION);
        }
    }
    GLString glSetString = nullptr;

  private:
    std::unique_ptr<OsLibrary> glDllLoad;
};
