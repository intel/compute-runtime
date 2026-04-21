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
    uint32_t count;
    const size_t *inputSizes;
    const uint8_t **pInputModules;
};

// NOLINTEND(readability-identifier-naming)

} // namespace L0
