/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/memory_manager/allocation_type.h"

#include <level_zero/ze_api.h>

namespace L0 {

// NOLINTBEGIN(readability-identifier-naming)

constexpr ze_structure_type_t ZE_STRUCTURE_TYPE_IMAGE_ALLOCATION_TYPE_EXT_DESC = static_cast<ze_structure_type_t>(0x00050000);

struct ze_image_allocation_type_ext_desc_t {
    ze_structure_type_t stype = ZE_STRUCTURE_TYPE_IMAGE_ALLOCATION_TYPE_EXT_DESC;
    const void *pNext = nullptr;
    NEO::AllocationType allocationType = NEO::AllocationType::unknown;
};

// NOLINTEND(readability-identifier-naming)

} // namespace L0
