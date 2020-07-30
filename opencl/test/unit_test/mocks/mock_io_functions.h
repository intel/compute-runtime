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
#include <unordered_map>

namespace NEO {
namespace IoFunctions {
extern uint32_t mockFopenCalled;
extern uint32_t mockVfptrinfCalled;
extern uint32_t mockFcloseCalled;
extern uint32_t mockGetenvCalled;

extern std::unordered_map<std::string, std::string> *mockableEnvValues;

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
    if (mockableEnvValues != nullptr && mockableEnvValues->find(name) != mockableEnvValues->end()) {
        return const_cast<char *>(mockableEnvValues->find(name)->second.c_str());
    }
    return getenv(name);
}

} // namespace IoFunctions
} // namespace NEO
