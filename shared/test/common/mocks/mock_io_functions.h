/*
 * Copyright (C) 2020-2022 Intel Corporation
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
extern uint32_t failAfterNFopenCount;
extern FILE *mockFopenReturned;
extern uint32_t mockVfptrinfCalled;
extern uint32_t mockFcloseCalled;
extern uint32_t mockGetenvCalled;
extern uint32_t mockFseekCalled;
extern uint32_t mockFtellCalled;
extern long int mockFtellReturn;
extern uint32_t mockRewindCalled;
extern uint32_t mockFreadCalled;
extern size_t mockFreadReturn;
extern uint32_t mockFwriteCalled;
extern size_t mockFwriteReturn;
extern bool mockVfptrinfUseStdioFunction;

extern std::unordered_map<std::string, std::string> *mockableEnvValues;

inline FILE *mockFopen(const char *filename, const char *mode) {
    mockFopenCalled++;
    if (failAfterNFopenCount > 0 && failAfterNFopenCount < mockFopenCalled) {
        return NULL;
    }
    return mockFopenReturned;
}

inline int mockVfptrinf(FILE *stream, const char *format, va_list arg) {
    mockVfptrinfCalled++;
    if (mockVfptrinfUseStdioFunction) {
        return vfprintf(stream, format, arg);
    }
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
    return nullptr;
}

inline int mockFseek(FILE *stream, long int offset, int origin) {
    mockFseekCalled++;
    return 0;
}

inline long int mockFtell(FILE *stream) {
    mockFtellCalled++;
    return mockFtellReturn;
}

inline void mockRewind(FILE *stream) {
    mockRewindCalled++;
}

inline size_t mockFread(void *ptr, size_t size, size_t count, FILE *stream) {
    mockFreadCalled++;
    return mockFreadReturn;
}

inline size_t mockFwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream) {
    mockFwriteCalled++;
    return mockFwriteReturn;
}

} // namespace IoFunctions
} // namespace NEO
