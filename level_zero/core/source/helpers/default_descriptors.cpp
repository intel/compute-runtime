/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/helpers/default_descriptors.h"

#include "level_zero/ze_intel_gpu.h"

namespace L0 {
namespace DefaultDescriptors {

const ze_context_desc_t contextDesc{
    .stype = ze_structure_type_t::ZE_STRUCTURE_TYPE_CONTEXT_DESC,
    .pNext = nullptr,
    .flags = static_cast<ze_context_flags_t>(0),
};
} // namespace DefaultDescriptors

} // namespace L0