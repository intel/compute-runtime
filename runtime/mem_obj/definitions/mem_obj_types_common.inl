/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "public/cl_ext_private.h"

namespace OCLRT {

struct MemoryPropertiesBase {
    MemoryPropertiesBase(cl_mem_flags flags) : flags(flags) {}
    cl_mem_flags flags = 0;
    cl_mem_flags_intel flags_intel = 0;
};

} // namespace OCLRT
