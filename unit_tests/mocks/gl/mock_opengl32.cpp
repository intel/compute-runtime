/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include <string.h>
#include "unit_tests/helpers/windows/mock_function.h"
#include "GL/gl.h"

#undef wglGetProcAddress
#pragma warning(disable : 4273)

extern "C" {
const char *glString = "Intel";
const char *glVersion = "4.0";
const unsigned char *_stdcall glGetString(unsigned int name) {
    if (name == GL_VENDOR)
        return reinterpret_cast<const unsigned char *>(glString);
    if (name == GL_VERSION)
        return reinterpret_cast<const unsigned char *>(glVersion);
    return reinterpret_cast<const unsigned char *>("");
};
void glSetString(const char *name, unsigned int var) {
    if (var == GL_VENDOR) {
        glString = name;
    } else if (var == GL_VERSION) {
        glVersion = name;
    }
};

PROC WINAPI wglGetProcAddress(LPCSTR name) {
    return nullptr;
}

void *__stdcall mockLoader(const char *name) {
    if (strcmp(name, "realFunction") == 0) {
        return *realFunction;
    }
    return nullptr;
}
}
