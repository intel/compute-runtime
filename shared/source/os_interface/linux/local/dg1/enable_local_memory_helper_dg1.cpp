/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/local_memory_helper.h"

namespace NEO {

static EnableProductLocalMemoryHelper<IGFX_DG1> enableLocalMemHelperDG1;

} // namespace NEO
