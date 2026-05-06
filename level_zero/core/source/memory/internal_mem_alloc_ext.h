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

constexpr ze_structure_type_t ZE_STRUCTURE_TYPE_WRITE_COMBINED_MEM_ALLOC_EXT_DESC = static_cast<ze_structure_type_t>(0x00050004);

struct ze_write_combined_mem_alloc_ext_desc_t {
    ze_structure_type_t stype = ZE_STRUCTURE_TYPE_WRITE_COMBINED_MEM_ALLOC_EXT_DESC;
    const void *pNext = nullptr;
};

// NOLINTEND(readability-identifier-naming)

} // namespace L0
