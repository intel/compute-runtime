/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/windows/wddm/wddm_residency_logger_defs.h"

namespace NEO {
namespace ResLog {
extern uint32_t mockFopenCalled;
extern uint32_t mockVfptrinfCalled;
extern uint32_t mockFcloseCalled;

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
} // namespace ResLog
} // namespace NEO
