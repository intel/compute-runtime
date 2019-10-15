/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "public/cl_ext_private.h"

namespace NEO {

struct MemoryPropertiesBase {
    MemoryPropertiesBase(cl_mem_flags flags) : flags(flags) {}
    cl_mem_flags flags = 0;
    cl_mem_flags_intel flagsIntel = 0;
};

} // namespace NEO
