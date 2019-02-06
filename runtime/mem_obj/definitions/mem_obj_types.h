/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/mem_obj/definitions/mem_obj_types_common.inl"

namespace OCLRT {

struct MemoryProperties : MemoryPropertiesBase {
    MemoryProperties() : MemoryPropertiesBase(0) {}
    MemoryProperties(cl_mem_flags flags) : MemoryPropertiesBase(flags) {}
};

} // namespace OCLRT
