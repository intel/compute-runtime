/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "mem_obj_types.h"
#include "memory_properties_flags.h"

namespace NEO {

class MemoryPropertiesFlagsParser {
  public:
    static void addExtraMemoryPropertiesFlags(MemoryPropertiesFlags &propertiesFlags, MemoryProperties properties);

    static MemoryPropertiesFlags createMemoryPropertiesFlags(MemoryProperties properties);
};
} // namespace NEO
