/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/utilities/io_functions.h"

#include <cstdint>
#include <string>

namespace NEO {
namespace IoFunctions {
extern uint32_t mockFopenCalled;
extern uint32_t mockVfptrinfCalled;
extern uint32_t mockFcloseCalled;
extern uint32_t mockGetenvCalled;

extern bool returnMockEnvValue;
extern std::string mockEnvValue;

inline FILE *mockFopen(const char *filename, const char *mode) {
    mockFopenCalled++;
    return reinterpret_cast<FILE *>(0x40);
}

inline int mockVfptrinf(FILE *stream, const char *format, va_list arg) {
    mockVfptrinfCalled++;
    return 0x10;
}

inline int mockFclose(FILE *stream) {
    mockFcloseCalled++;
    return 0;
}

inline char *mockGetenv(const char *name) noexcept {
    mockGetenvCalled++;
    if (returnMockEnvValue) {
        return const_cast<char *>(mockEnvValue.c_str());
    }
    return getenv(name);
}

} // namespace IoFunctions
} // namespace NEO
