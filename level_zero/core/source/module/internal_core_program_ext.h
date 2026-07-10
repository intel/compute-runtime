/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <level_zero/ze_api.h>

namespace L0 {

// NOLINTBEGIN(readability-identifier-naming)

constexpr ze_structure_type_t ZE_STRUCTURE_TYPE_MODULE_PROGRAM_LLVMBC_EXT_DESC = static_cast<ze_structure_type_t>(0x00051000);

struct ze_module_program_llvmbc_exp_desc_t {
    ze_structure_type_t stype = ZE_STRUCTURE_TYPE_MODULE_PROGRAM_LLVMBC_EXT_DESC;
    const void *pNext = nullptr;
    uint32_t count = 0u;
    const size_t *inputSizes = nullptr;
    const uint8_t **pInputModules = nullptr;
};

constexpr ze_structure_type_t ZE_STRUCTURE_TYPE_MODULE_PROGRAM_HEADERS_EXT_DESC = static_cast<ze_structure_type_t>(0x00051001);

struct ze_module_program_headers_exp_desc_t {
    ze_structure_type_t stype = ZE_STRUCTURE_TYPE_MODULE_PROGRAM_HEADERS_EXT_DESC;
    const void *pNext = nullptr;
    uint32_t count = 0;
    const char **headerSources = nullptr;
    const size_t *headerSourceSizes = nullptr;
    const char **headerNames = nullptr;
};

constexpr ze_structure_type_t ZE_STRUCTURE_TYPE_MODULE_OCL_EXTENSIONS_EXT_DESC = static_cast<ze_structure_type_t>(0x00051002);

// Carries LEO's -ocl-version + device-extensions options so its SPIR-V build/link uses the OpenCL-C
// compiler contract; native L0 clients omit it.
struct ze_module_ocl_extensions_exp_desc_t {
    ze_structure_type_t stype = ZE_STRUCTURE_TYPE_MODULE_OCL_EXTENSIONS_EXT_DESC;
    const void *pNext = nullptr;
    const char *pInternalBuildOptions = nullptr;
};

// NOLINTEND(readability-identifier-naming)

} // namespace L0
